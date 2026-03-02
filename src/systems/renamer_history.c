#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "systems/renamer_history.h"
#include "systems/renamer_tags.h"

#define RENAMER_HISTORY_MAX_ENTRIES 200

static void SetError(char *out_error, size_t out_error_size, const char *fmt,
                     ...) {
  if (!out_error || out_error_size == 0 || !fmt) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(out_error, out_error_size, fmt, args);
  va_end(args);
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

static bool LoadFileText(const char *path, char **out_text) {
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

static bool SaveFileText(const char *path, const char *text) {
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

static bool EnsureHistoryPaths(const char *env_dir, char *out_history_dir,
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

static cJSON *CreateDefaultIndex(void) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return NULL;
  }
  cJSON_AddNumberToObject(root, "version", 1);
  cJSON_AddItemToObject(root, "entries", cJSON_CreateArray());
  return root;
}

static bool LoadIndex(const char *index_path, cJSON **out_root,
                      char *out_error, size_t out_error_size) {
  if (!index_path || !out_root) {
    SetError(out_error, out_error_size, "invalid rename history index path");
    return false;
  }

  *out_root = NULL;
  char *text = NULL;
  if (!LoadFileText(index_path, &text)) {
    *out_root = CreateDefaultIndex();
    if (!*out_root) {
      SetError(out_error, out_error_size,
               "failed to initialize rename history index");
      return false;
    }
    return true;
  }

  cJSON *root = cJSON_Parse(text);
  free(text);
  if (!root) {
    *out_root = CreateDefaultIndex();
    if (!*out_root) {
      SetError(out_error, out_error_size,
               "failed to recover malformed rename history index");
      return false;
    }
    return true;
  }

  cJSON *entries = cJSON_GetObjectItem(root, "entries");
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(root);
    *out_root = CreateDefaultIndex();
    if (!*out_root) {
      SetError(out_error, out_error_size,
               "failed to rebuild malformed rename history index");
      return false;
    }
    return true;
  }

  *out_root = root;
  return true;
}

static bool SaveIndex(const char *index_path, const cJSON *index_root,
                      char *out_error, size_t out_error_size) {
  char *text = cJSON_Print(index_root);
  if (!text) {
    SetError(out_error, out_error_size,
             "failed to serialize rename history index");
    return false;
  }

  bool ok = SaveFileText(index_path, text);
  cJSON_free(text);
  if (!ok) {
    SetError(out_error, out_error_size,
             "failed to write rename history index '%s'", index_path);
    return false;
  }

  return true;
}

static cJSON *BuildHistoryEntryJson(const RenamerHistoryEntry *entry) {
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

static bool ParseHistoryEntry(const cJSON *json, RenamerHistoryEntry *out_entry) {
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

static bool BuildOperationManifestJson(const RenamerHistoryEntry *entry,
                                       const RenamerOperationMove *moves,
                                       int move_count, cJSON **out_json) {
  if (!entry || !moves || move_count < 0 || !out_json) {
    return false;
  }

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    return false;
  }

  cJSON_AddNumberToObject(json, "version", 1);
  cJSON_AddStringToObject(json, "operationId", entry->operation_id);
  cJSON_AddStringToObject(json, "previewId", entry->preview_id);
  cJSON_AddStringToObject(json, "createdAtUtc", entry->created_at_utc);

  cJSON *counts = cJSON_CreateObject();
  cJSON_AddNumberToObject(counts, "renamed", entry->renamed_count);
  cJSON_AddNumberToObject(counts, "skipped", entry->skipped_count);
  cJSON_AddNumberToObject(counts, "failed", entry->failed_count);
  cJSON_AddNumberToObject(counts, "collisionResolved",
                          entry->collision_resolved_count);
  cJSON_AddNumberToObject(counts, "truncation", entry->truncation_count);
  cJSON_AddItemToObject(json, "counts", counts);

  cJSON *items = cJSON_CreateArray();
  if (!items) {
    cJSON_Delete(json);
    return false;
  }

  for (int i = 0; i < move_count; i++) {
    cJSON *item = cJSON_CreateObject();
    if (!item) {
      continue;
    }

    cJSON_AddStringToObject(item, "source", moves[i].source_path);
    cJSON_AddStringToObject(item, "destination", moves[i].destination_path);
    cJSON_AddBoolToObject(item, "renamed", moves[i].renamed);
    cJSON_AddItemToArray(items, item);
  }

  cJSON_AddItemToObject(json, "items", items);
  *out_json = json;
  return true;
}

static bool RemoveOldestHistoryEntry(cJSON *entries, const char *history_dir) {
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

bool RenamerHistoryPersistOperation(const char *env_dir,
                                    const RenamerHistoryEntry *entry,
                                    const RenamerOperationMove *moves,
                                    int move_count, char *out_error,
                                    size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !entry || !moves || move_count < 0 ||
      entry->operation_id[0] == '\0') {
    SetError(out_error, out_error_size,
             "invalid arguments for rename history persistence");
    return false;
  }

  char history_dir[MAX_PATH_LENGTH] = {0};
  char index_path[MAX_PATH_LENGTH] = {0};
  if (!EnsureHistoryPaths(env_dir, history_dir, sizeof(history_dir), index_path,
                          sizeof(index_path))) {
    SetError(out_error, out_error_size,
             "failed to ensure rename history path under '%s'", env_dir);
    return false;
  }

  char operation_path[MAX_PATH_LENGTH] = {0};
  snprintf(operation_path, sizeof(operation_path), "%s/%s.json", history_dir,
           entry->operation_id);

  cJSON *manifest = NULL;
  if (!BuildOperationManifestJson(entry, moves, move_count, &manifest)) {
    SetError(out_error, out_error_size,
             "failed to build rename operation manifest");
    return false;
  }

  char *manifest_text = cJSON_Print(manifest);
  cJSON_Delete(manifest);
  if (!manifest_text) {
    SetError(out_error, out_error_size,
             "failed to serialize rename operation manifest");
    return false;
  }

  bool manifest_ok = SaveFileText(operation_path, manifest_text);
  cJSON_free(manifest_text);
  if (!manifest_ok) {
    SetError(out_error, out_error_size,
             "failed to write rename operation manifest '%s'", operation_path);
    return false;
  }

  cJSON *index_root = NULL;
  if (!LoadIndex(index_path, &index_root, out_error, out_error_size)) {
    return false;
  }

  cJSON *entries = cJSON_GetObjectItem(index_root, "entries");
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(index_root);
    SetError(out_error, out_error_size,
             "rename history index has invalid entries payload");
    return false;
  }

  cJSON *entry_json = BuildHistoryEntryJson(entry);
  if (!entry_json) {
    cJSON_Delete(index_root);
    SetError(out_error, out_error_size,
             "failed to encode rename history entry");
    return false;
  }

  cJSON_AddItemToArray(entries, entry_json);
  while (cJSON_GetArraySize(entries) > RENAMER_HISTORY_MAX_ENTRIES) {
    RemoveOldestHistoryEntry(entries, history_dir);
  }

  bool ok = SaveIndex(index_path, index_root, out_error, out_error_size);
  cJSON_Delete(index_root);
  return ok;
}

bool RenamerHistoryList(const char *env_dir, RenamerHistoryEntry **out_entries,
                        int *out_count, char *out_error,
                        size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_entries || !out_count) {
    SetError(out_error, out_error_size,
             "invalid arguments for rename history listing");
    return false;
  }

  *out_entries = NULL;
  *out_count = 0;

  char history_dir[MAX_PATH_LENGTH] = {0};
  char index_path[MAX_PATH_LENGTH] = {0};
  if (!EnsureHistoryPaths(env_dir, history_dir, sizeof(history_dir), index_path,
                          sizeof(index_path))) {
    SetError(out_error, out_error_size,
             "failed to ensure rename history path under '%s'", env_dir);
    return false;
  }

  cJSON *index_root = NULL;
  if (!LoadIndex(index_path, &index_root, out_error, out_error_size)) {
    return false;
  }

  cJSON *entries = cJSON_GetObjectItem(index_root, "entries");
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(index_root);
    SetError(out_error, out_error_size,
             "rename history index entries payload is invalid");
    return false;
  }

  int count = cJSON_GetArraySize(entries);
  if (count <= 0) {
    cJSON_Delete(index_root);
    return true;
  }

  RenamerHistoryEntry *list = calloc((size_t)count, sizeof(RenamerHistoryEntry));
  if (!list) {
    cJSON_Delete(index_root);
    SetError(out_error, out_error_size,
             "out of memory while loading rename history entries");
    return false;
  }

  int written = 0;
  for (int i = count - 1; i >= 0; i--) {
    cJSON *node = cJSON_GetArrayItem(entries, i);
    if (!ParseHistoryEntry(node, &list[written])) {
      continue;
    }
    written++;
  }

  cJSON_Delete(index_root);
  *out_entries = list;
  *out_count = written;
  return true;
}

void RenamerHistoryFreeEntries(RenamerHistoryEntry *entries) { free(entries); }

static bool UpdateIndexRollbackStatus(const char *index_path,
                                      const char *operation_id,
                                      const RenamerRollbackStats *stats,
                                      char *out_error,
                                      size_t out_error_size) {
  cJSON *index_root = NULL;
  if (!LoadIndex(index_path, &index_root, out_error, out_error_size)) {
    return false;
  }

  cJSON *entries = cJSON_GetObjectItem(index_root, "entries");
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(index_root);
    SetError(out_error, out_error_size,
             "rename history index entries payload is invalid");
    return false;
  }

  cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, entries) {
    cJSON *id = cJSON_GetObjectItem(entry, "operationId");
    if (!cJSON_IsString(id) || !id->valuestring) {
      continue;
    }
    if (strcmp(id->valuestring, operation_id) != 0) {
      continue;
    }

    cJSON_ReplaceItemInObject(entry, "rollbackPerformed", cJSON_CreateBool(true));
    cJSON_ReplaceItemInObject(entry, "rollbackRestoredCount",
                              cJSON_CreateNumber(stats->restored_count));
    cJSON_ReplaceItemInObject(entry, "rollbackSkippedCount",
                              cJSON_CreateNumber(stats->skipped_count));
    cJSON_ReplaceItemInObject(entry, "rollbackFailedCount",
                              cJSON_CreateNumber(stats->failed_count));

    char rolled_back_at[32] = {0};
    NowUtc(rolled_back_at, sizeof(rolled_back_at));
    cJSON_ReplaceItemInObject(entry, "rolledBackAtUtc",
                              cJSON_CreateString(rolled_back_at));
    break;
  }

  bool ok = SaveIndex(index_path, index_root, out_error, out_error_size);
  cJSON_Delete(index_root);
  return ok;
}

static bool UpdateManifestRollbackStatus(const char *operation_path,
                                         const RenamerRollbackStats *stats) {
  char *text = NULL;
  if (!LoadFileText(operation_path, &text)) {
    return false;
  }

  cJSON *json = cJSON_Parse(text);
  free(text);
  if (!json) {
    return false;
  }

  cJSON *rollback = cJSON_CreateObject();
  if (!rollback) {
    cJSON_Delete(json);
    return false;
  }

  cJSON_AddNumberToObject(rollback, "restored", stats->restored_count);
  cJSON_AddNumberToObject(rollback, "skipped", stats->skipped_count);
  cJSON_AddNumberToObject(rollback, "failed", stats->failed_count);

  char rolled_back_at[32] = {0};
  NowUtc(rolled_back_at, sizeof(rolled_back_at));
  cJSON_AddStringToObject(rollback, "rolledBackAtUtc", rolled_back_at);

  cJSON_ReplaceItemInObject(json, "rollback", rollback);

  char *updated = cJSON_Print(json);
  cJSON_Delete(json);
  if (!updated) {
    return false;
  }

  bool ok = SaveFileText(operation_path, updated);
  cJSON_free(updated);
  return ok;
}

bool RenamerHistoryRollback(const char *env_dir, const char *operation_id,
                            RenamerRollbackStats *out_stats,
                            char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !operation_id || operation_id[0] == '\0' ||
      !out_stats) {
    SetError(out_error, out_error_size,
             "env_dir, operation_id, and output stats are required for rollback");
    return false;
  }

  memset(out_stats, 0, sizeof(*out_stats));

  char history_dir[MAX_PATH_LENGTH] = {0};
  char index_path[MAX_PATH_LENGTH] = {0};
  if (!EnsureHistoryPaths(env_dir, history_dir, sizeof(history_dir), index_path,
                          sizeof(index_path))) {
    SetError(out_error, out_error_size,
             "failed to ensure rename history path under '%s'", env_dir);
    return false;
  }

  char operation_path[MAX_PATH_LENGTH] = {0};
  snprintf(operation_path, sizeof(operation_path), "%s/%s.json", history_dir,
           operation_id);

  char *manifest_text = NULL;
  if (!LoadFileText(operation_path, &manifest_text)) {
    SetError(out_error, out_error_size,
             "rename operation '%s' was not found", operation_id);
    return false;
  }

  cJSON *manifest = cJSON_Parse(manifest_text);
  free(manifest_text);
  if (!manifest) {
    SetError(out_error, out_error_size,
             "rename operation '%s' is malformed", operation_id);
    return false;
  }

  cJSON *items = cJSON_GetObjectItem(manifest, "items");
  if (!cJSON_IsArray(items)) {
    cJSON_Delete(manifest);
    SetError(out_error, out_error_size,
             "rename operation '%s' has invalid items payload", operation_id);
    return false;
  }

  cJSON *tag_root = NULL;
  char tag_error[256] = {0};
  if (!RenamerTagsLoadSidecar(env_dir, &tag_root, tag_error, sizeof(tag_error))) {
    cJSON_Delete(manifest);
    SetError(out_error, out_error_size, "%s", tag_error);
    return false;
  }

  bool tags_updated = false;
  int count = cJSON_GetArraySize(items);
  for (int i = count - 1; i >= 0; i--) {
    cJSON *item = cJSON_GetArrayItem(items, i);
    if (!cJSON_IsObject(item)) {
      continue;
    }

    cJSON *source = cJSON_GetObjectItem(item, "source");
    cJSON *destination = cJSON_GetObjectItem(item, "destination");
    cJSON *renamed = cJSON_GetObjectItem(item, "renamed");

    if (!cJSON_IsString(source) || !source->valuestring ||
        !cJSON_IsString(destination) || !destination->valuestring ||
        !(cJSON_IsBool(renamed) && cJSON_IsTrue(renamed))) {
      continue;
    }

    if (access(destination->valuestring, F_OK) != 0) {
      out_stats->skipped_count++;
      continue;
    }

    if (access(source->valuestring, F_OK) == 0) {
      out_stats->failed_count++;
      continue;
    }

    char parent_dir[MAX_PATH_LENGTH] = {0};
    strncpy(parent_dir, source->valuestring, sizeof(parent_dir) - 1);
    parent_dir[sizeof(parent_dir) - 1] = '\0';
    char *slash = strrchr(parent_dir, '/');
    if (slash) {
      *slash = '\0';
      if (parent_dir[0] != '\0') {
        FsMakeDirRecursive(parent_dir);
      }
    }

    if (FsRenameFile(destination->valuestring, source->valuestring)) {
      out_stats->restored_count++;
      RenamerTagsMovePathKey(tag_root, destination->valuestring,
                             source->valuestring);
      tags_updated = true;
    } else {
      out_stats->failed_count++;
    }
  }

  cJSON_Delete(manifest);

  if (tags_updated) {
    if (!RenamerTagsSaveSidecar(env_dir, tag_root, tag_error, sizeof(tag_error))) {
      cJSON_Delete(tag_root);
      SetError(out_error, out_error_size, "%s", tag_error);
      return false;
    }
  }
  cJSON_Delete(tag_root);

  UpdateManifestRollbackStatus(operation_path, out_stats);
  if (!UpdateIndexRollbackStatus(index_path, operation_id, out_stats, out_error,
                                 out_error_size)) {
    return false;
  }

  return true;
}
