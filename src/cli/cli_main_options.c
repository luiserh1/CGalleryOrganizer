#include <string.h>

#include "cli/cli_main_options.h"
#include "cli/cli_main_options_internal.h"

void CliMainOptionsInit(CliMainOptions *options) {
  if (!options) {
    return;
  }

  memset(options, 0, sizeof(*options));
  options->sim_incremental = true;
  options->jobs_arg = "auto";
  options->sim_threshold = 0.92f;
  options->sim_topk = 5;
  options->sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
  options->cache_compression_mode = APP_CACHE_COMPRESSION_NONE;
  options->cache_compression_level = 3;
  options->rename_history_prune_keep_count = -1;
}

bool CliMainHasRenameAction(const CliMainOptions *options) {
  if (!options) {
    return false;
  }
  return options->rename_action_count > 0;
}

bool CliParseMainOptions(CliMainOptions *options,
                         CliRenameHistoryFilter *out_rename_history_filter,
                         int argc, char **argv, int *out_exit_code) {
  if (!options || !out_rename_history_filter || !argv || argc < 1 ||
      !out_exit_code) {
    return false;
  }

  *out_exit_code = 1;
  CliMainOptionsInit(options);

  if (!CliMainParseArguments(options, argc, argv, out_exit_code)) {
    return false;
  }

  return CliMainFinalizeOptions(options, out_rename_history_filter,
                                out_exit_code);
}
