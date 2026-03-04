#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "systems/renamer_history_internal.h"

void RenamerHistorySetError(char *out_error, size_t out_error_size,
                            const char *fmt, ...) {
  if (!out_error || out_error_size == 0 || !fmt) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(out_error, out_error_size, fmt, args);
  va_end(args);
}

void RenamerHistoryNowUtc(char *out_text, size_t out_text_size) {
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

bool RenamerHistoryLoadFileText(const char *path, char **out_text) {
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

  size_t bytes_read = fread(text, 1, (size_t)size, f);
  fclose(f);
  text[bytes_read] = '\0';

  *out_text = text;
  return true;
}

bool RenamerHistorySaveFileText(const char *path, const char *text) {
  if (!path || !text) {
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

bool RenamerHistoryEnsureHistoryPaths(const char *env_dir, char *out_history_dir,
                                      size_t out_history_dir_size,
                                      char *out_index_path,
                                      size_t out_index_path_size) {
  if (!env_dir || env_dir[0] == '\0') {
    return false;
  }

  char cache_dir[MAX_PATH_LENGTH] = {0};
  snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", env_dir);
  if (!FsMakeDirRecursive(cache_dir)) {
    return false;
  }

  char history_dir[MAX_PATH_LENGTH] = {0};
  snprintf(history_dir, sizeof(history_dir), "%s/rename_history", cache_dir);
  if (!FsMakeDirRecursive(history_dir)) {
    return false;
  }

  if (out_history_dir && out_history_dir_size > 0) {
    strncpy(out_history_dir, history_dir, out_history_dir_size - 1);
    out_history_dir[out_history_dir_size - 1] = '\0';
  }

  if (out_index_path && out_index_path_size > 0) {
    snprintf(out_index_path, out_index_path_size, "%s/index.json", history_dir);
  }

  return true;
}

cJSON *RenamerHistoryCreateDefaultIndex(void) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return NULL;
  }
  cJSON_AddNumberToObject(root, "version", 1);
  cJSON_AddItemToObject(root, "entries", cJSON_CreateArray());
  return root;
}

bool RenamerHistoryLoadIndex(const char *index_path, cJSON **out_root,
                             char *out_error, size_t out_error_size) {
  if (!index_path || !out_root) {
    RenamerHistorySetError(out_error, out_error_size,
                           "invalid rename history index path");
    return false;
  }

  *out_root = NULL;
  char *text = NULL;
  if (!RenamerHistoryLoadFileText(index_path, &text)) {
    *out_root = RenamerHistoryCreateDefaultIndex();
    if (!*out_root) {
      RenamerHistorySetError(out_error, out_error_size,
                             "failed to initialize rename history index");
      return false;
    }
    return true;
  }

  cJSON *root = cJSON_Parse(text);
  free(text);
  if (!root) {
    *out_root = RenamerHistoryCreateDefaultIndex();
    if (!*out_root) {
      RenamerHistorySetError(out_error, out_error_size,
                             "failed to recover malformed rename history index");
      return false;
    }
    return true;
  }

  cJSON *entries = cJSON_GetObjectItem(root, "entries");
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(root);
    *out_root = RenamerHistoryCreateDefaultIndex();
    if (!*out_root) {
      RenamerHistorySetError(out_error, out_error_size,
                             "failed to rebuild malformed rename history index");
      return false;
    }
    return true;
  }

  *out_root = root;
  return true;
}

bool RenamerHistorySaveIndex(const char *index_path, const cJSON *index_root,
                             char *out_error, size_t out_error_size) {
  char *text = cJSON_Print(index_root);
  if (!text) {
    RenamerHistorySetError(out_error, out_error_size,
                           "failed to serialize rename history index");
    return false;
  }

  bool ok = RenamerHistorySaveFileText(index_path, text);
  cJSON_free(text);
  if (!ok) {
    RenamerHistorySetError(out_error, out_error_size,
                           "failed to write rename history index '%s'",
                           index_path);
    return false;
  }

  return true;
}

cJSON *RenamerHistoryBuildHistoryEntryJson(const RenamerHistoryEntry *entry) {
  if (!entry) {
    return NULL;
  }

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    return NULL;
  }

  cJSON_AddStringToObject(json, "operationId", entry->operation_id);
  cJSON_AddStringToObject(json, "previewId", entry->preview_id);
  cJSON_AddStringToObject(json, "createdAtUtc", entry->created_at_utc);
  cJSON_AddNumberToObject(json, "renamedCount", entry->renamed_count);
  cJSON_AddNumberToObject(json, "skippedCount", entry->skipped_count);
  cJSON_AddNumberToObject(json, "failedCount", entry->failed_count);
  cJSON_AddNumberToObject(json, "collisionResolvedCount",
                          entry->collision_resolved_count);
  cJSON_AddNumberToObject(json, "truncationCount", entry->truncation_count);
  cJSON_AddBoolToObject(json, "rollbackPerformed", entry->rollback_performed);
  cJSON_AddNumberToObject(json, "rollbackRestoredCount",
                          entry->rollback_restored_count);
  cJSON_AddNumberToObject(json, "rollbackSkippedCount",
                          entry->rollback_skipped_count);
  cJSON_AddNumberToObject(json, "rollbackFailedCount",
                          entry->rollback_failed_count);

  return json;
}

bool RenamerHistoryParseHistoryEntry(const cJSON *json,
                                     RenamerHistoryEntry *out_entry) {
  if (!json || !out_entry || !cJSON_IsObject((cJSON *)json)) {
    return false;
  }

  memset(out_entry, 0, sizeof(*out_entry));

  cJSON *operation_id = cJSON_GetObjectItem((cJSON *)json, "operationId");
  cJSON *preview_id = cJSON_GetObjectItem((cJSON *)json, "previewId");
  cJSON *created_at = cJSON_GetObjectItem((cJSON *)json, "createdAtUtc");
  cJSON *renamed = cJSON_GetObjectItem((cJSON *)json, "renamedCount");
  cJSON *skipped = cJSON_GetObjectItem((cJSON *)json, "skippedCount");
  cJSON *failed = cJSON_GetObjectItem((cJSON *)json, "failedCount");
  cJSON *collision_resolved =
      cJSON_GetObjectItem((cJSON *)json, "collisionResolvedCount");
  cJSON *truncation = cJSON_GetObjectItem((cJSON *)json, "truncationCount");
  cJSON *rollback_performed =
      cJSON_GetObjectItem((cJSON *)json, "rollbackPerformed");
  cJSON *rollback_restored =
      cJSON_GetObjectItem((cJSON *)json, "rollbackRestoredCount");
  cJSON *rollback_skipped =
      cJSON_GetObjectItem((cJSON *)json, "rollbackSkippedCount");
  cJSON *rollback_failed =
      cJSON_GetObjectItem((cJSON *)json, "rollbackFailedCount");

  if (!cJSON_IsString(operation_id) || !operation_id->valuestring) {
    return false;
  }

  strncpy(out_entry->operation_id, operation_id->valuestring,
          sizeof(out_entry->operation_id) - 1);
  out_entry->operation_id[sizeof(out_entry->operation_id) - 1] = '\0';

  if (cJSON_IsString(preview_id) && preview_id->valuestring) {
    strncpy(out_entry->preview_id, preview_id->valuestring,
            sizeof(out_entry->preview_id) - 1);
    out_entry->preview_id[sizeof(out_entry->preview_id) - 1] = '\0';
  }
  if (cJSON_IsString(created_at) && created_at->valuestring) {
    strncpy(out_entry->created_at_utc, created_at->valuestring,
            sizeof(out_entry->created_at_utc) - 1);
    out_entry->created_at_utc[sizeof(out_entry->created_at_utc) - 1] = '\0';
  }

  out_entry->renamed_count = cJSON_IsNumber(renamed) ? (int)renamed->valuedouble : 0;
  out_entry->skipped_count = cJSON_IsNumber(skipped) ? (int)skipped->valuedouble : 0;
  out_entry->failed_count = cJSON_IsNumber(failed) ? (int)failed->valuedouble : 0;
  out_entry->collision_resolved_count =
      cJSON_IsNumber(collision_resolved) ? (int)collision_resolved->valuedouble : 0;
  out_entry->truncation_count =
      cJSON_IsNumber(truncation) ? (int)truncation->valuedouble : 0;
  out_entry->rollback_performed =
      cJSON_IsBool(rollback_performed) && cJSON_IsTrue(rollback_performed);
  out_entry->rollback_restored_count =
      cJSON_IsNumber(rollback_restored) ? (int)rollback_restored->valuedouble : 0;
  out_entry->rollback_skipped_count =
      cJSON_IsNumber(rollback_skipped) ? (int)rollback_skipped->valuedouble : 0;
  out_entry->rollback_failed_count =
      cJSON_IsNumber(rollback_failed) ? (int)rollback_failed->valuedouble : 0;

  return true;
}

bool RenamerHistoryRemoveOldestHistoryEntry(cJSON *entries,
                                            const char *history_dir) {
  if (!cJSON_IsArray(entries) || !history_dir) {
    return false;
  }

  cJSON *oldest = cJSON_GetArrayItem(entries, 0);
  if (!cJSON_IsObject(oldest)) {
    cJSON_DeleteItemFromArray(entries, 0);
    return true;
  }

  cJSON *operation_id = cJSON_GetObjectItem(oldest, "operationId");
  if (cJSON_IsString(operation_id) && operation_id->valuestring) {
    char path[MAX_PATH_LENGTH] = {0};
    snprintf(path, sizeof(path), "%s/%s.json", history_dir,
             operation_id->valuestring);
    remove(path);
  }

  cJSON_DeleteItemFromArray(entries, 0);
  return true;
}
