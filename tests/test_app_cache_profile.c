#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_cache_profile.h"
#include "app_api.h"
#include "fs_utils.h"
#include "test_framework.h"

static bool WriteTextFile(const char *path, const char *content) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  size_t len = content ? strlen(content) : 0;
  if (len > 0 && fwrite(content, 1, len, f) != len) {
    fclose(f);
    return false;
  }
  fclose(f);
  return true;
}

static bool FileExists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  fclose(f);
  return true;
}

static bool AlwaysCancel(void *user_data) {
  (void)user_data;
  return true;
}

static AppStatus RunScan(AppContext *ctx, const char *target_dir,
                         const char *env_dir, bool exhaustive, bool ml_enrich,
                         bool similarity_report, int jobs,
                         AppCacheCompressionMode compression_mode,
                         int compression_level, const char *models_root_override,
                         AppCancelCallback cancel_cb, AppScanResult *out_result) {
  AppScanRequest request = {
      .target_dir = target_dir,
      .env_dir = env_dir,
      .exhaustive = exhaustive,
      .ml_enrich = ml_enrich,
      .similarity_report = similarity_report,
      .sim_incremental = true,
      .jobs = jobs,
      .cache_compression_mode = compression_mode,
      .cache_compression_level = compression_level,
      .models_root_override = models_root_override,
      .hooks = {.cancel_cb = cancel_cb, .progress_cb = NULL, .user_data = NULL},
  };
  return AppRunScan(ctx, &request, out_result);
}

void test_app_cache_profile_roundtrip(void) {
  system("rm -rf build/test_app_profile_roundtrip");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_profile_roundtrip"));

  const char *path = "build/test_app_profile_roundtrip/profile.json";
  AppCacheProfile profile = {0};
  strncpy(profile.source_root_abs, "/tmp/sourceA",
          sizeof(profile.source_root_abs) - 1);
  profile.exhaustive = true;
  profile.ml_enrich_requested = true;
  profile.similarity_prep_requested = false;
  strncpy(profile.models_fingerprint,
          "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef",
          sizeof(profile.models_fingerprint) - 1);
  profile.has_cache_entry_count = true;
  profile.cache_entry_count = 42;

  ASSERT_TRUE(AppSaveCacheProfile(path, &profile));

  AppCacheProfile loaded = {0};
  ASSERT_EQ(APP_CACHE_PROFILE_LOAD_OK, AppLoadCacheProfile(path, &loaded));
  ASSERT_TRUE(loaded.has_cache_entry_count);
  ASSERT_EQ(42, loaded.cache_entry_count);

  char reason[APP_MAX_ERROR] = {0};
  ASSERT_TRUE(AppCompareCacheProfiles(&profile, &loaded, reason, sizeof(reason)));
  ASSERT_STR_EQ("cache profile matched", reason);

  system("rm -rf build/test_app_profile_roundtrip");
}

void test_app_cache_profile_entry_count_not_part_of_match_semantics(void) {
  AppCacheProfile expected = {0};
  AppCacheProfile actual = {0};

  strncpy(expected.source_root_abs, "/tmp/sourceA",
          sizeof(expected.source_root_abs) - 1);
  expected.exhaustive = true;
  expected.ml_enrich_requested = true;
  expected.similarity_prep_requested = true;
  strncpy(expected.models_fingerprint,
          "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          sizeof(expected.models_fingerprint) - 1);
  expected.has_cache_entry_count = true;
  expected.cache_entry_count = 1;

  actual = expected;
  actual.has_cache_entry_count = true;
  actual.cache_entry_count = 9000;

  char reason[APP_MAX_ERROR] = {0};
  ASSERT_TRUE(AppCompareCacheProfiles(&expected, &actual, reason, sizeof(reason)));
  ASSERT_STR_EQ("cache profile matched", reason);
}

void test_app_cache_profile_optional_entry_count_absent_is_supported(void) {
  system("rm -rf build/test_app_profile_optional_count");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_profile_optional_count"));

  const char *path = "build/test_app_profile_optional_count/profile.json";
  ASSERT_TRUE(WriteTextFile(
      path,
      "{"
      "\"schemaVersion\":1,"
      "\"sourceRootAbs\":\"/tmp/sourceA\","
      "\"exhaustive\":false,"
      "\"mlEnrichRequested\":false,"
      "\"similarityPrepRequested\":false,"
      "\"modelsFingerprint\":\"\","
      "\"updatedAtUtc\":\"2026-02-26T00:00:00Z\""
      "}"));

  AppCacheProfile loaded = {0};
  ASSERT_EQ(APP_CACHE_PROFILE_LOAD_OK, AppLoadCacheProfile(path, &loaded));
  ASSERT_FALSE(loaded.has_cache_entry_count);
  ASSERT_EQ(0, loaded.cache_entry_count);

  system("rm -rf build/test_app_profile_optional_count");
}

void test_app_scan_profile_missing_triggers_rebuild_and_creation(void) {
  system("rm -rf build/test_app_profile_missing_env");

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanResult result = {0};
  AppStatus status =
      RunScan(ctx, "tests/assets/png", "build/test_app_profile_missing_env", false,
              false, false, 1, APP_CACHE_COMPRESSION_NONE, 3, NULL, NULL,
              &result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_FALSE(result.cache_profile_matched);
  ASSERT_TRUE(result.cache_profile_rebuilt);
  ASSERT_STR_EQ("cache profile missing", result.cache_profile_reason);
  ASSERT_TRUE(FileExists(
      "build/test_app_profile_missing_env/.cache/gallery_cache.profile.json"));

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_missing_env");
}

void test_app_scan_profile_malformed_triggers_rebuild(void) {
  system("rm -rf build/test_app_profile_malformed_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_profile_malformed_env/.cache"));
  ASSERT_TRUE(WriteTextFile(
      "build/test_app_profile_malformed_env/.cache/gallery_cache.profile.json",
      "{bad-json"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanResult result = {0};
  AppStatus status = RunScan(ctx, "tests/assets/png",
                             "build/test_app_profile_malformed_env", false, false,
                             false, 1, APP_CACHE_COMPRESSION_NONE, 3, NULL, NULL,
                             &result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_FALSE(result.cache_profile_matched);
  ASSERT_TRUE(result.cache_profile_rebuilt);
  ASSERT_STR_EQ("cache profile malformed or unreadable",
                result.cache_profile_reason);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_malformed_env");
}

void test_app_scan_profile_same_does_not_rebuild(void) {
  system("rm -rf build/test_app_profile_same_env");

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanResult first = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_same_env", false,
                                   false, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                   NULL, NULL, &first));
  ASSERT_TRUE(first.cache_profile_rebuilt);

  AppScanResult second = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_same_env", false,
                                   false, false, 2, APP_CACHE_COMPRESSION_NONE, 8,
                                   NULL, NULL, &second));
  ASSERT_TRUE(second.cache_profile_matched);
  ASSERT_FALSE(second.cache_profile_rebuilt);
  ASSERT_STR_EQ("cache profile matched", second.cache_profile_reason);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_same_env");
}

void test_app_scan_profile_exhaustive_change_triggers_rebuild(void) {
  system("rm -rf build/test_app_profile_exhaustive_env");

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanResult first = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_exhaustive_env", false,
                                   false, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                   NULL, NULL, &first));

  AppScanResult second = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_exhaustive_env", true,
                                   false, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                   NULL, NULL, &second));
  ASSERT_FALSE(second.cache_profile_matched);
  ASSERT_TRUE(second.cache_profile_rebuilt);
  ASSERT_STR_EQ("exhaustive setting changed", second.cache_profile_reason);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_exhaustive_env");
}

void test_app_scan_profile_source_change_triggers_rebuild(void) {
  system("rm -rf build/test_app_profile_source_env");

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanResult first = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_source_env", false,
                                   false, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                   NULL, NULL, &first));

  AppScanResult second = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/jpg",
                                   "build/test_app_profile_source_env", false,
                                   false, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                   NULL, NULL, &second));
  ASSERT_FALSE(second.cache_profile_matched);
  ASSERT_TRUE(second.cache_profile_rebuilt);
  ASSERT_TRUE(strstr(second.cache_profile_reason, "source directory changed") !=
              NULL);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_source_env");
}

void test_app_scan_profile_model_fingerprint_change_triggers_rebuild(void) {
  system("rm -rf build/test_app_profile_models_env build/test_app_profile_models");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_profile_models_env"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_profile_models"));
  ASSERT_TRUE(WriteTextFile("build/test_app_profile_models/clf-default.onnx",
                            "model-a-v1"));
  ASSERT_TRUE(WriteTextFile("build/test_app_profile_models/text-default.onnx",
                            "model-b-v1"));
  ASSERT_TRUE(WriteTextFile("build/test_app_profile_models/embed-default.onnx",
                            "model-c-v1"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanResult first = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_models_env", false,
                                   true, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                   "build/test_app_profile_models", NULL, &first));

  AppScanResult second = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_models_env", false,
                                   true, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                   "build/test_app_profile_models", NULL, &second));
  ASSERT_TRUE(second.cache_profile_matched);
  ASSERT_FALSE(second.cache_profile_rebuilt);

  ASSERT_TRUE(WriteTextFile("build/test_app_profile_models/text-default.onnx",
                            "model-b-v2"));

  AppScanResult third = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_models_env", false,
                                   true, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                   "build/test_app_profile_models", NULL, &third));
  ASSERT_FALSE(third.cache_profile_matched);
  ASSERT_TRUE(third.cache_profile_rebuilt);
  ASSERT_STR_EQ("model fingerprint changed", third.cache_profile_reason);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_models_env build/test_app_profile_models");
}

void test_app_scan_profile_non_semantic_changes_do_not_rebuild(void) {
  system("rm -rf build/test_app_profile_nonsemantic_env");

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanResult first = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_nonsemantic_env", false,
                                   false, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                   NULL, NULL, &first));

  AppScanResult second = {0};
  ASSERT_EQ(APP_STATUS_OK, RunScan(ctx, "tests/assets/png",
                                   "build/test_app_profile_nonsemantic_env", false,
                                   false, false, 4, APP_CACHE_COMPRESSION_NONE, 11,
                                   NULL, NULL, &second));
  ASSERT_TRUE(second.cache_profile_matched);
  ASSERT_FALSE(second.cache_profile_rebuilt);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_nonsemantic_env");
}

void test_app_scan_profile_not_saved_on_failure_or_cancel(void) {
  system("rm -rf build/test_app_profile_fail_env build/test_app_profile_cancel_env");

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanResult failed = {0};
  AppStatus failed_status = RunScan(ctx, "build/does-not-exist",
                                    "build/test_app_profile_fail_env", false,
                                    false, false, 1, APP_CACHE_COMPRESSION_NONE, 3,
                                    NULL, NULL, &failed);
  ASSERT_EQ(APP_STATUS_IO_ERROR, failed_status);
  ASSERT_FALSE(FileExists(
      "build/test_app_profile_fail_env/.cache/gallery_cache.profile.json"));

  AppScanResult cancelled = {0};
  AppStatus cancelled_status =
      RunScan(ctx, "tests/assets/png", "build/test_app_profile_cancel_env", false,
              false, false, 1, APP_CACHE_COMPRESSION_NONE, 3, NULL, AlwaysCancel,
              &cancelled);
  ASSERT_EQ(APP_STATUS_CANCELLED, cancelled_status);
  ASSERT_FALSE(FileExists(
      "build/test_app_profile_cancel_env/.cache/gallery_cache.profile.json"));

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_fail_env build/test_app_profile_cancel_env");
}

void register_app_cache_profile_tests(void) {
  register_test("test_app_cache_profile_roundtrip",
                test_app_cache_profile_roundtrip, "unit");
  register_test("test_app_cache_profile_entry_count_not_part_of_match_semantics",
                test_app_cache_profile_entry_count_not_part_of_match_semantics,
                "unit");
  register_test("test_app_cache_profile_optional_entry_count_absent_is_supported",
                test_app_cache_profile_optional_entry_count_absent_is_supported,
                "unit");
  register_test("test_app_scan_profile_missing_triggers_rebuild_and_creation",
                test_app_scan_profile_missing_triggers_rebuild_and_creation,
                "integration");
  register_test("test_app_scan_profile_malformed_triggers_rebuild",
                test_app_scan_profile_malformed_triggers_rebuild, "integration");
  register_test("test_app_scan_profile_same_does_not_rebuild",
                test_app_scan_profile_same_does_not_rebuild, "integration");
  register_test("test_app_scan_profile_exhaustive_change_triggers_rebuild",
                test_app_scan_profile_exhaustive_change_triggers_rebuild,
                "integration");
  register_test("test_app_scan_profile_source_change_triggers_rebuild",
                test_app_scan_profile_source_change_triggers_rebuild, "integration");
  register_test("test_app_scan_profile_model_fingerprint_change_triggers_rebuild",
                test_app_scan_profile_model_fingerprint_change_triggers_rebuild,
                "integration");
  register_test("test_app_scan_profile_non_semantic_changes_do_not_rebuild",
                test_app_scan_profile_non_semantic_changes_do_not_rebuild,
                "integration");
  register_test("test_app_scan_profile_not_saved_on_failure_or_cancel",
                test_app_scan_profile_not_saved_on_failure_or_cancel,
                "integration");
}
