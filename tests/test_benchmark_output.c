#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_utils.h"
#include "test_framework.h"
#include "integration_test_helpers.h"

static int SetTestEnvVar(const char *key, const char *value) {
#if defined(_WIN32)
  return _putenv_s(key, value);
#else
  return setenv(key, value, 1);
#endif
}

static void ClearTestEnvVar(const char *key) {
#if defined(_WIN32)
  (void)_putenv_s(key, "");
#else
  unsetenv(key);
#endif
}

void test_benchmark_output_jsonl_contract(void) {
#if defined(_WIN32)
  printf("  INFO: skipping benchmark shell integration test on Windows\n");
  return;
#endif
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_bench_output"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_bench_output"));
  ASSERT_EQ(0, SetTestEnvVar("BENCHMARK_DATASET", "tests/assets/png"));
  ASSERT_EQ(0, SetTestEnvVar("BENCHMARK_HISTORY_PATH",
                             "build/test_bench_output/history.jsonl"));
  ASSERT_EQ(
      0, SetTestEnvVar("BENCHMARK_LAST_PATH", "build/test_bench_output/last.json"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "build/tests/bin/benchmark_runner --profile uncompressed --workload "
      "cache_metadata_only 2>&1",
      output, sizeof(output));
  ClearTestEnvVar("BENCHMARK_LAST_PATH");
  ClearTestEnvVar("BENCHMARK_HISTORY_PATH");
  ClearTestEnvVar("BENCHMARK_DATASET");
  ASSERT_EQ(0, code);

  FILE *f = fopen("build/test_bench_output/history.jsonl", "r");
  ASSERT_TRUE(f != NULL);
  char line[8192] = {0};
  ASSERT_TRUE(fgets(line, sizeof(line), f) != NULL);
  fclose(f);

  ASSERT_TRUE(strstr(line, "\"timestampUtc\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"gitCommit\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"profile\":\"uncompressed\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"simMemoryMode\":\"chunked\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"workload\":\"cache_metadata_only\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"runCount\":1") != NULL);
  ASSERT_TRUE(strstr(line, "\"cacheBytes\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"timeMs\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"peakRssBytes\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"success\":true") != NULL);

  f = fopen("build/test_bench_output/last.json", "r");
  ASSERT_TRUE(f != NULL);
  fclose(f);

  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_bench_output"));
}

void test_benchmark_stats_and_comparison_report(void) {
#if defined(_WIN32)
  printf("  INFO: skipping benchmark shell integration test on Windows\n");
  return;
#endif
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_bench_stats"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_bench_stats"));
  ASSERT_EQ(0, SetTestEnvVar("BENCHMARK_DATASET", "tests/assets/png"));
  ASSERT_EQ(0, SetTestEnvVar("BENCHMARK_HISTORY_PATH",
                             "build/test_bench_stats/history.jsonl"));
  ASSERT_EQ(0,
            SetTestEnvVar("BENCHMARK_LAST_PATH",
                          "build/test_bench_stats/last.json"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "build/tests/bin/benchmark_runner --profile uncompressed "
      "--compare-profile uncompressed --comparison-path "
      "build/test_bench_stats/compare.json --workload cache_metadata_only "
      "--runs 2 --warmup-runs 1 2>&1",
      output, sizeof(output));
  ClearTestEnvVar("BENCHMARK_LAST_PATH");
  ClearTestEnvVar("BENCHMARK_HISTORY_PATH");
  ClearTestEnvVar("BENCHMARK_DATASET");
  ASSERT_EQ(0, code);

  FILE *f = fopen("build/test_bench_stats/last.json", "r");
  ASSERT_TRUE(f != NULL);
  char last_buf[8192] = {0};
  size_t n = fread(last_buf, 1, sizeof(last_buf) - 1, f);
  last_buf[n] = '\0';
  fclose(f);
  ASSERT_TRUE(strstr(last_buf, "\"workloads\"") != NULL);
  ASSERT_TRUE(strstr(last_buf, "\"median\"") != NULL);
  ASSERT_TRUE(strstr(last_buf, "\"p95\"") != NULL);
  ASSERT_TRUE(strstr(last_buf, "\"stddev\"") != NULL);

  f = fopen("build/test_bench_stats/compare.json", "r");
  ASSERT_TRUE(f != NULL);
  char compare_buf[8192] = {0};
  n = fread(compare_buf, 1, sizeof(compare_buf) - 1, f);
  compare_buf[n] = '\0';
  fclose(f);
  ASSERT_TRUE(strstr(compare_buf, "\"baselineProfile\"") != NULL);
  ASSERT_TRUE(strstr(compare_buf, "\"candidateProfile\"") != NULL);
  ASSERT_TRUE(strstr(compare_buf, "\"deltaPct\"") != NULL);

  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_bench_stats"));
}

void register_benchmark_output_tests(void) {
  register_test("test_benchmark_output_jsonl_contract",
                test_benchmark_output_jsonl_contract, "benchmark");
  register_test("test_benchmark_stats_and_comparison_report",
                test_benchmark_stats_and_comparison_report, "benchmark");
}
