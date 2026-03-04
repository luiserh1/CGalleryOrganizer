#include <string.h>

#include "systems/renamer_history_internal.h"

bool RenamerHistoryPrune(const char *env_dir, int keep_count,
                         RenamerHistoryPruneStats *out_stats,
                         char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || keep_count < 0 || !out_stats) {
    RenamerHistorySetError(out_error, out_error_size,
                           "env_dir, non-negative keep_count, and output stats are required");
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

  out_stats->before_count = cJSON_GetArraySize(entries);
  while (cJSON_GetArraySize(entries) > keep_count) {
    if (!RenamerHistoryRemoveOldestHistoryEntry(entries, history_dir)) {
      cJSON_Delete(index_root);
      RenamerHistorySetError(out_error, out_error_size,
                             "failed to prune rename history");
      return false;
    }
    out_stats->pruned_count++;
  }
  out_stats->after_count = cJSON_GetArraySize(entries);

  bool ok = RenamerHistorySaveIndex(index_path, index_root, out_error,
                                    out_error_size);
  cJSON_Delete(index_root);
  return ok;
}
