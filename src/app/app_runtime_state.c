#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

#include "cJSON.h"
#include "core/cache_codec.h"

#include "app/app_internal.h"

static const int APP_DEFAULT_MAX_JOBS = 8;

static bool AppFileExistsRegular(const char *path) {
  if (!path || path[0] == '\0') {
    return false;
  }

  struct stat st;
  return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int AppDetectLogicalCores(void) {
  long cores = sysconf(_SC_NPROCESSORS_ONLN);
  if (cores < 1) {
    return 1;
  }
  if (cores > 256) {
    return 256;
  }
  return (int)cores;
}

static int AppRecommendJobs(int logical_cores) {
  int recommended = logical_cores;
  if (recommended < 1) {
    recommended = 1;
  }
  if (recommended > APP_DEFAULT_MAX_JOBS) {
    recommended = APP_DEFAULT_MAX_JOBS;
  }
  return recommended;
}

static int AppCountJsonObjectKeys(cJSON *root) {
  if (!root || !cJSON_IsObject(root)) {
    return 0;
  }

  int count = 0;
  for (cJSON *child = root->child; child; child = child->next) {
    count++;
  }
  return count;
}

static AppStatus AppReadCacheEntryCountFromFile(AppContext *ctx,
                                                const char *cache_path,
                                                int *out_count) {
  if (!ctx || !cache_path || !out_count) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  *out_count = 0;

  FILE *f = fopen(cache_path, "rb");
  if (!f) {
    AppSetError(ctx, "failed to open cache file '%s'", cache_path);
    return APP_STATUS_IO_ERROR;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    AppSetError(ctx, "failed to seek cache file '%s'", cache_path);
    return APP_STATUS_IO_ERROR;
  }

  long len = ftell(f);
  if (len < 0) {
    fclose(f);
    AppSetError(ctx, "failed to read cache length for '%s'", cache_path);
    return APP_STATUS_IO_ERROR;
  }

  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    AppSetError(ctx, "failed to rewind cache file '%s'", cache_path);
    return APP_STATUS_IO_ERROR;
  }

  if (len == 0) {
    fclose(f);
    return APP_STATUS_OK;
  }

  unsigned char *raw = malloc((size_t)len);
  if (!raw) {
    fclose(f);
    AppSetError(ctx, "out of memory reading cache '%s'", cache_path);
    return APP_STATUS_INTERNAL_ERROR;
  }

  size_t read_bytes = fread(raw, 1, (size_t)len, f);
  fclose(f);
  if (read_bytes != (size_t)len) {
    free(raw);
    AppSetError(ctx, "failed to read cache bytes from '%s'", cache_path);
    return APP_STATUS_IO_ERROR;
  }

  unsigned char *decoded = NULL;
  size_t decoded_len = 0;
  CacheCompressionMode detected_mode = CACHE_COMPRESSION_NONE;
  char decode_error[256] = {0};
  if (!CacheCodecDecode(raw, (size_t)len, &decoded, &decoded_len, &detected_mode,
                        decode_error, sizeof(decode_error))) {
    free(raw);
    AppSetError(ctx, "failed to decode cache '%s': %s", cache_path,
                decode_error[0] != '\0' ? decode_error : "unknown");
    return APP_STATUS_CACHE_ERROR;
  }
  free(raw);

  char *json_text = malloc(decoded_len + 1);
  if (!json_text) {
    free(decoded);
    AppSetError(ctx, "out of memory while parsing cache '%s'", cache_path);
    return APP_STATUS_INTERNAL_ERROR;
  }

  if (decoded_len > 0) {
    memcpy(json_text, decoded, decoded_len);
  }
  json_text[decoded_len] = '\0';
  free(decoded);

  cJSON *root = cJSON_Parse(json_text);
  free(json_text);
  if (!root) {
    AppSetError(ctx, "failed to parse cache JSON '%s'", cache_path);
    return APP_STATUS_CACHE_ERROR;
  }

  *out_count = AppCountJsonObjectKeys(root);
  cJSON_Delete(root);
  return APP_STATUS_OK;
}

static void AppResolveModelsRoot(const AppContext *ctx,
                                 const char *models_root_override,
                                 char *out_models_root,
                                 size_t out_size) {
  if (!out_models_root || out_size == 0) {
    return;
  }

  out_models_root[0] = '\0';
  if (models_root_override && models_root_override[0] != '\0') {
    strncpy(out_models_root, models_root_override, out_size - 1);
    out_models_root[out_size - 1] = '\0';
    return;
  }

  if (ctx && ctx->models_root[0] != '\0') {
    strncpy(out_models_root, ctx->models_root, out_size - 1);
    out_models_root[out_size - 1] = '\0';
    return;
  }

  strncpy(out_models_root, "build/models", out_size - 1);
  out_models_root[out_size - 1] = '\0';
}

static void AppRecordMissingModel(AppModelAvailability *models,
                                  const char *model_id) {
  if (!models || !model_id) {
    return;
  }

  if (models->missing_count < 3) {
    strncpy(models->missing_ids[models->missing_count], model_id,
            sizeof(models->missing_ids[0]) - 1);
    models->missing_ids[models->missing_count][sizeof(models->missing_ids[0]) - 1] =
        '\0';
  }
  models->missing_count++;
}

static void AppProbeModelPresence(const char *models_root,
                                  AppModelAvailability *out_models) {
  if (!out_models) {
    return;
  }
  memset(out_models, 0, sizeof(*out_models));

  char path[APP_MAX_PATH] = {0};

  snprintf(path, sizeof(path), "%s/%s", models_root,
           APP_MODEL_FILE_CLASSIFICATION);
  out_models->classification_present = AppFileExistsRegular(path);
  if (!out_models->classification_present) {
    AppRecordMissingModel(out_models, APP_MODEL_ID_CLASSIFICATION);
  }

  snprintf(path, sizeof(path), "%s/%s", models_root,
           APP_MODEL_FILE_TEXT_DETECTION);
  out_models->text_detection_present = AppFileExistsRegular(path);
  if (!out_models->text_detection_present) {
    AppRecordMissingModel(out_models, APP_MODEL_ID_TEXT_DETECTION);
  }

  snprintf(path, sizeof(path), "%s/%s", models_root, APP_MODEL_FILE_EMBEDDING);
  out_models->embedding_present = AppFileExistsRegular(path);
  if (!out_models->embedding_present) {
    AppRecordMissingModel(out_models, APP_MODEL_ID_EMBEDDING);
  }
}

AppStatus AppInspectRuntimeState(AppContext *ctx,
                                 const AppRuntimeStateRequest *request,
                                 AppRuntimeState *out_state) {
  if (!ctx || !request || !out_state) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_state, 0, sizeof(*out_state));

  AppResolveModelsRoot(ctx, request->models_root_override, out_state->models_root,
                       sizeof(out_state->models_root));
  AppProbeModelPresence(out_state->models_root, &out_state->models);

  out_state->logical_cores = AppDetectLogicalCores();
  out_state->recommended_jobs = AppRecommendJobs(out_state->logical_cores);

  if (!request->env_dir || request->env_dir[0] == '\0') {
    return APP_STATUS_OK;
  }

  char cache_dir[APP_MAX_PATH] = {0};
  char cache_path[APP_MAX_PATH] = {0};
  if (!AppBuildCachePaths(request->env_dir, cache_dir, sizeof(cache_dir),
                          cache_path, sizeof(cache_path))) {
    AppSetError(ctx, "failed to build cache path");
    return APP_STATUS_INTERNAL_ERROR;
  }

  out_state->cache_exists = AppFileExistsRegular(cache_path);
  if (out_state->cache_exists) {
    int entry_count = 0;
    AppStatus status = AppReadCacheEntryCountFromFile(ctx, cache_path, &entry_count);
    if (status != APP_STATUS_OK) {
      return status;
    }
    out_state->cache_entry_count = entry_count;
  }

  char manifest_path[APP_MAX_PATH] = {0};
  snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json",
           request->env_dir);
  out_state->rollback_manifest_exists = AppFileExistsRegular(manifest_path);

  return APP_STATUS_OK;
}
