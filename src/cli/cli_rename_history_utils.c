#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "cli/cli_rename_common.h"
#include "cli/cli_rename_utils.h"

#if !defined(NAME_MAX)
#define NAME_MAX 255
#endif

bool CliRenameResolveLatestPreviewId(const char *env_dir, char *out_preview_id,
                                     size_t out_preview_id_size,
                                     char *out_error,
                                     size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_preview_id ||
      out_preview_id_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "environment directory and output buffer are required");
    return false;
  }
  out_preview_id[0] = '\0';

  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  char preview_dir[MAX_PATH_LENGTH] = {0};
  snprintf(preview_dir, sizeof(preview_dir), "%s/.cache/rename_previews",
           env_abs);
  DIR *dir = opendir(preview_dir);
  if (!dir) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename preview directory not found: '%s'", preview_dir);
    return false;
  }

  char latest_name[NAME_MAX + 1] = {0};
  struct dirent *entry = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (!CliRenameCommonEndsWith(entry->d_name, ".json")) {
      continue;
    }
    if (strncmp(entry->d_name, "rnp-", 4) != 0) {
      continue;
    }
    if (latest_name[0] == '\0' || strcmp(entry->d_name, latest_name) > 0) {
      strncpy(latest_name, entry->d_name, sizeof(latest_name) - 1);
      latest_name[sizeof(latest_name) - 1] = '\0';
    }
  }
  closedir(dir);

  if (latest_name[0] == '\0') {
    CliRenameCommonSetError(out_error, out_error_size,
             "no rename preview artifacts found under '%s'", preview_dir);
    return false;
  }

  size_t len = strlen(latest_name);
  if (len <= 5) {
    CliRenameCommonSetError(out_error, out_error_size, "invalid rename preview artifact name");
    return false;
  }
  latest_name[len - 5] = '\0';
  strncpy(out_preview_id, latest_name, out_preview_id_size - 1);
  out_preview_id[out_preview_id_size - 1] = '\0';
  return true;
}

static bool LoadRenameHistoryEntries(const char *env_dir, cJSON **out_entries,
                                     char *out_error, size_t out_error_size) {
  if (!env_dir || env_dir[0] == '\0' || !out_entries) {
    CliRenameCommonSetError(out_error, out_error_size,
             "environment directory and output payload are required");
    return false;
  }

  *out_entries = NULL;
  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  char index_path[MAX_PATH_LENGTH] = {0};
  snprintf(index_path, sizeof(index_path), "%s/.cache/rename_history/index.json",
           env_abs);

  char *index_text = NULL;
  if (!CliRenameCommonLoadTextFile(index_path, &index_text)) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename history index not found: '%s'", index_path);
    return false;
  }

  cJSON *index_json = cJSON_Parse(index_text);
  free(index_text);
  if (!index_json) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename history index is malformed: '%s'", index_path);
    return false;
  }

  cJSON *entries = cJSON_DetachItemFromObject(index_json, "entries");
  cJSON_Delete(index_json);
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(entries);
    CliRenameCommonSetError(out_error, out_error_size,
             "rename history index entries payload is invalid");
    return false;
  }

  *out_entries = entries;
  return true;
}

bool CliRenameResolveLatestOperationId(const char *env_dir, char *out_operation_id,
                                       size_t out_operation_id_size,
                                       char *out_error,
                                       size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_operation_id ||
      out_operation_id_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "environment directory and output buffer are required");
    return false;
  }

  out_operation_id[0] = '\0';
  cJSON *entries = NULL;
  if (!LoadRenameHistoryEntries(env_dir, &entries, out_error, out_error_size)) {
    return false;
  }

  bool found = false;
  int count = cJSON_GetArraySize(entries);
  for (int i = count - 1; i >= 0; i--) {
    cJSON *entry = cJSON_GetArrayItem(entries, i);
    cJSON *operation_id = cJSON_GetObjectItem(entry, "operationId");
    if (!cJSON_IsString(operation_id) || !operation_id->valuestring ||
        operation_id->valuestring[0] == '\0') {
      continue;
    }
    strncpy(out_operation_id, operation_id->valuestring,
            out_operation_id_size - 1);
    out_operation_id[out_operation_id_size - 1] = '\0';
    found = true;
    break;
  }

  cJSON_Delete(entries);
  if (!found) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename history contains no operation ids");
    return false;
  }
  return true;
}

bool CliRenameResolvePreviewIdFromOperation(const char *env_dir,
                                            const char *operation_id,
                                            char *out_preview_id,
                                            size_t out_preview_id_size,
                                            char *out_error,
                                            size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !operation_id ||
      operation_id[0] == '\0' || !out_preview_id ||
      out_preview_id_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "env directory, operation id, and output buffer are required");
    return false;
  }
  out_preview_id[0] = '\0';

  cJSON *entries = NULL;
  if (!LoadRenameHistoryEntries(env_dir, &entries, out_error, out_error_size)) {
    return false;
  }

  bool found = false;
  int count = cJSON_GetArraySize(entries);
  for (int i = count - 1; i >= 0; i--) {
    cJSON *entry = cJSON_GetArrayItem(entries, i);
    cJSON *entry_operation_id = cJSON_GetObjectItem(entry, "operationId");
    if (!cJSON_IsString(entry_operation_id) ||
        !entry_operation_id->valuestring ||
        strcmp(entry_operation_id->valuestring, operation_id) != 0) {
      continue;
    }

    cJSON *entry_preview_id = cJSON_GetObjectItem(entry, "previewId");
    if (!cJSON_IsString(entry_preview_id) || !entry_preview_id->valuestring ||
        entry_preview_id->valuestring[0] == '\0') {
      cJSON_Delete(entries);
      CliRenameCommonSetError(out_error, out_error_size,
               "operation '%s' does not reference a preview id", operation_id);
      return false;
    }

    strncpy(out_preview_id, entry_preview_id->valuestring,
            out_preview_id_size - 1);
    out_preview_id[out_preview_id_size - 1] = '\0';
    found = true;
    break;
  }

  cJSON_Delete(entries);
  if (!found) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename operation '%s' not found in history", operation_id);
    return false;
  }
  return true;
}

bool CliRenameResolveOperationManifestPath(const char *env_dir,
                                           const char *operation_id,
                                           char *out_manifest_path,
                                           size_t out_manifest_path_size,
                                           char *out_error,
                                           size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !operation_id ||
      operation_id[0] == '\0' || !out_manifest_path ||
      out_manifest_path_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "env directory, operation id, and output buffer are required");
    return false;
  }

  out_manifest_path[0] = '\0';
  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  char manifest_path[MAX_PATH_LENGTH] = {0};
  snprintf(manifest_path, sizeof(manifest_path),
           "%s/.cache/rename_history/%s.json", env_abs, operation_id);
  if (access(manifest_path, F_OK) != 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename operation manifest not found: '%s'", manifest_path);
    return false;
  }

  strncpy(out_manifest_path, manifest_path, out_manifest_path_size - 1);
  out_manifest_path[out_manifest_path_size - 1] = '\0';
  return true;
}

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

static bool IsDigits(const char *text, size_t len) {
  if (!text) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    if (!isdigit((unsigned char)text[i])) {
      return false;
    }
  }
  return true;
}

bool CliRenameParseRollbackFilter(const char *raw_value,
                                  CliRenameRollbackFilter *out_filter,
                                  char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!out_filter) {
    CliRenameCommonSetError(out_error, out_error_size, "output rollback filter is required");
    return false;
  }

  if (!raw_value || raw_value[0] == '\0' || strcmp(raw_value, "any") == 0) {
    *out_filter = CLI_RENAME_ROLLBACK_FILTER_ANY;
    return true;
  }
  if (strcmp(raw_value, "yes") == 0 || strcmp(raw_value, "true") == 0) {
    *out_filter = CLI_RENAME_ROLLBACK_FILTER_YES;
    return true;
  }
  if (strcmp(raw_value, "no") == 0 || strcmp(raw_value, "false") == 0) {
    *out_filter = CLI_RENAME_ROLLBACK_FILTER_NO;
    return true;
  }

  CliRenameCommonSetError(out_error, out_error_size,
           "invalid rollback filter '%s' (allowed: any|yes|no)", raw_value);
  return false;
}

bool CliRenameNormalizeHistoryDateBound(const char *raw_value, bool is_upper_bound,
                                        char *out_utc, size_t out_utc_size,
                                        char *out_error,
                                        size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!out_utc || out_utc_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size, "output date buffer is required");
    return false;
  }

  out_utc[0] = '\0';
  if (!raw_value || raw_value[0] == '\0') {
    return true;
  }

  size_t len = strlen(raw_value);
  if (len == 10) {
    if (!IsDigits(raw_value, 4) || raw_value[4] != '-' ||
        !IsDigits(raw_value + 5, 2) || raw_value[7] != '-' ||
        !IsDigits(raw_value + 8, 2)) {
      CliRenameCommonSetError(out_error, out_error_size,
               "invalid date '%s' (expected YYYY-MM-DD)", raw_value);
      return false;
    }
    snprintf(out_utc, out_utc_size, "%sT%sZ", raw_value,
             is_upper_bound ? "23:59:59" : "00:00:00");
    return true;
  }

  if (len == 20 && IsDigits(raw_value, 4) && raw_value[4] == '-' &&
      IsDigits(raw_value + 5, 2) && raw_value[7] == '-' &&
      IsDigits(raw_value + 8, 2) && raw_value[10] == 'T' &&
      IsDigits(raw_value + 11, 2) && raw_value[13] == ':' &&
      IsDigits(raw_value + 14, 2) && raw_value[16] == ':' &&
      IsDigits(raw_value + 17, 2) && raw_value[19] == 'Z') {
    strncpy(out_utc, raw_value, out_utc_size - 1);
    out_utc[out_utc_size - 1] = '\0';
    return true;
  }

  CliRenameCommonSetError(out_error, out_error_size,
           "invalid date '%s' (expected YYYY-MM-DD or YYYY-MM-DDTHH:MM:SSZ)",
           raw_value);
  return false;
}

static bool HistoryEntryMatchesFilter(const AppRenameHistoryEntry *entry,
                                      const CliRenameHistoryFilter *filter) {
  if (!entry || !filter) {
    return false;
  }

  if (filter->operation_id_prefix[0] != '\0') {
    size_t prefix_len = strlen(filter->operation_id_prefix);
    if (strncmp(entry->operation_id, filter->operation_id_prefix, prefix_len) !=
        0) {
      return false;
    }
  }

  if (filter->rollback_filter == CLI_RENAME_ROLLBACK_FILTER_YES &&
      !entry->rollback_performed) {
    return false;
  }
  if (filter->rollback_filter == CLI_RENAME_ROLLBACK_FILTER_NO &&
      entry->rollback_performed) {
    return false;
  }

  if (filter->created_from_utc[0] != '\0') {
    if (entry->created_at_utc[0] == '\0' ||
        strcmp(entry->created_at_utc, filter->created_from_utc) < 0) {
      return false;
    }
  }
  if (filter->created_to_utc[0] != '\0') {
    if (entry->created_at_utc[0] == '\0' ||
        strcmp(entry->created_at_utc, filter->created_to_utc) > 0) {
      return false;
    }
  }

  return true;
}

bool CliRenameBuildHistoryFilterIndex(const AppRenameHistoryEntry *entries,
                                      int entry_count,
                                      const CliRenameHistoryFilter *filter,
                                      int **out_indices,
                                      int *out_filtered_count,
                                      char *out_error,
                                      size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!entries || entry_count < 0 || !filter || !out_indices ||
      !out_filtered_count) {
    CliRenameCommonSetError(out_error, out_error_size,
             "entries, filter, and output pointers are required");
    return false;
  }

  *out_indices = NULL;
  *out_filtered_count = 0;
  if (entry_count == 0) {
    return true;
  }

  int *indices = calloc((size_t)entry_count, sizeof(int));
  if (!indices) {
    CliRenameCommonSetError(out_error, out_error_size,
             "out of memory while building filtered history index");
    return false;
  }

  int written = 0;
  for (int i = 0; i < entry_count; i++) {
    if (HistoryEntryMatchesFilter(&entries[i], filter)) {
      indices[written++] = i;
    }
  }

  if (written == 0) {
    free(indices);
    indices = NULL;
  }

  *out_indices = indices;
  *out_filtered_count = written;
  return true;
}

void CliRenameFreeHistoryFilterIndex(int *indices) { free(indices); }

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
