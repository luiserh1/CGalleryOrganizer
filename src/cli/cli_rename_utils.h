#ifndef CLI_RENAME_UTILS_H
#define CLI_RENAME_UTILS_H

#include <stdbool.h>
#include <stddef.h>

#include "app_api_types.h"
#include "fs_utils.h"

typedef struct {
  char target_abs[MAX_PATH_LENGTH];
  char env_abs[MAX_PATH_LENGTH];
  int media_files_in_scope;
} CliRenameInitResult;

typedef struct {
  char map_path[MAX_PATH_LENGTH];
  int files_scanned;
  int files_with_tags;
} CliRenameBootstrapResult;

typedef enum {
  CLI_RENAME_ROLLBACK_FILTER_ANY = 0,
  CLI_RENAME_ROLLBACK_FILTER_YES = 1,
  CLI_RENAME_ROLLBACK_FILTER_NO = 2
} CliRenameRollbackFilter;

typedef struct {
  char operation_id_prefix[64];
  CliRenameRollbackFilter rollback_filter;
  char created_from_utc[32];
  char created_to_utc[32];
} CliRenameHistoryFilter;

bool CliRenameInitEnvironment(const char *target_dir, const char *env_dir,
                              CliRenameInitResult *out_result,
                              char *out_error, size_t out_error_size);

bool CliRenameBootstrapTagsFromFilename(const char *target_dir,
                                        const char *env_dir,
                                        CliRenameBootstrapResult *out_result,
                                        char *out_error,
                                        size_t out_error_size);

bool CliRenameResolveLatestPreviewId(const char *env_dir, char *out_preview_id,
                                     size_t out_preview_id_size,
                                     char *out_error,
                                     size_t out_error_size);

bool CliRenameResolveLatestOperationId(const char *env_dir, char *out_operation_id,
                                       size_t out_operation_id_size,
                                       char *out_error,
                                       size_t out_error_size);

bool CliRenameResolvePreviewIdFromOperation(const char *env_dir,
                                            const char *operation_id,
                                            char *out_preview_id,
                                            size_t out_preview_id_size,
                                            char *out_error,
                                            size_t out_error_size);

bool CliRenameResolveOperationManifestPath(const char *env_dir,
                                           const char *operation_id,
                                           char *out_manifest_path,
                                           size_t out_manifest_path_size,
                                           char *out_error,
                                           size_t out_error_size);

bool CliRenameParseRollbackFilter(const char *raw_value,
                                  CliRenameRollbackFilter *out_filter,
                                  char *out_error, size_t out_error_size);

bool CliRenameNormalizeHistoryDateBound(const char *raw_value, bool is_upper_bound,
                                        char *out_utc, size_t out_utc_size,
                                        char *out_error, size_t out_error_size);

bool CliRenameBuildHistoryFilterIndex(const AppRenameHistoryEntry *entries,
                                      int entry_count,
                                      const CliRenameHistoryFilter *filter,
                                      int **out_indices,
                                      int *out_filtered_count,
                                      char *out_error,
                                      size_t out_error_size);

void CliRenameFreeHistoryFilterIndex(int *indices);

bool CliRenameExportHistoryJson(const char *env_dir,
                                const AppRenameHistoryEntry *entries,
                                const int *indices, int index_count,
                                const CliRenameHistoryFilter *filter,
                                const char *output_path, char *out_error,
                                size_t out_error_size);

bool CliRenameSuggestPath(const char *missing_path, char *out_suggestion,
                          size_t out_suggestion_size);

#endif // CLI_RENAME_UTILS_H
