#ifndef CLI_MAIN_OPTIONS_H
#define CLI_MAIN_OPTIONS_H

#include <stdbool.h>

#include "app_api_types.h"
#include "cli/cli_rename_utils.h"

typedef struct {
  bool exhaustive;
  bool ml_enrich;
  bool similarity_report;
  bool sim_incremental;
  bool organize;
  bool preview;
  bool duplicates_report;
  bool duplicates_move;
  bool rollback;

  bool rename_preview;
  bool rename_apply;
  bool rename_apply_latest;
  bool rename_preview_latest_id;
  bool rename_init;
  bool rename_bootstrap_tags_from_filename;
  bool rename_history;
  bool rename_history_export;
  bool rename_history_prune;
  bool rename_history_latest_id;
  const char *rename_history_detail_operation;
  const char *rename_history_export_path;
  const char *rename_history_id_prefix;
  const char *rename_history_rollback_filter;
  const char *rename_history_from;
  const char *rename_history_to;
  int rename_history_prune_keep_count;
  const char *rename_redo_operation;
  const char *rename_rollback_operation;
  const char *rename_rollback_preflight_operation;
  const char *rename_pattern;
  const char *rename_tags_map_path;
  const char *rename_tag_add_csv;
  const char *rename_tag_remove_csv;
  const char *rename_meta_tag_add_csv;
  const char *rename_meta_tag_remove_csv;
  bool rename_metadata_fields;
  bool rename_preview_json;
  const char *rename_preview_json_out;
  const char *rename_from_preview;
  bool rename_accept_auto_suffix;

  const char *jobs_arg;
  bool jobs_set_by_cli;
  float sim_threshold;
  int sim_topk;
  AppSimilarityMemoryMode sim_memory_mode;
  AppCacheCompressionMode cache_compression_mode;
  int cache_compression_level;
  bool cache_compression_level_set;
  const char *target_dir;
  const char *env_dir;
  const char *group_by_arg;

  bool rename_rollback;
  bool rename_rollback_preflight;
  bool rename_history_detail;
  bool rename_redo;
  bool rename_apply_action;
  int rename_action_count;
} CliMainOptions;

void CliMainOptionsInit(CliMainOptions *options);

bool CliMainHasRenameAction(const CliMainOptions *options);

bool CliParseMainOptions(CliMainOptions *options,
                         CliRenameHistoryFilter *out_rename_history_filter,
                         int argc, char **argv, int *out_exit_code);

#endif // CLI_MAIN_OPTIONS_H
