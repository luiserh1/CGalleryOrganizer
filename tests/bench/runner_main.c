#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_utils.h"

#include "runner_internal.h"

static void InitRecord(BenchmarkRecord *rec, const char *profile_name,
                       SimilarityMemoryMode sim_memory_mode,
                       const char *dataset_path, int dataset_file_count,
                       int run_index, int run_count, int warmup_runs) {
  memset(rec, 0, sizeof(*rec));
  BenchFillNowUtcIso(rec->timestampUtc);
  BenchFillGitCommit(rec->gitCommit);
  snprintf(rec->profile, sizeof(rec->profile), "%s", profile_name);
  snprintf(rec->simMemoryMode, sizeof(rec->simMemoryMode), "%s",
           sim_memory_mode == SIM_MEMORY_MODE_EAGER ? "eager" : "chunked");
  snprintf(rec->datasetPath, sizeof(rec->datasetPath), "%s", dataset_path);
  rec->datasetFileCount = dataset_file_count;
  rec->cacheBytes = -1;
  rec->similarityReportBytes = -1;
  rec->rssStartBytes = -1;
  rec->rssEndBytes = -1;
  rec->rssDeltaBytes = -1;
  rec->peakRssBytes = -1;
  rec->runIndex = run_index;
  rec->runCount = run_count;
  rec->warmupRuns = warmup_runs;
}

static bool SummarizeWorkload(BenchmarkWorkload workload, const char *profile_name,
                              SimilarityMemoryMode sim_memory_mode,
                              BenchmarkRecord *records, int runs,
                              int warmup_runs, WorkloadSummary *out_summary) {
  if (!records || runs <= 0 || !out_summary) {
    return false;
  }

  memset(out_summary, 0, sizeof(*out_summary));
  out_summary->workload = workload;
  snprintf(out_summary->workloadName, sizeof(out_summary->workloadName), "%s",
           BenchWorkloadToName(workload));
  snprintf(out_summary->profile, sizeof(out_summary->profile), "%s", profile_name);
  snprintf(out_summary->simMemoryMode, sizeof(out_summary->simMemoryMode), "%s",
           sim_memory_mode == SIM_MEMORY_MODE_EAGER ? "eager" : "chunked");
  out_summary->runs = runs;
  out_summary->warmupRuns = warmup_runs;

  for (int i = 0; i < runs; i++) {
    if (records[i].success) {
      out_summary->successCount++;
    } else {
      out_summary->failureCount++;
    }
  }

  if (!BenchBuildMetricStats(records, runs, BenchMetricTime, &out_summary->timeMs) ||
      !BenchBuildMetricStats(records, runs, BenchMetricCache,
                             &out_summary->cacheBytes) ||
      !BenchBuildMetricStats(records, runs, BenchMetricRssDelta,
                             &out_summary->rssDeltaBytes) ||
      !BenchBuildMetricStats(records, runs, BenchMetricPeakRss,
                             &out_summary->peakRssBytes) ||
      !BenchBuildMetricStats(records, runs, BenchMetricSimilarityReport,
                             &out_summary->similarityReportBytes)) {
    return false;
  }

  return true;
}

static int RunSuite(BenchmarkWorkload *workloads, int workload_count,
                    const char *dataset_path, int dataset_file_count,
                    const char *models_root, CacheCompressionMode mode,
                    int compression_level, const char *profile_name,
                    SimilarityMemoryMode sim_memory_mode, int runs,
                    int warmup_runs, const char *history_path,
                    WorkloadSummary *out_summaries) {
  int failures = 0;

  for (int i = 0; i < workload_count; i++) {
    for (int w = 0; w < warmup_runs; w++) {
      BenchmarkRecord warmup = {0};
      InitRecord(&warmup, profile_name, sim_memory_mode, dataset_path,
                 dataset_file_count, 0, runs, warmup_runs);
      BenchRunWorkload(workloads[i], dataset_path, models_root, mode,
                       compression_level, profile_name, sim_memory_mode,
                       &warmup);
    }

    BenchmarkRecord *records = calloc((size_t)runs, sizeof(BenchmarkRecord));
    if (!records) {
      fprintf(stderr, "Error: failed to allocate benchmark records.\n");
      return workload_count;
    }

    int workload_failures = 0;
    for (int r = 0; r < runs; r++) {
      InitRecord(&records[r], profile_name, sim_memory_mode, dataset_path,
                 dataset_file_count, r + 1, runs, warmup_runs);
      bool ok = BenchRunWorkload(workloads[i], dataset_path, models_root, mode,
                                 compression_level, profile_name,
                                 sim_memory_mode, &records[r]);

      if (!BenchAppendBenchmarkRecord(history_path, &records[r])) {
        fprintf(stderr, "Error: failed to append benchmark history.\n");
        free(records);
        return workload_count;
      }

      printf("[%s][sim:%s][run %d/%d] %s => %s | time=%lldms cache=%lldB rssDelta=%lldB peakRss=%lldB\n",
             records[r].profile, records[r].simMemoryMode, r + 1, runs,
             records[r].workload, ok ? "OK" : "FAIL", records[r].timeMs,
             records[r].cacheBytes, records[r].rssDeltaBytes,
             records[r].peakRssBytes);

      if (!ok) {
        workload_failures++;
        printf("  error: %s\n", records[r].error);
      }
    }

    if (!SummarizeWorkload(workloads[i], profile_name, sim_memory_mode, records,
                           runs, warmup_runs, &out_summaries[i])) {
      fprintf(stderr, "Error: failed to summarize workload stats.\n");
      free(records);
      return workload_count;
    }

    if (out_summaries[i].timeMs.valid) {
      printf("  stats: median=%.0fms p95=%.0fms min=%.0fms max=%.0fms stddev=%.2f\n",
             out_summaries[i].timeMs.median, out_summaries[i].timeMs.p95,
             out_summaries[i].timeMs.min, out_summaries[i].timeMs.max,
             out_summaries[i].timeMs.stddev);
    }

    failures += workload_failures;
    free(records);
  }

  return failures;
}

int BenchRunnerMain(int argc, char **argv) {
  const char *profile_arg = "uncompressed";
  const char *workload_arg = "all";
  const char *compare_profile_arg = NULL;
  const char *comparison_path = NULL;
  int runs = 1;
  int warmup_runs = 0;
  SimilarityMemoryMode sim_memory_mode = SIM_MEMORY_MODE_CHUNKED;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--profile") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --profile requires a value.\n");
        return 1;
      }
      profile_arg = argv[++i];
    } else if (strcmp(argv[i], "--workload") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --workload requires a value.\n");
        return 1;
      }
      workload_arg = argv[++i];
    } else if (strcmp(argv[i], "--sim-memory-mode") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --sim-memory-mode requires eager|chunked.\n");
        return 1;
      }
      const char *mode = argv[++i];
      if (strcmp(mode, "eager") == 0) {
        sim_memory_mode = SIM_MEMORY_MODE_EAGER;
      } else if (strcmp(mode, "chunked") == 0) {
        sim_memory_mode = SIM_MEMORY_MODE_CHUNKED;
      } else {
        fprintf(stderr, "Error: --sim-memory-mode must be eager|chunked.\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--runs") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --runs requires a positive integer.\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed <= 0 || parsed > 1000) {
        fprintf(stderr, "Error: --runs must be in [1,1000].\n");
        return 1;
      }
      runs = (int)parsed;
    } else if (strcmp(argv[i], "--warmup-runs") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr,
                "Error: --warmup-runs requires a non-negative integer.\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed < 0 || parsed > 1000) {
        fprintf(stderr, "Error: --warmup-runs must be in [0,1000].\n");
        return 1;
      }
      warmup_runs = (int)parsed;
    } else if (strcmp(argv[i], "--compare-profile") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --compare-profile requires a value.\n");
        return 1;
      }
      compare_profile_arg = argv[++i];
    } else if (strcmp(argv[i], "--comparison-path") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --comparison-path requires a value.\n");
        return 1;
      }
      comparison_path = argv[++i];
    } else {
      fprintf(stderr, "Error: unknown option '%s'.\n", argv[i]);
      return 1;
    }
  }

  CacheCompressionMode compression_mode = CACHE_COMPRESSION_NONE;
  int compression_level = 3;
  char profile_name[32] = {0};
  char parse_error[128] = {0};
  if (!BenchParseProfile(profile_arg, &compression_mode, &compression_level,
                         profile_name, sizeof(profile_name), parse_error,
                         sizeof(parse_error))) {
    fprintf(stderr, "Error: %s\n", parse_error);
    return 1;
  }

  CacheCompressionMode compare_mode = CACHE_COMPRESSION_NONE;
  int compare_level = 3;
  char compare_profile_name[32] = {0};
  if (compare_profile_arg &&
      !BenchParseProfile(compare_profile_arg, &compare_mode, &compare_level,
                         compare_profile_name, sizeof(compare_profile_name),
                         parse_error, sizeof(parse_error))) {
    fprintf(stderr, "Error: %s\n", parse_error);
    return 1;
  }

  const char *dataset_path = getenv("BENCHMARK_DATASET");
  if (!dataset_path || dataset_path[0] == '\0') {
    dataset_path = "build/stress_data";
  }

  const char *models_root = getenv("BENCHMARK_MODELS_ROOT");
  if (!models_root || models_root[0] == '\0') {
    models_root = "build/models";
  }

  const char *history_path = getenv("BENCHMARK_HISTORY_PATH");
  if (!history_path || history_path[0] == '\0') {
    history_path = "build/benchmark_history.jsonl";
  }

  const char *last_path = getenv("BENCHMARK_LAST_PATH");
  if (!last_path || last_path[0] == '\0') {
    last_path = "build/benchmark_last.json";
  }

  if (!comparison_path || comparison_path[0] == '\0') {
    comparison_path = "build/benchmark_compare.json";
  }

  if (!FsMakeDirRecursive("build")) {
    fprintf(stderr, "Error: failed to initialize build directory.\n");
    return 1;
  }

  int dataset_file_count = BenchCountDatasetFiles(dataset_path);
  if (dataset_file_count < 0) {
    fprintf(stderr, "Error: failed to scan dataset path '%s'.\n", dataset_path);
    return 1;
  }

  BenchmarkWorkload selected_workload = WORKLOAD_CACHE_METADATA_ONLY;
  int run_all = 1;
  if (!BenchParseWorkload(workload_arg, &selected_workload, &run_all)) {
    fprintf(stderr, "Error: invalid workload '%s'.\n", workload_arg);
    return 1;
  }

  BenchmarkWorkload workloads[3] = {WORKLOAD_CACHE_METADATA_ONLY,
                                    WORKLOAD_CACHE_FULL_WITH_EMBEDDINGS,
                                    WORKLOAD_SIMILARITY_SEARCH};
  int workload_count = run_all ? 3 : 1;
  if (!run_all) {
    workloads[0] = selected_workload;
  }

  WorkloadSummary baseline[3];
  memset(baseline, 0, sizeof(baseline));

  int failures = RunSuite(workloads, workload_count, dataset_path,
                          dataset_file_count, models_root, compression_mode,
                          compression_level, profile_name, sim_memory_mode,
                          runs, warmup_runs, history_path, baseline);

  if (!BenchWriteLastSummary(last_path, baseline, workload_count)) {
    fprintf(stderr, "Error: failed to write benchmark last summary.\n");
    return 1;
  }

  if (compare_profile_arg) {
    WorkloadSummary candidate[3];
    memset(candidate, 0, sizeof(candidate));

    int compare_failures =
        RunSuite(workloads, workload_count, dataset_path, dataset_file_count,
                 models_root, compare_mode, compare_level, compare_profile_name,
                 sim_memory_mode, runs, warmup_runs, history_path, candidate);
    failures += compare_failures;

    if (!BenchWriteComparisonReport(comparison_path, profile_name,
                                    compare_profile_name, baseline, candidate,
                                    workload_count)) {
      fprintf(stderr, "Error: failed to write comparison report.\n");
      return 1;
    }
  }

  return (failures == 0) ? 0 : 1;
}
