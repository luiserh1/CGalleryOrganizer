#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "systems/renamer_apply.h"
#include "systems/renamer_history.h"
#include "systems/renamer_preview.h"

#include "app/app_internal.h"

static bool AppLoadTextFile(const char *path, char **out_text) {
  if (!path || !out_text) {
    return false;
  }

  *out_text = NULL;
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }

  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return false;
  }
  rewind(f);

  char *text = calloc((size_t)size + 1, 1);
  if (!text) {
    fclose(f);
    return false;
  }

  size_t read_bytes = fread(text, 1, (size_t)size, f);
  fclose(f);
  text[read_bytes] = '\0';
  *out_text = text;
  return true;
}

AppStatus AppPreviewRename(AppContext *ctx,
                           const AppRenamePreviewRequest *request,
                           AppRenamePreviewResult *out_result) {
  if (!ctx || !request || !out_result || !request->target_dir ||
      request->target_dir[0] == '\0' || !request->env_dir ||
      request->env_dir[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  AppStatus status =
      AppEnsureCacheReady(ctx, request->env_dir, APP_CACHE_COMPRESSION_NONE, 3,
                          true);
  if (status != APP_STATUS_OK) {
    return status;
  }

  if (AppShouldCancel(&request->hooks)) {
    AppSetError(ctx, "rename preview cancelled");
    return APP_STATUS_CANCELLED;
  }

  AppEmitProgress(&request->hooks, "rename_preview", 0, 1,
                  "Building rename preview");

  RenamerPreviewRequest preview_request = {
      .env_dir = request->env_dir,
      .target_dir = request->target_dir,
      .pattern = request->pattern,
      .tags_map_path = request->tags_map_json_path,
      .tag_add_csv = request->tag_add_csv,
      .tag_remove_csv = request->tag_remove_csv,
      .meta_tag_add_csv = request->meta_tag_add_csv,
      .meta_tag_remove_csv = request->meta_tag_remove_csv,
      .recursive = true,
  };

  RenamerPreviewArtifact preview = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!RenamerPreviewBuild(&preview_request, &preview, error, sizeof(error))) {
    AppSetError(ctx, "%s", error[0] != '\0' ? error : "rename preview failed");
    return APP_STATUS_IO_ERROR;
  }

  char artifact_path[APP_MAX_PATH] = {0};
  if (!RenamerPreviewSaveArtifact(&preview, artifact_path, sizeof(artifact_path),
                                  error, sizeof(error))) {
    RenamerPreviewFree(&preview);
    AppSetError(ctx, "%s", error[0] != '\0' ? error : "rename preview save failed");
    return APP_STATUS_IO_ERROR;
  }

  strncpy(out_result->preview_id, preview.preview_id,
          sizeof(out_result->preview_id) - 1);
  out_result->preview_id[sizeof(out_result->preview_id) - 1] = '\0';
  strncpy(out_result->preview_path, artifact_path,
          sizeof(out_result->preview_path) - 1);
  out_result->preview_path[sizeof(out_result->preview_path) - 1] = '\0';
  strncpy(out_result->effective_pattern, preview.pattern,
          sizeof(out_result->effective_pattern) - 1);
  out_result->effective_pattern[sizeof(out_result->effective_pattern) - 1] =
      '\0';
  strncpy(out_result->fingerprint, preview.fingerprint,
          sizeof(out_result->fingerprint) - 1);
  out_result->fingerprint[sizeof(out_result->fingerprint) - 1] = '\0';
  out_result->file_count = preview.file_count;
  out_result->collision_group_count = preview.collision_group_count;
  out_result->collision_count = preview.collision_count;
  out_result->truncation_count = preview.truncation_count;
  out_result->requires_auto_suffix_acceptance =
      preview.requires_auto_suffix_acceptance;

  if (!AppLoadTextFile(artifact_path, &out_result->details_json)) {
    out_result->details_json = NULL;
  }

  RenamerPreviewFree(&preview);

  AppEmitProgress(&request->hooks, "rename_preview", 1, 1,
                  "Rename preview ready");
  return APP_STATUS_OK;
}

AppStatus AppApplyRename(AppContext *ctx, const AppRenameApplyRequest *request,
                         AppRenameApplyResult *out_result) {
  if (!ctx || !request || !out_result || !request->env_dir ||
      request->env_dir[0] == '\0' || !request->preview_id ||
      request->preview_id[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  if (AppShouldCancel(&request->hooks)) {
    AppSetError(ctx, "rename apply cancelled");
    return APP_STATUS_CANCELLED;
  }

  AppEmitProgress(&request->hooks, "rename_apply", 0, 1,
                  "Loading rename preview artifact");

  RenamerPreviewArtifact preview = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!RenamerPreviewLoadArtifact(request->env_dir, request->preview_id, &preview,
                                  error, sizeof(error))) {
    AppSetError(ctx, "%s",
                error[0] != '\0' ? error : "failed to load rename preview");
    return APP_STATUS_IO_ERROR;
  }

  char recomputed_fingerprint[64] = {0};
  if (!RenamerPreviewRecomputeFingerprint(&preview, recomputed_fingerprint,
                                          sizeof(recomputed_fingerprint), error,
                                          sizeof(error))) {
    RenamerPreviewFree(&preview);
    AppSetError(ctx, "%s", error[0] != '\0' ? error
                                          : "rename preview revalidation failed");
    return APP_STATUS_IO_ERROR;
  }

  if (strcmp(recomputed_fingerprint, preview.fingerprint) != 0) {
    RenamerPreviewFree(&preview);
    AppSetError(ctx,
                "rename preview fingerprint mismatch; regenerate preview before apply");
    return APP_STATUS_INVALID_ARGUMENT;
  }

  if (preview.requires_auto_suffix_acceptance && !request->accept_auto_suffix) {
    RenamerPreviewFree(&preview);
    AppSetError(ctx,
                "rename apply requires explicit collision acceptance flag");
    return APP_STATUS_INVALID_ARGUMENT;
  }

  RenamerApplyRequest apply_request = {
      .env_dir = request->env_dir,
      .preview = &preview,
      .accept_auto_suffix = request->accept_auto_suffix,
  };

  RenamerApplyResult apply_result = {0};
  if (!RenamerApplyExecute(&apply_request, &apply_result, error,
                           sizeof(error))) {
    RenamerPreviewFree(&preview);
    AppSetError(ctx, "%s", error[0] != '\0' ? error : "rename apply failed");
    return APP_STATUS_IO_ERROR;
  }

  RenamerPreviewFree(&preview);

  strncpy(out_result->operation_id, apply_result.operation_id,
          sizeof(out_result->operation_id) - 1);
  out_result->operation_id[sizeof(out_result->operation_id) - 1] = '\0';
  strncpy(out_result->created_at_utc, apply_result.created_at_utc,
          sizeof(out_result->created_at_utc) - 1);
  out_result->created_at_utc[sizeof(out_result->created_at_utc) - 1] = '\0';
  out_result->renamed_count = apply_result.renamed_count;
  out_result->skipped_count = apply_result.skipped_count;
  out_result->failed_count = apply_result.failed_count;
  out_result->collision_resolved_count = apply_result.collision_resolved_count;
  out_result->truncation_count = apply_result.truncation_count;
  out_result->auto_suffix_applied = apply_result.auto_suffix_applied;

  AppEmitProgress(&request->hooks, "rename_apply", 1, 1,
                  "Rename apply completed");
  return APP_STATUS_OK;
}

AppStatus AppRollbackRename(AppContext *ctx,
                            const AppRenameRollbackRequest *request,
                            AppRenameRollbackResult *out_result) {
  if (!ctx || !request || !out_result || !request->env_dir ||
      request->env_dir[0] == '\0' || !request->operation_id ||
      request->operation_id[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  if (AppShouldCancel(&request->hooks)) {
    AppSetError(ctx, "rename rollback cancelled");
    return APP_STATUS_CANCELLED;
  }

  AppEmitProgress(&request->hooks, "rename_rollback", 0, 1,
                  "Executing rename rollback");

  RenamerRollbackStats stats = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!RenamerHistoryRollback(request->env_dir, request->operation_id, &stats,
                              error, sizeof(error))) {
    AppSetError(ctx, "%s",
                error[0] != '\0' ? error : "rename rollback execution failed");
    return APP_STATUS_IO_ERROR;
  }

  out_result->restored_count = stats.restored_count;
  out_result->skipped_count = stats.skipped_count;
  out_result->failed_count = stats.failed_count;

  AppEmitProgress(&request->hooks, "rename_rollback", 1, 1,
                  "Rename rollback completed");
  return APP_STATUS_OK;
}

AppStatus AppPreflightRenameRollback(
    AppContext *ctx, const AppRenameRollbackPreflightRequest *request,
    AppRenameRollbackPreflightResult *out_result) {
  if (!ctx || !request || !out_result || !request->env_dir ||
      request->env_dir[0] == '\0' || !request->operation_id ||
      request->operation_id[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  RenamerRollbackPreflight preflight = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!RenamerHistoryRollbackPreflight(request->env_dir, request->operation_id,
                                       &preflight, error, sizeof(error))) {
    AppSetError(ctx, "%s",
                error[0] != '\0' ? error
                                 : "rename rollback preflight failed");
    return APP_STATUS_IO_ERROR;
  }

  out_result->total_items = preflight.total_items;
  out_result->restorable_count = preflight.restorable_count;
  out_result->missing_destination_count = preflight.missing_destination_count;
  out_result->source_exists_conflict_count =
      preflight.source_exists_conflict_count;
  out_result->invalid_item_count = preflight.invalid_item_count;
  out_result->fully_restorable = preflight.fully_restorable;
  return APP_STATUS_OK;
}

AppStatus AppPruneRenameHistory(AppContext *ctx,
                                const AppRenameHistoryPruneRequest *request,
                                AppRenameHistoryPruneResult *out_result) {
  if (!ctx || !request || !out_result || !request->env_dir ||
      request->env_dir[0] == '\0' || request->keep_count < 0) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  RenamerHistoryPruneStats prune_stats = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!RenamerHistoryPrune(request->env_dir, request->keep_count, &prune_stats,
                           error, sizeof(error))) {
    AppSetError(ctx, "%s",
                error[0] != '\0' ? error : "rename history prune failed");
    return APP_STATUS_IO_ERROR;
  }

  out_result->before_count = prune_stats.before_count;
  out_result->after_count = prune_stats.after_count;
  out_result->pruned_count = prune_stats.pruned_count;
  return APP_STATUS_OK;
}

AppStatus AppListRenameHistory(AppContext *ctx, const char *env_dir,
                               AppRenameHistoryEntry **out_entries,
                               int *out_count) {
  if (!ctx || !env_dir || env_dir[0] == '\0' || !out_entries || !out_count) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  *out_entries = NULL;
  *out_count = 0;

  RenamerHistoryEntry *history_entries = NULL;
  int history_count = 0;
  char error[APP_MAX_ERROR] = {0};
  if (!RenamerHistoryList(env_dir, &history_entries, &history_count, error,
                          sizeof(error))) {
    AppSetError(ctx, "%s", error[0] != '\0' ? error
                                          : "failed to list rename history");
    return APP_STATUS_IO_ERROR;
  }

  if (history_count <= 0 || !history_entries) {
    RenamerHistoryFreeEntries(history_entries);
    return APP_STATUS_OK;
  }

  AppRenameHistoryEntry *entries =
      calloc((size_t)history_count, sizeof(AppRenameHistoryEntry));
  if (!entries) {
    RenamerHistoryFreeEntries(history_entries);
    AppSetError(ctx, "out of memory while building rename history response");
    return APP_STATUS_INTERNAL_ERROR;
  }

  for (int i = 0; i < history_count; i++) {
    strncpy(entries[i].operation_id, history_entries[i].operation_id,
            sizeof(entries[i].operation_id) - 1);
    entries[i].operation_id[sizeof(entries[i].operation_id) - 1] = '\0';

    strncpy(entries[i].preview_id, history_entries[i].preview_id,
            sizeof(entries[i].preview_id) - 1);
    entries[i].preview_id[sizeof(entries[i].preview_id) - 1] = '\0';

    strncpy(entries[i].created_at_utc, history_entries[i].created_at_utc,
            sizeof(entries[i].created_at_utc) - 1);
    entries[i].created_at_utc[sizeof(entries[i].created_at_utc) - 1] = '\0';

    entries[i].renamed_count = history_entries[i].renamed_count;
    entries[i].skipped_count = history_entries[i].skipped_count;
    entries[i].failed_count = history_entries[i].failed_count;
    entries[i].collision_resolved_count =
        history_entries[i].collision_resolved_count;
    entries[i].truncation_count = history_entries[i].truncation_count;
    entries[i].rollback_performed = history_entries[i].rollback_performed;
    entries[i].rollback_restored_count =
        history_entries[i].rollback_restored_count;
    entries[i].rollback_skipped_count = history_entries[i].rollback_skipped_count;
    entries[i].rollback_failed_count = history_entries[i].rollback_failed_count;
  }

  RenamerHistoryFreeEntries(history_entries);

  *out_entries = entries;
  *out_count = history_count;
  return APP_STATUS_OK;
}

void AppFreeRenamePreviewResult(AppRenamePreviewResult *result) {
  if (!result) {
    return;
  }

  free(result->details_json);
  result->details_json = NULL;
  result->preview_id[0] = '\0';
  result->preview_path[0] = '\0';
  result->effective_pattern[0] = '\0';
  result->fingerprint[0] = '\0';
  result->file_count = 0;
  result->collision_group_count = 0;
  result->collision_count = 0;
  result->truncation_count = 0;
  result->requires_auto_suffix_acceptance = false;
}

void AppFreeRenameHistoryEntries(AppRenameHistoryEntry *entries) {
  free(entries);
}
