#ifndef CLI_MAIN_OPTIONS_INTERNAL_H
#define CLI_MAIN_OPTIONS_INTERNAL_H

#include "cli/cli_main_options.h"

bool CliMainParseArguments(CliMainOptions *options, int argc, char **argv,
                           int *out_exit_code);

bool CliMainFinalizeOptions(CliMainOptions *options,
                            CliRenameHistoryFilter *out_rename_history_filter,
                            int *out_exit_code);

#endif // CLI_MAIN_OPTIONS_INTERNAL_H
