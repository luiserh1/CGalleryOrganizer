#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

#include "app/app_cache_profile.h"
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

static bool AppContextHasRequestedCacheOpen(const AppContext *ctx,
                                            const char *cache_path) {
  if (!ctx || !cache_path || cache_path[0] == '\0') {
    return false;
  }
  return ctx->cache_initialized &&
         strcmp(ctx->active_cache_path, cache_path) == 0;
}

static void AppResolveCacheEntryCount(AppContext *ctx, const char *env_dir,
                                      const char *cache_path,
                                      AppRuntimeState *out_state) {
  if (!ctx || !cache_path || !out_state || !out_state->cache_exists) {
    return;
  }

  out_state->cache_entry_count_known = false;
  out_state->cache_entry_count = 0;

  if (AppContextHasRequestedCacheOpen(ctx, cache_path)) {
    int in_memory_count = CacheGetEntryCount();
    if (in_memory_count >= 0) {
      out_state->cache_entry_count = in_memory_count;
      out_state->cache_entry_count_known = true;
      return;
    }
  }

  char profile_path[APP_MAX_PATH] = {0};
  if (!AppBuildCacheProfilePath(env_dir, profile_path, sizeof(profile_path))) {
    return;
  }

  int last_known_count = 0;
  if (AppLoadCacheProfileEntryCount(profile_path, &last_known_count) &&
      last_known_count >= 0) {
    out_state->cache_entry_count = last_known_count;
    out_state->cache_entry_count_known = true;
  }
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
    AppResolveCacheEntryCount(ctx, request->env_dir, cache_path, out_state);
  }

  char manifest_path[APP_MAX_PATH] = {0};
  snprintf(manifest_path, sizeof(manifest_path), "%s/manifest.json",
           request->env_dir);
  out_state->rollback_manifest_exists = AppFileExistsRegular(manifest_path);

  return APP_STATUS_OK;
}
