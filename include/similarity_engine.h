#ifndef SIMILARITY_ENGINE_H
#define SIMILARITY_ENGINE_H

#include <stdbool.h>

#include "gallery_cache.h"

typedef struct {
  char *path;
  float score;
} SimilarityNeighbor;

typedef struct {
  char *anchorPath;
  int neighborCount;
  SimilarityNeighbor *neighbors;
} SimilarityGroup;

typedef struct {
  char generatedAt[64];
  char modelId[64];
  float threshold;
  int topK;
  int groupCount;
  SimilarityGroup *groups;
} SimilarityReport;

bool SimilarityEncodeEmbeddingBase64(const float *embedding, int dim,
                                     char **out_base64);
bool SimilarityDecodeEmbeddingBase64(const char *base64, float **out_embedding,
                                     int *out_dim);

bool SimilarityCosine(const float *a, const float *b, int dim, float *out_score);

bool SimilarityBuildReport(const char *model_id, float threshold, int topk,
                           const ImageMetadata *items, int item_count,
                           SimilarityReport *out_report);

bool SimilarityWriteReportJson(const char *output_path,
                               const SimilarityReport *report);

void SimilarityFreeReport(SimilarityReport *report);

#endif // SIMILARITY_ENGINE_H
