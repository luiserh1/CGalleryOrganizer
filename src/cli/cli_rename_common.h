#ifndef CLI_RENAME_COMMON_H
#define CLI_RENAME_COMMON_H

#include <stdbool.h>
#include <stddef.h>

void CliRenameCommonSetError(char *out_error, size_t out_error_size,
                             const char *fmt, ...);

bool CliRenameCommonIsExistingDirectory(const char *path);

bool CliRenameCommonSaveTextFile(const char *path, const char *text);

bool CliRenameCommonLoadTextFile(const char *path, char **out_text);

bool CliRenameCommonEndsWith(const char *text, const char *suffix);

#endif // CLI_RENAME_COMMON_H
