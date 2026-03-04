#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "systems/renamer_history_internal.h"
#include "systems/renamer_tags.h"

static bool UpdateIndexRollbackStatus(const char *index_path,
                                      const char *operation_id,
                                      const RenamerRollbackStats *stats,
                                      char *out_error,
                                      size_t out_error_size) {
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
    RenamerHistoryNowUtc(rolled_back_at, sizeof(rolled_back_at));
    cJSON_ReplaceItemInObject(entry, "rolledBackAtUtc",
                              cJSON_CreateString(rolled_back_at));
    break;
  }

  bool ok = RenamerHistorySaveIndex(index_path, index_root, out_error,
                                    out_error_size);
  cJSON_Delete(index_root);
  return ok;
}

static bool UpdateManifestRollbackStatus(const char *operation_path,
                                         const RenamerRollbackStats *stats) {
  char *text = NULL;
  if (!RenamerHistoryLoadFileText(operation_path, &text)) {
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
  RenamerHistoryNowUtc(rolled_back_at, sizeof(rolled_back_at));
  cJSON_AddStringToObject(rollback, "rolledBackAtUtc", rolled_back_at);

  cJSON_ReplaceItemInObject(json, "rollback", rollback);

  char *updated = cJSON_Print(json);
  cJSON_Delete(json);
  if (!updated) {
    return false;
  }

  bool ok = RenamerHistorySaveFileText(operation_path, updated);
  cJSON_free(updated);
  return ok;
}

bool RenamerHistoryRollbackPreflight(const char *env_dir,
                                     const char *operation_id,
                                     RenamerRollbackPreflight *out_preflight,
                                     char *out_error,
                                     size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !operation_id || operation_id[0] == '\0' ||
      !out_preflight) {
    RenamerHistorySetError(out_error, out_error_size,
                           "env_dir, operation_id, and output preflight are required");
    return false;
  }

  memset(out_preflight, 0, sizeof(*out_preflight));

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
           operation_id);

  char *manifest_text = NULL;
  if (!RenamerHistoryLoadFileText(operation_path, &manifest_text)) {
    RenamerHistorySetError(out_error, out_error_size,
                           "rename operation '%s' was not found", operation_id);
    return false;
  }

  cJSON *manifest = cJSON_Parse(manifest_text);
  free(manifest_text);
  if (!manifest) {
    RenamerHistorySetError(out_error, out_error_size,
                           "rename operation '%s' is malformed", operation_id);
    return false;
  }

  cJSON *items = cJSON_GetObjectItem(manifest, "items");
  if (!cJSON_IsArray(items)) {
    cJSON_Delete(manifest);
    RenamerHistorySetError(out_error, out_error_size,
                           "rename operation '%s' has invalid items payload",
                           operation_id);
    return false;
  }

  int count = cJSON_GetArraySize(items);
  for (int i = count - 1; i >= 0; i--) {
    cJSON *item = cJSON_GetArrayItem(items, i);
    if (!cJSON_IsObject(item)) {
      out_preflight->invalid_item_count++;
      continue;
    }

    cJSON *source = cJSON_GetObjectItem(item, "source");
    cJSON *destination = cJSON_GetObjectItem(item, "destination");
    cJSON *renamed = cJSON_GetObjectItem(item, "renamed");
    if (!cJSON_IsString(source) || !source->valuestring ||
        !cJSON_IsString(destination) || !destination->valuestring ||
        !(cJSON_IsBool(renamed) && cJSON_IsTrue(renamed))) {
      out_preflight->invalid_item_count++;
      continue;
    }

    out_preflight->total_items++;

    if (access(destination->valuestring, F_OK) != 0) {
      out_preflight->missing_destination_count++;
      continue;
    }

    if (access(source->valuestring, F_OK) == 0) {
      out_preflight->source_exists_conflict_count++;
      continue;
    }

    out_preflight->restorable_count++;
  }

  cJSON_Delete(manifest);

  out_preflight->fully_restorable =
      out_preflight->total_items > 0 &&
      out_preflight->restorable_count == out_preflight->total_items &&
      out_preflight->missing_destination_count == 0 &&
      out_preflight->source_exists_conflict_count == 0 &&
      out_preflight->invalid_item_count == 0;
  return true;
}

bool RenamerHistoryRollback(const char *env_dir, const char *operation_id,
                            RenamerRollbackStats *out_stats,
                            char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !operation_id || operation_id[0] == '\0' ||
      !out_stats) {
    RenamerHistorySetError(
        out_error, out_error_size,
        "env_dir, operation_id, and output stats are required for rollback");
    return false;
  }

  memset(out_stats, 0, sizeof(*out_stats));

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
           operation_id);

  char *manifest_text = NULL;
  if (!RenamerHistoryLoadFileText(operation_path, &manifest_text)) {
    RenamerHistorySetError(out_error, out_error_size,
                           "rename operation '%s' was not found", operation_id);
    return false;
  }

  cJSON *manifest = cJSON_Parse(manifest_text);
  free(manifest_text);
  if (!manifest) {
    RenamerHistorySetError(out_error, out_error_size,
                           "rename operation '%s' is malformed", operation_id);
    return false;
  }

  cJSON *items = cJSON_GetObjectItem(manifest, "items");
  if (!cJSON_IsArray(items)) {
    cJSON_Delete(manifest);
    RenamerHistorySetError(out_error, out_error_size,
                           "rename operation '%s' has invalid items payload",
                           operation_id);
    return false;
  }

  cJSON *tag_root = NULL;
  char tag_error[256] = {0};
  if (!RenamerTagsLoadSidecar(env_dir, &tag_root, tag_error, sizeof(tag_error))) {
    cJSON_Delete(manifest);
    RenamerHistorySetError(out_error, out_error_size, "%s", tag_error);
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
      RenamerHistorySetError(out_error, out_error_size, "%s", tag_error);
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
