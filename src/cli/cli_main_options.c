#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/cli_args.h"
#include "cli/cli_main_options.h"
#include "cli/cli_parse_utils.h"

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

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      CliPrintUsage(argv[0]);
      *out_exit_code = 0;
      return false;
    } else if (strcmp(argv[i], "-e") == 0 ||
               strcmp(argv[i], "--exhaustive") == 0) {
      options->exhaustive = true;
    } else if (strcmp(argv[i], "--ml-enrich") == 0) {
      options->ml_enrich = true;
    } else if (strcmp(argv[i], "--similarity-report") == 0) {
      options->similarity_report = true;
    } else if (strcmp(argv[i], "--sim-incremental") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-incremental requires on|off.\n");
        *out_exit_code = 1;
        return false;
      }
      const char *value = argv[++i];
      if (!CliParseSimIncrementalValue(value, &options->sim_incremental)) {
        printf("Error: --sim-incremental must be on|off.\n");
        *out_exit_code = 1;
        return false;
      }
    } else if (strcmp(argv[i], "--sim-threshold") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-threshold requires a value in [0,1].\n");
        *out_exit_code = 1;
        return false;
      }
      char *endptr = NULL;
      options->sim_threshold = strtof(argv[++i], &endptr);
      if (!endptr || *endptr != '\0' || options->sim_threshold < 0.0f ||
          options->sim_threshold > 1.0f) {
        printf("Error: --sim-threshold must be a number in [0,1].\n");
        *out_exit_code = 1;
        return false;
      }
    } else if (strcmp(argv[i], "--sim-topk") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-topk requires a positive integer.\n");
        *out_exit_code = 1;
        return false;
      }
      long parsed = 0;
      if (!CliParseLongInRange(argv[++i], 1, 1000, &parsed)) {
        printf("Error: --sim-topk must be a positive integer.\n");
        *out_exit_code = 1;
        return false;
      }
      options->sim_topk = (int)parsed;
    } else if (strcmp(argv[i], "--jobs") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --jobs requires n|auto.\n");
        *out_exit_code = 1;
        return false;
      }
      options->jobs_arg = argv[++i];
      options->jobs_set_by_cli = true;
    } else if (strcmp(argv[i], "--sim-memory-mode") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-memory-mode requires eager|chunked.\n");
        *out_exit_code = 1;
        return false;
      }
      const char *mode = argv[++i];
      if (!CliParseSimilarityMemoryModeValue(mode, &options->sim_memory_mode)) {
        printf("Error: --sim-memory-mode must be eager|chunked.\n");
        *out_exit_code = 1;
        return false;
      }
    } else if (strcmp(argv[i], "--cache-compress") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --cache-compress requires a mode: none|zstd|auto.\n");
        *out_exit_code = 1;
        return false;
      }
      const char *mode = argv[++i];
      if (!CliParseCacheCompressionModeValue(mode, &options->cache_compression_mode)) {
        printf("Error: Invalid --cache-compress mode '%s'. Allowed: "
               "none|zstd|auto\n",
               mode);
        *out_exit_code = 1;
        return false;
      }
    } else if (strcmp(argv[i], "--cache-compress-level") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --cache-compress-level requires a value in [1,19].\n");
        *out_exit_code = 1;
        return false;
      }
      long parsed = 0;
      if (!CliParseLongInRange(argv[++i], 1, 19, &parsed)) {
        printf("Error: --cache-compress-level must be in [1,19].\n");
        *out_exit_code = 1;
        return false;
      }
      options->cache_compression_level = (int)parsed;
      options->cache_compression_level_set = true;
    } else if (strcmp(argv[i], "--organize") == 0) {
      options->organize = true;
    } else if (strcmp(argv[i], "--group-by") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --group-by requires a comma-separated key list.\n");
        *out_exit_code = 1;
        return false;
      }
      options->group_by_arg = argv[++i];
    } else if (strcmp(argv[i], "--preview") == 0) {
      options->preview = true;
    } else if (strcmp(argv[i], "--duplicates-report") == 0) {
      options->duplicates_report = true;
    } else if (strcmp(argv[i], "--duplicates-move") == 0) {
      options->duplicates_move = true;
    } else if (strcmp(argv[i], "--rollback") == 0) {
      options->rollback = true;
    } else if (strcmp(argv[i], "--rename-preview") == 0) {
      options->rename_preview = true;
    } else if (strcmp(argv[i], "--rename-apply") == 0) {
      options->rename_apply = true;
    } else if (strcmp(argv[i], "--rename-apply-latest") == 0) {
      options->rename_apply_latest = true;
    } else if (strcmp(argv[i], "--rename-preview-latest-id") == 0) {
      options->rename_preview_latest_id = true;
    } else if (strcmp(argv[i], "--rename-init") == 0) {
      options->rename_init = true;
    } else if (strcmp(argv[i], "--rename-bootstrap-tags-from-filename") == 0) {
      options->rename_bootstrap_tags_from_filename = true;
    } else if (strcmp(argv[i], "--rename-pattern") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-pattern requires a pattern value.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_pattern = argv[++i];
    } else if (strcmp(argv[i], "--rename-tags-map") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-tags-map requires a JSON path.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_tags_map_path = argv[++i];
    } else if (strcmp(argv[i], "--rename-tag-add") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-tag-add requires CSV tags.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_tag_add_csv = argv[++i];
    } else if (strcmp(argv[i], "--rename-tag-remove") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-tag-remove requires CSV tags.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_tag_remove_csv = argv[++i];
    } else if (strcmp(argv[i], "--rename-meta-tag-add") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-meta-tag-add requires CSV tags.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_meta_tag_add_csv = argv[++i];
    } else if (strcmp(argv[i], "--rename-meta-tag-remove") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-meta-tag-remove requires CSV tags.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_meta_tag_remove_csv = argv[++i];
    } else if (strcmp(argv[i], "--rename-meta-fields") == 0) {
      options->rename_metadata_fields = true;
    } else if (strcmp(argv[i], "--rename-preview-json") == 0) {
      options->rename_preview_json = true;
    } else if (strcmp(argv[i], "--rename-preview-json-out") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-preview-json-out requires an output path.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_preview_json_out = argv[++i];
    } else if (strcmp(argv[i], "--rename-from-preview") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-from-preview requires a preview id.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_from_preview = argv[++i];
    } else if (strcmp(argv[i], "--rename-accept-auto-suffix") == 0) {
      options->rename_accept_auto_suffix = true;
    } else if (strcmp(argv[i], "--rename-history") == 0) {
      options->rename_history = true;
    } else if (strcmp(argv[i], "--rename-history-export") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-export requires an output JSON path.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_history_export = true;
      options->rename_history_export_path = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-id-prefix") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-id-prefix requires a prefix value.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_history_id_prefix = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-rollback") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-rollback requires any|yes|no.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_history_rollback_filter = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-from") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-from requires YYYY-MM-DD or "
               "YYYY-MM-DDTHH:MM:SSZ.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_history_from = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-to") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-to requires YYYY-MM-DD or "
               "YYYY-MM-DDTHH:MM:SSZ.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_history_to = argv[++i];
    } else if (strcmp(argv[i], "--rename-history-prune") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-prune requires a non-negative keep "
               "count.\n");
        *out_exit_code = 1;
        return false;
      }
      long parsed = 0;
      if (!CliParseLongInRange(argv[++i], 0, 1000000, &parsed)) {
        printf("Error: --rename-history-prune keep count must be a non-negative "
               "integer.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_history_prune = true;
      options->rename_history_prune_keep_count = (int)parsed;
    } else if (strcmp(argv[i], "--rename-history-latest-id") == 0) {
      options->rename_history_latest_id = true;
    } else if (strcmp(argv[i], "--rename-history-detail") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-history-detail requires an operation id.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_history_detail_operation = argv[++i];
    } else if (strcmp(argv[i], "--rename-redo") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-redo requires an operation id.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_redo_operation = argv[++i];
    } else if (strcmp(argv[i], "--rename-rollback") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-rollback requires an operation id.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_rollback_operation = argv[++i];
    } else if (strcmp(argv[i], "--rename-rollback-preflight") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --rename-rollback-preflight requires an operation id.\n");
        *out_exit_code = 1;
        return false;
      }
      options->rename_rollback_preflight_operation = argv[++i];
    } else if (argv[i][0] == '-') {
      printf("Error: Unknown option '%s'.\n", argv[i]);
      CliPrintUsage(argv[0]);
      *out_exit_code = 1;
      return false;
    } else if (options->target_dir == NULL) {
      options->target_dir = argv[i];
    } else if (options->env_dir == NULL) {
      options->env_dir = argv[i];
    } else {
      printf("Error: Unexpected positional argument '%s'.\n", argv[i]);
      CliPrintUsage(argv[0]);
      *out_exit_code = 1;
      return false;
    }
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
