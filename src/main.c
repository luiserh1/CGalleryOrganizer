#include <stdio.h>
#include <stdlib.h>

#include "app_api.h"
#include "cli/cli_app_error.h"
#include "cli/cli_args.h"
#include "cli/cli_main_options.h"
#include "cli/cli_rename_commands.h"

static int HandleRollback(AppContext *app, const char *target_dir,
                          const char *env_dir, const char *argv0) {
  const char *rollback_env = CliResolveRollbackEnvDir(target_dir, env_dir);
  if (!rollback_env) {
    printf("Error: --rollback requires an environment directory.\n");
    CliPrintUsage(argv0);
    return 1;
  }

  printf("\n[*] Initiating Rollback from %s...\n", rollback_env);
  AppRollbackRequest request = {.env_dir = rollback_env};
  AppRollbackResult result = {0};
  AppStatus status = AppRollback(app, &request, &result);
  if (status != APP_STATUS_OK) {
    CliPrintAppError(app, "[!] Rollback failed to execute properly");
    return 1;
  }

  printf("[*] Rollback complete. Restored %d files.\n", result.restored_count);
  return 0;
}

static int HandleDuplicates(AppContext *app, const char *env_dir,
                            AppCacheCompressionMode compression_mode,
                            int compression_level, bool move_duplicates) {
  AppDuplicateFindRequest find_request = {
      .env_dir = env_dir,
      .cache_compression_mode = compression_mode,
      .cache_compression_level = compression_level,
  };
  AppDuplicateReport report = {0};
  AppStatus status = AppFindDuplicates(app, &find_request, &report);
  if (status != APP_STATUS_OK) {
    printf("Error: Failed to find duplicates.\n");
    return 1;
  }

  if (report.group_count <= 0) {
    printf("No exact duplicates found.\n");
    AppFreeDuplicateReport(&report);
    return 0;
  }

  if (move_duplicates) {
    if (!env_dir || env_dir[0] == '\0') {
      printf("Error: --duplicates-move requires an environment directory.\n");
      AppFreeDuplicateReport(&report);
      return 1;
    }

    AppDuplicateMoveRequest move_request = {
        .target_dir = env_dir,
        .report = &report,
    };
    AppDuplicateMoveResult move_result = {0};
    status = AppMoveDuplicates(app, &move_request, &move_result);
    if (status != APP_STATUS_OK) {
      CliPrintAppError(app, "Error: Failed to move duplicates");
      AppFreeDuplicateReport(&report);
      return 1;
    }

    printf("Found %d groups of exact duplicates. Moving copies to '%s'...\n",
           report.group_count, env_dir);
    printf("Successfully moved %d duplicate files.\n", move_result.moved_count);
    if (move_result.failed_count > 0) {
      printf("Failed to move %d duplicate files.\n", move_result.failed_count);
    }
  } else {
    printf("Found %d groups of exact duplicates (dry-run):\n", report.group_count);
    for (int i = 0; i < report.group_count; i++) {
      printf("  Group %d (Original: %s):\n", i + 1,
             report.groups[i].original_path);
      for (int j = 0; j < report.groups[i].duplicate_count; j++) {
        printf("    Duplicate: %s\n", report.groups[i].duplicate_paths[j]);
      }
    }
    printf("\nTo move duplicates, rerun with --duplicates-move.\n");
  }

  AppFreeDuplicateReport(&report);
  return 0;
}

int main(int argc, char **argv) {
  printf("CGalleryOrganizer v0.6.10\n");

  CliMainOptions options;
  CliRenameHistoryFilter rename_history_filter;
  int parse_exit_code = 0;
  if (!CliParseMainOptions(&options, &rename_history_filter, argc, argv,
                           &parse_exit_code)) {
    return parse_exit_code;
  }

  AppRuntimeOptions app_options = {0};
  AppContext *app = AppContextCreate(&app_options);
  if (!app) {
    printf("Error: Failed to initialize application context.\n");
    return 1;
  }

  if (CliMainHasRenameAction(&options)) {
    int rc =
        CliRunRenameActions(app, &options, &rename_history_filter, argv[0]);
    AppContextDestroy(app);
    return rc;
  }

  if (options.rollback) {
    int rc = HandleRollback(app, options.target_dir, options.env_dir, argv[0]);
    AppContextDestroy(app);
    return rc;
  }

  if (!options.target_dir) {
    CliPrintUsage(argv[0]);
    AppContextDestroy(app);
    return 1;
  }

  if (!options.jobs_set_by_cli) {
    const char *jobs_env = getenv("CGO_JOBS");
    if (jobs_env && jobs_env[0] != '\0') {
      options.jobs_arg = jobs_env;
    }
  }

  bool jobs_valid = false;
  int resolved_jobs = CliResolveJobsFromString(options.jobs_arg, &jobs_valid);
  if (!jobs_valid) {
    printf("Error: --jobs/CGO_JOBS must be 'auto' or a positive integer.\n");
    AppContextDestroy(app);
    return 1;
  }

  if (options.similarity_report && !options.env_dir) {
    printf("Error: --similarity-report requires an environment directory.\n");
    AppContextDestroy(app);
    return 1;
  }
  if (options.duplicates_move && !options.env_dir) {
    printf("Error: --duplicates-move requires an environment directory.\n");
    AppContextDestroy(app);
    return 1;
  }

  printf("Scanning directory: %s (Exhaustive: %s)\n", options.target_dir,
         options.exhaustive ? "ON" : "OFF");
  printf("Jobs: %d\n", resolved_jobs);

  AppScanRequest scan_request = {
      .target_dir = options.target_dir,
      .env_dir = options.env_dir,
      .exhaustive = options.exhaustive,
      .ml_enrich = options.ml_enrich,
      .similarity_report = options.similarity_report,
      .sim_incremental = options.sim_incremental,
      .jobs = resolved_jobs,
      .cache_compression_mode = options.cache_compression_mode,
      .cache_compression_level = options.cache_compression_level,
      .models_root_override = NULL,
      .hooks = {0},
  };

  AppScanResult scan_result = {0};
  AppStatus status = AppRunScan(app, &scan_request, &scan_result);
  if (status != APP_STATUS_OK) {
    if (status == APP_STATUS_ML_ERROR) {
      CliPrintAppError(app, "Error: Failed to initialize ML runtime");
      printf("Hint: run scripts/download_models.sh first or set CGO_MODELS_ROOT.\n");
    } else {
      CliPrintAppError(app, "Error: Failed to run scan");
    }
    AppContextDestroy(app);
    return 1;
  }

  printf("Scan complete.\n");
  printf("Files scanned: %d\n", scan_result.files_scanned);
  printf("Media files cached: %d\n", scan_result.files_cached);
  if (scan_result.cache_profile_matched) {
    printf("Cache profile: matched\n");
  } else if (scan_result.cache_profile_rebuilt) {
    printf("Cache profile: rebuilt (%s)\n",
           scan_result.cache_profile_reason[0] != '\0'
               ? scan_result.cache_profile_reason
               : "mismatch");
  }
  if (options.ml_enrich || options.similarity_report) {
    printf("ML evaluated: %d\n", scan_result.ml_files_evaluated);
    if (options.ml_enrich) {
      printf("ML classified: %d\n", scan_result.ml_files_classified);
      printf("ML with text: %d\n", scan_result.ml_files_with_text);
    }
    if (options.similarity_report) {
      printf("ML embedded: %d\n", scan_result.ml_files_embedded);
    }
    printf("ML failures/skips: %d\n", scan_result.ml_failures);
  }
  printf("\n");

  if (options.similarity_report) {
    AppSimilarityRequest request = {
        .env_dir = options.env_dir,
        .cache_compression_mode = options.cache_compression_mode,
        .cache_compression_level = options.cache_compression_level,
        .threshold = options.sim_threshold,
        .topk = options.sim_topk,
        .memory_mode = options.sim_memory_mode,
        .output_path_override = NULL,
        .hooks = {0},
    };
    AppSimilarityResult result = {0};
    status = AppGenerateSimilarityReport(app, &request, &result);
    if (status != APP_STATUS_OK) {
      CliPrintAppError(app, "Error: Failed to generate similarity report");
      AppContextDestroy(app);
      return 1;
    }
    printf("Similarity report generated: %s\n", result.report_path);
  }

  if (options.preview || options.organize) {
    if (!options.env_dir) {
      printf("Error: --organize and --preview require an environment directory.\n");
      AppContextDestroy(app);
      return 1;
    }

    AppOrganizePlanRequest preview_request = {
        .env_dir = options.env_dir,
        .cache_compression_mode = options.cache_compression_mode,
        .cache_compression_level = options.cache_compression_level,
        .group_by_arg = options.group_by_arg,
        .hooks = {0},
    };

    AppOrganizePlanResult preview_result = {0};
    status = AppPreviewOrganize(app, &preview_request, &preview_result);
    if (status != APP_STATUS_OK) {
      CliPrintAppError(app, "Error");
      AppContextDestroy(app);
      return 1;
    }

    printf("\n[*] Calculating Gallery Reorganization Plan...\n");
    if (preview_result.plan_text) {
      printf("%s", preview_result.plan_text);
    }

    if (options.preview) {
      printf("\n[*] Preview mode: No files were moved.\n");
      AppFreeOrganizePlanResult(&preview_result);
      AppContextDestroy(app);
      return 0;
    }

    printf("\nExecute plan? [y/N]: ");
    fflush(stdout);
    char buf[16];
    if (fgets(buf, sizeof(buf), stdin)) {
      if (buf[0] == 'y' || buf[0] == 'Y') {
        AppOrganizeExecuteRequest execute_request = {
            .env_dir = options.env_dir,
            .cache_compression_mode = options.cache_compression_mode,
            .cache_compression_level = options.cache_compression_level,
            .group_by_arg = options.group_by_arg,
            .hooks = {0},
        };
        AppOrganizeExecuteResult execute_result = {0};
        status = AppExecuteOrganize(app, &execute_request, &execute_result);
        if (status != APP_STATUS_OK) {
          CliPrintAppError(app, "Error: Failed to execute organize plan");
          AppFreeOrganizePlanResult(&preview_result);
          AppContextDestroy(app);
          return 1;
        }
      } else {
        printf("[*] Operation cancelled.\n");
      }
    }

    AppFreeOrganizePlanResult(&preview_result);
  } else if (options.duplicates_report || options.duplicates_move) {
    int rc = HandleDuplicates(app, options.env_dir, options.cache_compression_mode,
                              options.cache_compression_level,
                              options.duplicates_move);
    AppContextDestroy(app);
    return rc;
  }

  AppContextDestroy(app);
  return 0;
}
