#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli/cli_args.h"
#include "cli/cli_commands.h"
#include "cli/cli_scan_pipeline.h"
#include "duplicate_finder.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "ml_api.h"
#include "organizer.h"
#include "similarity_engine.h"

int main(int argc, char **argv) {
  printf("CGalleryOrganizer v0.4.5\n");

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
  SimilarityMemoryMode sim_memory_mode = SIM_MEMORY_MODE_CHUNKED;
  CacheCompressionMode cache_compression_mode = CACHE_COMPRESSION_NONE;
  int cache_compression_level = 3;
  bool cache_compression_level_set = false;
  const char *target_dir = NULL;
  const char *env_dir = NULL;
  const char *group_by_arg = NULL;
  bool cache_initialized = false;
  bool ml_initialized = false;

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
        sim_memory_mode = SIM_MEMORY_MODE_EAGER;
      } else if (strcmp(mode, "chunked") == 0) {
        sim_memory_mode = SIM_MEMORY_MODE_CHUNKED;
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
        cache_compression_mode = CACHE_COMPRESSION_NONE;
      } else if (strcmp(mode, "zstd") == 0) {
        cache_compression_mode = CACHE_COMPRESSION_ZSTD;
      } else if (strcmp(mode, "auto") == 0) {
        cache_compression_mode = CACHE_COMPRESSION_AUTO;
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
      cache_compression_mode != CACHE_COMPRESSION_ZSTD &&
      cache_compression_mode != CACHE_COMPRESSION_AUTO) {
    printf("Error: --cache-compress-level is only valid with --cache-compress "
           "zstd|auto.\n");
    return 1;
  }

  if (rollback) {
    const char *rollback_env = CliResolveRollbackEnvDir(target_dir, env_dir);
    if (!rollback_env) {
      printf("Error: --rollback requires an environment directory.\n");
      CliPrintUsage(argv[0]);
      return 1;
    }

    printf("\n[*] Initiating Rollback from %s...\n", rollback_env);
    int restored = OrganizerRollback(rollback_env);
    if (restored >= 0) {
      printf("[*] Rollback complete. Restored %d files.\n", restored);
      return 0;
    }

    printf("[!] Rollback failed to execute properly.\n");
    return 1;
  }

  if (target_dir == NULL) {
    CliPrintUsage(argv[0]);
    return 1;
  }
  if (similarity_report && !env_dir) {
    printf("Error: --similarity-report requires an environment directory.\n");
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

  char cache_dir[1024];
  char cache_path[1024];
  if (env_dir) {
    snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", env_dir);
    snprintf(cache_path, sizeof(cache_path), "%s/.cache/gallery_cache.json",
             env_dir);
  } else {
    snprintf(cache_dir, sizeof(cache_dir), ".cache");
    snprintf(cache_path, sizeof(cache_path), ".cache/gallery_cache.json");
  }

  FsMakeDirRecursive(cache_dir);
  if (!CacheSetCompression(cache_compression_mode, cache_compression_level)) {
    if (cache_compression_mode == CACHE_COMPRESSION_ZSTD ||
        cache_compression_mode == CACHE_COMPRESSION_AUTO) {
      printf("Error: zstd cache compression is unavailable or invalid.\n");
      printf("Hint: install zstd development package or use "
             "--cache-compress none.\n");
    } else {
      printf("Error: Invalid cache compression configuration.\n");
    }
    return 1;
  }
  if (!CacheInit(cache_path)) {
    printf("Error: Failed to initialize cache.\n");
    return 1;
  }
  cache_initialized = true;

  char models_root[1024] = {0};
  const char *models_override = getenv("CGO_MODELS_ROOT");
  if (models_override && models_override[0] != '\0') {
    strncpy(models_root, models_override, sizeof(models_root) - 1);
  } else {
    strncpy(models_root, "build/models", sizeof(models_root) - 1);
  }

  if (ml_enrich || similarity_report) {
    if (!MlInit(models_root)) {
      printf("Error: Failed to initialize ML runtime from %s.\n", models_root);
      printf("Hint: run scripts/download_models.sh first or set CGO_MODELS_ROOT.\n");
      CacheShutdown();
      return 1;
    }
    ml_initialized = true;
  }

  printf("Scanning directory: %s (Exhaustive: %s)\n", target_dir,
         exhaustive ? "ON" : "OFF");
  printf("Jobs: %d\n", resolved_jobs);

  AppScanContext scan_ctx = {.exhaustive = exhaustive,
                             .ml_enrich = ml_enrich,
                             .similarity_report = similarity_report,
                             .sim_incremental = sim_incremental,
                             .ml_enabled = ml_initialized,
                             .ml_files_evaluated = 0,
                             .ml_files_classified = 0,
                             .ml_files_with_text = 0,
                             .ml_files_embedded = 0,
                             .ml_failures = 0};
  ScanRunStats run_stats = {0};

  if (!CliRunScanPipeline(target_dir, &scan_ctx, resolved_jobs, &run_stats)) {
    printf("Error: Failed to collect directory paths for '%s'.\n", target_dir);
    if (ml_initialized) {
      MlShutdown();
    }
    CacheShutdown();
    return 1;
  }

  if (!CacheSave()) {
    printf("Error: Failed to save cache.\n");
  }

  printf("Scan complete.\n");
  printf("Files scanned: %d\n", run_stats.files_scanned);
  printf("Media files cached: %d\n", run_stats.files_cached);
  if (ml_enrich || similarity_report) {
    printf("ML evaluated: %d\n", scan_ctx.ml_files_evaluated);
    if (ml_enrich) {
      printf("ML classified: %d\n", scan_ctx.ml_files_classified);
      printf("ML with text: %d\n", scan_ctx.ml_files_with_text);
    }
    if (similarity_report) {
      printf("ML embedded: %d\n", scan_ctx.ml_files_embedded);
    }
    printf("ML failures/skips: %d\n", scan_ctx.ml_failures);
  }
  printf("\n");

  if (similarity_report) {
    SimilaritySetMemoryMode(sim_memory_mode);
    char report_path[1024] = {0};
    snprintf(report_path, sizeof(report_path), "%s/similarity_report.json",
             env_dir);
    if (!CliBuildSimilarityReportFromCache(report_path, sim_threshold,
                                           sim_topk)) {
      printf("Error: Failed to generate similarity report at %s.\n",
             report_path);
      if (ml_initialized) {
        MlShutdown();
      }
      CacheShutdown();
      return 1;
    }
    printf("Similarity report generated: %s\n", report_path);
  }

  if (preview || organize) {
    if (!env_dir) {
      printf("Error: --organize and --preview require an environment "
             "directory.\n");
      if (ml_initialized) {
        MlShutdown();
      }
      CacheShutdown();
      return 1;
    }

    const char *group_keys[16] = {0};
    int group_key_count = 0;
    char *group_keys_owned = NULL;
    if (!CliParseGroupByKeys(group_by_arg, group_keys, 16, &group_key_count,
                             &group_keys_owned)) {
      if (ml_initialized) {
        MlShutdown();
      }
      CacheShutdown();
      return 1;
    }

    printf("\n[*] Calculating Gallery Reorganization Plan...\n");

    if (!OrganizerInit(env_dir)) {
      printf("[!] Failed to initialize organizer state.\n");
      free(group_keys_owned);
      if (ml_initialized) {
        MlShutdown();
      }
      CacheShutdown();
      return 1;
    }

    OrganizerPlan *plan =
        OrganizerComputePlan(env_dir, group_keys, group_key_count);
    if (!plan) {
      printf("[!] Failed to compute reorganization plan.\n");
    } else {
      if (preview) {
        OrganizerPrintPlan(plan);
        printf("\n[*] Preview mode: No files were moved.\n");
      } else {
        OrganizerPrintPlan(plan);
        printf("\nExecute plan? [y/N]: ");
        fflush(stdout);
        char buf[16];
        if (fgets(buf, sizeof(buf), stdin)) {
          if (buf[0] == 'y' || buf[0] == 'Y') {
            OrganizerExecutePlan(plan);
          } else {
            printf("[*] Operation cancelled.\n");
          }
        }
      }
      OrganizerFreePlan(plan);
    }

    OrganizerShutdown();
    free(group_keys_owned);
  } else if (!similarity_report) {
    DuplicateReport rep = FindExactDuplicates();
    int moved_count = 0;

    if (rep.group_count > 0) {
      if (env_dir) {
        printf(
            "Found %d groups of exact duplicates. Moving copies to '%s'...\n",
            rep.group_count, env_dir);
        for (int i = 0; i < rep.group_count; i++) {
          for (int j = 0; j < rep.groups[i].duplicate_count; j++) {
            char moved_path[1024] = {0};
            if (FsMoveFile(rep.groups[i].duplicate_paths[j], env_dir,
                           moved_path, sizeof(moved_path))) {
              printf("  Moved: %s -> %s\n", rep.groups[i].duplicate_paths[j],
                     moved_path);
              moved_count++;
            } else {
              printf("  Failed to move: %s\n",
                     rep.groups[i].duplicate_paths[j]);
            }
          }
        }
        printf("Successfully moved %d duplicate files.\n", moved_count);
      } else {
        printf("Found %d groups of exact duplicates (dry-run):\n",
               rep.group_count);
        for (int i = 0; i < rep.group_count; i++) {
          printf("  Group %d (Original: %s):\n", i + 1,
                 rep.groups[i].original_path);
          for (int j = 0; j < rep.groups[i].duplicate_count; j++) {
            printf("    Duplicate: %s\n", rep.groups[i].duplicate_paths[j]);
          }
        }
        printf("\nTo automatically move these duplicates, provide a target "
               "directory as the second argument.\n");
      }
    } else {
      printf("No exact duplicates found.\n");
    }
    FreeDuplicateReport(&rep);
  }

  if (ml_initialized) {
    MlShutdown();
  }

  if (cache_initialized) {
    CacheShutdown();
  }

  return 0;
}
