#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ml_api.h"
#include "ml_provider.h"

static const MlProviderVTable *g_provider = NULL;
static bool g_ml_initialized = false;

bool MlInit(const char *models_root) {
  if (g_ml_initialized) {
    return true;
  }

  MlProviderConfig cfg = {.models_root = models_root};

  g_provider = MlGetOnnxProvider();
  if (!g_provider || !g_provider->init) {
    return false;
  }

  if (!g_provider->init(&cfg)) {
    g_provider = NULL;
    return false;
  }

  g_ml_initialized = true;
  return true;
}

void MlShutdown(void) {
  if (!g_ml_initialized) {
    return;
  }

  if (g_provider && g_provider->shutdown) {
    g_provider->shutdown();
  }

  g_provider = NULL;
  g_ml_initialized = false;
}

static bool HasRequestedFeature(MlFeature *features, int feature_count,
                                MlFeature feature) {
  if (!features || feature_count <= 0) {
    return false;
  }

  for (int i = 0; i < feature_count; i++) {
    if (features[i] == feature) {
      return true;
    }
  }

  return false;
}

bool MlInferImage(const char *filepath, MlFeature *features, int feature_count,
                  MlResult *out) {
  if (!g_ml_initialized || !g_provider || !g_provider->infer || !filepath ||
      !out) {
    return false;
  }

  if (!HasRequestedFeature(features, feature_count,
                           ML_FEATURE_CLASSIFICATION) &&
      !HasRequestedFeature(features, feature_count,
                           ML_FEATURE_TEXT_DETECTION)) {
    return false;
  }

  memset(out, 0, sizeof(MlResult));
  if (!g_provider->infer(filepath, features, feature_count, out)) {
    return false;
  }

  if (!MlBuildProviderRawJson(out, &out->provider_raw_json)) {
    out->provider_raw_json = NULL;
  }

  return true;
}

void MlFreeResult(MlResult *result) {
  if (!result) {
    return;
  }

  if (result->text_boxes) {
    free(result->text_boxes);
    result->text_boxes = NULL;
  }

  if (result->provider_raw_json) {
    free(result->provider_raw_json);
    result->provider_raw_json = NULL;
  }

  memset(result, 0, sizeof(MlResult));
}
