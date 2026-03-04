#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/cli_rename_utils.h"
#include "gui/gui_worker_internal.h"
#include "gui/gui_worker_tasks_internal.h"
static bool LoadTextFile(const char *path, char **out_text) {
  if (!path || path[0] == '\0' || !out_text) {
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
static bool BuildHistoryFilterFromTask(const GuiTaskInput *input,
                                       CliRenameHistoryFilter *out_filter,
                                       char *out_error,
                                       size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!input || !out_filter) {
    if (out_error && out_error_size > 0) {
      strncpy(out_error, "invalid history filter input", out_error_size - 1);
      out_error[out_error_size - 1] = '\0';
    }
    return false;
  }

  memset(out_filter, 0, sizeof(*out_filter));
  out_filter->rollback_filter = CLI_RENAME_ROLLBACK_FILTER_ANY;

  const char *prefix = input->rename_history_id_prefix;
  if ((!prefix || prefix[0] == '\0') && input->rename_operation_id[0] != '\0') {
    prefix = input->rename_operation_id;
  }
  if (prefix && prefix[0] != '\0') {
    strncpy(out_filter->operation_id_prefix, prefix,
            sizeof(out_filter->operation_id_prefix) - 1);
    out_filter->operation_id_prefix[sizeof(out_filter->operation_id_prefix) - 1] =
        '\0';
  }

  if (!CliRenameParseRollbackFilter(input->rename_history_rollback_filter,
                                    &out_filter->rollback_filter, out_error,
                                    out_error_size)) {
    return false;
  }
  if (!CliRenameNormalizeHistoryDateBound(
          input->rename_history_from, false, out_filter->created_from_utc,
          sizeof(out_filter->created_from_utc), out_error, out_error_size)) {
    return false;
  }
  if (!CliRenameNormalizeHistoryDateBound(
          input->rename_history_to, true, out_filter->created_to_utc,
          sizeof(out_filter->created_to_utc), out_error, out_error_size)) {
    return false;
  }
  if (out_filter->created_from_utc[0] != '\0' &&
      out_filter->created_to_utc[0] != '\0' &&
      strcmp(out_filter->created_from_utc, out_filter->created_to_utc) > 0) {
    if (out_error && out_error_size > 0) {
      strncpy(out_error, "history from-date must be <= to-date",
              out_error_size - 1);
      out_error[out_error_size - 1] = '\0';
    }
    return false;
  }
  return true;
}
static void WorkerRunRenameHistory(const GuiTaskInput *input) {
  CliRenameHistoryFilter filter = {0};
  char filter_error[APP_MAX_ERROR] = {0};
  if (!BuildHistoryFilterFromTask(input, &filter, filter_error,
                                  sizeof(filter_error))) {
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT,
                       filter_error[0] != '\0' ? filter_error
                                               : "Invalid history filter",
                       false);
    return;
  }

  AppRenameHistoryEntry *entries = NULL;
  int count = 0;
  AppStatus status =
      AppListRenameHistory(g_worker.app, input->env_dir, &entries, &count);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename history query failed", false);
    return;
  }

  int *indices = NULL;
  int filtered_count = 0;
  if (!CliRenameBuildHistoryFilterIndex(entries, count, &filter, &indices,
                                        &filtered_count, filter_error,
                                        sizeof(filter_error))) {
    AppFreeRenameHistoryEntries(entries);
    GuiWorkerSetResult(APP_STATUS_IO_ERROR,
                       filter_error[0] != '\0' ? filter_error
                                               : "Failed to filter rename history",
                       false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rename_history_count = count;
  g_worker.snapshot.rename_history_filtered_count = filtered_count;
  g_worker.snapshot.detail_text[0] = '\0';
  char line[512] = {0};
  snprintf(line, sizeof(line), "Rename history entries: %d (filtered: %d)\n",
           count, filtered_count);
  strncat(g_worker.snapshot.detail_text, line,
          sizeof(g_worker.snapshot.detail_text) -
              strlen(g_worker.snapshot.detail_text) - 1);
  for (int i = 0; i < filtered_count; i++) {
    const AppRenameHistoryEntry *entry = &entries[indices[i]];
    snprintf(line, sizeof(line),
             "%d. %s (%s) renamed=%d skipped=%d failed=%d rollback=%s\n",
             i + 1, entry->operation_id,
             entry->created_at_utc[0] != '\0' ? entry->created_at_utc
                                              : "unknown-time",
             entry->renamed_count, entry->skipped_count, entry->failed_count,
             entry->rollback_performed ? "yes" : "no");
    strncat(g_worker.snapshot.detail_text, line,
            sizeof(g_worker.snapshot.detail_text) -
                strlen(g_worker.snapshot.detail_text) - 1);
  }
  pthread_mutex_unlock(&g_worker.mutex);

  CliRenameFreeHistoryFilterIndex(indices);
  AppFreeRenameHistoryEntries(entries);
  GuiWorkerSetResult(APP_STATUS_OK, "Rename history loaded", true);
}
static void WorkerRunRenameHistoryExport(const GuiTaskInput *input) {
  if (input->rename_history_export_path[0] == '\0') {
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT,
                       "Rename history export requires output path", false);
    return;
  }

  CliRenameHistoryFilter filter = {0};
  char filter_error[APP_MAX_ERROR] = {0};
  if (!BuildHistoryFilterFromTask(input, &filter, filter_error,
                                  sizeof(filter_error))) {
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT,
                       filter_error[0] != '\0' ? filter_error
                                               : "Invalid history filter",
                       false);
    return;
  }

  AppRenameHistoryEntry *entries = NULL;
  int count = 0;
  AppStatus status =
      AppListRenameHistory(g_worker.app, input->env_dir, &entries, &count);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename history export query failed", false);
    return;
  }

  int *indices = NULL;
  int filtered_count = 0;
  if (!CliRenameBuildHistoryFilterIndex(entries, count, &filter, &indices,
                                        &filtered_count, filter_error,
                                        sizeof(filter_error))) {
    AppFreeRenameHistoryEntries(entries);
    GuiWorkerSetResult(APP_STATUS_IO_ERROR,
                       filter_error[0] != '\0' ? filter_error
                                               : "Failed to filter rename history",
                       false);
    return;
  }

  char export_error[APP_MAX_ERROR] = {0};
  if (!CliRenameExportHistoryJson(input->env_dir, entries, indices, filtered_count,
                                  &filter, input->rename_history_export_path,
                                  export_error, sizeof(export_error))) {
    AppFreeRenameHistoryEntries(entries);
    CliRenameFreeHistoryFilterIndex(indices);
    GuiWorkerSetResult(APP_STATUS_IO_ERROR,
                       export_error[0] != '\0' ? export_error
                                               : "Rename history export failed",
                       false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rename_history_count = count;
  g_worker.snapshot.rename_history_filtered_count = filtered_count;
  strncpy(g_worker.snapshot.rename_history_export_path,
          input->rename_history_export_path,
          sizeof(g_worker.snapshot.rename_history_export_path) - 1);
  g_worker.snapshot.rename_history_export_path
      [sizeof(g_worker.snapshot.rename_history_export_path) - 1] = '\0';
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Rename history export completed.\nOutput: %s\nTotal entries: %d\n"
           "Filtered entries: %d\n",
           input->rename_history_export_path, count, filtered_count);
  pthread_mutex_unlock(&g_worker.mutex);

  AppFreeRenameHistoryEntries(entries);
  CliRenameFreeHistoryFilterIndex(indices);
  GuiWorkerSetResult(APP_STATUS_OK, "Rename history export completed", true);
}

static void WorkerRunRenameHistoryPrune(const GuiTaskInput *input) {
  if (input->rename_history_prune_keep_count < 0) {
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT,
                       "Rename history prune keep count must be non-negative",
                       false);
    return;
  }

  AppRenameHistoryPruneRequest request = {
      .env_dir = input->env_dir,
      .keep_count = input->rename_history_prune_keep_count,
  };
  AppRenameHistoryPruneResult result = {0};
  AppStatus status = AppPruneRenameHistory(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename history prune failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rename_history_prune_result = result;
  g_worker.snapshot.rename_history_count = result.after_count;
  g_worker.snapshot.rename_history_filtered_count = result.after_count;
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Rename history prune completed.\nBefore: %d\nAfter: %d\nPruned: %d\n",
           result.before_count, result.after_count, result.pruned_count);
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Rename history prune completed", true);
}

static void WorkerRunRenameHistoryLatestId(const GuiTaskInput *input) {
  char latest_operation_id[64] = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!CliRenameResolveLatestOperationId(input->env_dir, latest_operation_id,
                                         sizeof(latest_operation_id), error,
                                         sizeof(error))) {
    GuiWorkerSetResult(APP_STATUS_IO_ERROR,
                       error[0] != '\0' ? error
                                        : "Failed to resolve latest operation id",
                       false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  strncpy(g_worker.snapshot.rename_latest_operation_id, latest_operation_id,
          sizeof(g_worker.snapshot.rename_latest_operation_id) - 1);
  g_worker.snapshot.rename_latest_operation_id
      [sizeof(g_worker.snapshot.rename_latest_operation_id) - 1] = '\0';
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Latest operation id: %s\n", latest_operation_id);
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Latest operation id resolved", true);
}

static void WorkerRunRenameHistoryDetail(const GuiTaskInput *input) {
  if (input->rename_operation_id[0] == '\0') {
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT,
                       "Rename history detail requires an operation id", false);
    return;
  }

  AppRenameHistoryEntry *entries = NULL;
  int count = 0;
  AppStatus status =
      AppListRenameHistory(g_worker.app, input->env_dir, &entries, &count);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename history detail query failed", false);
    return;
  }

  AppRenameHistoryEntry *selected = NULL;
  for (int i = 0; i < count; i++) {
    if (strcmp(entries[i].operation_id, input->rename_operation_id) == 0) {
      selected = &entries[i];
      break;
    }
  }
  if (!selected) {
    AppFreeRenameHistoryEntries(entries);
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT,
                       "Rename history operation id was not found", false);
    return;
  }

  char manifest_path[MAX_PATH_LENGTH] = {0};
  char resolve_error[APP_MAX_ERROR] = {0};
  if (!CliRenameResolveOperationManifestPath(
          input->env_dir, input->rename_operation_id, manifest_path,
          sizeof(manifest_path), resolve_error, sizeof(resolve_error))) {
    AppFreeRenameHistoryEntries(entries);
    GuiWorkerSetResult(APP_STATUS_IO_ERROR,
                       resolve_error[0] != '\0'
                           ? resolve_error
                           : "Failed to resolve rename operation manifest path",
                       false);
    return;
  }

  char *manifest_text = NULL;
  if (!LoadTextFile(manifest_path, &manifest_text)) {
    AppFreeRenameHistoryEntries(entries);
    GuiWorkerSetResult(APP_STATUS_IO_ERROR,
                       "Failed to load rename operation manifest", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rename_history_count = count;
  strncpy(g_worker.snapshot.rename_latest_operation_id, selected->operation_id,
          sizeof(g_worker.snapshot.rename_latest_operation_id) - 1);
  g_worker.snapshot.rename_latest_operation_id
      [sizeof(g_worker.snapshot.rename_latest_operation_id) - 1] = '\0';
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Rename history detail\n"
           "Operation ID: %s\n"
           "Preview ID: %s\n"
           "Created At: %s\n"
           "Counts: renamed=%d skipped=%d failed=%d collisions=%d truncations=%d\n"
           "Rollback: %s restored=%d skipped=%d failed=%d\n"
           "Manifest path: %s\n"
           "\n--- Operation Manifest (JSON) ---\n%s\n",
           selected->operation_id,
           selected->preview_id[0] != '\0' ? selected->preview_id
                                            : "unknown-preview",
           selected->created_at_utc[0] != '\0' ? selected->created_at_utc
                                                : "unknown-time",
           selected->renamed_count, selected->skipped_count,
           selected->failed_count, selected->collision_resolved_count,
           selected->truncation_count,
           selected->rollback_performed ? "yes" : "no",
           selected->rollback_restored_count, selected->rollback_skipped_count,
           selected->rollback_failed_count, manifest_path, manifest_text);
  pthread_mutex_unlock(&g_worker.mutex);

  free(manifest_text);
  AppFreeRenameHistoryEntries(entries);
  GuiWorkerSetResult(APP_STATUS_OK, "Rename history detail loaded", true);
}

bool GuiWorkerRunRenameHistoryTask(const GuiTaskInput *input) {
  if (!input) {
    return false;
  }

  switch (input->type) {
  case GUI_TASK_RENAME_HISTORY:
    WorkerRunRenameHistory(input);
    return true;
  case GUI_TASK_RENAME_HISTORY_EXPORT:
    WorkerRunRenameHistoryExport(input);
    return true;
  case GUI_TASK_RENAME_HISTORY_PRUNE:
    WorkerRunRenameHistoryPrune(input);
    return true;
  case GUI_TASK_RENAME_HISTORY_LATEST_ID:
    WorkerRunRenameHistoryLatestId(input);
    return true;
  case GUI_TASK_RENAME_HISTORY_DETAIL:
    WorkerRunRenameHistoryDetail(input);
    return true;
  default:
    return false;
  }
}
