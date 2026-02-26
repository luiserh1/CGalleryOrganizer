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

void test_similarity_memory_mode_parity(void) {
  ImageMetadata items[4] = {0};
  const char *paths[4] = {"/tmp/a.jpg", "/tmp/b.jpg", "/tmp/c.jpg", "/tmp/d.jpg"};
  const float emb[4][4] = {
      {1.0f, 0.0f, 0.0f, 0.0f},
      {0.95f, 0.1f, 0.0f, 0.0f},
      {0.2f, 0.9f, 0.0f, 0.0f},
      {0.96f, 0.05f, 0.0f, 0.0f},
  };

  for (int i = 0; i < 4; i++) {
    items[i].path = strdup(paths[i]);
    items[i].mlEmbeddingDim = 4;
    strncpy(items[i].mlModelEmbedding, "embed-default",
            sizeof(items[i].mlModelEmbedding) - 1);
    ASSERT_TRUE(
        SimilarityEncodeEmbeddingBase64(emb[i], 4, &items[i].mlEmbeddingBase64));
  }

  SimilarityReport eager = {0};
  SimilarityReport chunked = {0};
  SimilaritySetMemoryMode(SIM_MEMORY_MODE_EAGER);
  ASSERT_TRUE(SimilarityBuildReport("embed-default", 0.8f, 3, items, 4, &eager));
  SimilaritySetMemoryMode(SIM_MEMORY_MODE_CHUNKED);
  ASSERT_TRUE(
      SimilarityBuildReport("embed-default", 0.8f, 3, items, 4, &chunked));

  ASSERT_EQ(eager.groupCount, chunked.groupCount);
  for (int i = 0; i < eager.groupCount; i++) {
    ASSERT_STR_EQ(eager.groups[i].anchorPath, chunked.groups[i].anchorPath);
    ASSERT_EQ(eager.groups[i].neighborCount, chunked.groups[i].neighborCount);
    for (int j = 0; j < eager.groups[i].neighborCount; j++) {
      ASSERT_STR_EQ(eager.groups[i].neighbors[j].path,
                    chunked.groups[i].neighbors[j].path);
      float diff = eager.groups[i].neighbors[j].score -
                   chunked.groups[i].neighbors[j].score;
      if (diff < 0.0f) {
        diff = -diff;
      }
      ASSERT_TRUE(diff < 0.0001f);
    }
  }

  SimilarityFreeReport(&eager);
  SimilarityFreeReport(&chunked);
  for (int i = 0; i < 4; i++) {
    CacheFreeMetadata(&items[i]);
  }
}

void register_similarity_engine_tests(void) {
  register_test("test_similarity_base64_roundtrip", test_similarity_base64_roundtrip,
                "similarity");
  register_test("test_similarity_cosine_math", test_similarity_cosine_math,
                "similarity");
  register_test("test_similarity_report_generation",
                test_similarity_report_generation, "similarity");
  register_test("test_similarity_memory_mode_parity",
                test_similarity_memory_mode_parity, "similarity");
}
