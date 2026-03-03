#ifndef CLI_RENAME_COMMANDS_H
#define CLI_RENAME_COMMANDS_H

#include "app_api.h"
#include "cli/cli_main_options.h"

int CliRunRenameActions(AppContext *app, const CliMainOptions *options,
                        const CliRenameHistoryFilter *rename_history_filter,
                        const char *argv0);

#endif // CLI_RENAME_COMMANDS_H
