#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_utils.h"
#include "ml_api.h"

#include "app/app_internal.h"

static const char *GROUP_KEYS[] = {
    "date", "camera", "format", "orientation", "resolution"};

void AppClearError(AppContext *ctx) {
  if (!ctx) {
    return;
  }
  ctx->last_error[0] = '\0';
}

void AppSetError(AppContext *ctx, const char *fmt, ...) {
  if (!ctx || !fmt) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(ctx->last_error, sizeof(ctx->last_error), fmt, args);
  va_end(args);
}

void AppEmitProgress(const AppOperationHooks *hooks, const char *stage,
                     int current, int total, const char *message) {
  if (!hooks || !hooks->progress_cb) {
    return;
  }

  AppProgressEvent event = {
      .stage = stage,
      .current = current,
      .total = total,
      .message = message,
  };
  hooks->progress_cb(&event, hooks->user_data);
}

bool AppShouldCancel(const AppOperationHooks *hooks) {
  if (!hooks || !hooks->cancel_cb) {
    return false;
  }
  return hooks->cancel_cb(hooks->user_data);
}

bool AppBuildCachePaths(const char *env_dir, char *out_cache_dir,
                        size_t cache_dir_size, char *out_cache_path,
                        size_t cache_path_size) {
  if (!out_cache_dir || !out_cache_path || cache_dir_size == 0 ||
      cache_path_size == 0) {
    return false;
  }

  if (env_dir && env_dir[0] != '\0') {
    snprintf(out_cache_dir, cache_dir_size, "%s/.cache", env_dir);
    snprintf(out_cache_path, cache_path_size, "%s/.cache/gallery_cache.json",
             env_dir);
  } else {
    snprintf(out_cache_dir, cache_dir_size, ".cache");
    snprintf(out_cache_path, cache_path_size, ".cache/gallery_cache.json");
  }

  return true;
}

CacheCompressionMode AppMapCompressionMode(AppCacheCompressionMode mode) {
  switch (mode) {
  case APP_CACHE_COMPRESSION_NONE:
    return CACHE_COMPRESSION_NONE;
  case APP_CACHE_COMPRESSION_ZSTD:
    return CACHE_COMPRESSION_ZSTD;
  case APP_CACHE_COMPRESSION_AUTO:
    return CACHE_COMPRESSION_AUTO;
  default:
    return CACHE_COMPRESSION_NONE;
  }
}

SimilarityMemoryMode AppMapSimilarityMemoryMode(AppSimilarityMemoryMode mode) {
  switch (mode) {
  case APP_SIM_MEMORY_EAGER:
    return SIM_MEMORY_MODE_EAGER;
  case APP_SIM_MEMORY_CHUNKED:
    return SIM_MEMORY_MODE_CHUNKED;
  default:
    return SIM_MEMORY_MODE_CHUNKED;
  }
}

void AppReleaseCache(AppContext *ctx) {
  if (!ctx || !ctx->cache_initialized) {
    return;
  }
  CacheShutdown();
  ctx->cache_initialized = false;
  ctx->active_cache_path[0] = '\0';
}

void AppReleaseMl(AppContext *ctx) {
  if (!ctx || !ctx->ml_initialized) {
    return;
  }
  MlShutdown();
  ctx->ml_initialized = false;
}

AppStatus AppEnsureCacheReady(AppContext *ctx, const char *env_dir,
                              AppCacheCompressionMode mode, int level,
                              bool create_cache_dir) {
  if (!ctx) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  char cache_dir[APP_MAX_PATH] = {0};
  char cache_path[APP_MAX_PATH] = {0};
  if (!AppBuildCachePaths(env_dir, cache_dir, sizeof(cache_dir), cache_path,
                          sizeof(cache_path))) {
    AppSetError(ctx, "failed to build cache path");
    return APP_STATUS_INTERNAL_ERROR;
  }

  CacheCompressionMode mapped = AppMapCompressionMode(mode);
  if (!CacheSetCompression(mapped, level)) {
    AppSetError(ctx, "invalid cache compression configuration");
    return APP_STATUS_INVALID_ARGUMENT;
  }

  if (ctx->cache_initialized && strcmp(ctx->active_cache_path, cache_path) == 0) {
    return APP_STATUS_OK;
  }

  if (ctx->cache_initialized) {
    AppReleaseCache(ctx);
  }

  if (create_cache_dir && !FsMakeDirRecursive(cache_dir)) {
    AppSetError(ctx, "failed to create cache directory '%s'", cache_dir);
    return APP_STATUS_IO_ERROR;
  }

  if (!CacheInit(cache_path)) {
    AppSetError(ctx, "failed to initialize cache at '%s'", cache_path);
    return APP_STATUS_CACHE_ERROR;
  }

  ctx->cache_initialized = true;
  strncpy(ctx->active_cache_path, cache_path, sizeof(ctx->active_cache_path) - 1);
  ctx->active_cache_path[sizeof(ctx->active_cache_path) - 1] = '\0';
  return APP_STATUS_OK;
}

AppStatus AppEnsureMlReady(AppContext *ctx, const char *models_root_override) {
  if (!ctx) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  if (ctx->ml_initialized) {
    return APP_STATUS_OK;
  }

  const char *models_root = models_root_override;
  if (!models_root || models_root[0] == '\0') {
    models_root = ctx->models_root;
  }

  if (!MlInit(models_root)) {
    AppSetError(ctx, "failed to initialize ML runtime from %s", models_root);
    return APP_STATUS_ML_ERROR;
  }

  ctx->ml_initialized = true;
  return APP_STATUS_OK;
}

static bool AppGroupKeyIsValid(const char *key) {
  if (!key || key[0] == '\0') {
    return false;
  }

  size_t key_count = sizeof(GROUP_KEYS) / sizeof(GROUP_KEYS[0]);
  for (size_t i = 0; i < key_count; i++) {
    if (strcmp(key, GROUP_KEYS[i]) == 0) {
      return true;
    }
  }
  return false;
}

static char *AppTrimInPlace(char *s) {
  if (!s) {
    return s;
  }

  while (*s != '\0' && isspace((unsigned char)*s)) {
    s++;
  }

  if (*s == '\0') {
    return s;
  }

  char *end = s + strlen(s) - 1;
  while (end > s && isspace((unsigned char)*end)) {
    *end = '\0';
    end--;
  }

  return s;
}

bool AppParseGroupByKeys(const char *group_by_arg, const char **out_keys,
                         int max_keys, int *out_count,
                         char **out_owned_buffer, char *out_error,
                         size_t out_error_size) {
  if (!out_keys || !out_count || !out_owned_buffer || max_keys <= 0) {
    return false;
  }

  *out_count = 0;
  *out_owned_buffer = NULL;
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }

  if (!group_by_arg) {
    out_keys[0] = "date";
    *out_count = 1;
    return true;
  }

  char *owned = strdup(group_by_arg);
  if (!owned) {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size, "out of memory while parsing --group-by");
    }
    return false;
  }

  char *cursor = owned;
  while (true) {
    char *comma = strchr(cursor, ',');
    if (comma) {
      *comma = '\0';
    }

    char *trimmed = AppTrimInPlace(cursor);
    if (trimmed[0] == '\0') {
      if (out_error && out_error_size > 0) {
        snprintf(out_error, out_error_size,
                 "--group-by cannot include empty keys. Allowed keys: "
                 "date,camera,format,orientation,resolution");
      }
      free(owned);
      return false;
    }

    if (!AppGroupKeyIsValid(trimmed)) {
      if (out_error && out_error_size > 0) {
        snprintf(out_error, out_error_size,
                 "Invalid --group-by key '%s'. Allowed keys: "
                 "date,camera,format,orientation,resolution",
                 trimmed);
      }
      free(owned);
      return false;
    }

    if (*out_count >= max_keys) {
      if (out_error && out_error_size > 0) {
        snprintf(out_error, out_error_size,
                 "Too many --group-by keys (max %d)", max_keys);
      }
      free(owned);
      return false;
    }

    out_keys[*out_count] = trimmed;
    (*out_count)++;

    if (!comma) {
      break;
    }
    cursor = comma + 1;
  }

  *out_owned_buffer = owned;
  return true;
}

void AppGroupKeyListFree(AppGroupKeyList *list) {
  if (!list) {
    return;
  }
  free(list->owned_buffer);
  list->owned_buffer = NULL;
  list->keys = NULL;
  list->count = 0;
}
