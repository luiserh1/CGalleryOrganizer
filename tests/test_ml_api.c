#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_utils.h"
#include "ml_api.h"
#include "test_framework.h"

static void WriteFile(const char *path, const char *content) {
  FILE *f = fopen(path, "wb");
  if (!f) {
    return;
  }
  if (content) {
    fwrite(content, 1, strlen(content), f);
  }
  fclose(f);
}

void test_ml_init_missing_models(void) {
  system("rm -rf build/test_models_missing");
  ASSERT_FALSE(MlInit("build/test_models_missing"));
  MlShutdown();
}

void test_ml_infer_success(void) {
  system("rm -rf build/test_models_ok");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_models_ok"));
  WriteFile("build/test_models_ok/clf-default.onnx", "dummy-clf");
  WriteFile("build/test_models_ok/text-default.onnx", "dummy-text");

  ASSERT_TRUE(MlInit("build/test_models_ok"));

  MlFeature features[2] = {ML_FEATURE_CLASSIFICATION,
                           ML_FEATURE_TEXT_DETECTION};
  MlResult out = {0};
  ASSERT_TRUE(MlInferImage("tests/assets/png/sample_no_exif.png", features, 2,
                           &out));

  ASSERT_TRUE(out.has_classification);
  ASSERT_TRUE(out.topk_count > 0);
  ASSERT_TRUE(out.provider_raw_json != NULL);
  ASSERT_TRUE(out.has_text_detection);

  MlFreeResult(&out);
  MlShutdown();
}

void test_ml_infer_invalid_feature_set(void) {
  system("rm -rf build/test_models_invalid");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_models_invalid"));
  WriteFile("build/test_models_invalid/clf-default.onnx", "dummy-clf");
  WriteFile("build/test_models_invalid/text-default.onnx", "dummy-text");

  ASSERT_TRUE(MlInit("build/test_models_invalid"));

  MlResult out = {0};
  ASSERT_FALSE(MlInferImage("tests/assets/png/sample_no_exif.png", NULL, 0,
                            &out));

  MlShutdown();
}

void register_ml_api_tests(void) {
  register_test("test_ml_init_missing_models", test_ml_init_missing_models,
                "ml");
  register_test("test_ml_infer_success", test_ml_infer_success, "ml");
  register_test("test_ml_infer_invalid_feature_set",
                test_ml_infer_invalid_feature_set, "ml");
}
