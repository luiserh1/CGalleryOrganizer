#ifndef CLI_RENAME_UTILS_H
#define CLI_RENAME_UTILS_H

#include <stdbool.h>
#include <stddef.h>

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

bool CliRenameSuggestPath(const char *missing_path, char *out_suggestion,
                          size_t out_suggestion_size);

#endif // CLI_RENAME_UTILS_H
