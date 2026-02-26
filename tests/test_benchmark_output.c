#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#include "fs_utils.h"
#include "test_framework.h"

static int RunCommandCapture(const char *cmd, char *output, size_t output_size) {
  if (!cmd || !output || output_size == 0) {
    return -1;
  }

  output[0] = '\0';
  FILE *pipe = popen(cmd, "r");
  if (!pipe) {
    return -1;
  }

  size_t used = 0;
  while (!feof(pipe) && used < output_size - 1) {
    size_t n = fread(output + used, 1, output_size - 1 - used, pipe);
    if (n == 0) {
      break;
    }
    used += n;
  }
  output[used] = '\0';

  int status = pclose(pipe);
  if (status == -1) {
    return -1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return -1;
}

void test_benchmark_output_jsonl_contract(void) {
  system("rm -rf build/test_bench_output");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_bench_output"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "BENCHMARK_DATASET=tests/assets/png "
      "BENCHMARK_HISTORY_PATH=build/test_bench_output/history.jsonl "
      "BENCHMARK_LAST_PATH=build/test_bench_output/last.json "
      "./build/tests/bin/benchmark_runner --profile uncompressed --workload "
      "cache_metadata_only 2>&1",
      output, sizeof(output));
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

  system("rm -rf build/test_bench_output");
}

void test_benchmark_stats_and_comparison_report(void) {
  system("rm -rf build/test_bench_stats");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_bench_stats"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "BENCHMARK_DATASET=tests/assets/png "
      "BENCHMARK_HISTORY_PATH=build/test_bench_stats/history.jsonl "
      "BENCHMARK_LAST_PATH=build/test_bench_stats/last.json "
      "./build/tests/bin/benchmark_runner --profile uncompressed "
      "--compare-profile uncompressed --comparison-path "
      "build/test_bench_stats/compare.json --workload cache_metadata_only "
      "--runs 2 --warmup-runs 1 2>&1",
      output, sizeof(output));
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

  system("rm -rf build/test_bench_stats");
}

void register_benchmark_output_tests(void) {
  register_test("test_benchmark_output_jsonl_contract",
                test_benchmark_output_jsonl_contract, "benchmark");
  register_test("test_benchmark_stats_and_comparison_report",
                test_benchmark_stats_and_comparison_report, "benchmark");
}
