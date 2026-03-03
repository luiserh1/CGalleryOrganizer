#ifndef RENAMER_HISTORY_H
#define RENAMER_HISTORY_H

#include <stdbool.h>
#include <stddef.h>

#include "fs_utils.h"

typedef struct {
  char source_path[MAX_PATH_LENGTH];
  char destination_path[MAX_PATH_LENGTH];
  bool renamed;
} RenamerOperationMove;

typedef struct {
  char operation_id[64];
  char preview_id[64];
  char created_at_utc[32];
  int renamed_count;
  int skipped_count;
  int failed_count;
  int collision_resolved_count;
  int truncation_count;
  bool rollback_performed;
  int rollback_restored_count;
  int rollback_skipped_count;
  int rollback_failed_count;
} RenamerHistoryEntry;

typedef struct {
  int restored_count;
  int skipped_count;
  int failed_count;
} RenamerRollbackStats;

typedef struct {
  int total_items;
  int restorable_count;
  int missing_destination_count;
  int source_exists_conflict_count;
  int invalid_item_count;
  bool fully_restorable;
} RenamerRollbackPreflight;

typedef struct {
  int before_count;
  int after_count;
  int pruned_count;
} RenamerHistoryPruneStats;

bool RenamerHistoryPersistOperation(const char *env_dir,
                                    const RenamerHistoryEntry *entry,
                                    const RenamerOperationMove *moves,
                                    int move_count, char *out_error,
                                    size_t out_error_size);

bool RenamerHistoryList(const char *env_dir, RenamerHistoryEntry **out_entries,
                        int *out_count, char *out_error,
                        size_t out_error_size);

void RenamerHistoryFreeEntries(RenamerHistoryEntry *entries);

bool RenamerHistoryRollback(const char *env_dir, const char *operation_id,
                            RenamerRollbackStats *out_stats,
                            char *out_error, size_t out_error_size);

bool RenamerHistoryRollbackPreflight(const char *env_dir,
                                     const char *operation_id,
                                     RenamerRollbackPreflight *out_preflight,
                                     char *out_error, size_t out_error_size);

bool RenamerHistoryPrune(const char *env_dir, int keep_count,
                         RenamerHistoryPruneStats *out_stats,
                         char *out_error, size_t out_error_size);

#endif // RENAMER_HISTORY_H
