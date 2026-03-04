#include <stdio.h>
#include <stdlib.h>

#include "systems/renamer_history_internal.h"

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
    RenamerHistorySetError(out_error, out_error_size,
                           "invalid arguments for rename history persistence");
    return false;
  }

  char history_dir[MAX_PATH_LENGTH] = {0};
  char index_path[MAX_PATH_LENGTH] = {0};
  if (!RenamerHistoryEnsureHistoryPaths(env_dir, history_dir, sizeof(history_dir),
                                        index_path, sizeof(index_path))) {
    RenamerHistorySetError(out_error, out_error_size,
                           "failed to ensure rename history path under '%s'",
                           env_dir);
    return false;
  }

  char operation_path[MAX_PATH_LENGTH] = {0};
  snprintf(operation_path, sizeof(operation_path), "%s/%s.json", history_dir,
           entry->operation_id);

  cJSON *manifest = NULL;
  if (!BuildOperationManifestJson(entry, moves, move_count, &manifest)) {
    RenamerHistorySetError(out_error, out_error_size,
                           "failed to build rename operation manifest");
    return false;
  }

  char *manifest_text = cJSON_Print(manifest);
  cJSON_Delete(manifest);
  if (!manifest_text) {
    RenamerHistorySetError(out_error, out_error_size,
                           "failed to serialize rename operation manifest");
    return false;
  }

  bool manifest_ok = RenamerHistorySaveFileText(operation_path, manifest_text);
  cJSON_free(manifest_text);
  if (!manifest_ok) {
    RenamerHistorySetError(out_error, out_error_size,
                           "failed to write rename operation manifest '%s'",
                           operation_path);
    return false;
  }

  cJSON *index_root = NULL;
  if (!RenamerHistoryLoadIndex(index_path, &index_root, out_error,
                               out_error_size)) {
    return false;
  }

  cJSON *entries = cJSON_GetObjectItem(index_root, "entries");
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(index_root);
    RenamerHistorySetError(out_error, out_error_size,
                           "rename history index has invalid entries payload");
    return false;
  }

  cJSON *entry_json = RenamerHistoryBuildHistoryEntryJson(entry);
  if (!entry_json) {
    cJSON_Delete(index_root);
    RenamerHistorySetError(out_error, out_error_size,
                           "failed to encode rename history entry");
    return false;
  }

  cJSON_AddItemToArray(entries, entry_json);
  while (cJSON_GetArraySize(entries) > RENAMER_HISTORY_MAX_ENTRIES) {
    RenamerHistoryRemoveOldestHistoryEntry(entries, history_dir);
  }

  bool ok = RenamerHistorySaveIndex(index_path, index_root, out_error,
                                    out_error_size);
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
    RenamerHistorySetError(out_error, out_error_size,
                           "invalid arguments for rename history listing");
    return false;
  }

  *out_entries = NULL;
  *out_count = 0;

  char history_dir[MAX_PATH_LENGTH] = {0};
  char index_path[MAX_PATH_LENGTH] = {0};
  if (!RenamerHistoryEnsureHistoryPaths(env_dir, history_dir, sizeof(history_dir),
                                        index_path, sizeof(index_path))) {
    RenamerHistorySetError(out_error, out_error_size,
                           "failed to ensure rename history path under '%s'",
                           env_dir);
    return false;
  }

  cJSON *index_root = NULL;
  if (!RenamerHistoryLoadIndex(index_path, &index_root, out_error,
                               out_error_size)) {
    return false;
  }

  cJSON *entries = cJSON_GetObjectItem(index_root, "entries");
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(index_root);
    RenamerHistorySetError(out_error, out_error_size,
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
    RenamerHistorySetError(out_error, out_error_size,
                           "out of memory while loading rename history entries");
    return false;
  }

  int written = 0;
  for (int i = count - 1; i >= 0; i--) {
    cJSON *node = cJSON_GetArrayItem(entries, i);
    if (!RenamerHistoryParseHistoryEntry(node, &list[written])) {
      continue;
    }
    written++;
  }

  cJSON_Delete(index_root);
  *out_entries = list;
  *out_count = written;
  return true;
}
