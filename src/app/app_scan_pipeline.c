#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "app/app_scan_pipeline.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "hash_utils.h"
#include "metadata_parser.h"
#include "ml_api.h"
#include "similarity_engine.h"

typedef struct {
  char **items;
  int count;
  int capacity;
} PathList;

typedef struct {
  pthread_mutex_t cache_mutex;
  pthread_mutex_t stats_mutex;
  ScanRunStats *stats;
  AppScanContext *scan_ctx;
} ScanPipelineContext;

typedef struct {
  PathList *paths;
  int start;
  int end;
  ScanPipelineContext *pipeline;
  bool *failed;
  pthread_mutex_t *failed_mutex;
} ScanWorkerContext;

static void EmitProgress(AppScanContext *ctx, int current, int total,
                         const char *message) {
  if (!ctx || !ctx->progress_cb) {
    return;
  }
  ctx->progress_cb("scan", current, total, ctx->callback_user_data);
  if (message) {
    ctx->progress_cb("scan_detail", current, total, ctx->callback_user_data);
  }
}

static bool IsCancelled(AppScanContext *ctx) {
  if (!ctx || !ctx->cancel_cb) {
    return false;
  }
  if (ctx->cancel_cb(ctx->callback_user_data)) {
    ctx->cancelled = true;
    return true;
  }
  return false;
}
static void ApplyMlResultToMetadata(ImageMetadata *md, const MlResult *ml) {
  if (!md || !ml) {
    return;
  }

  if (ml->has_classification && ml->topk_count > 0) {
    strncpy(md->mlPrimaryClass, ml->topk[0].label,
            sizeof(md->mlPrimaryClass) - 1);
    md->mlPrimaryClass[sizeof(md->mlPrimaryClass) - 1] = '\0';
    md->mlPrimaryClassConfidence = ml->topk[0].confidence;
  }

  md->mlHasText = ml->has_text_detection && ml->text_box_count > 0;
  md->mlTextBoxCount = ml->text_box_count;

  if (ml->model_id_classification[0] != '\0') {
    strncpy(md->mlModelClassification, ml->model_id_classification,
            sizeof(md->mlModelClassification) - 1);
    md->mlModelClassification[sizeof(md->mlModelClassification) - 1] = '\0';
  }

  if (ml->model_id_text_detection[0] != '\0') {
    strncpy(md->mlModelTextDetection, ml->model_id_text_detection,
            sizeof(md->mlModelTextDetection) - 1);
    md->mlModelTextDetection[sizeof(md->mlModelTextDetection) - 1] = '\0';
  }

  if (ml->provider_raw_json) {
    if (md->mlRawJson) {
      free(md->mlRawJson);
    }
    md->mlRawJson = strdup(ml->provider_raw_json);
  }

  if (ml->has_embedding && ml->embedding && ml->embedding_dim > 0) {
    char *encoded = NULL;
    if (SimilarityEncodeEmbeddingBase64(ml->embedding, ml->embedding_dim,
                                        &encoded)) {
      if (md->mlEmbeddingBase64) {
        free(md->mlEmbeddingBase64);
      }
      md->mlEmbeddingBase64 = encoded;
      md->mlEmbeddingDim = ml->embedding_dim;
    }
  }

  if (ml->model_id_embedding[0] != '\0') {
    strncpy(md->mlModelEmbedding, ml->model_id_embedding,
            sizeof(md->mlModelEmbedding) - 1);
    md->mlModelEmbedding[sizeof(md->mlModelEmbedding) - 1] = '\0';
  }
}
static void RunMlInferenceIfRequested(const char *absolute_path, ImageMetadata *md,
                                      ScanPipelineContext *pipeline) {
  AppScanContext *ctx = pipeline->scan_ctx;
  if (!ctx || !md || !ctx->ml_enabled ||
      (!ctx->ml_enrich && !ctx->similarity_report)) {
    return;
  }

  bool need_classification =
      ctx->ml_enrich && md->mlModelClassification[0] == '\0';
  bool need_text = ctx->ml_enrich && md->mlModelTextDetection[0] == '\0';
  bool need_embedding =
      ctx->similarity_report &&
      (!ctx->sim_incremental || md->mlEmbeddingDim <= 0 ||
       md->mlEmbeddingBase64 == NULL || md->mlModelEmbedding[0] == '\0');

  if (!need_classification && !need_text && !need_embedding) {
    return;
  }

  if (IsCancelled(ctx)) {
    return;
  }

  pthread_mutex_lock(&pipeline->stats_mutex);
  ctx->ml_files_evaluated++;
  pthread_mutex_unlock(&pipeline->stats_mutex);

  MlFeature requested[3] = {0};
  int requested_count = 0;
  if (need_classification) {
    requested[requested_count++] = ML_FEATURE_CLASSIFICATION;
  }
  if (need_text) {
    requested[requested_count++] = ML_FEATURE_TEXT_DETECTION;
  }
  if (need_embedding) {
    requested[requested_count++] = ML_FEATURE_EMBEDDING;
  }

  MlResult result = {0};
  if (!MlInferImage(absolute_path, requested, requested_count, &result)) {
    pthread_mutex_lock(&pipeline->stats_mutex);
    ctx->ml_failures++;
    pthread_mutex_unlock(&pipeline->stats_mutex);
    return;
  }

  ApplyMlResultToMetadata(md, &result);
  pthread_mutex_lock(&pipeline->stats_mutex);
  if (result.has_classification && result.topk_count > 0) {
    ctx->ml_files_classified++;
  }
  if (result.has_text_detection && result.text_box_count > 0) {
    ctx->ml_files_with_text++;
  }
  if (result.has_embedding && result.embedding_dim > 0) {
    ctx->ml_files_embedded++;
  }
  pthread_mutex_unlock(&pipeline->stats_mutex);

  MlFreeResult(&result);
}

static bool ProcessMediaPath(const char *absolute_path,
                             ScanPipelineContext *pipeline) {
  AppScanContext *ctx = pipeline->scan_ctx;
  if (!FsIsSupportedMedia(absolute_path)) {
    return true;
  }

  double mod_date = 0;
  long size = 0;

  if (ExtractBasicMetadata(absolute_path, &mod_date, &size)) {
    ImageMetadata md = {0};

    bool exhaustive = (ctx != NULL) ? ctx->exhaustive : false;

    pthread_mutex_lock(&pipeline->cache_mutex);
    bool valid_entry = CacheGetValidEntry(absolute_path, mod_date, size, &md);
    pthread_mutex_unlock(&pipeline->cache_mutex);
    if (valid_entry) {
      if (md.exactHashMd5[0] == '\0' ||
          (exhaustive && md.allMetadataJson == NULL)) {
        ImageMetadata full = ExtractMetadata(absolute_path, exhaustive);
        md.width = full.width;
        md.height = full.height;
        strncpy(md.dateTaken, full.dateTaken, METADATA_MAX_STRING - 1);
        md.dateTaken[METADATA_MAX_STRING - 1] = '\0';
        strncpy(md.cameraMake, full.cameraMake, METADATA_MAX_STRING - 1);
        md.cameraMake[METADATA_MAX_STRING - 1] = '\0';
        strncpy(md.cameraModel, full.cameraModel, METADATA_MAX_STRING - 1);
        md.cameraModel[METADATA_MAX_STRING - 1] = '\0';
        md.orientation = full.orientation;
        md.hasGps = full.hasGps;
        md.gpsLatitude = full.gpsLatitude;
        md.gpsLongitude = full.gpsLongitude;
        if (full.allMetadataJson) {
          if (md.allMetadataJson) {
            free(md.allMetadataJson);
          }
          md.allMetadataJson = strdup(full.allMetadataJson);
        }
        if (md.exactHashMd5[0] == '\0') {
          ComputeFileMd5(absolute_path, md.exactHashMd5);
        }
        CacheFreeMetadata(&full);
      }

      RunMlInferenceIfRequested(absolute_path, &md, pipeline);
      pthread_mutex_lock(&pipeline->cache_mutex);
      bool updated = CacheUpdateEntry(&md);
      pthread_mutex_unlock(&pipeline->cache_mutex);
      if (updated) {
        pthread_mutex_lock(&pipeline->stats_mutex);
        pipeline->stats->files_cached++;
        pthread_mutex_unlock(&pipeline->stats_mutex);
      }
      CacheFreeMetadata(&md);
      return true;
    }

    md.path = strdup(absolute_path);
    if (!md.path) {
      return true;
    }

    md.modificationDate = mod_date;
    md.fileSize = size;

    ImageMetadata extracted = ExtractMetadata(absolute_path, exhaustive);
    if (extracted.width > 0) {
      md.width = extracted.width;
    }
    if (extracted.height > 0) {
      md.height = extracted.height;
    }
    if (extracted.dateTaken[0] != '\0') {
      strncpy(md.dateTaken, extracted.dateTaken, METADATA_MAX_STRING - 1);
      md.dateTaken[METADATA_MAX_STRING - 1] = '\0';
    }
    if (extracted.cameraMake[0] != '\0') {
      strncpy(md.cameraMake, extracted.cameraMake, METADATA_MAX_STRING - 1);
      md.cameraMake[METADATA_MAX_STRING - 1] = '\0';
    }
    if (extracted.cameraModel[0] != '\0') {
      strncpy(md.cameraModel, extracted.cameraModel, METADATA_MAX_STRING - 1);
      md.cameraModel[METADATA_MAX_STRING - 1] = '\0';
    }
    md.orientation = extracted.orientation;
    md.hasGps = extracted.hasGps;
    md.gpsLatitude = extracted.gpsLatitude;
    md.gpsLongitude = extracted.gpsLongitude;

    if (extracted.allMetadataJson) {
      md.allMetadataJson = strdup(extracted.allMetadataJson);
    }

    ComputeFileMd5(absolute_path, md.exactHashMd5);
    RunMlInferenceIfRequested(absolute_path, &md, pipeline);

    pthread_mutex_lock(&pipeline->cache_mutex);
    bool updated = CacheUpdateEntry(&md);
    pthread_mutex_unlock(&pipeline->cache_mutex);
    if (updated) {
      pthread_mutex_lock(&pipeline->stats_mutex);
      pipeline->stats->files_cached++;
      pthread_mutex_unlock(&pipeline->stats_mutex);
    }
    CacheFreeMetadata(&md);
    CacheFreeMetadata(&extracted);
  }
  return true;
}
static bool PathListReserve(PathList *list, int new_capacity) {
  if (!list || new_capacity <= list->capacity) {
    return true;
  }
  char **resized = realloc(list->items, sizeof(char *) * (size_t)new_capacity);
  if (!resized) {
    return false;
  }
  list->items = resized;
  list->capacity = new_capacity;
  return true;
}
static bool PathListPush(PathList *list, const char *path) {
  if (!list || !path) {
    return false;
  }
  if (list->count == list->capacity) {
    int next = list->capacity == 0 ? 128 : list->capacity * 2;
    if (!PathListReserve(list, next)) {
      return false;
    }
  }
  list->items[list->count] = strdup(path);
  if (!list->items[list->count]) {
    return false;
  }
  list->count++;
  return true;
}
static void PathListFree(PathList *list) {
  if (!list) {
    return;
  }
  for (int i = 0; i < list->count; i++) {
    free(list->items[i]);
  }
  free(list->items);
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}
static int ComparePathStrings(const void *a, const void *b) {
  const char *const *pa = (const char *const *)a;
  const char *const *pb = (const char *const *)b;
  return strcmp(*pa, *pb);
}

static bool CollectPathCallback(const char *absolute_path, void *user_data) {
  PathList *list = (PathList *)user_data;
  return PathListPush(list, absolute_path);
}

static bool CollectSortedPaths(const char *root_dir, PathList *out_paths) {
  if (!root_dir || !out_paths) {
    return false;
  }
  memset(out_paths, 0, sizeof(PathList));
  if (!FsWalkDirectory(root_dir, CollectPathCallback, out_paths)) {
    PathListFree(out_paths);
    return false;
  }
  qsort(out_paths->items, (size_t)out_paths->count, sizeof(char *),
        ComparePathStrings);
  return true;
}

static bool WorkerShouldStop(ScanWorkerContext *ctx) {
  bool failed = false;
  pthread_mutex_lock(ctx->failed_mutex);
  failed = *ctx->failed;
  if (!failed && IsCancelled(ctx->pipeline->scan_ctx)) {
    *ctx->failed = true;
    failed = true;
  }
  pthread_mutex_unlock(ctx->failed_mutex);
  return failed;
}

static void WorkerMarkFailed(ScanWorkerContext *ctx) {
  pthread_mutex_lock(ctx->failed_mutex);
  *ctx->failed = true;
  pthread_mutex_unlock(ctx->failed_mutex);
}

static void WorkerEmitProgress(ScanWorkerContext *ctx) {
  AppScanContext *scan_ctx = ctx->pipeline->scan_ctx;
  if (!scan_ctx || !scan_ctx->progress_cb) {
    return;
  }

  int scanned = 0;
  pthread_mutex_lock(&ctx->pipeline->stats_mutex);
  scanned = ctx->pipeline->stats->files_scanned;
  pthread_mutex_unlock(&ctx->pipeline->stats_mutex);

  EmitProgress(scan_ctx, scanned, scan_ctx->total_files, NULL);
}

static void *ScanWorkerRun(void *arg) {
  ScanWorkerContext *ctx = (ScanWorkerContext *)arg;
  for (int i = ctx->start; i < ctx->end; i++) {
    if (WorkerShouldStop(ctx)) {
      return NULL;
    }

    pthread_mutex_lock(&ctx->pipeline->stats_mutex);
    ctx->pipeline->stats->files_scanned++;
    pthread_mutex_unlock(&ctx->pipeline->stats_mutex);

    WorkerEmitProgress(ctx);

    if (!ProcessMediaPath(ctx->paths->items[i], ctx->pipeline)) {
      WorkerMarkFailed(ctx);
      return NULL;
    }
  }
  return NULL;
}

bool AppRunScanPipeline(const char *target_dir, AppScanContext *ctx,
                        int resolved_jobs, ScanRunStats *out_stats) {
  if (!target_dir || !ctx || !out_stats) {
    return false;
  }

  out_stats->files_scanned = 0;
  out_stats->files_cached = 0;
  ctx->cancelled = false;
  ctx->total_files = 0;

  ScanPipelineContext pipeline = {
      .cache_mutex = PTHREAD_MUTEX_INITIALIZER,
      .stats_mutex = PTHREAD_MUTEX_INITIALIZER,
      .stats = out_stats,
      .scan_ctx = ctx,
  };

  PathList scan_paths = {0};
  if (!CollectSortedPaths(target_dir, &scan_paths)) {
    return false;
  }

  ctx->total_files = scan_paths.count;
  EmitProgress(ctx, 0, ctx->total_files, NULL);

  bool scan_failed = false;
  if (resolved_jobs <= 1 || scan_paths.count <= 1) {
    for (int i = 0; i < scan_paths.count; i++) {
      if (IsCancelled(ctx)) {
        scan_failed = true;
        break;
      }

      pthread_mutex_lock(&pipeline.stats_mutex);
      pipeline.stats->files_scanned++;
      pthread_mutex_unlock(&pipeline.stats_mutex);

      EmitProgress(ctx, pipeline.stats->files_scanned, ctx->total_files, NULL);

      if (!ProcessMediaPath(scan_paths.items[i], &pipeline)) {
        scan_failed = true;
        break;
      }
    }
  } else {
    int worker_count = resolved_jobs;
    if (worker_count > scan_paths.count) {
      worker_count = scan_paths.count;
    }
    pthread_t *threads = calloc((size_t)worker_count, sizeof(pthread_t));
    ScanWorkerContext *worker_ctx =
        calloc((size_t)worker_count, sizeof(ScanWorkerContext));
    pthread_mutex_t failed_mutex = PTHREAD_MUTEX_INITIALIZER;
    bool failed = false;

    if (!threads || !worker_ctx) {
      free(threads);
      free(worker_ctx);
      PathListFree(&scan_paths);
      return false;
    }

    int base = scan_paths.count / worker_count;
    int rem = scan_paths.count % worker_count;
    int cursor = 0;
    int launched = 0;
    for (int i = 0; i < worker_count; i++) {
      int span = base + (i < rem ? 1 : 0);
      worker_ctx[i].paths = &scan_paths;
      worker_ctx[i].start = cursor;
      worker_ctx[i].end = cursor + span;
      worker_ctx[i].pipeline = &pipeline;
      worker_ctx[i].failed = &failed;
      worker_ctx[i].failed_mutex = &failed_mutex;
      cursor += span;
      if (pthread_create(&threads[i], NULL, ScanWorkerRun, &worker_ctx[i]) !=
          0) {
        failed = true;
        break;
      }
      launched++;
    }
    for (int i = 0; i < launched; i++) {
      pthread_join(threads[i], NULL);
    }

    scan_failed = failed;
    free(threads);
    free(worker_ctx);
  }

  PathListFree(&scan_paths);
  return !scan_failed;
}
