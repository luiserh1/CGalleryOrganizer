#ifndef CLI_ARGS_H
#define CLI_ARGS_H

#include <stdbool.h>

void CliPrintUsage(const char *argv0);

const char *CliResolveRollbackEnvDir(const char *first_positional,
                                     const char *second_positional);

int CliResolveJobsFromString(const char *raw_value, bool *out_valid);

#endif // CLI_ARGS_H
