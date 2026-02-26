#include <stdio.h>
#include <string.h>

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

  out_result->files_scanned = stats.files_scanned;
  out_result->files_cached = stats.files_cached;
  out_result->ml_files_evaluated = pipeline_ctx.ml_files_evaluated;
  out_result->ml_files_classified = pipeline_ctx.ml_files_classified;
  out_result->ml_files_with_text = pipeline_ctx.ml_files_with_text;
  out_result->ml_files_embedded = pipeline_ctx.ml_files_embedded;
  out_result->ml_failures = pipeline_ctx.ml_failures;

  return APP_STATUS_OK;
}
