#include <stdio.h>
#include <string.h>

#include "app/app_cache_profile.h"
#include "app/app_internal.h"

static bool AppScanCancelBridge(void *user_data) {
  const AppOperationHooks *hooks = (const AppOperationHooks *)user_data;
  return AppShouldCancel(hooks);
}

static void AppScanProgressBridge(const char *stage, int current, int total,
                                  void *user_data) {
  const AppOperationHooks *hooks = (const AppOperationHooks *)user_data;
  AppEmitProgress(hooks, stage, current, total, NULL);
}

static AppStatus AppRebuildCacheForProfileMismatch(
    AppContext *ctx, const AppScanRequest *request, const char *cache_path,
    const char *profile_path) {
  if (!ctx || !request || !cache_path || !profile_path) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppReleaseCache(ctx);
  remove(cache_path);
  remove(profile_path);

  return AppEnsureCacheReady(ctx, request->env_dir,
                             request->cache_compression_mode,
                             request->cache_compression_level, true);
}

static AppStatus AppEvaluateScanProfileDecision(
    AppContext *ctx, const AppScanRequest *request,
    AppScanProfileDecision *out_decision, AppCacheProfile *out_requested_profile,
    char *out_profile_path, size_t out_profile_path_size) {
  if (!ctx || !request || !out_decision) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  memset(out_decision, 0, sizeof(*out_decision));
  if (out_requested_profile) {
    memset(out_requested_profile, 0, sizeof(*out_requested_profile));
  }
  if (out_profile_path && out_profile_path_size > 0) {
    out_profile_path[0] = '\0';
  }

  char profile_path[APP_MAX_PATH] = {0};
  if (!AppBuildCacheProfilePath(request->env_dir, profile_path,
                                sizeof(profile_path))) {
    AppSetError(ctx, "failed to resolve cache profile path");
    return APP_STATUS_INTERNAL_ERROR;
  }

  AppCacheProfile requested_profile = {0};
  char profile_error[APP_MAX_ERROR] = {0};
  if (!AppBuildRequestedCacheProfile(ctx, request, &requested_profile,
                                     profile_error, sizeof(profile_error))) {
    AppSetError(ctx, "failed to build cache profile: %s",
                profile_error[0] != '\0' ? profile_error : "unknown");
    return APP_STATUS_IO_ERROR;
  }

  AppCacheProfile stored_profile = {0};
  AppCacheProfileLoadStatus load_status =
      AppLoadCacheProfile(profile_path, &stored_profile);
  if (load_status == APP_CACHE_PROFILE_LOAD_OK) {
    out_decision->profile_present = true;
    out_decision->profile_match =
        AppCompareCacheProfiles(&requested_profile, &stored_profile,
                                out_decision->reason,
                                sizeof(out_decision->reason));
  } else if (load_status == APP_CACHE_PROFILE_LOAD_MISSING) {
    out_decision->profile_present = false;
    out_decision->profile_match = false;
    snprintf(out_decision->reason, sizeof(out_decision->reason),
             "cache profile missing");
  } else {
    out_decision->profile_present = false;
    out_decision->profile_match = false;
    snprintf(out_decision->reason, sizeof(out_decision->reason),
             "cache profile malformed or unreadable");
  }
  out_decision->will_rebuild_cache = !out_decision->profile_match;

  if (out_requested_profile) {
    *out_requested_profile = requested_profile;
  }
  if (out_profile_path && out_profile_path_size > 0) {
    strncpy(out_profile_path, profile_path, out_profile_path_size - 1);
    out_profile_path[out_profile_path_size - 1] = '\0';
  }

  return APP_STATUS_OK;
}

AppStatus AppInspectScanProfile(AppContext *ctx, const AppScanRequest *request,
                                AppScanProfileDecision *out_decision) {
  if (!ctx || !request || !out_decision || !request->target_dir ||
      request->target_dir[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);

  if (request->jobs < 1) {
    AppSetError(ctx, "invalid jobs value %d", request->jobs);
    return APP_STATUS_INVALID_ARGUMENT;
  }

  return AppEvaluateScanProfileDecision(ctx, request, out_decision, NULL, NULL,
                                        0);
}

AppStatus AppRunScan(AppContext *ctx, const AppScanRequest *request,
                     AppScanResult *out_result) {
  if (!ctx || !request || !out_result || !request->target_dir ||
      request->target_dir[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  if (request->jobs < 1) {
    AppSetError(ctx, "invalid jobs value %d", request->jobs);
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppStatus status = AppEnsureCacheReady(
      ctx, request->env_dir, request->cache_compression_mode,
      request->cache_compression_level, true);
  if (status != APP_STATUS_OK) {
    return status;
  }

  bool needs_ml = request->ml_enrich || request->similarity_report;
  if (needs_ml) {
    status = AppEnsureMlReady(ctx, request->models_root_override);
    if (status != APP_STATUS_OK) {
      return status;
    }
  }

  char cache_path[APP_MAX_PATH] = {0};
  char cache_dir[APP_MAX_PATH] = {0};
  if (!AppBuildCachePaths(request->env_dir, cache_dir, sizeof(cache_dir),
                          cache_path, sizeof(cache_path))) {
    AppSetError(ctx, "failed to resolve cache profile paths");
    return APP_STATUS_INTERNAL_ERROR;
  }

  AppCacheProfile requested_profile = {0};
  AppScanProfileDecision profile_decision = {0};
  char profile_path[APP_MAX_PATH] = {0};
  status = AppEvaluateScanProfileDecision(
      ctx, request, &profile_decision, &requested_profile, profile_path,
      sizeof(profile_path));
  if (status != APP_STATUS_OK) {
    return status;
  }
  out_result->cache_profile_matched = profile_decision.profile_match;
  strncpy(out_result->cache_profile_reason, profile_decision.reason,
          sizeof(out_result->cache_profile_reason) - 1);
  out_result->cache_profile_reason[sizeof(out_result->cache_profile_reason) - 1] =
      '\0';

  if (profile_decision.will_rebuild_cache) {
    status = AppRebuildCacheForProfileMismatch(ctx, request, cache_path,
                                               profile_path);
    if (status != APP_STATUS_OK) {
      return status;
    }
    out_result->cache_profile_rebuilt = true;
  }

  AppScanContext pipeline_ctx = {
      .exhaustive = request->exhaustive,
      .ml_enrich = request->ml_enrich,
      .similarity_report = request->similarity_report,
      .sim_incremental = request->sim_incremental,
      .ml_enabled = needs_ml,
      .ml_files_evaluated = 0,
      .ml_files_classified = 0,
      .ml_files_with_text = 0,
      .ml_files_embedded = 0,
      .ml_failures = 0,
      .cancelled = false,
      .total_files = 0,
      .cancel_cb = AppScanCancelBridge,
      .progress_cb = AppScanProgressBridge,
      .callback_user_data = (void *)&request->hooks,
  };

  ScanRunStats stats = {0};
  if (!CliRunScanPipeline(request->target_dir, &pipeline_ctx, request->jobs,
                          &stats)) {
    if (pipeline_ctx.cancelled) {
      AppSetError(ctx, "scan cancelled");
      return APP_STATUS_CANCELLED;
    }
    AppSetError(ctx, "failed to scan '%s'", request->target_dir);
    return APP_STATUS_IO_ERROR;
  }

  if (!CacheSave()) {
    AppSetError(ctx, "failed to save cache");
    return APP_STATUS_CACHE_ERROR;
  }

  int cache_entry_count = CacheGetEntryCount();
  if (cache_entry_count >= 0) {
    requested_profile.has_cache_entry_count = true;
    requested_profile.cache_entry_count = cache_entry_count;
  } else {
    requested_profile.has_cache_entry_count = false;
    requested_profile.cache_entry_count = 0;
  }

  if (!AppSaveCacheProfile(profile_path, &requested_profile)) {
    AppSetError(ctx, "failed to save cache profile '%s'", profile_path);
    return APP_STATUS_IO_ERROR;
  }

  out_result->files_scanned = stats.files_scanned;
  out_result->files_cached = stats.files_cached;
  out_result->ml_files_evaluated = pipeline_ctx.ml_files_evaluated;
  out_result->ml_files_classified = pipeline_ctx.ml_files_classified;
  out_result->ml_files_with_text = pipeline_ctx.ml_files_with_text;
  out_result->ml_files_embedded = pipeline_ctx.ml_files_embedded;
  out_result->ml_failures = pipeline_ctx.ml_failures;

  return APP_STATUS_OK;
}
