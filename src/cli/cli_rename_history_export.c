#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "cli/cli_rename_common.h"
#include "cli/cli_rename_utils.h"

static void NowUtc(char *out_text, size_t out_text_size) {
  if (!out_text || out_text_size == 0) {
    return;
  }

  out_text[0] = '\0';
  time_t now = time(NULL);
  struct tm tm_utc;
#if defined(_WIN32)
  gmtime_s(&tm_utc, &now);
#else
  gmtime_r(&now, &tm_utc);
#endif
  strftime(out_text, out_text_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static const char *RollbackFilterToString(CliRenameRollbackFilter filter) {
  switch (filter) {
  case CLI_RENAME_ROLLBACK_FILTER_YES:
    return "yes";
  case CLI_RENAME_ROLLBACK_FILTER_NO:
    return "no";
  case CLI_RENAME_ROLLBACK_FILTER_ANY:
  default:
    return "any";
  }
}

bool CliRenameExportHistoryJson(const char *env_dir,
                                const AppRenameHistoryEntry *entries,
                                const int *indices, int index_count,
                                const CliRenameHistoryFilter *filter,
                                const char *output_path, char *out_error,
                                size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !entries || !filter || !output_path ||
      output_path[0] == '\0' || index_count < 0 ||
      (index_count > 0 && !indices)) {
    CliRenameCommonSetError(out_error, out_error_size,
             "env_dir, entries, filter, output_path, and valid index set are "
             "required");
    return false;
  }

  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON *items = cJSON_CreateArray();
  cJSON *filters = cJSON_CreateObject();
  if (!root || !items || !filters) {
    cJSON_Delete(root);
    cJSON_Delete(items);
    cJSON_Delete(filters);
    CliRenameCommonSetError(out_error, out_error_size,
             "out of memory while building history export payload");
    return false;
  }

  cJSON_AddNumberToObject(root, "version", 1);
  char created_at[32] = {0};
  NowUtc(created_at, sizeof(created_at));
  cJSON_AddStringToObject(root, "createdAtUtc", created_at);
  cJSON_AddStringToObject(root, "envDir", env_abs);
  cJSON_AddNumberToObject(root, "entryCount", index_count);

  cJSON_AddStringToObject(filters, "operationIdPrefix",
                          filter->operation_id_prefix);
  cJSON_AddStringToObject(filters, "rollback",
                          RollbackFilterToString(filter->rollback_filter));
  cJSON_AddStringToObject(filters, "createdFromUtc", filter->created_from_utc);
  cJSON_AddStringToObject(filters, "createdToUtc", filter->created_to_utc);
  cJSON_AddItemToObject(root, "filters", filters);

  for (int i = 0; i < index_count; i++) {
    int idx = indices[i];
    const AppRenameHistoryEntry *entry = &entries[idx];

    cJSON *item = cJSON_CreateObject();
    if (!item) {
      continue;
    }

    cJSON_AddStringToObject(item, "operationId", entry->operation_id);
    cJSON_AddStringToObject(item, "previewId", entry->preview_id);
    cJSON_AddStringToObject(item, "createdAtUtc", entry->created_at_utc);
    cJSON_AddNumberToObject(item, "renamedCount", entry->renamed_count);
    cJSON_AddNumberToObject(item, "skippedCount", entry->skipped_count);
    cJSON_AddNumberToObject(item, "failedCount", entry->failed_count);
    cJSON_AddNumberToObject(item, "collisionResolvedCount",
                            entry->collision_resolved_count);
    cJSON_AddNumberToObject(item, "truncationCount", entry->truncation_count);
    cJSON_AddBoolToObject(item, "rollbackPerformed", entry->rollback_performed);
    cJSON_AddNumberToObject(item, "rollbackRestoredCount",
                            entry->rollback_restored_count);
    cJSON_AddNumberToObject(item, "rollbackSkippedCount",
                            entry->rollback_skipped_count);
    cJSON_AddNumberToObject(item, "rollbackFailedCount",
                            entry->rollback_failed_count);

    char manifest_path[MAX_PATH_LENGTH] = {0};
    bool manifest_available =
        CliRenameResolveOperationManifestPath(env_abs, entry->operation_id,
                                             manifest_path,
                                             sizeof(manifest_path), NULL, 0);
    cJSON_AddBoolToObject(item, "manifestAvailable", manifest_available);
    if (manifest_available) {
      cJSON_AddStringToObject(item, "manifestPath", manifest_path);
    }

    int manifest_item_count = 0;
    bool manifest_rollback_present = false;
    if (manifest_available) {
      char *manifest_text = NULL;
      if (CliRenameCommonLoadTextFile(manifest_path, &manifest_text)) {
        cJSON *manifest = cJSON_Parse(manifest_text);
        free(manifest_text);
        if (manifest) {
          cJSON *manifest_items = cJSON_GetObjectItem(manifest, "items");
          if (cJSON_IsArray(manifest_items)) {
            manifest_item_count = cJSON_GetArraySize(manifest_items);
          }
          cJSON *manifest_rollback = cJSON_GetObjectItem(manifest, "rollback");
          manifest_rollback_present = cJSON_IsObject(manifest_rollback);
          cJSON_Delete(manifest);
        }
      }
    }
    cJSON_AddNumberToObject(item, "manifestItemCount", manifest_item_count);
    cJSON_AddBoolToObject(item, "manifestRollbackPresent",
                          manifest_rollback_present);

    cJSON_AddItemToArray(items, item);
  }

  cJSON_AddItemToObject(root, "entries", items);

  char *payload = cJSON_Print(root);
  cJSON_Delete(root);
  if (!payload) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to serialize history export payload");
    return false;
  }

  bool saved = CliRenameCommonSaveTextFile(output_path, payload);
  cJSON_free(payload);
  if (!saved) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to write history export '%s'", output_path);
    return false;
  }
  return true;
}
