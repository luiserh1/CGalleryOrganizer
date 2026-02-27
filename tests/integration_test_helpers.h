#ifndef INTEGRATION_TEST_HELPERS_H
#define INTEGRATION_TEST_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

int RunCommandCapture(const char *cmd, char *output, size_t output_size);
bool ReportsEquivalentIgnoringTimestamp(const char *left_path,
                                        const char *right_path);
bool WriteRollbackManifest(const char *env_dir);
bool WriteBootstrapModels(const char *models_dir);
bool CopyFileForTest(const char *source_path, const char *target_path);
bool RemovePathRecursiveForTest(const char *path);
bool RemovePathsForTest(const char *paths[], size_t count);
bool ResetDirForTest(const char *path);

#endif
