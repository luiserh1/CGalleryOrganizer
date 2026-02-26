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
  ASSERT_TRUE(strstr(line, "\"workload\":\"cache_metadata_only\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"cacheBytes\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"timeMs\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"peakRssBytes\"") != NULL);
  ASSERT_TRUE(strstr(line, "\"success\":true") != NULL);

  f = fopen("build/test_bench_output/last.json", "r");
  ASSERT_TRUE(f != NULL);
  fclose(f);

  system("rm -rf build/test_bench_output");
}

void register_benchmark_output_tests(void) {
  register_test("test_benchmark_output_jsonl_contract",
                test_benchmark_output_jsonl_contract, "benchmark");
}
