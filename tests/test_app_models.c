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

void test_app_install_models_invalid_arguments(void) {
  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppModelInstallRequest req = {0};
  AppModelInstallResult result = {0};

  ASSERT_EQ(APP_STATUS_INVALID_ARGUMENT, AppInstallModels(NULL, &req, &result));
  ASSERT_EQ(APP_STATUS_INVALID_ARGUMENT, AppInstallModels(ctx, NULL, &result));
  ASSERT_EQ(APP_STATUS_INVALID_ARGUMENT, AppInstallModels(ctx, &req, NULL));

  AppContextDestroy(ctx);
}

void test_app_install_models_data_url_and_lockfile(void) {
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_app_models_ok"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_models_ok"));

  const char *manifest_json =
      "{"
      "\"models\":[{"
      "\"id\":\"clf-default\","
      "\"task\":\"classification\","
      "\"url\":\"data:application/octet-stream;base64,YWJj\","
      "\"sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\","
      "\"license_name\":\"CC0-1.0\","
      "\"license_url\":\"https://creativecommons.org/publicdomain/zero/1.0/\","
      "\"author\":\"Tester\","
      "\"source_url\":\"https://example.com\","
      "\"credit_text\":\"fixture\","
      "\"version\":\"1\","
      "\"filename\":\"clf-default.onnx\""
      "}]"
      "}";
  ASSERT_TRUE(WriteTextFile("build/test_app_models_ok/manifest.json", manifest_json));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppModelInstallRequest req = {
      .manifest_path_override = "build/test_app_models_ok/manifest.json",
      .models_root_override = "build/test_app_models_ok/models",
      .force_redownload = false,
  };
  AppModelInstallResult result = {0};
  AppStatus status = AppInstallModels(ctx, &req, &result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_EQ(1, result.manifest_model_count);
  ASSERT_EQ(1, result.installed_count);
  ASSERT_EQ(0, result.skipped_count);

  FILE *model_file = fopen("build/test_app_models_ok/models/clf-default.onnx", "rb");
  ASSERT_TRUE(model_file != NULL);
  fclose(model_file);
  FILE *lockfile = fopen("build/test_app_models_ok/models/.installed.json", "rb");
  ASSERT_TRUE(lockfile != NULL);
  fclose(lockfile);

  AppModelInstallResult second = {0};
  status = AppInstallModels(ctx, &req, &second);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_EQ(0, second.installed_count);
  ASSERT_EQ(1, second.skipped_count);

  AppContextDestroy(ctx);
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_app_models_ok"));
}

void test_app_install_models_bad_checksum_fails(void) {
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_app_models_badsha"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_models_badsha"));

  const char *manifest_json =
      "{"
      "\"models\":[{"
      "\"id\":\"clf-default\","
      "\"task\":\"classification\","
      "\"url\":\"data:application/octet-stream;base64,YWJj\","
      "\"sha256\":\"0000000000000000000000000000000000000000000000000000000000000000\","
      "\"license_name\":\"CC0-1.0\","
      "\"license_url\":\"https://creativecommons.org/publicdomain/zero/1.0/\","
      "\"author\":\"Tester\","
      "\"source_url\":\"https://example.com\","
      "\"credit_text\":\"fixture\","
      "\"version\":\"1\","
      "\"filename\":\"clf-default.onnx\""
      "}]"
      "}";
  ASSERT_TRUE(
      WriteTextFile("build/test_app_models_badsha/manifest.json", manifest_json));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppModelInstallRequest req = {
      .manifest_path_override = "build/test_app_models_badsha/manifest.json",
      .models_root_override = "build/test_app_models_badsha/models",
      .force_redownload = true,
  };
  AppModelInstallResult result = {0};
  AppStatus status = AppInstallModels(ctx, &req, &result);
  ASSERT_EQ(APP_STATUS_IO_ERROR, status);
  const char *err = AppGetLastError(ctx);
  ASSERT_TRUE(err != NULL);
  ASSERT_TRUE(strstr(err, "SHA256 mismatch") != NULL);

  AppContextDestroy(ctx);
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_app_models_badsha"));
}

void register_app_models_tests(void) {
  register_test("test_app_install_models_invalid_arguments",
                test_app_install_models_invalid_arguments, "unit");
  register_test("test_app_install_models_data_url_and_lockfile",
                test_app_install_models_data_url_and_lockfile, "unit");
  register_test("test_app_install_models_bad_checksum_fails",
                test_app_install_models_bad_checksum_fails, "unit");
}
