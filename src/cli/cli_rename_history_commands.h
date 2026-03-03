#ifndef CLI_RENAME_HISTORY_COMMANDS_H
#define CLI_RENAME_HISTORY_COMMANDS_H

#include "app_api.h"
#include "cli/cli_rename_utils.h"

int CliHandleRenameHistory(AppContext *app, const char *target_dir,
                           const char *env_dir,
                           const CliRenameHistoryFilter *filter,
                           const char *argv0);

int CliHandleRenameHistoryExport(AppContext *app, const char *target_dir,
                                 const char *env_dir,
                                 const CliRenameHistoryFilter *filter,
                                 const char *output_path,
                                 const char *argv0);

int CliHandleRenameHistoryLatestId(const char *target_dir, const char *env_dir,
                                   const char *argv0);

int CliHandleRenamePreviewLatestId(const char *target_dir, const char *env_dir,
                                   const char *argv0);

int CliHandleRenameHistoryDetail(AppContext *app, const char *target_dir,
                                 const char *env_dir,
                                 const char *operation_id,
                                 const char *argv0);

int CliHandleRenameRollback(AppContext *app, const char *target_dir,
                            const char *env_dir, const char *operation_id,
                            const char *argv0);

int CliHandleRenameRollbackPreflight(AppContext *app, const char *target_dir,
                                     const char *env_dir,
                                     const char *operation_id,
                                     const char *argv0);

int CliHandleRenameHistoryPrune(AppContext *app, const char *target_dir,
                                const char *env_dir, int keep_count,
                                const char *argv0);

#endif // CLI_RENAME_HISTORY_COMMANDS_H
