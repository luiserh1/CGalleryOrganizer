#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_utils.h"
#include "gallery_cache.h"
#include "hash_utils.h"
#include "metadata_parser.h"
#include "ml_api.h"
#include "similarity_engine.h"
#include "utils/memory_metrics.h"

#include "runner_internal.h"

typedef struct {
  int count;
} CountContext;

typedef struct {
  int mode;
  int failures;
  MlFeature features[3];
  int feature_count;
} ScanContext;

enum {
  SCAN_MODE_METADATA_ONLY = 0,
  SCAN_MODE_ML = 1,
};

static bool CountScanCallback(const char *absolute_path, void *user_data) {
  CountContext *ctx = (CountContext *)user_data;
  if (FsIsSupportedMedia(absolute_path)) {
    ctx->count++;
  }
  return true;
}

static int CountMediaFiles(const char *dataset_path) {
  CountContext ctx = {0};
  if (!FsWalkDirectory(dataset_path, CountScanCallback, &ctx)) {
    return -1;
  }
  return ctx.count;
}

static long long FileSizeBytesOrMinus1(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return -1;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return -1;
  }
  long size = ftell(f);
  fclose(f);
  if (size < 0) {
    return -1;
  }
  return (long long)size;
}

static bool ApplyMlToMetadata(const char *path, ImageMetadata *md,
                              const ScanContext *ctx) {
  if (!path || !md || !ctx || ctx->mode != SCAN_MODE_ML ||
      ctx->feature_count <= 0) {
    return true;
  }

  MlResult result = {0};
  if (!MlInferImage(path, (MlFeature *)ctx->features, ctx->feature_count,
                    &result)) {
    return false;
  }

  if (result.has_classification && result.topk_count > 0) {
    strncpy(md->mlPrimaryClass, result.topk[0].label,
            sizeof(md->mlPrimaryClass) - 1);
    md->mlPrimaryClass[sizeof(md->mlPrimaryClass) - 1] = '\0';
    md->mlPrimaryClassConfidence = result.topk[0].confidence;
  }

  md->mlHasText = result.has_text_detection && result.text_box_count > 0;
  md->mlTextBoxCount = result.text_box_count;

  if (result.model_id_classification[0] != '\0') {
    strncpy(md->mlModelClassification, result.model_id_classification,
            sizeof(md->mlModelClassification) - 1);
    md->mlModelClassification[sizeof(md->mlModelClassification) - 1] = '\0';
  }
  if (result.model_id_text_detection[0] != '\0') {
    strncpy(md->mlModelTextDetection, result.model_id_text_detection,
            sizeof(md->mlModelTextDetection) - 1);
    md->mlModelTextDetection[sizeof(md->mlModelTextDetection) - 1] = '\0';
  }
  if (result.model_id_embedding[0] != '\0') {
    strncpy(md->mlModelEmbedding, result.model_id_embedding,
            sizeof(md->mlModelEmbedding) - 1);
    md->mlModelEmbedding[sizeof(md->mlModelEmbedding) - 1] = '\0';
  }

  if (result.provider_raw_json) {
    md->mlRawJson = strdup(result.provider_raw_json);
  }

  if (result.has_embedding && result.embedding_dim > 0 && result.embedding) {
    char *encoded = NULL;
    if (SimilarityEncodeEmbeddingBase64(result.embedding, result.embedding_dim,
                                        &encoded)) {
      md->mlEmbeddingBase64 = encoded;
      md->mlEmbeddingDim = result.embedding_dim;
    }
  }

  MlFreeResult(&result);
  return true;
}

static bool BenchmarkScanCallback(const char *absolute_path, void *user_data) {
  ScanContext *ctx = (ScanContext *)user_data;
  if (!FsIsSupportedMedia(absolute_path)) {
    return true;
  }

  double mod_date = 0;
  long file_size = 0;
  if (!ExtractBasicMetadata(absolute_path, &mod_date, &file_size)) {
    ctx->failures++;
    return true;
  }

  ImageMetadata parsed = ExtractMetadata(absolute_path, false);
  ImageMetadata md = {0};
  md.path = strdup(absolute_path);
  if (!md.path) {
    CacheFreeMetadata(&parsed);
    ctx->failures++;
    return true;
  }

  md.modificationDate = mod_date;
  md.fileSize = file_size;
  md.width = parsed.width;
  md.height = parsed.height;
  md.orientation = parsed.orientation;
  md.hasGps = parsed.hasGps;
  md.gpsLatitude = parsed.gpsLatitude;
  md.gpsLongitude = parsed.gpsLongitude;
  strncpy(md.dateTaken, parsed.dateTaken, METADATA_MAX_STRING - 1);
  strncpy(md.cameraMake, parsed.cameraMake, METADATA_MAX_STRING - 1);
  strncpy(md.cameraModel, parsed.cameraModel, METADATA_MAX_STRING - 1);
  CacheFreeMetadata(&parsed);

  if (ctx->mode == SCAN_MODE_ML) {
    ComputeFileMd5(absolute_path, md.exactHashMd5);
    if (!ApplyMlToMetadata(absolute_path, &md, ctx)) {
      CacheFreeMetadata(&md);
      ctx->failures++;
      return true;
    }
  }

  if (!CacheUpdateEntry(&md)) {
    ctx->failures++;
  }

  CacheFreeMetadata(&md);
  return true;
}

static bool BuildSimilarityReportFromCache(const char *report_path,
                                           float threshold, int topk,
                                           SimilarityMemoryMode mode) {
  int key_count = 0;
  char **keys = CacheGetAllKeys(&key_count);
  if (key_count < 0) {
    return false;
  }

  ImageMetadata *entries = NULL;
  if (key_count > 0) {
    entries = calloc((size_t)key_count, sizeof(ImageMetadata));
    if (!entries) {
      CacheFreeKeys(keys, key_count);
      return false;
    }
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
  const char *model_id = (usable_count > 0) ? entries[0].mlModelEmbedding
                                             : "embed-default";
  SimilaritySetMemoryMode(mode);
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

bool BenchRunWorkload(BenchmarkWorkload workload, const char *dataset_path,
                      const char *models_root, CacheCompressionMode mode,
                      int compression_level, const char *profile_name,
                      SimilarityMemoryMode sim_memory_mode,
                      BenchmarkRecord *record) {
  char profile_slug[64] = {0};
  BenchProfileSlug(profile_name, profile_slug, sizeof(profile_slug));

  char env_dir[1024];
  snprintf(env_dir, sizeof(env_dir), "build/benchmark/%s/%s", profile_slug,
           BenchWorkloadToName(workload));
  if (!FsMakeDirRecursive(env_dir)) {
    snprintf(record->error, sizeof(record->error),
             "failed to create benchmark env dir");
    return false;
  }

  snprintf(record->workload, sizeof(record->workload), "%s",
           BenchWorkloadToName(workload));
  snprintf(record->cachePath, sizeof(record->cachePath), "%s/gallery_cache.json",
           env_dir);

  char report_path[1024];
  snprintf(report_path, sizeof(report_path), "%s/similarity_report.json", env_dir);

  remove(record->cachePath);
  remove(report_path);

  if (!CacheSetCompression(mode, compression_level)) {
    snprintf(record->error, sizeof(record->error),
             "cache compression mode unavailable");
    return false;
  }

  long long start_ms = BenchNowMs();
  record->rssStartBytes = GetCurrentRssBytes();

  if (!CacheInit(record->cachePath)) {
    snprintf(record->error, sizeof(record->error), "cache init failed");
    return false;
  }

  bool ok = true;
  ScanContext scan_ctx = {0};

  if (workload == WORKLOAD_CACHE_METADATA_ONLY) {
    scan_ctx.mode = SCAN_MODE_METADATA_ONLY;
    scan_ctx.feature_count = 0;

    if (!FsWalkDirectory(dataset_path, BenchmarkScanCallback, &scan_ctx)) {
      ok = false;
      snprintf(record->error, sizeof(record->error), "dataset scan failed");
    }
  } else if (workload == WORKLOAD_CACHE_FULL_WITH_EMBEDDINGS ||
             workload == WORKLOAD_SIMILARITY_SEARCH) {
    if (!MlInit(models_root)) {
      ok = false;
      snprintf(record->error, sizeof(record->error), "ml init failed");
    } else {
      scan_ctx.mode = SCAN_MODE_ML;
      scan_ctx.features[0] = ML_FEATURE_CLASSIFICATION;
      scan_ctx.features[1] = ML_FEATURE_TEXT_DETECTION;
      scan_ctx.features[2] = ML_FEATURE_EMBEDDING;
      scan_ctx.feature_count = 3;

      if (!FsWalkDirectory(dataset_path, BenchmarkScanCallback, &scan_ctx)) {
        ok = false;
        snprintf(record->error, sizeof(record->error), "dataset scan failed");
      }

      MlShutdown();
    }
  }

  if (ok && scan_ctx.failures > 0) {
    ok = false;
    snprintf(record->error, sizeof(record->error),
             "scan completed with %d item failures", scan_ctx.failures);
  }

  if (ok && !CacheSave()) {
    ok = false;
    snprintf(record->error, sizeof(record->error), "cache save failed");
  }

  if (ok && workload == WORKLOAD_SIMILARITY_SEARCH) {
    if (!BuildSimilarityReportFromCache(report_path, 0.92f, 5,
                                        sim_memory_mode)) {
      ok = false;
      snprintf(record->error, sizeof(record->error),
               "similarity report generation failed");
    }
  }

  CacheShutdown();

  record->rssEndBytes = GetCurrentRssBytes();
  record->peakRssBytes = GetPeakRssBytes();
  record->timeMs = BenchNowMs() - start_ms;
  record->cacheBytes = FileSizeBytesOrMinus1(record->cachePath);
  record->similarityReportBytes =
      (workload == WORKLOAD_SIMILARITY_SEARCH)
          ? FileSizeBytesOrMinus1(report_path)
          : -1;

  if (record->rssStartBytes >= 0 && record->rssEndBytes >= 0) {
    record->rssDeltaBytes = record->rssEndBytes - record->rssStartBytes;
  } else {
    record->rssDeltaBytes = -1;
  }

  record->success = ok ? 1 : 0;
  return ok;
}

int BenchCountDatasetFiles(const char *dataset_path) {
  return CountMediaFiles(dataset_path);
}
