#include <stdlib.h>
#include <string.h>

#include "cli/cli_commands.h"
#include "gallery_cache.h"
#include "similarity_engine.h"

static int ComparePathStrings(const void *a, const void *b) {
  const char *const *pa = (const char *const *)a;
  const char *const *pb = (const char *const *)b;
  return strcmp(*pa, *pb);
}

bool CliBuildSimilarityReportFromCache(const char *report_path, float threshold,
                                       int topk) {
  if (!report_path) {
    return false;
  }

  int key_count = 0;
  char **keys = CacheGetAllKeys(&key_count);
  if (key_count <= 0) {
    SimilarityReport empty = {0};
    if (!SimilarityBuildReport("embed-default", threshold, topk, NULL, 0,
                               &empty)) {
      CacheFreeKeys(keys, key_count);
      return false;
    }
    bool written = SimilarityWriteReportJson(report_path, &empty);
    SimilarityFreeReport(&empty);
    CacheFreeKeys(keys, key_count);
    return written;
  }
  if (!keys) {
    return false;
  }

  qsort(keys, (size_t)key_count, sizeof(char *), ComparePathStrings);

  ImageMetadata *entries = calloc((size_t)key_count, sizeof(ImageMetadata));
  if (!entries) {
    CacheFreeKeys(keys, key_count);
    return false;
  }

  int usable_count = 0;
  for (int i = 0; i < key_count; i++) {
    ImageMetadata md = {0};
    if (!CacheGetRawEntry(keys[i], &md)) {
      continue;
    }
    if (md.mlEmbeddingDim > 0 && md.mlEmbeddingBase64 &&
        md.mlModelEmbedding[0] != '\0') {
      entries[usable_count++] = md;
    } else {
      CacheFreeMetadata(&md);
    }
  }

  SimilarityReport report = {0};
  const char *model_id =
      usable_count > 0 ? entries[0].mlModelEmbedding : "embed-default";
  bool ok = SimilarityBuildReport(model_id, threshold, topk, entries,
                                  usable_count, &report);
  if (ok) {
    ok = SimilarityWriteReportJson(report_path, &report);
  }

  SimilarityFreeReport(&report);
  for (int i = 0; i < usable_count; i++) {
    CacheFreeMetadata(&entries[i]);
  }

  free(entries);
  CacheFreeKeys(keys, key_count);
  return ok;
}
