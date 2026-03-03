#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "app_api.h"
#include "cJSON.h"
#include "cli/cli_args.h"
#include "cli/cli_rename_utils.h"

static void PrintAppError(AppContext *app, const char *prefix);
static void PrintRenameMetadataFields(const char *details_json);
static bool SaveTextFile(const char *path, const char *text);
static bool LoadTextFile(const char *path, char **out_text);

static int HandleRollback(AppContext *app, const char *target_dir,
                          const char *env_dir, const char *argv0) {
  const char *rollback_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!rollback_env) {
    printf("Error: --rollback requires an environment directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  printf("\n[*] Initiating Rollback from %s...\n", rollback_env);
  AppRollbackRequest request = {.env_dir = rollback_env};
  AppRollbackResult result = {0};
  AppStatus status = AppRollback(app, &request, &result);
  if (status != APP_STATUS_OK) {
    PrintAppError(app, "[!] Rollback failed to execute properly");
    return 1;
  }

  printf("[*] Rollback complete. Restored %d files.\n", result.restored_count);
  return 0;
}

static int HandleRenameInit(const char *target_dir, const char *env_dir,
                            const char *argv0) {
  if (!target_dir || target_dir[0] == '\0' || !env_dir || env_dir[0] == '\0') {
    printf("Error: --rename-init requires <target_dir> <env_dir>.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  CliRenameInitResult result = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!CliRenameInitEnvironment(target_dir, env_dir, &result, error,
                                sizeof(error))) {
    printf("Error: Rename init failed: %s\n", error);
    if (strstr(error, "does not exist") != NULL) {
      char suggestion[MAX_PATH_LENGTH] = {0};
      if (CliRenameSuggestPath(target_dir, suggestion, sizeof(suggestion))) {
        printf("Hint: did you mean '%s'?\n", suggestion);
      }
    }
    return 1;
  }

  printf("Rename init ready.\n");
  printf("Target (abs): %s\n", result.target_abs);
  printf("Env (abs): %s\n", result.env_abs);
  printf("Files in scope: %d\n", result.media_files_in_scope);
  printf("Cache layout: %s/.cache/{rename_previews,rename_history}\n",
         result.env_abs);
  return 0;
}

static int HandleRenameBootstrap(const char *target_dir, const char *env_dir,
                                 const char *argv0) {
  if (!target_dir || target_dir[0] == '\0' || !env_dir || env_dir[0] == '\0') {
    printf("Error: --rename-bootstrap-tags-from-filename requires <target_dir> "
           "<env_dir>.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  CliRenameBootstrapResult result = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!CliRenameBootstrapTagsFromFilename(target_dir, env_dir, &result, error,
                                          sizeof(error))) {
    printf("Error: rename bootstrap failed: %s\n", error);
    if (strstr(error, "does not exist") != NULL) {
      char suggestion[MAX_PATH_LENGTH] = {0};
      if (CliRenameSuggestPath(target_dir, suggestion, sizeof(suggestion))) {
        printf("Hint: did you mean '%s'?\n", suggestion);
      }
    }
    return 1;
  }

  printf("Rename tag bootstrap completed.\n");
  printf("Tags map: %s\n", result.map_path);
  printf("Files scanned: %d\n", result.files_scanned);
  printf("Files with inferred tags: %d\n", result.files_with_tags);
  printf("Use with preview: --rename-tags-map \"%s\"\n", result.map_path);
  return 0;
}

static int HandleRenamePreview(AppContext *app, const char *target_dir,
                               const char *env_dir, const char *pattern,
                               const char *tags_map_path,
                               const char *tag_add_csv,
                               const char *tag_remove_csv,
                               const char *meta_tag_add_csv,
                               const char *meta_tag_remove_csv,
                               bool show_metadata_fields,
                               bool preview_json,
                               const char *preview_json_out,
                               const char *argv0) {
  if (!target_dir || target_dir[0] == '\0' || !env_dir || env_dir[0] == '\0') {
    printf("Error: --rename-preview requires <target_dir> <env_dir>.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  struct stat st;
  if (stat(target_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
    printf("Error: Failed to generate rename preview: target directory '%s' "
           "does not exist.\n",
           target_dir);
    char suggestion[MAX_PATH_LENGTH] = {0};
    if (CliRenameSuggestPath(target_dir, suggestion, sizeof(suggestion))) {
      printf("Hint: did you mean '%s'?\n", suggestion);
    }
    return 1;
  }

  AppRenamePreviewRequest request = {
      .target_dir = target_dir,
      .env_dir = env_dir,
      .pattern = pattern,
      .tags_map_json_path = tags_map_path,
      .tag_add_csv = tag_add_csv,
      .tag_remove_csv = tag_remove_csv,
      .meta_tag_add_csv = meta_tag_add_csv,
      .meta_tag_remove_csv = meta_tag_remove_csv,
      .hooks = {0},
  };
  AppRenamePreviewResult result = {0};

  AppStatus status = AppPreviewRename(app, &request, &result);
  if (status != APP_STATUS_OK) {
    PrintAppError(app, "Error: Failed to generate rename preview");
    AppFreeRenamePreviewResult(&result);
    return 1;
  }

  printf("Rename preview ready.\n");
  printf("Preview ID: %s\n", result.preview_id);
  printf("Preview artifact: %s\n", result.preview_path);
  printf("Pattern: %s\n", result.effective_pattern);
  printf("Files in scope: %d\n", result.file_count);
  printf("Collision groups: %d\n", result.collision_group_count);
  printf("Colliding items: %d\n", result.collision_count);
  printf("Truncated names: %d\n", result.truncation_count);
  if (result.requires_auto_suffix_acceptance) {
    printf("Apply guard: collisions detected; --rename-accept-auto-suffix "
           "is required on apply.\n");
  }
  if (preview_json_out && preview_json_out[0] != '\0') {
    if (!result.details_json || result.details_json[0] == '\0') {
      printf("Warning: preview details JSON was empty; nothing written to '%s'.\n",
             preview_json_out);
    } else if (!SaveTextFile(preview_json_out, result.details_json)) {
      printf("Error: Failed to write preview JSON to '%s'.\n",
             preview_json_out);
      AppFreeRenamePreviewResult(&result);
      return 1;
    } else {
      printf("Preview JSON saved: %s\n", preview_json_out);
    }
  }
  if (preview_json && result.details_json && result.details_json[0] != '\0') {
    printf("\n--- Preview Details (JSON) ---\n%s\n", result.details_json);
  } else if (!preview_json) {
    printf("Hint: use --rename-preview-json or --rename-preview-json-out <path> "
           "for full JSON details.\n");
  }
  if (show_metadata_fields) {
    PrintRenameMetadataFields(result.details_json);
  }

  AppFreeRenamePreviewResult(&result);
  return 0;
}

static int HandleRenameApply(AppContext *app, const char *target_dir,
                             const char *env_dir,
                             const char *rename_from_preview,
                             bool accept_auto_suffix, const char *argv0) {
  const char *apply_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!apply_env) {
    printf("Error: --rename-apply requires an environment directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }
  if (!rename_from_preview || rename_from_preview[0] == '\0') {
    printf("Error: --rename-apply requires --rename-from-preview <preview_id>.\n");
    return 1;
  }

  AppRenameApplyRequest request = {
      .env_dir = apply_env,
      .preview_id = rename_from_preview,
      .accept_auto_suffix = accept_auto_suffix,
      .hooks = {0},
  };
  AppRenameApplyResult result = {0};

  AppStatus status = AppApplyRename(app, &request, &result);
  if (status != APP_STATUS_OK) {
    PrintAppError(app, "Error: Rename apply failed");
    return 1;
  }

  printf("Rename apply completed.\n");
  printf("Operation ID: %s\n", result.operation_id);
  printf("Renamed: %d\n", result.renamed_count);
  printf("Skipped: %d\n", result.skipped_count);
  printf("Failed: %d\n", result.failed_count);
  printf("Collision resolutions: %d\n", result.collision_resolved_count);
  printf("Truncated names in plan: %d\n", result.truncation_count);
  if (result.auto_suffix_applied) {
    printf("Auto suffix policy applied: yes\n");
  }

  return 0;
}

static bool IsHistoryFilterActive(const CliRenameHistoryFilter *filter) {
  if (!filter) {
    return false;
  }
  return filter->operation_id_prefix[0] != '\0' ||
         filter->rollback_filter != CLI_RENAME_ROLLBACK_FILTER_ANY ||
         filter->created_from_utc[0] != '\0' ||
         filter->created_to_utc[0] != '\0';
}

static int HandleRenameHistory(AppContext *app, const char *target_dir,
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
    PrintAppError(app, "Error: Failed to list rename history");
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

static int HandleRenameHistoryExport(AppContext *app, const char *target_dir,
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
    PrintAppError(app, "Error: Failed to list rename history");
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

static int HandleRenameHistoryLatestId(const char *target_dir,
                                       const char *env_dir,
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

static int HandleRenamePreviewLatestId(const char *target_dir, const char *env_dir,
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

static int HandleRenameHistoryDetail(AppContext *app, const char *target_dir,
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
    PrintAppError(app, "Error: Failed to list rename history");
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

static int HandleRenameRollback(AppContext *app, const char *target_dir,
                                const char *env_dir,
                                const char *operation_id,
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
    PrintAppError(app, "Error: Rename rollback failed");
    return 1;
  }

  printf("Rename rollback complete. Restored=%d Skipped=%d Failed=%d\n",
         result.restored_count, result.skipped_count, result.failed_count);
  return 0;
}

static int HandleRenameRollbackPreflight(AppContext *app, const char *target_dir,
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
    PrintAppError(app, "Error: Rename rollback preflight failed");
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

static int HandleRenameHistoryPrune(AppContext *app, const char *target_dir,
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
    PrintAppError(app, "Error: Rename history prune failed");
    return 1;
  }

  printf("Rename history prune completed.\n");
  printf("Before: %d\n", result.before_count);
  printf("After: %d\n", result.after_count);
  printf("Pruned: %d\n", result.pruned_count);
  return 0;
}

static int HandleDuplicates(AppContext *app, const char *env_dir,
                            AppCacheCompressionMode compression_mode,
                            int compression_level, bool move_duplicates) {
  AppDuplicateFindRequest find_request = {
      .env_dir = env_dir,
      .cache_compression_mode = compression_mode,
      .cache_compression_level = compression_level,
  };
  AppDuplicateReport report = {0};
  AppStatus status = AppFindDuplicates(app, &find_request, &report);
  if (status != APP_STATUS_OK) {
    printf("Error: Failed to find duplicates.\n");
    return 1;
  }

  if (report.group_count <= 0) {
    printf("No exact duplicates found.\n");
    AppFreeDuplicateReport(&report);
    return 0;
  }

  if (move_duplicates) {
    if (!env_dir || env_dir[0] == '\0') {
      printf("Error: --duplicates-move requires an environment directory.\n");
      AppFreeDuplicateReport(&report);
      return 1;
    }

    AppDuplicateMoveRequest move_request = {
        .target_dir = env_dir,
        .report = &report,
    };
    AppDuplicateMoveResult move_result = {0};
    status = AppMoveDuplicates(app, &move_request, &move_result);
    if (status != APP_STATUS_OK) {
      PrintAppError(app, "Error: Failed to move duplicates");
      AppFreeDuplicateReport(&report);
      return 1;
    }

    printf("Found %d groups of exact duplicates. Moving copies to '%s'...\n",
           report.group_count, env_dir);
    printf("Successfully moved %d duplicate files.\n", move_result.moved_count);
    if (move_result.failed_count > 0) {
      printf("Failed to move %d duplicate files.\n", move_result.failed_count);
    }
  } else {
    printf("Found %d groups of exact duplicates (dry-run):\n", report.group_count);
    for (int i = 0; i < report.group_count; i++) {
      printf("  Group %d (Original: %s):\n", i + 1,
             report.groups[i].original_path);
      for (int j = 0; j < report.groups[i].duplicate_count; j++) {
        printf("    Duplicate: %s\n", report.groups[i].duplicate_paths[j]);
      }
    }
    printf("\nTo move duplicates, rerun with --duplicates-move.\n");
  }

  AppFreeDuplicateReport(&report);
  return 0;
}

static void PrintRenameMetadataFields(const char *details_json) {
  if (!details_json || details_json[0] == '\0') {
    printf("Metadata tag fields in scope: unavailable (no preview JSON details).\n");
    return;
  }

  cJSON *root = cJSON_Parse(details_json);
  if (!root) {
    printf("Metadata tag fields in scope: unavailable (preview JSON parse failed).\n");
    return;
  }

  cJSON *fields = cJSON_GetObjectItem(root, "metadataTagFields");
  if (!cJSON_IsArray(fields) || cJSON_GetArraySize(fields) <= 0) {
    printf("Metadata tag fields in scope: none detected from whitelist.\n");
    cJSON_Delete(root);
    return;
  }

  int count = cJSON_GetArraySize(fields);
  printf("Metadata tag fields in scope (%d):\n", count);
  for (int i = 0; i < count; i++) {
    cJSON *field = cJSON_GetArrayItem(fields, i);
    if (!cJSON_IsString(field) || !field->valuestring ||
        field->valuestring[0] == '\0') {
      continue;
    }
    printf("  - %s\n", field->valuestring);
  }

  cJSON_Delete(root);
}

static void PrintAppError(AppContext *app, const char *prefix) {
  const char *details = AppGetLastError(app);
  if (details && details[0] != '\0') {
    printf("%s: %s\n", prefix, details);
  } else {
    printf("%s.\n", prefix);
  }
}

static bool SaveTextFile(const char *path, const char *text) {
  if (!path || path[0] == '\0' || !text) {
    return false;
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }

  bool ok = fputs(text, f) >= 0;
  fclose(f);
  return ok;
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

int main(int argc, char **argv) {
  printf("CGalleryOrganizer v0.6.10\n");

  bool exhaustive = false;
  bool ml_enrich = false;
  bool similarity_report = false;
  bool sim_incremental = true;
  bool organize = false;
  bool preview = false;
  bool duplicates_report = false;
  bool duplicates_move = false;
  bool rollback = false;

  bool rename_preview = false;
  bool rename_apply = false;
  bool rename_apply_latest = false;
  bool rename_preview_latest_id = false;
  bool rename_init = false;
  bool rename_bootstrap_tags_from_filename = false;
  bool rename_history = false;
  bool rename_history_export = false;
  bool rename_history_prune = false;
  bool rename_history_latest_id = false;
  const char *rename_history_detail_operation = NULL;
  const char *rename_history_export_path = NULL;
  const char *rename_history_id_prefix = NULL;
  const char *rename_history_rollback_filter = NULL;
  const char *rename_history_from = NULL;
  const char *rename_history_to = NULL;
  int rename_history_prune_keep_count = -1;
  const char *rename_redo_operation = NULL;
  const char *rename_rollback_operation = NULL;
  const char *rename_rollback_preflight_operation = NULL;
  const char *rename_pattern = NULL;
  const char *rename_tags_map_path = NULL;
  const char *rename_tag_add_csv = NULL;
  const char *rename_tag_remove_csv = NULL;
  const char *rename_meta_tag_add_csv = NULL;
  const char *rename_meta_tag_remove_csv = NULL;
  bool rename_metadata_fields = false;
  bool rename_preview_json = false;
  const char *rename_preview_json_out = NULL;
  const char *rename_from_preview = NULL;
  bool rename_accept_auto_suffix = false;

  const char *jobs_arg = "auto";
  bool jobs_set_by_cli = false;
  float sim_threshold = 0.92f;
  int sim_topk = 5;
  AppSimilarityMemoryMode sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
  AppCacheCompressionMode cache_compression_mode = APP_CACHE_COMPRESSION_NONE;
  int cache_compression_level = 3;
  bool cache_compression_level_set = false;
  const char *target_dir = NULL;
  const char *env_dir = NULL;
  const char *group_by_arg = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      CliPrintUsage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-e") == 0 ||
               strcmp(argv[i], "--exhaustive") == 0) {
      exhaustive = true;
    } else if (strcmp(argv[i], "--ml-enrich") == 0) {
      ml_enrich = true;
    } else if (strcmp(argv[i], "--similarity-report") == 0) {
      similarity_report = true;
    } else if (strcmp(argv[i], "--sim-incremental") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-incremental requires on|off.\n");
        return 1;
      }
      const char *value = argv[++i];
      if (strcmp(value, "on") == 0) {
        sim_incremental = true;
      } else if (strcmp(value, "off") == 0) {
        sim_incremental = false;
      } else {
        printf("Error: --sim-incremental must be on|off.\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--sim-threshold") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-threshold requires a value in [0,1].\n");
        return 1;
      }
      char *endptr = NULL;
      sim_threshold = strtof(argv[++i], &endptr);
      if (!endptr || *endptr != '\0' || sim_threshold < 0.0f ||
          sim_threshold > 1.0f) {
        printf("Error: --sim-threshold must be a number in [0,1].\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--sim-topk") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-topk requires a positive integer.\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed <= 0 || parsed > 1000) {
        printf("Error: --sim-topk must be a positive integer.\n");
        return 1;
      }
      sim_topk = (int)parsed;
    } else if (strcmp(argv[i], "--jobs") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --jobs requires n|auto.\n");
        return 1;
      }
      jobs_arg = argv[++i];
      jobs_set_by_cli = true;
    } else if (strcmp(argv[i], "--sim-memory-mode") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-memory-mode requires eager|chunked.\n");
        return 1;
      }
      const char *mode = argv[++i];
      if (strcmp(mode, "eager") == 0) {
        sim_memory_mode = APP_SIM_MEMORY_EAGER;
      } else if (strcmp(mode, "chunked") == 0) {
        sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
      } else {
        printf("Error: --sim-memory-mode must be eager|chunked.\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--cache-compress") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --cache-compress requires a mode: none|zstd|auto.\n");
        return 1;
      }
      const char *mode = argv[++i];
      if (strcmp(mode, "none") == 0) {
        cache_compression_mode = APP_CACHE_COMPRESSION_NONE;
      } else if (strcmp(mode, "zstd") == 0) {
        cache_compression_mode = APP_CACHE_COMPRESSION_ZSTD;
      } else if (strcmp(mode, "auto") == 0) {
        cache_compression_mode = APP_CACHE_COMPRESSION_AUTO;
      } else {
        printf("Error: Invalid --cache-compress mode '%s'. Allowed: "
               "none|zstd|auto\n",
               mode);
        return 1;
      }
    } else if (strcmp(argv[i], "--cache-compress-level") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --cache-compress-level requires a value in [1,19].\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed < 1 || parsed > 19) {
        printf("Error: --cache-compress-level must be in [1,19].\n");
        return 1;
      }
      cache_compression_level = (int)parsed;
      cache_compression_level_set = true;
    } else if (strcmp(argv[i], "--organize") == 0) {
      organize = true;
    } else if (strcmp(argv[i], "--group-by") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --group-by requires a comma-separated key list.\n");
        return 1;
      }
      group_by_arg = argv[++i];
    } else if (strcmp(argv[i], "--preview") == 0) {
      preview = true;
    } else if (strcmp(argv[i], "--duplicates-report") == 0) {
      duplicates_report = true;
    } else if (strcmp(argv[i], "--duplicates-move") == 0) {
      duplicates_move = true;
    } else if (strcmp(argv[i], "--rollback") == 0) {
      rollback = true;
    } else if (strcmp(argv[i], "--rename-preview") == 0) {
      rename_preview = true;
    } else if (strcmp(argv[i], "--rename-apply") == 0) {
      rename_apply = true;
    } else if (strcmp(argv[i], "--rename-apply-latest") == 0) {
      rename_apply_latest = true;
    } else if (strcmp(argv[i], "--rename-preview-latest-id") == 0) {
      rename_preview_latest_id = true;
    } else if (strcmp(argv[i], "--rename-init") == 0) {
      rename_init = true;
    } else if (strcmp(argv[i], "--rename-bootstrap-tags-from-filename") == 0) {
      rename_bootstrap_tags_from_filename = true;
    } else if (strcmp(argv[i], "--rename-pattern") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-pattern requires a pattern value.\n");
        return 1;
      }
      rename_pattern = argv[++i];
    } else if (strcmp(argv[i], "--rename-tags-map") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-tags-map requires a JSON path.\n");
        return 1;
      }
      rename_tags_map_path = argv[++i];
    } else if (strcmp(argv[i], "--rename-tag-add") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-tag-add requires CSV tags.\n");
        return 1;
      }
      rename_tag_add_csv = argv[++i];
    } else if (strcmp(argv[i], "--rename-tag-remove") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-tag-remove requires CSV tags.\n");
        return 1;
      }
      rename_tag_remove_csv = argv[++i];
    } else if (strcmp(argv[i], "--rename-meta-tag-add") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-meta-tag-add requires CSV tags.\n");
        return 1;
      }
      rename_meta_tag_add_csv = argv[++i];
    } else if (strcmp(argv[i], "--rename-meta-tag-remove") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-meta-tag-remove requires CSV tags.\n");
        return 1;
      }
      rename_meta_tag_remove_csv = argv[++i];
    } else if (strcmp(argv[i], "--rename-meta-fields") == 0) {
      rename_metadata_fields = true;
    } else if (strcmp(argv[i], "--rename-preview-json") == 0) {
      rename_preview_json = true;
    } else if (strcmp(argv[i], "--rename-preview-json-out") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-preview-json-out requires an output path.\n");
        return 1;
      }
      rename_preview_json_out = argv[++i];
    } else if (strcmp(argv[i], "--rename-from-preview") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-from-preview requires a preview id.\n");
        return 1;
      }
      rename_from_preview = argv[++i];
    } else if (strcmp(argv[i], "--rename-accept-auto-suffix") == 0) {
      rename_accept_auto_suffix = true;
    } else if (strcmp(argv[i], "--rename-history") == 0) {
      rename_history = true;
    } else if (strcmp(argv[i], "--rename-history-export") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-export requires an output JSON path.\n");
        return 1;
      }
      rename_history_export = true;
      rename_history_export_path = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-id-prefix") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-id-prefix requires a prefix value.\n");
        return 1;
      }
      rename_history_id_prefix = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-rollback") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-rollback requires any|yes|no.\n");
        return 1;
      }
      rename_history_rollback_filter = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-from") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-from requires YYYY-MM-DD or "
               "YYYY-MM-DDTHH:MM:SSZ.\n");
        return 1;
      }
      rename_history_from = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-to") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-to requires YYYY-MM-DD or "
               "YYYY-MM-DDTHH:MM:SSZ.\n");
        return 1;
      }
      rename_history_to = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-prune") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-prune requires a non-negative keep "
               "count.\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed < 0 || parsed > 1000000) {
        printf("Error: --rename-history-prune keep count must be a non-negative "
               "integer.\n");
        return 1;
      }
      rename_history_prune = true;
      rename_history_prune_keep_count = (int)parsed;
    } else if (strcmp(argv[i], "--rename-history-latest-id") == 0) {
      rename_history_latest_id = true;
    } else if (strcmp(argv[i], "--rename-history-detail") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-detail requires an operation id.\n");
        return 1;
      }
      rename_history_detail_operation = argv[++i];
    } else if (strcmp(argv[i], "--rename-redo") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-redo requires an operation id.\n");
        return 1;
      }
      rename_redo_operation = argv[++i];
    } else if (strcmp(argv[i], "--rename-rollback") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-rollback requires an operation id.\n");
        return 1;
      }
      rename_rollback_operation = argv[++i];
    } else if (strcmp(argv[i], "--rename-rollback-preflight") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-rollback-preflight requires an operation id.\n");
        return 1;
      }
      rename_rollback_preflight_operation = argv[++i];
    } else if (argv[i][0] == '-') {
      printf("Error: Unknown option '%s'.\n", argv[i]);
      CliPrintUsage(argv[0]);
      return 1;
    } else if (target_dir == NULL) {
      target_dir = argv[i];
    } else if (env_dir == NULL) {
      env_dir = argv[i];
    } else {
      printf("Error: Unexpected positional argument '%s'.\n", argv[i]);
      CliPrintUsage(argv[0]);
      return 1;
    }
  }

  bool rename_rollback =
      rename_rollback_operation && rename_rollback_operation[0] != '\0';
  bool rename_rollback_preflight = rename_rollback_preflight_operation &&
                                   rename_rollback_preflight_operation[0] != '\0';
  bool rename_history_detail =
      rename_history_detail_operation && rename_history_detail_operation[0] != '\0';
  bool rename_redo =
      rename_redo_operation && rename_redo_operation[0] != '\0';
  bool rename_apply_action = rename_apply || rename_apply_latest || rename_redo;
  bool rename_history_filter_active =
      (rename_history_id_prefix && rename_history_id_prefix[0] != '\0') ||
      (rename_history_rollback_filter &&
       rename_history_rollback_filter[0] != '\0') ||
      (rename_history_from && rename_history_from[0] != '\0') ||
      (rename_history_to && rename_history_to[0] != '\0');
  int rename_apply_variant_count = 0;
  rename_apply_variant_count += rename_apply ? 1 : 0;
  rename_apply_variant_count += rename_apply_latest ? 1 : 0;
  rename_apply_variant_count += rename_redo ? 1 : 0;

  int rename_action_count = 0;
  rename_action_count += rename_init ? 1 : 0;
  rename_action_count += rename_bootstrap_tags_from_filename ? 1 : 0;
  rename_action_count += rename_preview ? 1 : 0;
  rename_action_count += rename_preview_latest_id ? 1 : 0;
  rename_action_count += rename_apply_action ? 1 : 0;
  rename_action_count += rename_history ? 1 : 0;
  rename_action_count += rename_history_export ? 1 : 0;
  rename_action_count += rename_history_prune ? 1 : 0;
  rename_action_count += rename_history_latest_id ? 1 : 0;
  rename_action_count += rename_history_detail ? 1 : 0;
  rename_action_count += rename_rollback ? 1 : 0;
  rename_action_count += rename_rollback_preflight ? 1 : 0;

  if (rename_action_count > 1) {
    printf("Error: Rename actions are mutually exclusive "
           "(--rename-init|--rename-bootstrap-tags-from-filename|"
           "--rename-preview|--rename-preview-latest-id|"
           "--rename-apply|--rename-apply-latest|--rename-redo|"
           "--rename-history|--rename-history-export|"
           "--rename-history-prune|--rename-history-latest-id|"
           "--rename-history-detail|--rename-rollback|"
           "--rename-rollback-preflight).\n");
    return 1;
  }

  if (rename_apply_variant_count > 1) {
    printf("Error: --rename-apply, --rename-apply-latest, and --rename-redo "
           "cannot be used together.\n");
    return 1;
  }

  if (rename_apply && (!rename_from_preview || rename_from_preview[0] == '\0')) {
    printf("Error: --rename-apply requires --rename-from-preview <preview_id>.\n");
    return 1;
  }

  if (!rename_apply && rename_from_preview && rename_from_preview[0] != '\0') {
    printf("Error: --rename-from-preview can only be used with --rename-apply.\n");
    return 1;
  }

  if (rename_apply_latest && rename_from_preview && rename_from_preview[0] != '\0') {
    printf("Error: --rename-from-preview cannot be used with "
           "--rename-apply-latest.\n");
    return 1;
  }

  if (rename_redo && rename_from_preview && rename_from_preview[0] != '\0') {
    printf("Error: --rename-from-preview cannot be used with --rename-redo.\n");
    return 1;
  }

  if (!rename_apply_action && rename_accept_auto_suffix) {
    printf("Error: --rename-accept-auto-suffix can only be used with "
           "--rename-apply or --rename-apply-latest.\n");
    return 1;
  }

  if (!rename_preview &&
      (rename_preview_json ||
       (rename_preview_json_out && rename_preview_json_out[0] != '\0'))) {
    printf("Error: --rename-preview-json and --rename-preview-json-out are only "
           "valid with --rename-preview.\n");
    return 1;
  }

  if (!rename_preview &&
      ((rename_pattern && rename_pattern[0] != '\0') ||
       (rename_tags_map_path && rename_tags_map_path[0] != '\0') ||
       (rename_tag_add_csv && rename_tag_add_csv[0] != '\0') ||
       (rename_tag_remove_csv && rename_tag_remove_csv[0] != '\0') ||
       (rename_meta_tag_add_csv && rename_meta_tag_add_csv[0] != '\0') ||
       (rename_meta_tag_remove_csv && rename_meta_tag_remove_csv[0] != '\0') ||
       rename_metadata_fields)) {
    printf("Error: rename pattern/tag edit options are only valid with "
           "--rename-preview.\n");
    return 1;
  }

  if (rename_history_filter_active &&
      !(rename_history || rename_history_export)) {
    printf("Error: --rename-history-id-prefix/--rename-history-rollback/"
           "--rename-history-from/--rename-history-to require "
           "--rename-history or --rename-history-export.\n");
    return 1;
  }

  if (rename_history_export &&
      (!rename_history_export_path || rename_history_export_path[0] == '\0')) {
    printf("Error: --rename-history-export requires an output JSON path.\n");
    return 1;
  }

  bool has_non_rename_actions = rollback || organize || preview || duplicates_report ||
                                duplicates_move || similarity_report || ml_enrich;
  if (rename_action_count > 0 && has_non_rename_actions) {
    printf("Error: Rename actions cannot be combined with scan/organize/"
           "similarity/duplicate/rollback actions.\n");
    return 1;
  }

  if (rename_action_count > 0 && (jobs_set_by_cli || exhaustive || group_by_arg)) {
    printf("Error: --jobs/--exhaustive/--group-by are not applicable to dedicated "
           "rename actions.\n");
    return 1;
  }

  if ((rename_init || rename_bootstrap_tags_from_filename) &&
      (!target_dir || target_dir[0] == '\0' || !env_dir || env_dir[0] == '\0')) {
    printf("Error: --rename-init and --rename-bootstrap-tags-from-filename "
           "require <target_dir> <env_dir>.\n");
    return 1;
  }

  if (rollback && (organize || preview)) {
    printf("Error: Cannot specify --rollback with --organize or --preview.\n");
    return 1;
  }
  if (rollback && (duplicates_report || duplicates_move)) {
    printf("Error: Cannot specify --rollback with duplicate actions.\n");
    return 1;
  }
  if (rollback && similarity_report) {
    printf("Error: Cannot specify --rollback with --similarity-report.\n");
    return 1;
  }
  if (organize && preview) {
    printf("Error: Cannot specify --organize and --preview together.\n");
    return 1;
  }
  if (similarity_report && (organize || preview)) {
    printf("Error: --similarity-report cannot be combined with --organize/"
           "--preview.\n");
    return 1;
  }
  if (duplicates_report && duplicates_move) {
    printf("Error: Cannot specify --duplicates-report and --duplicates-move together.\n");
    return 1;
  }
  if ((duplicates_report || duplicates_move) &&
      (organize || preview || similarity_report)) {
    printf("Error: Duplicate actions cannot be combined with "
           "--similarity-report/--organize/--preview.\n");
    return 1;
  }
  if (cache_compression_level_set &&
      cache_compression_mode != APP_CACHE_COMPRESSION_ZSTD &&
      cache_compression_mode != APP_CACHE_COMPRESSION_AUTO) {
    printf("Error: --cache-compress-level is only valid with --cache-compress "
           "zstd|auto.\n");
    return 1;
  }

  CliRenameHistoryFilter rename_history_filter = {0};
  rename_history_filter.rollback_filter = CLI_RENAME_ROLLBACK_FILTER_ANY;
  if (rename_history_id_prefix && rename_history_id_prefix[0] != '\0') {
    strncpy(rename_history_filter.operation_id_prefix, rename_history_id_prefix,
            sizeof(rename_history_filter.operation_id_prefix) - 1);
    rename_history_filter
        .operation_id_prefix[sizeof(rename_history_filter.operation_id_prefix) -
                             1] = '\0';
  }
  char normalize_error[APP_MAX_ERROR] = {0};
  if (!CliRenameParseRollbackFilter(rename_history_rollback_filter,
                                    &rename_history_filter.rollback_filter,
                                    normalize_error, sizeof(normalize_error))) {
    printf("Error: %s\n", normalize_error);
    return 1;
  }
  if (!CliRenameNormalizeHistoryDateBound(
          rename_history_from, false, rename_history_filter.created_from_utc,
          sizeof(rename_history_filter.created_from_utc), normalize_error,
          sizeof(normalize_error))) {
    printf("Error: %s\n", normalize_error);
    return 1;
  }
  if (!CliRenameNormalizeHistoryDateBound(
          rename_history_to, true, rename_history_filter.created_to_utc,
          sizeof(rename_history_filter.created_to_utc), normalize_error,
          sizeof(normalize_error))) {
    printf("Error: %s\n", normalize_error);
    return 1;
  }
  if (rename_history_filter.created_from_utc[0] != '\0' &&
      rename_history_filter.created_to_utc[0] != '\0' &&
      strcmp(rename_history_filter.created_from_utc,
             rename_history_filter.created_to_utc) > 0) {
    printf("Error: --rename-history-from must be <= --rename-history-to.\n");
    return 1;
  }

  AppRuntimeOptions options = {0};
  AppContext *app = AppContextCreate(&options);
  if (!app) {
    printf("Error: Failed to initialize application context.\n");
    return 1;
  }

  if (rename_init) {
    int rc = HandleRenameInit(target_dir, env_dir, argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_bootstrap_tags_from_filename) {
    int rc = HandleRenameBootstrap(target_dir, env_dir, argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_preview) {
    int rc = HandleRenamePreview(app, target_dir, env_dir, rename_pattern,
                                 rename_tags_map_path, rename_tag_add_csv,
                                 rename_tag_remove_csv, rename_meta_tag_add_csv,
                                 rename_meta_tag_remove_csv,
                                 rename_metadata_fields, rename_preview_json,
                                 rename_preview_json_out, argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_apply_action) {
    const char *resolved_preview_id = rename_from_preview;
    char latest_preview_id[64] = {0};
    char operation_preview_id[64] = {0};
    if (rename_apply_latest) {
      const char *apply_env = CliResolveRollbackEnvDir(target_dir, env_dir);
      if (!apply_env) {
        printf("Error: --rename-apply-latest requires an environment directory.\n");
        AppContextDestroy(app);
        return 1;
      }
      char resolve_error[APP_MAX_ERROR] = {0};
      if (!CliRenameResolveLatestPreviewId(apply_env, latest_preview_id,
                                           sizeof(latest_preview_id),
                                           resolve_error,
                                           sizeof(resolve_error))) {
        printf("Error: --rename-apply-latest failed: %s\n", resolve_error);
        AppContextDestroy(app);
        return 1;
      }
      printf("Resolved latest preview id: %s\n", latest_preview_id);
      resolved_preview_id = latest_preview_id;
    }
    if (rename_redo) {
      const char *apply_env = CliResolveRollbackEnvDir(target_dir, env_dir);
      if (!apply_env) {
        printf("Error: --rename-redo requires an environment directory.\n");
        AppContextDestroy(app);
        return 1;
      }
      char resolve_error[APP_MAX_ERROR] = {0};
      if (!CliRenameResolvePreviewIdFromOperation(
              apply_env, rename_redo_operation, operation_preview_id,
              sizeof(operation_preview_id), resolve_error,
              sizeof(resolve_error))) {
        printf("Error: --rename-redo failed: %s\n", resolve_error);
        AppContextDestroy(app);
        return 1;
      }
      printf("Resolved preview id from operation %s: %s\n",
             rename_redo_operation, operation_preview_id);
      resolved_preview_id = operation_preview_id;
    }

    int rc = HandleRenameApply(app, target_dir, env_dir, resolved_preview_id,
                               rename_accept_auto_suffix, argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_history) {
    int rc =
        HandleRenameHistory(app, target_dir, env_dir, &rename_history_filter,
                            argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_history_export) {
    int rc = HandleRenameHistoryExport(app, target_dir, env_dir,
                                       &rename_history_filter,
                                       rename_history_export_path, argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_history_prune) {
    int rc = HandleRenameHistoryPrune(app, target_dir, env_dir,
                                      rename_history_prune_keep_count, argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_history_latest_id) {
    int rc = HandleRenameHistoryLatestId(target_dir, env_dir, argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_history_detail) {
    int rc = HandleRenameHistoryDetail(app, target_dir, env_dir,
                                       rename_history_detail_operation,
                                       argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_preview_latest_id) {
    int rc = HandleRenamePreviewLatestId(target_dir, env_dir, argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_rollback) {
    int rc = HandleRenameRollback(app, target_dir, env_dir,
                                  rename_rollback_operation, argv[0]);
    AppContextDestroy(app);
    return rc;
  }
  if (rename_rollback_preflight) {
    int rc = HandleRenameRollbackPreflight(app, target_dir, env_dir,
                                           rename_rollback_preflight_operation,
                                           argv[0]);
    AppContextDestroy(app);
    return rc;
  }

  if (rollback) {
    int rc = HandleRollback(app, target_dir, env_dir, argv[0]);
    AppContextDestroy(app);
    return rc;
  }

  if (target_dir == NULL) {
    CliPrintUsage(argv[0]);
    AppContextDestroy(app);
    return 1;
  }

  if (!jobs_set_by_cli) {
    const char *jobs_env = getenv("CGO_JOBS");
    if (jobs_env && jobs_env[0] != '\0') {
      jobs_arg = jobs_env;
    }
  }

  bool jobs_valid = false;
  int resolved_jobs = CliResolveJobsFromString(jobs_arg, &jobs_valid);
  if (!jobs_valid) {
    printf("Error: --jobs/CGO_JOBS must be 'auto' or a positive integer.\n");
    AppContextDestroy(app);
    return 1;
  }

  if (similarity_report && !env_dir) {
    printf("Error: --similarity-report requires an environment directory.\n");
    AppContextDestroy(app);
    return 1;
  }
  if (duplicates_move && !env_dir) {
    printf("Error: --duplicates-move requires an environment directory.\n");
    AppContextDestroy(app);
    return 1;
  }

  printf("Scanning directory: %s (Exhaustive: %s)\n", target_dir,
         exhaustive ? "ON" : "OFF");
  printf("Jobs: %d\n", resolved_jobs);

  AppScanRequest scan_request = {
      .target_dir = target_dir,
      .env_dir = env_dir,
      .exhaustive = exhaustive,
      .ml_enrich = ml_enrich,
      .similarity_report = similarity_report,
      .sim_incremental = sim_incremental,
      .jobs = resolved_jobs,
      .cache_compression_mode = cache_compression_mode,
      .cache_compression_level = cache_compression_level,
      .models_root_override = NULL,
      .hooks = {0},
  };

  AppScanResult scan_result = {0};
  AppStatus status = AppRunScan(app, &scan_request, &scan_result);
  if (status != APP_STATUS_OK) {
    if (status == APP_STATUS_ML_ERROR) {
      PrintAppError(app, "Error: Failed to initialize ML runtime");
      printf("Hint: run scripts/download_models.sh first or set CGO_MODELS_ROOT.\n");
    } else {
      PrintAppError(app, "Error: Failed to run scan");
    }
    AppContextDestroy(app);
    return 1;
  }

  printf("Scan complete.\n");
  printf("Files scanned: %d\n", scan_result.files_scanned);
  printf("Media files cached: %d\n", scan_result.files_cached);
  if (scan_result.cache_profile_matched) {
    printf("Cache profile: matched\n");
  } else if (scan_result.cache_profile_rebuilt) {
    printf("Cache profile: rebuilt (%s)\n",
           scan_result.cache_profile_reason[0] != '\0'
               ? scan_result.cache_profile_reason
               : "mismatch");
  }
  if (ml_enrich || similarity_report) {
    printf("ML evaluated: %d\n", scan_result.ml_files_evaluated);
    if (ml_enrich) {
      printf("ML classified: %d\n", scan_result.ml_files_classified);
      printf("ML with text: %d\n", scan_result.ml_files_with_text);
    }
    if (similarity_report) {
      printf("ML embedded: %d\n", scan_result.ml_files_embedded);
    }
    printf("ML failures/skips: %d\n", scan_result.ml_failures);
  }
  printf("\n");

  if (similarity_report) {
    AppSimilarityRequest request = {
        .env_dir = env_dir,
        .cache_compression_mode = cache_compression_mode,
        .cache_compression_level = cache_compression_level,
        .threshold = sim_threshold,
        .topk = sim_topk,
        .memory_mode = sim_memory_mode,
        .output_path_override = NULL,
        .hooks = {0},
    };
    AppSimilarityResult result = {0};
    status = AppGenerateSimilarityReport(app, &request, &result);
    if (status != APP_STATUS_OK) {
      PrintAppError(app, "Error: Failed to generate similarity report");
      AppContextDestroy(app);
      return 1;
    }
    printf("Similarity report generated: %s\n", result.report_path);
  }

  if (preview || organize) {
    if (!env_dir) {
      printf("Error: --organize and --preview require an environment directory.\n");
      AppContextDestroy(app);
      return 1;
    }

    AppOrganizePlanRequest preview_request = {
        .env_dir = env_dir,
        .cache_compression_mode = cache_compression_mode,
        .cache_compression_level = cache_compression_level,
        .group_by_arg = group_by_arg,
        .hooks = {0},
    };

    AppOrganizePlanResult preview_result = {0};
    status = AppPreviewOrganize(app, &preview_request, &preview_result);
    if (status != APP_STATUS_OK) {
      PrintAppError(app, "Error");
      AppContextDestroy(app);
      return 1;
    }

    printf("\n[*] Calculating Gallery Reorganization Plan...\n");
    if (preview_result.plan_text) {
      printf("%s", preview_result.plan_text);
    }

    if (preview) {
      printf("\n[*] Preview mode: No files were moved.\n");
      AppFreeOrganizePlanResult(&preview_result);
      AppContextDestroy(app);
      return 0;
    }

    printf("\nExecute plan? [y/N]: ");
    fflush(stdout);
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin)) {
      if (buf[0] == 'y' || buf[0] == 'Y') {
        AppOrganizeExecuteRequest execute_request = {
            .env_dir = env_dir,
            .cache_compression_mode = cache_compression_mode,
            .cache_compression_level = cache_compression_level,
            .group_by_arg = group_by_arg,
            .hooks = {0},
        };
        AppOrganizeExecuteResult execute_result = {0};
        status = AppExecuteOrganize(app, &execute_request, &execute_result);
        if (status != APP_STATUS_OK) {
          PrintAppError(app, "Error: Failed to execute organize plan");
          AppFreeOrganizePlanResult(&preview_result);
          AppContextDestroy(app);
          return 1;
        }
      } else {
        printf("[*] Operation cancelled.\n");
      }
    }

    AppFreeOrganizePlanResult(&preview_result);
  } else if (duplicates_report || duplicates_move) {
    int rc = HandleDuplicates(app, env_dir, cache_compression_mode,
                              cache_compression_level, duplicates_move);
    AppContextDestroy(app);
    return rc;
  }

  AppContextDestroy(app);
  return 0;
}
