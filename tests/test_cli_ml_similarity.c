#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/cache_codec.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "test_framework.h"

#include "integration_test_helpers.h"

void test_cli_ml_enrich_missing_models(void) {
  system("rm -rf build/test_cli_ml_missing_src build/test_cli_ml_missing_env "
         "build/test_cli_ml_missing_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_ml_missing_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_ml_missing_env"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_ml_missing_src/a.png"));

  char output[4096];
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_ml_missing_models "
      "./build/bin/gallery_organizer build/test_cli_ml_missing_src "
      "build/test_cli_ml_missing_env --ml-enrich 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "Failed to initialize ML runtime") != NULL);

  system("rm -rf build/test_cli_ml_missing_src build/test_cli_ml_missing_env "
         "build/test_cli_ml_missing_models");
}

void test_cli_ml_enrich_updates_cache(void) {
  system("rm -rf build/test_cli_ml_src build/test_cli_ml_env build/test_cli_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_ml_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_ml_env"));
  ASSERT_TRUE(WriteBootstrapModels("build/test_cli_models"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_ml_src/a.png"));

  char output[4096];
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_models "
      "./build/bin/gallery_organizer build/test_cli_ml_src "
      "build/test_cli_ml_env --ml-enrich 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "ML evaluated:") != NULL);

  FILE *f = fopen("build/test_cli_ml_env/.cache/gallery_cache.json", "r");
  ASSERT_TRUE(f != NULL);
  char cache_buf[8192] = {0};
  size_t bytes = fread(cache_buf, 1, sizeof(cache_buf) - 1, f);
  cache_buf[bytes] = '\0';
  fclose(f);
  ASSERT_TRUE(strstr(cache_buf, "\"mlPrimaryClass\"") != NULL);
  ASSERT_TRUE(strstr(cache_buf, "\"mlModelClassification\"") != NULL);

  system("rm -rf build/test_cli_ml_src build/test_cli_ml_env build/test_cli_models");
}

void test_cli_similarity_report_generates_json(void) {
  system("rm -rf build/test_cli_sim_src build/test_cli_sim_env build/test_cli_sim_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_env"));
  ASSERT_TRUE(WriteBootstrapModels("build/test_cli_sim_models"));
  ASSERT_EQ(0,
            system("cp tests/assets/png/sample_no_exif.png build/test_cli_sim_src/a.png"));
  ASSERT_EQ(0,
            system("cp tests/assets/jpg/sample_exif.jpg build/test_cli_sim_src/b.jpg"));

  char output[4096];
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_models "
      "./build/bin/gallery_organizer build/test_cli_sim_src "
      "build/test_cli_sim_env --similarity-report --sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Similarity report generated:") != NULL);

  FILE *f = fopen("build/test_cli_sim_env/similarity_report.json", "r");
  ASSERT_TRUE(f != NULL);
  char report_buf[8192] = {0};
  size_t bytes = fread(report_buf, 1, sizeof(report_buf) - 1, f);
  report_buf[bytes] = '\0';
  fclose(f);
  ASSERT_TRUE(strstr(report_buf, "\"groupCount\"") != NULL);
  ASSERT_TRUE(strstr(report_buf, "\"groups\"") != NULL);

  system("rm -rf build/test_cli_sim_src build/test_cli_sim_env build/test_cli_sim_models");
}

void test_cli_similarity_report_requires_env(void) {
  char output[2048];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png --similarity-report 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "requires an environment directory") != NULL);
}

void test_cli_jobs_similarity_report_parity(void) {
  system("rm -rf build/test_cli_jobs_parity_src build/test_cli_jobs_parity_env "
         "build/test_cli_jobs_parity_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_jobs_parity_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_jobs_parity_env"));
  ASSERT_TRUE(WriteBootstrapModels("build/test_cli_jobs_parity_models"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_jobs_parity_src/a.png"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_exif.jpg "
                      "build/test_cli_jobs_parity_src/b.jpg"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_no_exif.jpg "
                      "build/test_cli_jobs_parity_src/c.jpg"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_jobs_parity_models "
      "./build/bin/gallery_organizer build/test_cli_jobs_parity_src "
      "build/test_cli_jobs_parity_env --similarity-report --jobs 1 "
      "--sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_EQ(0,
            system("cp build/test_cli_jobs_parity_env/similarity_report.json "
                   "build/test_cli_jobs_parity_env/sim_jobs1_report.json"));

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_jobs_parity_models "
      "./build/bin/gallery_organizer build/test_cli_jobs_parity_src "
      "build/test_cli_jobs_parity_env --similarity-report --jobs 2 "
      "--sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  ASSERT_TRUE(ReportsEquivalentIgnoringTimestamp(
      "build/test_cli_jobs_parity_env/sim_jobs1_report.json",
      "build/test_cli_jobs_parity_env/similarity_report.json"));

  system("rm -rf build/test_cli_jobs_parity_src build/test_cli_jobs_parity_env "
         "build/test_cli_jobs_parity_models");
}

void test_cli_similarity_memory_mode_parity(void) {
  system("rm -rf build/test_cli_sim_mode_src build/test_cli_sim_mode_env "
         "build/test_cli_sim_mode_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_mode_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_mode_env"));
  ASSERT_TRUE(WriteBootstrapModels("build/test_cli_sim_mode_models"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_sim_mode_src/a.png"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_exif.jpg "
                      "build/test_cli_sim_mode_src/b.jpg"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_no_exif.jpg "
                      "build/test_cli_sim_mode_src/c.jpg"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_mode_models "
      "./build/bin/gallery_organizer build/test_cli_sim_mode_src "
      "build/test_cli_sim_mode_env --similarity-report --sim-memory-mode eager "
      "--sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_EQ(0, system("cp build/test_cli_sim_mode_env/similarity_report.json "
                      "build/test_cli_sim_mode_env/eager_report.json"));

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_mode_models "
      "./build/bin/gallery_organizer build/test_cli_sim_mode_src "
      "build/test_cli_sim_mode_env --similarity-report --sim-memory-mode chunked "
      "--sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  ASSERT_TRUE(ReportsEquivalentIgnoringTimestamp(
      "build/test_cli_sim_mode_env/eager_report.json",
      "build/test_cli_sim_mode_env/similarity_report.json"));

  system("rm -rf build/test_cli_sim_mode_src build/test_cli_sim_mode_env "
         "build/test_cli_sim_mode_models");
}

void test_cli_cache_compress_flag_validation(void) {
  char output[2048];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png "
      "build/test_cli_cache_mode --cache-compress bogus 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "Invalid --cache-compress mode") != NULL);

  code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png "
      "build/test_cli_cache_mode --cache-compress-level 3 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "only valid with --cache-compress zstd|auto") !=
              NULL);
}

void test_cli_cache_compress_zstd_roundtrip_or_skip(void) {
  if (!CacheCodecIsAvailable(CACHE_COMPRESSION_ZSTD)) {
    return;
  }

  system("rm -rf build/test_cli_cache_zstd_src build/test_cli_cache_zstd_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_cache_zstd_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_cache_zstd_env"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_cache_zstd_src/a.png"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_cache_zstd_src "
      "build/test_cli_cache_zstd_env --cache-compress zstd "
      "--cache-compress-level 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  FILE *f = fopen("build/test_cli_cache_zstd_env/.cache/gallery_cache.json", "rb");
  ASSERT_TRUE(f != NULL);
  char magic[9] = {0};
  ASSERT_TRUE(fread(magic, 1, 8, f) == 8);
  fclose(f);
  ASSERT_TRUE(memcmp(magic, CACHE_CODEC_MAGIC, 8) == 0);

  code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_cache_zstd_src "
      "build/test_cli_cache_zstd_env 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  system("rm -rf build/test_cli_cache_zstd_src build/test_cli_cache_zstd_env");
}

void test_cli_similarity_incremental_toggle_behavior(void) {
  system("rm -rf build/test_cli_sim_inc_src build/test_cli_sim_inc_env "
         "build/test_cli_sim_inc_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_inc_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_sim_inc_env"));
  ASSERT_TRUE(WriteBootstrapModels("build/test_cli_sim_inc_models"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_sim_inc_src/a.png"));
  ASSERT_EQ(0, system("cp tests/assets/jpg/sample_exif.jpg "
                      "build/test_cli_sim_inc_src/b.jpg"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_inc_models "
      "./build/bin/gallery_organizer build/test_cli_sim_inc_src "
      "build/test_cli_sim_inc_env --similarity-report --sim-threshold 0.1 "
      "--sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "ML evaluated: 2") != NULL);

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_inc_models "
      "./build/bin/gallery_organizer build/test_cli_sim_inc_src "
      "build/test_cli_sim_inc_env --similarity-report --sim-threshold 0.1 "
      "--sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "ML evaluated: 0") != NULL);

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_MODELS_ROOT=build/test_cli_sim_inc_models "
      "./build/bin/gallery_organizer build/test_cli_sim_inc_src "
      "build/test_cli_sim_inc_env --similarity-report --sim-incremental off "
      "--sim-threshold 0.1 --sim-topk 3 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "ML evaluated: 2") != NULL);

  system("rm -rf build/test_cli_sim_inc_src build/test_cli_sim_inc_env "
         "build/test_cli_sim_inc_models");
}

void test_cli_cache_compress_auto_threshold_behavior(void) {
  if (!CacheCodecIsAvailable(CACHE_COMPRESSION_ZSTD)) {
    return;
  }

  system("rm -rf build/test_cli_cache_auto_src build/test_cli_cache_auto_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_cache_auto_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_cache_auto_env"));
  ASSERT_EQ(0, system("cp tests/assets/png/sample_no_exif.png "
                      "build/test_cli_cache_auto_src/a.png"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "CGO_CACHE_AUTO_THRESHOLD_BYTES=999999999 "
      "./build/bin/gallery_organizer build/test_cli_cache_auto_src "
      "build/test_cli_cache_auto_env --cache-compress auto 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  FILE *f = fopen("build/test_cli_cache_auto_env/.cache/gallery_cache.json", "rb");
  ASSERT_TRUE(f != NULL);
  char head[8] = {0};
  ASSERT_TRUE(fread(head, 1, sizeof(head), f) == sizeof(head));
  fclose(f);
  ASSERT_TRUE(memcmp(head, CACHE_CODEC_MAGIC, sizeof(head)) != 0);

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_CACHE_AUTO_THRESHOLD_BYTES=1 "
      "./build/bin/gallery_organizer build/test_cli_cache_auto_src "
      "build/test_cli_cache_auto_env --cache-compress auto 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);

  f = fopen("build/test_cli_cache_auto_env/.cache/gallery_cache.json", "rb");
  ASSERT_TRUE(f != NULL);
  char magic[9] = {0};
  ASSERT_TRUE(fread(magic, 1, 8, f) == 8);
  fclose(f);
  ASSERT_TRUE(memcmp(magic, CACHE_CODEC_MAGIC, 8) == 0);

  system("rm -rf build/test_cli_cache_auto_src build/test_cli_cache_auto_env");
}

void register_cli_ml_similarity_tests(void) {
  register_test("test_cli_ml_enrich_missing_models",
                test_cli_ml_enrich_missing_models, "integration");
  register_test("test_cli_ml_enrich_updates_cache",
                test_cli_ml_enrich_updates_cache, "integration");
  register_test("test_cli_similarity_report_generates_json",
                test_cli_similarity_report_generates_json, "integration");
  register_test("test_cli_similarity_report_requires_env",
                test_cli_similarity_report_requires_env, "integration");
  register_test("test_cli_jobs_similarity_report_parity",
                test_cli_jobs_similarity_report_parity, "integration");
  register_test("test_cli_similarity_memory_mode_parity",
                test_cli_similarity_memory_mode_parity, "integration");
  register_test("test_cli_cache_compress_flag_validation",
                test_cli_cache_compress_flag_validation, "integration");
  register_test("test_cli_cache_compress_zstd_roundtrip_or_skip",
                test_cli_cache_compress_zstd_roundtrip_or_skip,
                "integration");
  register_test("test_cli_similarity_incremental_toggle_behavior",
                test_cli_similarity_incremental_toggle_behavior,
                "integration");
  register_test("test_cli_cache_compress_auto_threshold_behavior",
                test_cli_cache_compress_auto_threshold_behavior,
                "integration");
}
