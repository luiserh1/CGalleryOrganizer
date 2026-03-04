#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/cli_args.h"
#include "cli/cli_main_options_internal.h"
#include "cli/cli_parse_utils.h"

bool CliMainParseArguments(CliMainOptions *options, int argc, char **argv,
                           int *out_exit_code) {
  if (!options || !argv || argc < 1 || !out_exit_code) {
    return false;
  }

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
      if (!CliParseCacheCompressionModeValue(mode,
                                             &options->cache_compression_mode)) {
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

  return true;
}
