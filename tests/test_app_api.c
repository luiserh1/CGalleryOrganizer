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

void register_app_api_tests(void) {
  register_test("test_app_status_string_basic", test_app_status_string_basic,
                "unit");
  register_test("test_app_scan_invalid_arguments",
                test_app_scan_invalid_arguments, "unit");
  register_test("test_app_scan_cancelled", test_app_scan_cancelled, "unit");
  register_test("test_app_similarity_requires_env",
                test_app_similarity_requires_env, "unit");
}
