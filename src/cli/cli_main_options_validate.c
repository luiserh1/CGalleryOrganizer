#include <stdio.h>
#include <string.h>

#include "cli/cli_main_options_internal.h"

bool CliMainFinalizeOptions(CliMainOptions *options,
                            CliRenameHistoryFilter *out_rename_history_filter,
                            int *out_exit_code) {
  if (!options || !out_rename_history_filter || !out_exit_code) {
    return false;
  }

  options->rename_rollback =
      options->rename_rollback_operation &&
      options->rename_rollback_operation[0] != '\0';
  options->rename_rollback_preflight = options->rename_rollback_preflight_operation &&
                                        options->rename_rollback_preflight_operation[0] != '\0';
  options->rename_history_detail =
      options->rename_history_detail_operation &&
      options->rename_history_detail_operation[0] != '\0';
  options->rename_redo =
      options->rename_redo_operation && options->rename_redo_operation[0] != '\0';
  options->rename_apply_action =
      options->rename_apply || options->rename_apply_latest || options->rename_redo;

  bool rename_history_filter_active =
      (options->rename_history_id_prefix &&
       options->rename_history_id_prefix[0] != '\0') ||
      (options->rename_history_rollback_filter &&
       options->rename_history_rollback_filter[0] != '\0') ||
      (options->rename_history_from && options->rename_history_from[0] != '\0') ||
      (options->rename_history_to && options->rename_history_to[0] != '\0');

  int rename_apply_variant_count = 0;
  rename_apply_variant_count += options->rename_apply ? 1 : 0;
  rename_apply_variant_count += options->rename_apply_latest ? 1 : 0;
  rename_apply_variant_count += options->rename_redo ? 1 : 0;

  options->rename_action_count = 0;
  options->rename_action_count += options->rename_init ? 1 : 0;
  options->rename_action_count += options->rename_bootstrap_tags_from_filename ? 1 : 0;
  options->rename_action_count += options->rename_preview ? 1 : 0;
  options->rename_action_count += options->rename_preview_latest_id ? 1 : 0;
  options->rename_action_count += options->rename_apply_action ? 1 : 0;
  options->rename_action_count += options->rename_history ? 1 : 0;
  options->rename_action_count += options->rename_history_export ? 1 : 0;
  options->rename_action_count += options->rename_history_prune ? 1 : 0;
  options->rename_action_count += options->rename_history_latest_id ? 1 : 0;
  options->rename_action_count += options->rename_history_detail ? 1 : 0;
  options->rename_action_count += options->rename_rollback ? 1 : 0;
  options->rename_action_count += options->rename_rollback_preflight ? 1 : 0;

  if (options->rename_action_count > 1) {
    printf("Error: Rename actions are mutually exclusive "
           "(--rename-init|--rename-bootstrap-tags-from-filename|"
           "--rename-preview|--rename-preview-latest-id|"
           "--rename-apply|--rename-apply-latest|--rename-redo|"
           "--rename-history|--rename-history-export|"
           "--rename-history-prune|--rename-history-latest-id|"
           "--rename-history-detail|--rename-rollback|"
           "--rename-rollback-preflight).\n");
    *out_exit_code = 1;
    return false;
  }

  if (rename_apply_variant_count > 1) {
    printf("Error: --rename-apply, --rename-apply-latest, and --rename-redo "
           "cannot be used together.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->rename_apply &&
      (!options->rename_from_preview || options->rename_from_preview[0] == '\0')) {
    printf("Error: --rename-apply requires --rename-from-preview <preview_id>.\n");
    *out_exit_code = 1;
    return false;
  }

  if (!options->rename_apply && options->rename_from_preview &&
      options->rename_from_preview[0] != '\0') {
    printf("Error: --rename-from-preview can only be used with --rename-apply.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->rename_apply_latest && options->rename_from_preview &&
      options->rename_from_preview[0] != '\0') {
    printf("Error: --rename-from-preview cannot be used with "
           "--rename-apply-latest.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->rename_redo && options->rename_from_preview &&
      options->rename_from_preview[0] != '\0') {
    printf("Error: --rename-from-preview cannot be used with --rename-redo.\n");
    *out_exit_code = 1;
    return false;
  }

  if (!options->rename_apply_action && options->rename_accept_auto_suffix) {
    printf("Error: --rename-accept-auto-suffix can only be used with "
           "--rename-apply or --rename-apply-latest.\n");
    *out_exit_code = 1;
    return false;
  }

  if (!options->rename_preview &&
      (options->rename_preview_json ||
       (options->rename_preview_json_out &&
        options->rename_preview_json_out[0] != '\0'))) {
    printf("Error: --rename-preview-json and --rename-preview-json-out are only "
           "valid with --rename-preview.\n");
    *out_exit_code = 1;
    return false;
  }

  if (!options->rename_preview &&
      ((options->rename_pattern && options->rename_pattern[0] != '\0') ||
       (options->rename_tags_map_path && options->rename_tags_map_path[0] != '\0') ||
       (options->rename_tag_add_csv && options->rename_tag_add_csv[0] != '\0') ||
       (options->rename_tag_remove_csv &&
        options->rename_tag_remove_csv[0] != '\0') ||
       (options->rename_meta_tag_add_csv &&
        options->rename_meta_tag_add_csv[0] != '\0') ||
       (options->rename_meta_tag_remove_csv &&
        options->rename_meta_tag_remove_csv[0] != '\0') ||
       options->rename_metadata_fields)) {
    printf("Error: rename pattern/tag edit options are only valid with "
           "--rename-preview.\n");
    *out_exit_code = 1;
    return false;
  }

  if (rename_history_filter_active &&
      !(options->rename_history || options->rename_history_export)) {
    printf("Error: --rename-history-id-prefix/--rename-history-rollback/"
           "--rename-history-from/--rename-history-to require "
           "--rename-history or --rename-history-export.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->rename_history_export &&
      (!options->rename_history_export_path ||
       options->rename_history_export_path[0] == '\0')) {
    printf("Error: --rename-history-export requires an output JSON path.\n");
    *out_exit_code = 1;
    return false;
  }

  bool has_non_rename_actions = options->rollback || options->organize ||
                                options->preview || options->duplicates_report ||
                                options->duplicates_move ||
                                options->similarity_report || options->ml_enrich;
  if (options->rename_action_count > 0 && has_non_rename_actions) {
    printf("Error: Rename actions cannot be combined with scan/organize/"
           "similarity/duplicate/rollback actions.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->rename_action_count > 0 &&
      (options->jobs_set_by_cli || options->exhaustive || options->group_by_arg)) {
    printf("Error: --jobs/--exhaustive/--group-by are not applicable to dedicated "
           "rename actions.\n");
    *out_exit_code = 1;
    return false;
  }

  if ((options->rename_init || options->rename_bootstrap_tags_from_filename) &&
      (!options->target_dir || options->target_dir[0] == '\0' ||
       !options->env_dir || options->env_dir[0] == '\0')) {
    printf("Error: --rename-init and --rename-bootstrap-tags-from-filename "
           "require <target_dir> <env_dir>.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->rollback && (options->organize || options->preview)) {
    printf("Error: Cannot specify --rollback with --organize or --preview.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->rollback && (options->duplicates_report || options->duplicates_move)) {
    printf("Error: Cannot specify --rollback with duplicate actions.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->rollback && options->similarity_report) {
    printf("Error: Cannot specify --rollback with --similarity-report.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->organize && options->preview) {
    printf("Error: Cannot specify --organize and --preview together.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->similarity_report && (options->organize || options->preview)) {
    printf("Error: --similarity-report cannot be combined with --organize/"
           "--preview.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->duplicates_report && options->duplicates_move) {
    printf("Error: Cannot specify --duplicates-report and --duplicates-move together.\n");
    *out_exit_code = 1;
    return false;
  }

  if ((options->duplicates_report || options->duplicates_move) &&
      (options->organize || options->preview || options->similarity_report)) {
    printf("Error: Duplicate actions cannot be combined with "
           "--similarity-report/--organize/--preview.\n");
    *out_exit_code = 1;
    return false;
  }

  if (options->cache_compression_level_set &&
      options->cache_compression_mode != APP_CACHE_COMPRESSION_ZSTD &&
      options->cache_compression_mode != APP_CACHE_COMPRESSION_AUTO) {
    printf("Error: --cache-compress-level is only valid with --cache-compress "
           "zstd|auto.\n");
    *out_exit_code = 1;
    return false;
  }

  memset(out_rename_history_filter, 0, sizeof(*out_rename_history_filter));
  out_rename_history_filter->rollback_filter = CLI_RENAME_ROLLBACK_FILTER_ANY;

  if (options->rename_history_id_prefix &&
      options->rename_history_id_prefix[0] != '\0') {
    strncpy(out_rename_history_filter->operation_id_prefix,
            options->rename_history_id_prefix,
            sizeof(out_rename_history_filter->operation_id_prefix) - 1);
    out_rename_history_filter
        ->operation_id_prefix[sizeof(out_rename_history_filter->operation_id_prefix) -
                              1] = '\0';
  }

  char normalize_error[APP_MAX_ERROR] = {0};
  if (!CliRenameParseRollbackFilter(options->rename_history_rollback_filter,
                                    &out_rename_history_filter->rollback_filter,
                                    normalize_error, sizeof(normalize_error))) {
    printf("Error: %s\n", normalize_error);
    *out_exit_code = 1;
    return false;
  }

  if (!CliRenameNormalizeHistoryDateBound(
          options->rename_history_from, false,
          out_rename_history_filter->created_from_utc,
          sizeof(out_rename_history_filter->created_from_utc), normalize_error,
          sizeof(normalize_error))) {
    printf("Error: %s\n", normalize_error);
    *out_exit_code = 1;
    return false;
  }

  if (!CliRenameNormalizeHistoryDateBound(
          options->rename_history_to, true,
          out_rename_history_filter->created_to_utc,
          sizeof(out_rename_history_filter->created_to_utc), normalize_error,
          sizeof(normalize_error))) {
    printf("Error: %s\n", normalize_error);
    *out_exit_code = 1;
    return false;
  }

  if (out_rename_history_filter->created_from_utc[0] != '\0' &&
      out_rename_history_filter->created_to_utc[0] != '\0' &&
      strcmp(out_rename_history_filter->created_from_utc,
             out_rename_history_filter->created_to_utc) > 0) {
    printf("Error: --rename-history-from must be <= --rename-history-to.\n");
    *out_exit_code = 1;
    return false;
  }

  *out_exit_code = 0;
  return true;
}
