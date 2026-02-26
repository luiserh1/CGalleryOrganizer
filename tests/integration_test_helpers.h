#ifndef INTEGRATION_TEST_HELPERS_H
#define INTEGRATION_TEST_HELPERS_H

#include <stdbool.h>
#include <stddef.h>

int RunCommandCapture(const char *cmd, char *output, size_t output_size);
bool ReportsEquivalentIgnoringTimestamp(const char *left_path,
                                        const char *right_path);
bool WriteRollbackManifest(const char *env_dir);
bool WriteBootstrapModels(const char *models_dir);

#endif
