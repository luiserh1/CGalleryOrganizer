#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/cli_app_error.h"
#include "cli/cli_args.h"
#include "cli/cli_rename_history_commands.h"

static bool IsHistoryFilterActive(const CliRenameHistoryFilter *filter) {
  if (!filter) {
    return false;
  }
  return filter->operation_id_prefix[0] != '\0' ||
         filter->rollback_filter != CLI_RENAME_ROLLBACK_FILTER_ANY ||
         filter->created_from_utc[0] != '\0' ||
         filter->created_to_utc[0] != '\0';
}

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

int CliHandleRenameHistory(AppContext *app, const char *target_dir,
                           const char *env_dir,
                           const CliRenameHistoryFilter *filter,
                           const char *argv0) {
  const char *history_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!history_env) {
    printf("Error: --rename-history requires an environment directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }
  if (!filter) {
    printf("Error: invalid rename history filter.\n");
    return 1;
  }

  AppRenameHistoryEntry *entries = NULL;
  int count = 0;
  AppStatus status = AppListRenameHistory(app, history_env, &entries, &count);
  if (status != APP_STATUS_OK) {
    CliPrintAppError(app, "Error: Failed to list rename history");
    return 1;
  }

  if (count <= 0 || !entries) {
    printf("Rename history is empty.\n");
    AppFreeRenameHistoryEntries(entries);
    return 0;
  }

  int *indices = NULL;
  int filtered_count = 0;
  char filter_error[APP_MAX_ERROR] = {0};
  if (!CliRenameBuildHistoryFilterIndex(entries, count, filter, &indices,
                                        &filtered_count, filter_error,
                                        sizeof(filter_error))) {
    printf("Error: Failed to filter rename history: %s\n", filter_error);
    AppFreeRenameHistoryEntries(entries);
    return 1;
  }

  if (filtered_count <= 0) {
    printf("Rename history has no entries for current filter.\n");
    AppFreeRenameHistoryEntries(entries);
    CliRenameFreeHistoryFilterIndex(indices);
    return 0;
  }

  if (IsHistoryFilterActive(filter)) {
    printf("Rename history entries: %d (filtered from %d)\n", filtered_count,
           count);
  } else {
    printf("Rename history entries: %d\n", filtered_count);
  }
  for (int i = 0; i < filtered_count; i++) {
    const AppRenameHistoryEntry *entry = &entries[indices[i]];
    printf("%d. %s (%s) renamed=%d skipped=%d failed=%d collisions=%d"
           " rollback=%s restored=%d\n",
           i + 1, entry->operation_id,
           entry->created_at_utc[0] != '\0' ? entry->created_at_utc
                                            : "unknown-time",
           entry->renamed_count, entry->skipped_count, entry->failed_count,
           entry->collision_resolved_count,
           entry->rollback_performed ? "yes" : "no",
           entry->rollback_restored_count);
  }

  CliRenameFreeHistoryFilterIndex(indices);
  AppFreeRenameHistoryEntries(entries);
  return 0;
}

int CliHandleRenameHistoryExport(AppContext *app, const char *target_dir,
                                 const char *env_dir,
                                 const CliRenameHistoryFilter *filter,
                                 const char *output_path,
                                 const char *argv0) {
  const char *history_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!history_env) {
    printf("Error: --rename-history-export requires an environment directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }
  if (!filter || !output_path || output_path[0] == '\0') {
    printf("Error: --rename-history-export requires a valid output path.\n");
    return 1;
  }

  AppRenameHistoryEntry *entries = NULL;
  int count = 0;
  AppStatus status = AppListRenameHistory(app, history_env, &entries, &count);
  if (status != APP_STATUS_OK) {
    CliPrintAppError(app, "Error: Failed to list rename history");
    return 1;
  }

  int *indices = NULL;
  int filtered_count = 0;
  char filter_error[APP_MAX_ERROR] = {0};
  if (!CliRenameBuildHistoryFilterIndex(entries, count, filter, &indices,
                                        &filtered_count, filter_error,
                                        sizeof(filter_error))) {
    printf("Error: Failed to filter rename history: %s\n", filter_error);
    AppFreeRenameHistoryEntries(entries);
    return 1;
  }

  char export_error[APP_MAX_ERROR] = {0};
  if (!CliRenameExportHistoryJson(history_env, entries, indices, filtered_count,
                                  filter, output_path, export_error,
                                  sizeof(export_error))) {
    printf("Error: Failed to export rename history: %s\n", export_error);
    AppFreeRenameHistoryEntries(entries);
    CliRenameFreeHistoryFilterIndex(indices);
    return 1;
  }

  printf("Rename history export saved: %s\n", output_path);
  printf("Exported entries: %d\n", filtered_count);
  if (IsHistoryFilterActive(filter)) {
    printf("Filter summary: prefix='%s' rollback=%s from=%s to=%s\n",
           filter->operation_id_prefix[0] != '\0' ? filter->operation_id_prefix
                                                  : "(none)",
           filter->rollback_filter == CLI_RENAME_ROLLBACK_FILTER_YES
               ? "yes"
               : (filter->rollback_filter == CLI_RENAME_ROLLBACK_FILTER_NO ? "no"
                                                                           : "any"),
           filter->created_from_utc[0] != '\0' ? filter->created_from_utc
                                               : "(none)",
           filter->created_to_utc[0] != '\0' ? filter->created_to_utc
                                             : "(none)");
  }

  AppFreeRenameHistoryEntries(entries);
  CliRenameFreeHistoryFilterIndex(indices);
  return 0;
}

int CliHandleRenameHistoryLatestId(const char *target_dir, const char *env_dir,
                                   const char *argv0) {
  const char *history_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!history_env) {
    printf("Error: --rename-history-latest-id requires an environment "
           "directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  char latest_operation_id[64] = {0};
  char resolve_error[APP_MAX_ERROR] = {0};
  if (!CliRenameResolveLatestOperationId(history_env, latest_operation_id,
                                         sizeof(latest_operation_id),
                                         resolve_error,
                                         sizeof(resolve_error))) {
    printf("Error: --rename-history-latest-id failed: %s\n", resolve_error);
    return 1;
  }

  printf("Latest operation ID: %s\n", latest_operation_id);
  return 0;
}

int CliHandleRenamePreviewLatestId(const char *target_dir, const char *env_dir,
                                   const char *argv0) {
  const char *preview_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!preview_env) {
    printf("Error: --rename-preview-latest-id requires an environment "
           "directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  char latest_preview_id[64] = {0};
  char resolve_error[APP_MAX_ERROR] = {0};
  if (!CliRenameResolveLatestPreviewId(preview_env, latest_preview_id,
                                       sizeof(latest_preview_id), resolve_error,
                                       sizeof(resolve_error))) {
    printf("Error: --rename-preview-latest-id failed: %s\n", resolve_error);
    return 1;
  }

  printf("Latest preview ID: %s\n", latest_preview_id);
  return 0;
}

int CliHandleRenameHistoryDetail(AppContext *app, const char *target_dir,
                                 const char *env_dir,
                                 const char *operation_id,
                                 const char *argv0) {
  const char *history_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!history_env) {
    printf("Error: --rename-history-detail requires an environment "
           "directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }
  if (!operation_id || operation_id[0] == '\0') {
    printf("Error: --rename-history-detail requires <operation_id>.\n");
    return 1;
  }

  AppRenameHistoryEntry *entries = NULL;
  int count = 0;
  AppStatus status = AppListRenameHistory(app, history_env, &entries, &count);
  if (status != APP_STATUS_OK) {
    CliPrintAppError(app, "Error: Failed to list rename history");
    return 1;
  }

  AppRenameHistoryEntry *entry = NULL;
  for (int i = 0; i < count; i++) {
    if (strcmp(entries[i].operation_id, operation_id) == 0) {
      entry = &entries[i];
      break;
    }
  }
  if (!entry) {
    printf("Error: Rename history entry '%s' was not found.\n", operation_id);
    AppFreeRenameHistoryEntries(entries);
    return 1;
  }

  char manifest_path[MAX_PATH_LENGTH] = {0};
  char resolve_error[APP_MAX_ERROR] = {0};
  if (!CliRenameResolveOperationManifestPath(history_env, operation_id,
                                             manifest_path,
                                             sizeof(manifest_path),
                                             resolve_error,
                                             sizeof(resolve_error))) {
    printf("Error: --rename-history-detail failed: %s\n", resolve_error);
    AppFreeRenameHistoryEntries(entries);
    return 1;
  }

  char *manifest_text = NULL;
  if (!LoadTextFile(manifest_path, &manifest_text)) {
    printf("Error: Failed to load rename operation manifest '%s'.\n",
           manifest_path);
    AppFreeRenameHistoryEntries(entries);
    return 1;
  }

  printf("Rename history detail:\n");
  printf("Operation ID: %s\n", entry->operation_id);
  printf("Preview ID: %s\n", entry->preview_id[0] != '\0' ? entry->preview_id
                                                           : "unknown-preview");
  printf("Created At: %s\n",
         entry->created_at_utc[0] != '\0' ? entry->created_at_utc
                                           : "unknown-time");
  printf("Counts: renamed=%d skipped=%d failed=%d collisions=%d truncations=%d\n",
         entry->renamed_count, entry->skipped_count, entry->failed_count,
         entry->collision_resolved_count, entry->truncation_count);
  printf("Rollback: %s restored=%d skipped=%d failed=%d\n",
         entry->rollback_performed ? "yes" : "no",
         entry->rollback_restored_count, entry->rollback_skipped_count,
         entry->rollback_failed_count);
  printf("Manifest path: %s\n", manifest_path);
  printf("\n--- Operation Manifest (JSON) ---\n%s\n", manifest_text);

  free(manifest_text);
  AppFreeRenameHistoryEntries(entries);
  return 0;
}

int CliHandleRenameRollback(AppContext *app, const char *target_dir,
                            const char *env_dir, const char *operation_id,
                            const char *argv0) {
  const char *rollback_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!rollback_env) {
    printf("Error: --rename-rollback requires an environment directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }
  if (!operation_id || operation_id[0] == '\0') {
    printf("Error: --rename-rollback requires <operation_id>.\n");
    return 1;
  }

  AppRenameRollbackRequest request = {
      .env_dir = rollback_env,
      .operation_id = operation_id,
      .hooks = {0},
  };
  AppRenameRollbackResult result = {0};

  AppStatus status = AppRollbackRename(app, &request, &result);
  if (status != APP_STATUS_OK) {
    CliPrintAppError(app, "Error: Rename rollback failed");
    return 1;
  }

  printf("Rename rollback complete. Restored=%d Skipped=%d Failed=%d\n",
         result.restored_count, result.skipped_count, result.failed_count);
  return 0;
}

int CliHandleRenameRollbackPreflight(AppContext *app, const char *target_dir,
                                     const char *env_dir,
                                     const char *operation_id,
                                     const char *argv0) {
  const char *rollback_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!rollback_env) {
    printf("Error: --rename-rollback-preflight requires an environment "
           "directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }
  if (!operation_id || operation_id[0] == '\0') {
    printf("Error: --rename-rollback-preflight requires <operation_id>.\n");
    return 1;
  }

  AppRenameRollbackPreflightRequest request = {
      .env_dir = rollback_env,
      .operation_id = operation_id,
  };
  AppRenameRollbackPreflightResult result = {0};

  AppStatus status = AppPreflightRenameRollback(app, &request, &result);
  if (status != APP_STATUS_OK) {
    CliPrintAppError(app, "Error: Rename rollback preflight failed");
    return 1;
  }

  printf("Rename rollback preflight complete.\n");
  printf("Operation ID: %s\n", operation_id);
  printf("Total items: %d\n", result.total_items);
  printf("Restorable: %d\n", result.restorable_count);
  printf("Missing destination: %d\n", result.missing_destination_count);
  printf("Source conflicts: %d\n", result.source_exists_conflict_count);
  printf("Invalid items: %d\n", result.invalid_item_count);
  printf("Fully restorable: %s\n", result.fully_restorable ? "yes" : "no");
  return 0;
}

int CliHandleRenameHistoryPrune(AppContext *app, const char *target_dir,
                                const char *env_dir, int keep_count,
                                const char *argv0) {
  const char *history_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!history_env) {
    printf("Error: --rename-history-prune requires an environment directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }
  if (keep_count < 0) {
    printf("Error: --rename-history-prune requires non-negative keep count.\n");
    return 1;
  }

  AppRenameHistoryPruneRequest request = {
      .env_dir = history_env,
      .keep_count = keep_count,
  };
  AppRenameHistoryPruneResult result = {0};
  AppStatus status = AppPruneRenameHistory(app, &request, &result);
  if (status != APP_STATUS_OK) {
    CliPrintAppError(app, "Error: Rename history prune failed");
    return 1;
  }

  printf("Rename history prune completed.\n");
  printf("Before: %d\n", result.before_count);
  printf("After: %d\n", result.after_count);
  printf("Pruned: %d\n", result.pruned_count);
  return 0;
}
