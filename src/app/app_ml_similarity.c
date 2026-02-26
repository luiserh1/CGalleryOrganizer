#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_internal.h"

static int ComparePathStrings(const void *a, const void *b) {
  const char *const *pa = (const char *const *)a;
  const char *const *pb = (const char *const *)b;
  return strcmp(*pa, *pb);
}

static bool BuildSimilarityReportFromCache(const char *report_path,
                                           float threshold, int topk) {
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

AppStatus AppGenerateSimilarityReport(AppContext *ctx,
                                      const AppSimilarityRequest *request,
                                      AppSimilarityResult *out_result) {
  if (!ctx || !request || !out_result || !request->env_dir ||
      request->env_dir[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  if (request->threshold < 0.0f || request->threshold > 1.0f ||
      request->topk <= 0) {
    AppSetError(ctx, "invalid similarity threshold/topk");
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppStatus status =
      AppEnsureCacheReady(ctx, request->env_dir, request->cache_compression_mode,
                          request->cache_compression_level, true);
  if (status != APP_STATUS_OK) {
    return status;
  }

  if (AppShouldCancel(&request->hooks)) {
    AppSetError(ctx, "similarity report cancelled");
    return APP_STATUS_CANCELLED;
  }

  SimilaritySetMemoryMode(AppMapSimilarityMemoryMode(request->memory_mode));

  char report_path[APP_MAX_PATH] = {0};
  if (request->output_path_override && request->output_path_override[0] != '\0') {
    strncpy(report_path, request->output_path_override, sizeof(report_path) - 1);
  } else {
    snprintf(report_path, sizeof(report_path), "%s/similarity_report.json",
             request->env_dir);
  }

  AppEmitProgress(&request->hooks, "similarity", 0, 1,
                  "Generating similarity report");

  if (!BuildSimilarityReportFromCache(report_path, request->threshold,
                                      request->topk)) {
    AppSetError(ctx, "failed to generate similarity report at '%s'", report_path);
    return APP_STATUS_INTERNAL_ERROR;
  }

  AppEmitProgress(&request->hooks, "similarity", 1, 1,
                  "Similarity report completed");

  strncpy(out_result->report_path, report_path, sizeof(out_result->report_path) - 1);
  out_result->report_path[sizeof(out_result->report_path) - 1] = '\0';
  return APP_STATUS_OK;
}
