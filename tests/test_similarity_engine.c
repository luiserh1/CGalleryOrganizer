#include <stdlib.h>
#include <string.h>

#include "gallery_cache.h"
#include "similarity_engine.h"
#include "test_framework.h"

void test_similarity_base64_roundtrip(void) {
  float input[4] = {0.1f, 0.2f, 0.3f, 0.4f};

  char *encoded = NULL;
  ASSERT_TRUE(SimilarityEncodeEmbeddingBase64(input, 4, &encoded));
  ASSERT_TRUE(encoded != NULL);

  float *decoded = NULL;
  int dim = 0;
  ASSERT_TRUE(SimilarityDecodeEmbeddingBase64(encoded, &decoded, &dim));
  ASSERT_EQ(4, dim);

  for (int i = 0; i < dim; i++) {
    float diff = decoded[i] - input[i];
    if (diff < 0.0f) {
      diff = -diff;
    }
    ASSERT_TRUE(diff < 0.0001f);
  }

  free(decoded);
  free(encoded);
}

void test_similarity_cosine_math(void) {
  const float a[3] = {1.0f, 0.0f, 0.0f};
  const float b[3] = {1.0f, 0.0f, 0.0f};
  const float c[3] = {0.0f, 1.0f, 0.0f};

  float score = 0.0f;
  ASSERT_TRUE(SimilarityCosine(a, b, 3, &score));
  ASSERT_TRUE(score > 0.999f);

  ASSERT_TRUE(SimilarityCosine(a, c, 3, &score));
  ASSERT_TRUE(score < 0.001f);
}

void test_similarity_report_generation(void) {
  ImageMetadata items[2] = {0};

  items[0].path = strdup("/tmp/a.jpg");
  items[0].mlEmbeddingDim = 4;
  strncpy(items[0].mlModelEmbedding, "embed-default",
          sizeof(items[0].mlModelEmbedding) - 1);
  items[1].path = strdup("/tmp/b.jpg");
  items[1].mlEmbeddingDim = 4;
  strncpy(items[1].mlModelEmbedding, "embed-default",
          sizeof(items[1].mlModelEmbedding) - 1);

  const float e0[4] = {1.0f, 0.0f, 0.0f, 0.0f};
  const float e1[4] = {0.99f, 0.0f, 0.0f, 0.0f};
  ASSERT_TRUE(
      SimilarityEncodeEmbeddingBase64(e0, 4, &items[0].mlEmbeddingBase64));
  ASSERT_TRUE(
      SimilarityEncodeEmbeddingBase64(e1, 4, &items[1].mlEmbeddingBase64));

  SimilarityReport report = {0};
  ASSERT_TRUE(SimilarityBuildReport("embed-default", 0.9f, 5, items, 2,
                                    &report));
  ASSERT_TRUE(report.groupCount >= 1);

  SimilarityFreeReport(&report);
  CacheFreeMetadata(&items[0]);
  CacheFreeMetadata(&items[1]);
}

void register_similarity_engine_tests(void) {
  register_test("test_similarity_base64_roundtrip", test_similarity_base64_roundtrip,
                "similarity");
  register_test("test_similarity_cosine_math", test_similarity_cosine_math,
                "similarity");
  register_test("test_similarity_report_generation",
                test_similarity_report_generation, "similarity");
}
