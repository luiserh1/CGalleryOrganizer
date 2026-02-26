#include <stdlib.h>
#include <string.h>

#include "app_api.h"
#include "fs_utils.h"
#include "test_framework.h"

static bool AlwaysCancel(void *user_data) {
  (void)user_data;
  return true;
}

void test_app_status_string_basic(void) {
  ASSERT_STR_EQ("ok", AppStatusToString(APP_STATUS_OK));
  ASSERT_STR_EQ("invalid_argument",
                AppStatusToString(APP_STATUS_INVALID_ARGUMENT));
}

void test_app_scan_invalid_arguments(void) {
  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanRequest req = {
      .target_dir = "tests/assets/png",
      .env_dir = "build/test_app_invalid_env",
      .jobs = 0,
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
  };
  AppScanResult res = {0};

  AppStatus status = AppRunScan(ctx, &req, &res);
  ASSERT_EQ(APP_STATUS_INVALID_ARGUMENT, status);

  AppContextDestroy(ctx);
}

void test_app_scan_cancelled(void) {
  system("rm -rf build/test_app_cancel_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_cancel_env"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanRequest req = {
      .target_dir = "tests/assets/png",
      .env_dir = "build/test_app_cancel_env",
      .jobs = 1,
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
      .hooks = {.cancel_cb = AlwaysCancel, .progress_cb = NULL, .user_data = NULL},
  };
  AppScanResult res = {0};

  AppStatus status = AppRunScan(ctx, &req, &res);
  ASSERT_EQ(APP_STATUS_CANCELLED, status);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_cancel_env");
}

void test_app_similarity_requires_env(void) {
  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppSimilarityRequest req = {
      .env_dir = NULL,
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
      .threshold = 0.9f,
      .topk = 5,
      .memory_mode = APP_SIM_MEMORY_CHUNKED,
  };
  AppSimilarityResult res = {0};

  AppStatus status = AppGenerateSimilarityReport(ctx, &req, &res);
  ASSERT_EQ(APP_STATUS_INVALID_ARGUMENT, status);

  AppContextDestroy(ctx);
}

void test_app_inspect_scan_profile_missing_sidecar(void) {
  system("rm -rf build/test_app_profile_missing_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_profile_missing_env"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanRequest req = {
      .target_dir = "tests/assets/png",
      .env_dir = "build/test_app_profile_missing_env",
      .exhaustive = false,
      .ml_enrich = false,
      .similarity_report = false,
      .sim_incremental = true,
      .jobs = 1,
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
  };
  AppScanProfileDecision decision = {0};

  AppStatus status = AppInspectScanProfile(ctx, &req, &decision);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_FALSE(decision.profile_present);
  ASSERT_FALSE(decision.profile_match);
  ASSERT_TRUE(decision.will_rebuild_cache);
  ASSERT_TRUE(strstr(decision.reason, "missing") != NULL);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_missing_env");
}

void test_app_inspect_scan_profile_ignores_nonsemantic_settings(void) {
  system("rm -rf build/test_app_profile_nonsemantic_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_profile_nonsemantic_env"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanRequest scan_req = {
      .target_dir = "tests/assets/png",
      .env_dir = "build/test_app_profile_nonsemantic_env",
      .exhaustive = false,
      .ml_enrich = false,
      .similarity_report = false,
      .sim_incremental = true,
      .jobs = 1,
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
  };
  AppScanResult scan_result = {0};
  AppStatus status = AppRunScan(ctx, &scan_req, &scan_result);
  ASSERT_EQ(APP_STATUS_OK, status);

  AppScanRequest inspect_req = scan_req;
  inspect_req.jobs = 8;
  inspect_req.cache_compression_mode = APP_CACHE_COMPRESSION_ZSTD;
  inspect_req.cache_compression_level = 19;

  AppScanProfileDecision decision = {0};
  status = AppInspectScanProfile(ctx, &inspect_req, &decision);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(decision.profile_present);
  ASSERT_TRUE(decision.profile_match);
  ASSERT_FALSE(decision.will_rebuild_cache);
  ASSERT_TRUE(strstr(decision.reason, "matched") != NULL);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_profile_nonsemantic_env");
}

void register_app_api_tests(void) {
  register_test("test_app_status_string_basic", test_app_status_string_basic,
                "unit");
  register_test("test_app_scan_invalid_arguments",
                test_app_scan_invalid_arguments, "unit");
  register_test("test_app_scan_cancelled", test_app_scan_cancelled, "unit");
  register_test("test_app_similarity_requires_env",
                test_app_similarity_requires_env, "unit");
  register_test("test_app_inspect_scan_profile_missing_sidecar",
                test_app_inspect_scan_profile_missing_sidecar, "unit");
  register_test("test_app_inspect_scan_profile_ignores_nonsemantic_settings",
                test_app_inspect_scan_profile_ignores_nonsemantic_settings,
                "unit");
}
