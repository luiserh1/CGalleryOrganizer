#ifndef RENAMER_HISTORY_INTERNAL_H
#define RENAMER_HISTORY_INTERNAL_H

#include "cJSON.h"
#include "systems/renamer_history.h"

#define RENAMER_HISTORY_MAX_ENTRIES 200

void RenamerHistorySetError(char *out_error, size_t out_error_size,
                            const char *fmt, ...);

void RenamerHistoryNowUtc(char *out_text, size_t out_text_size);

bool RenamerHistoryLoadFileText(const char *path, char **out_text);

bool RenamerHistorySaveFileText(const char *path, const char *text);

bool RenamerHistoryEnsureHistoryPaths(const char *env_dir, char *out_history_dir,
                                      size_t out_history_dir_size,
                                      char *out_index_path,
                                      size_t out_index_path_size);

cJSON *RenamerHistoryCreateDefaultIndex(void);

bool RenamerHistoryLoadIndex(const char *index_path, cJSON **out_root,
                             char *out_error, size_t out_error_size);

bool RenamerHistorySaveIndex(const char *index_path, const cJSON *index_root,
                             char *out_error, size_t out_error_size);

cJSON *RenamerHistoryBuildHistoryEntryJson(const RenamerHistoryEntry *entry);

bool RenamerHistoryParseHistoryEntry(const cJSON *json,
                                     RenamerHistoryEntry *out_entry);

bool RenamerHistoryRemoveOldestHistoryEntry(cJSON *entries,
                                            const char *history_dir);

#endif // RENAMER_HISTORY_INTERNAL_H
