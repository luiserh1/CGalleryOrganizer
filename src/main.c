#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_api.h"
#include "cli/cli_args.h"

static void PrintAppError(AppContext *app, const char *prefix);

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
    printf("[!] Rollback failed to execute properly.\n");
    return 1;
  }

  printf("[*] Rollback complete. Restored %d files.\n", result.restored_count);
  return 0;
}

static int HandleDuplicates(AppContext *app, const char *env_dir,
                            AppCacheCompressionMode compression_mode,
                            int compression_level) {
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

  if (env_dir && env_dir[0] != '\0') {
    AppDuplicateMoveRequest move_request = {
        .target_dir = env_dir,
        .report = &report,
    };
    AppDuplicateMoveResult move_result = {0};
    status = AppMoveDuplicates(app, &move_request, &move_result);
    if (status != APP_STATUS_OK) {
      PrintAppError(app, "Error: Failed to move duplicates");
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
    printf("\nTo automatically move these duplicates, provide a target "
           "directory as the second argument.\n");
  }

  AppFreeDuplicateReport(&report);
  return 0;
}

static void PrintAppError(AppContext *app, const char *prefix) {
  const char *details = AppGetLastError(app);
  if (details && details[0] != '\0') {
    printf("%s: %s\n", prefix, details);
  } else {
    printf("%s.\n", prefix);
  }
}

int main(int argc, char **argv) {
  printf("CGalleryOrganizer v0.5.1\n");

  bool exhaustive = false;
  bool ml_enrich = false;
  bool similarity_report = false;
  bool sim_incremental = true;
  bool organize = false;
  bool preview = false;
  bool rollback = false;
  const char *jobs_arg = "auto";
  bool jobs_set_by_cli = false;
  float sim_threshold = 0.92f;
  int sim_topk = 5;
  AppSimilarityMemoryMode sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
  AppCacheCompressionMode cache_compression_mode = APP_CACHE_COMPRESSION_NONE;
  int cache_compression_level = 3;
  bool cache_compression_level_set = false;
  const char *target_dir = NULL;
  const char *env_dir = NULL;
  const char *group_by_arg = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      CliPrintUsage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "-e") == 0 ||
               strcmp(argv[i], "--exhaustive") == 0) {
      exhaustive = true;
    } else if (strcmp(argv[i], "--ml-enrich") == 0) {
      ml_enrich = true;
    } else if (strcmp(argv[i], "--similarity-report") == 0) {
      similarity_report = true;
    } else if (strcmp(argv[i], "--sim-incremental") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-incremental requires on|off.\n");
        return 1;
      }
      const char *value = argv[++i];
      if (strcmp(value, "on") == 0) {
        sim_incremental = true;
      } else if (strcmp(value, "off") == 0) {
        sim_incremental = false;
      } else {
        printf("Error: --sim-incremental must be on|off.\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--sim-threshold") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-threshold requires a value in [0,1].\n");
        return 1;
      }
      char *endptr = NULL;
      sim_threshold = strtof(argv[++i], &endptr);
      if (!endptr || *endptr != '\0' || sim_threshold < 0.0f ||
          sim_threshold > 1.0f) {
        printf("Error: --sim-threshold must be a number in [0,1].\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--sim-topk") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-topk requires a positive integer.\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed <= 0 || parsed > 1000) {
        printf("Error: --sim-topk must be a positive integer.\n");
        return 1;
      }
      sim_topk = (int)parsed;
    } else if (strcmp(argv[i], "--jobs") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --jobs requires n|auto.\n");
        return 1;
      }
      jobs_arg = argv[++i];
      jobs_set_by_cli = true;
    } else if (strcmp(argv[i], "--sim-memory-mode") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --sim-memory-mode requires eager|chunked.\n");
        return 1;
      }
      const char *mode = argv[++i];
      if (strcmp(mode, "eager") == 0) {
        sim_memory_mode = APP_SIM_MEMORY_EAGER;
      } else if (strcmp(mode, "chunked") == 0) {
        sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
      } else {
        printf("Error: --sim-memory-mode must be eager|chunked.\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--cache-compress") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --cache-compress requires a mode: none|zstd|auto.\n");
        return 1;
      }
      const char *mode = argv[++i];
      if (strcmp(mode, "none") == 0) {
        cache_compression_mode = APP_CACHE_COMPRESSION_NONE;
      } else if (strcmp(mode, "zstd") == 0) {
        cache_compression_mode = APP_CACHE_COMPRESSION_ZSTD;
      } else if (strcmp(mode, "auto") == 0) {
        cache_compression_mode = APP_CACHE_COMPRESSION_AUTO;
      } else {
        printf(
            "Error: Invalid --cache-compress mode '%s'. Allowed: none|zstd|auto\n",
            mode);
        return 1;
      }
    } else if (strcmp(argv[i], "--cache-compress-level") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --cache-compress-level requires a value in [1,19].\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed < 1 || parsed > 19) {
        printf("Error: --cache-compress-level must be in [1,19].\n");
        return 1;
      }
      cache_compression_level = (int)parsed;
      cache_compression_level_set = true;
    } else if (strcmp(argv[i], "--organize") == 0) {
      organize = true;
    } else if (strcmp(argv[i], "--group-by") == 0) {
      if (i + 1 >= argc) {
        printf("Error: --group-by requires a comma-separated key list.\n");
        return 1;
      }
      group_by_arg = argv[++i];
    } else if (strcmp(argv[i], "--preview") == 0) {
      preview = true;
    } else if (strcmp(argv[i], "--rollback") == 0) {
      rollback = true;
    } else if (argv[i][0] == '-') {
      printf("Error: Unknown option '%s'.\n", argv[i]);
      CliPrintUsage(argv[0]);
      return 1;
    } else if (target_dir == NULL) {
      target_dir = argv[i];
    } else if (env_dir == NULL) {
      env_dir = argv[i];
    } else {
      printf("Error: Unexpected positional argument '%s'.\n", argv[i]);
      CliPrintUsage(argv[0]);
      return 1;
    }
  }

  if (rollback && (organize || preview)) {
    printf("Error: Cannot specify --rollback with --organize or --preview.\n");
    return 1;
  }
  if (rollback && similarity_report) {
    printf("Error: Cannot specify --rollback with --similarity-report.\n");
    return 1;
  }
  if (organize && preview) {
    printf("Error: Cannot specify --organize and --preview together.\n");
    return 1;
  }
  if (similarity_report && (organize || preview)) {
    printf("Error: --similarity-report cannot be combined with --organize/--preview.\n");
    return 1;
  }
  if (cache_compression_level_set &&
      cache_compression_mode != APP_CACHE_COMPRESSION_ZSTD &&
      cache_compression_mode != APP_CACHE_COMPRESSION_AUTO) {
    printf("Error: --cache-compress-level is only valid with --cache-compress "
           "zstd|auto.\n");
    return 1;
  }

  if (!jobs_set_by_cli) {
    const char *jobs_env = getenv("CGO_JOBS");
    if (jobs_env && jobs_env[0] != '\0') {
      jobs_arg = jobs_env;
    }
  }

  bool jobs_valid = false;
  int resolved_jobs = CliResolveJobsFromString(jobs_arg, &jobs_valid);
  if (!jobs_valid) {
    printf("Error: --jobs/CGO_JOBS must be 'auto' or a positive integer.\n");
    return 1;
  }

  AppRuntimeOptions options = {0};
  AppContext *app = AppContextCreate(&options);
  if (!app) {
    printf("Error: Failed to initialize application context.\n");
    return 1;
  }

  if (rollback) {
    int rc = HandleRollback(app, target_dir, env_dir, argv[0]);
    AppContextDestroy(app);
    return rc;
  }

  if (target_dir == NULL) {
    CliPrintUsage(argv[0]);
    AppContextDestroy(app);
    return 1;
  }
  if (similarity_report && !env_dir) {
    printf("Error: --similarity-report requires an environment directory.\n");
    AppContextDestroy(app);
    return 1;
  }

  printf("Scanning directory: %s (Exhaustive: %s)\n", target_dir,
         exhaustive ? "ON" : "OFF");
  printf("Jobs: %d\n", resolved_jobs);

  AppScanRequest scan_request = {
      .target_dir = target_dir,
      .env_dir = env_dir,
      .exhaustive = exhaustive,
      .ml_enrich = ml_enrich,
      .similarity_report = similarity_report,
      .sim_incremental = sim_incremental,
      .jobs = resolved_jobs,
      .cache_compression_mode = cache_compression_mode,
      .cache_compression_level = cache_compression_level,
      .models_root_override = NULL,
      .hooks = {0},
  };

  AppScanResult scan_result = {0};
  AppStatus status = AppRunScan(app, &scan_request, &scan_result);
  if (status != APP_STATUS_OK) {
    if (status == APP_STATUS_ML_ERROR) {
      PrintAppError(app, "Error: Failed to initialize ML runtime");
      printf("Hint: run scripts/download_models.sh first or set CGO_MODELS_ROOT.\n");
    } else {
      PrintAppError(app, "Error: Failed to run scan");
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
  if (ml_enrich || similarity_report) {
    printf("ML evaluated: %d\n", scan_result.ml_files_evaluated);
    if (ml_enrich) {
      printf("ML classified: %d\n", scan_result.ml_files_classified);
      printf("ML with text: %d\n", scan_result.ml_files_with_text);
    }
    if (similarity_report) {
      printf("ML embedded: %d\n", scan_result.ml_files_embedded);
    }
    printf("ML failures/skips: %d\n", scan_result.ml_failures);
  }
  printf("\n");

  if (similarity_report) {
    AppSimilarityRequest request = {
        .env_dir = env_dir,
        .cache_compression_mode = cache_compression_mode,
        .cache_compression_level = cache_compression_level,
        .threshold = sim_threshold,
        .topk = sim_topk,
        .memory_mode = sim_memory_mode,
        .output_path_override = NULL,
        .hooks = {0},
    };
    AppSimilarityResult result = {0};
    status = AppGenerateSimilarityReport(app, &request, &result);
    if (status != APP_STATUS_OK) {
      PrintAppError(app, "Error: Failed to generate similarity report");
      AppContextDestroy(app);
      return 1;
    }
    printf("Similarity report generated: %s\n", result.report_path);
  }

  if (preview || organize) {
    if (!env_dir) {
      printf("Error: --organize and --preview require an environment "
             "directory.\n");
      AppContextDestroy(app);
      return 1;
    }

    AppOrganizePlanRequest preview_request = {
        .env_dir = env_dir,
        .cache_compression_mode = cache_compression_mode,
        .cache_compression_level = cache_compression_level,
        .group_by_arg = group_by_arg,
        .hooks = {0},
    };

    AppOrganizePlanResult preview_result = {0};
    status = AppPreviewOrganize(app, &preview_request, &preview_result);
    if (status != APP_STATUS_OK) {
      PrintAppError(app, "Error");
      AppContextDestroy(app);
      return 1;
    }

    printf("\n[*] Calculating Gallery Reorganization Plan...\n");
    if (preview_result.plan_text) {
      printf("%s", preview_result.plan_text);
    }

    if (preview) {
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
            .env_dir = env_dir,
            .cache_compression_mode = cache_compression_mode,
            .cache_compression_level = cache_compression_level,
            .group_by_arg = group_by_arg,
            .hooks = {0},
        };
        AppOrganizeExecuteResult execute_result = {0};
        status = AppExecuteOrganize(app, &execute_request, &execute_result);
        if (status != APP_STATUS_OK) {
          PrintAppError(app, "Error: Failed to execute organize plan");
          AppFreeOrganizePlanResult(&preview_result);
          AppContextDestroy(app);
          return 1;
        }
      } else {
        printf("[*] Operation cancelled.\n");
      }
    }

    AppFreeOrganizePlanResult(&preview_result);
  } else if (!similarity_report) {
    int rc = HandleDuplicates(app, env_dir, cache_compression_mode,
                              cache_compression_level);
    AppContextDestroy(app);
    return rc;
  }

  AppContextDestroy(app);
  return 0;
}
