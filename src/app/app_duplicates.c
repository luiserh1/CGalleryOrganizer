#include <stdlib.h>
#include <string.h>

#include "duplicate_finder.h"
#include "fs_utils.h"

#include "app/app_internal.h"

static void FreePartialDuplicateReport(AppDuplicateReport *report,
                                       int group_count) {
  if (!report || !report->groups) {
    return;
  }

  for (int i = 0; i < group_count; i++) {
    free(report->groups[i].original_path);
    for (int j = 0; j < report->groups[i].duplicate_count; j++) {
      free(report->groups[i].duplicate_paths[j]);
    }
    free(report->groups[i].duplicate_paths);
  }
  free(report->groups);
  report->groups = NULL;
  report->group_count = 0;
}

AppStatus AppFindDuplicates(AppContext *ctx,
                            const AppDuplicateFindRequest *request,
                            AppDuplicateReport *out_report) {
  if (!ctx || !request || !out_report) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_report, 0, sizeof(*out_report));

  AppStatus status =
      AppEnsureCacheReady(ctx, request->env_dir, request->cache_compression_mode,
                          request->cache_compression_level, true);
  if (status != APP_STATUS_OK) {
    return status;
  }

  DuplicateReport raw = FindExactDuplicates();
  if (raw.group_count <= 0) {
    return APP_STATUS_OK;
  }

  out_report->groups =
      calloc((size_t)raw.group_count, sizeof(AppDuplicateGroup));
  if (!out_report->groups) {
    FreeDuplicateReport(&raw);
    AppSetError(ctx, "out of memory while building duplicate report");
    return APP_STATUS_INTERNAL_ERROR;
  }

  out_report->group_count = raw.group_count;
  for (int i = 0; i < raw.group_count; i++) {
    out_report->groups[i].original_path = strdup(raw.groups[i].original_path);
    out_report->groups[i].duplicate_count = raw.groups[i].duplicate_count;

    if (!out_report->groups[i].original_path) {
      FreeDuplicateReport(&raw);
      FreePartialDuplicateReport(out_report, i);
      AppSetError(ctx, "out of memory while copying duplicate report");
      return APP_STATUS_INTERNAL_ERROR;
    }

    out_report->groups[i].duplicate_paths =
        calloc((size_t)raw.groups[i].duplicate_count, sizeof(char *));
    if (!out_report->groups[i].duplicate_paths) {
      FreeDuplicateReport(&raw);
      FreePartialDuplicateReport(out_report, i + 1);
      AppSetError(ctx, "out of memory while copying duplicate paths");
      return APP_STATUS_INTERNAL_ERROR;
    }

    for (int j = 0; j < raw.groups[i].duplicate_count; j++) {
      out_report->groups[i].duplicate_paths[j] =
          strdup(raw.groups[i].duplicate_paths[j]);
      if (!out_report->groups[i].duplicate_paths[j]) {
        FreeDuplicateReport(&raw);
        FreePartialDuplicateReport(out_report, i + 1);
        AppSetError(ctx, "out of memory while copying duplicate paths");
        return APP_STATUS_INTERNAL_ERROR;
      }
    }
  }

  FreeDuplicateReport(&raw);
  return APP_STATUS_OK;
}

AppStatus AppMoveDuplicates(AppContext *ctx,
                            const AppDuplicateMoveRequest *request,
                            AppDuplicateMoveResult *out_result) {
  if (!ctx || !request || !out_result || !request->target_dir ||
      request->target_dir[0] == '\0' || !request->report) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  for (int i = 0; i < request->report->group_count; i++) {
    for (int j = 0; j < request->report->groups[i].duplicate_count; j++) {
      char moved_path[APP_MAX_PATH] = {0};
      if (FsMoveFile(request->report->groups[i].duplicate_paths[j],
                     request->target_dir, moved_path, sizeof(moved_path))) {
        out_result->moved_count++;
      } else {
        out_result->failed_count++;
      }
    }
  }

  if (out_result->failed_count > 0) {
    AppSetError(ctx, "failed to move %d duplicate files", out_result->failed_count);
  }

  return APP_STATUS_OK;
}

void AppFreeDuplicateReport(AppDuplicateReport *report) {
  if (!report) {
    return;
  }

  for (int i = 0; i < report->group_count; i++) {
    free(report->groups[i].original_path);
    for (int j = 0; j < report->groups[i].duplicate_count; j++) {
      free(report->groups[i].duplicate_paths[j]);
    }
    free(report->groups[i].duplicate_paths);
  }
  free(report->groups);

  report->groups = NULL;
  report->group_count = 0;
}
