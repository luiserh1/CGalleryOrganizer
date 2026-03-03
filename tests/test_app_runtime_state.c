#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_api.h"
#include "fs_utils.h"
#include "test_framework.h"
#include "integration_test_helpers.h"

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

static AppStatus RunScan(AppContext *ctx, const char *target_dir,
                         const char *env_dir, AppScanResult *out_result) {
  AppScanRequest request = {
      .target_dir = target_dir,
      .env_dir = env_dir,
      .exhaustive = false,
      .ml_enrich = false,
      .similarity_report = false,
      .sim_incremental = true,
      .jobs = 1,
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
      .models_root_override = NULL,
      .hooks = {0},
  };
  return AppRunScan(ctx, &request, out_result);
}

void test_app_runtime_state_invalid_arguments(void) {
  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppRuntimeState state = {0};
  AppRuntimeStateRequest request = {0};

  ASSERT_EQ(APP_STATUS_INVALID_ARGUMENT,
            AppInspectRuntimeState(NULL, &request, &state));
  ASSERT_EQ(APP_STATUS_INVALID_ARGUMENT,
            AppInspectRuntimeState(ctx, NULL, &state));
  ASSERT_EQ(APP_STATUS_INVALID_ARGUMENT,
            AppInspectRuntimeState(ctx, &request, NULL));

  AppContextDestroy(ctx);
}

void test_app_runtime_state_detects_cache_manifest_and_models(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_app_state_env", "build/test_app_state_models"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_state_env/.cache"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_state_models"));

  ASSERT_TRUE(WriteTextFile("build/test_app_state_env/.cache/gallery_cache.json",
                            "{\"/tmp/a\": {\"path\":\"/tmp/a\"}}"));
  ASSERT_TRUE(WriteTextFile("build/test_app_state_env/manifest.json", "{}"));
  ASSERT_TRUE(WriteTextFile("build/test_app_state_models/clf-default.onnx",
                            "dummy-clf"));
  ASSERT_TRUE(WriteTextFile("build/test_app_state_models/text-default.onnx",
                            "dummy-text"));
  ASSERT_TRUE(WriteTextFile("build/test_app_state_models/embed-default.onnx",
                            "dummy-embed"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppRuntimeStateRequest request = {
      .env_dir = "build/test_app_state_env",
      .models_root_override = "build/test_app_state_models",
  };
  AppRuntimeState state = {0};
  AppStatus status = AppInspectRuntimeState(ctx, &request, &state);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(state.cache_exists);
  ASSERT_FALSE(state.cache_entry_count_known);
  ASSERT_EQ(0, state.cache_entry_count);
  ASSERT_TRUE(state.rollback_manifest_exists);
  ASSERT_TRUE(state.models.classification_present);
  ASSERT_TRUE(state.models.text_detection_present);
  ASSERT_TRUE(state.models.embedding_present);
  ASSERT_EQ(0, state.models.missing_count);
  ASSERT_TRUE(state.logical_cores >= 1);
  ASSERT_TRUE(state.recommended_jobs >= 1);
  ASSERT_TRUE(state.recommended_jobs <= 8);
  ASSERT_STR_EQ("build/test_app_state_models", state.models_root);

  AppContextDestroy(ctx);
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_app_state_env", "build/test_app_state_models"},
      2));
}

void test_app_runtime_state_reports_missing_models(void) {
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_app_state_missing_models"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_state_missing_models"));
  ASSERT_TRUE(WriteTextFile("build/test_app_state_missing_models/clf-default.onnx",
                            "dummy-clf"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppRuntimeStateRequest request = {
      .env_dir = NULL,
      .models_root_override = "build/test_app_state_missing_models",
  };
  AppRuntimeState state = {0};
  AppStatus status = AppInspectRuntimeState(ctx, &request, &state);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(state.models.classification_present);
  ASSERT_FALSE(state.models.text_detection_present);
  ASSERT_FALSE(state.models.embedding_present);
  ASSERT_EQ(2, state.models.missing_count);
  ASSERT_STR_EQ("text-default", state.models.missing_ids[0]);
  ASSERT_STR_EQ("embed-default", state.models.missing_ids[1]);

  AppContextDestroy(ctx);
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_app_state_missing_models"));
}

void test_app_runtime_state_uses_in_memory_count_for_active_cache(void) {
#if defined(_WIN32)
  printf("  INFO: skipping active-cache runtime-state integration test on Windows\n");
  return;
#endif
  ASSERT_TRUE(
      RemovePathRecursiveForTest("build/test_app_state_active_cache_env"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_state_active_cache_env"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanResult scan_result = {0};
  ASSERT_EQ(APP_STATUS_OK,
            RunScan(ctx, "tests/assets/png", "build/test_app_state_active_cache_env",
                    &scan_result));
  ASSERT_TRUE(scan_result.files_cached > 0);

  AppRuntimeStateRequest request = {
      .env_dir = "build/test_app_state_active_cache_env",
      .models_root_override = NULL,
  };
  AppRuntimeState state = {0};
  AppStatus status = AppInspectRuntimeState(ctx, &request, &state);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(state.cache_exists);
  ASSERT_TRUE(state.cache_entry_count_known);
  ASSERT_EQ(scan_result.files_cached, state.cache_entry_count);

  AppContextDestroy(ctx);
  ASSERT_TRUE(
      RemovePathRecursiveForTest("build/test_app_state_active_cache_env"));
}

void test_app_runtime_state_malformed_cache_does_not_fail_when_count_unknown(void) {
  ASSERT_TRUE(
      RemovePathRecursiveForTest("build/test_app_state_malformed_cache_env"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_state_malformed_cache_env/.cache"));
  ASSERT_TRUE(WriteTextFile(
      "build/test_app_state_malformed_cache_env/.cache/gallery_cache.json",
      "{bad-json"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppRuntimeStateRequest request = {
      .env_dir = "build/test_app_state_malformed_cache_env",
      .models_root_override = NULL,
  };
  AppRuntimeState state = {0};
  AppStatus status = AppInspectRuntimeState(ctx, &request, &state);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(state.cache_exists);
  ASSERT_FALSE(state.cache_entry_count_known);
  ASSERT_EQ(0, state.cache_entry_count);

  AppContextDestroy(ctx);
  ASSERT_TRUE(
      RemovePathRecursiveForTest("build/test_app_state_malformed_cache_env"));
}

void register_app_runtime_state_tests(void) {
  register_test("test_app_runtime_state_invalid_arguments",
                test_app_runtime_state_invalid_arguments, "unit");
  register_test("test_app_runtime_state_detects_cache_manifest_and_models",
                test_app_runtime_state_detects_cache_manifest_and_models, "unit");
  register_test("test_app_runtime_state_reports_missing_models",
                test_app_runtime_state_reports_missing_models, "unit");
  register_test("test_app_runtime_state_uses_in_memory_count_for_active_cache",
                test_app_runtime_state_uses_in_memory_count_for_active_cache,
                "integration");
  register_test("test_app_runtime_state_malformed_cache_does_not_fail_when_count_unknown",
                test_app_runtime_state_malformed_cache_does_not_fail_when_count_unknown,
                "unit");
}
