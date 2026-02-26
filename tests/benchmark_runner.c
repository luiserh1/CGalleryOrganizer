#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "hash_utils.h"
#include "metadata_parser.h"
#include "ml_api.h"
#include "similarity_engine.h"
#include "utils/memory_metrics.h"

typedef enum {
  WORKLOAD_CACHE_METADATA_ONLY = 0,
  WORKLOAD_CACHE_FULL_WITH_EMBEDDINGS = 1,
  WORKLOAD_SIMILARITY_SEARCH = 2
} BenchmarkWorkload;

typedef struct {
  char timestampUtc[64];
  char gitCommit[32];
  char profile[32];
  char simMemoryMode[16];
  char workload[64];
  char datasetPath[1024];
  int datasetFileCount;
  char cachePath[1024];
  long long cacheBytes;
  long long similarityReportBytes;
  long long timeMs;
  long long rssStartBytes;
  long long rssEndBytes;
  long long rssDeltaBytes;
  long long peakRssBytes;
  int runIndex;
  int runCount;
  int warmupRuns;
  int success;
  char error[256];
} BenchmarkRecord;

typedef struct {
  int count;
} CountContext;

typedef struct {
  int mode;
  int failures;
  MlFeature features[3];
  int feature_count;
} ScanContext;

typedef struct {
  bool valid;
  double min;
  double max;
  double median;
  double p95;
  double mean;
  double stddev;
} MetricStats;

typedef struct {
  BenchmarkWorkload workload;
  char workloadName[64];
  char profile[32];
  char simMemoryMode[16];
  int runs;
  int warmupRuns;
  int successCount;
  int failureCount;
  MetricStats timeMs;
  MetricStats cacheBytes;
  MetricStats rssDeltaBytes;
  MetricStats peakRssBytes;
  MetricStats similarityReportBytes;
} WorkloadSummary;

enum {
  SCAN_MODE_METADATA_ONLY = 0,
  SCAN_MODE_ML = 1,
};

static long long NowMs(void) {
#if defined(CLOCK_MONOTONIC)
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000LL);
  }
#endif
  struct timespec ts_fallback;
  timespec_get(&ts_fallback, TIME_UTC);
  return (long long)ts_fallback.tv_sec * 1000LL +
         (long long)(ts_fallback.tv_nsec / 1000000LL);
}

static int CompareLongLongAsc(const void *a, const void *b) {
  long long lhs = *(const long long *)a;
  long long rhs = *(const long long *)b;
  if (lhs < rhs) {
    return -1;
  }
  if (lhs > rhs) {
    return 1;
  }
  return 0;
}

static bool BuildMetricStats(const BenchmarkRecord *records, int record_count,
                             long long (*metric)(const BenchmarkRecord *),
                             MetricStats *out) {
  if (!records || record_count <= 0 || !metric || !out) {
    return false;
  }

  long long *vals = calloc((size_t)record_count, sizeof(long long));
  if (!vals) {
    return false;
  }

  int n = 0;
  for (int i = 0; i < record_count; i++) {
    if (!records[i].success) {
      continue;
    }
    long long v = metric(&records[i]);
    if (v < 0) {
      continue;
    }
    vals[n++] = v;
  }

  if (n == 0) {
    free(vals);
    memset(out, 0, sizeof(*out));
    out->valid = false;
    return true;
  }

  qsort(vals, (size_t)n, sizeof(long long), CompareLongLongAsc);

  double sum = 0.0;
  for (int i = 0; i < n; i++) {
    sum += (double)vals[i];
  }
  double mean = sum / (double)n;

  double var = 0.0;
  for (int i = 0; i < n; i++) {
    double d = (double)vals[i] - mean;
    var += d * d;
  }
  var = var / (double)n;

  int median_idx = n / 2;
  int p95_idx = (int)ceil(0.95 * (double)n) - 1;
  if (p95_idx < 0) {
    p95_idx = 0;
  }
  if (p95_idx >= n) {
    p95_idx = n - 1;
  }

  memset(out, 0, sizeof(*out));
  out->valid = true;
  out->min = (double)vals[0];
  out->max = (double)vals[n - 1];
  out->median = (double)vals[median_idx];
  out->p95 = (double)vals[p95_idx];
  out->mean = mean;
  out->stddev = sqrt(var);

  free(vals);
  return true;
}

static long long MetricTime(const BenchmarkRecord *r) { return r->timeMs; }
static long long MetricCache(const BenchmarkRecord *r) { return r->cacheBytes; }
static long long MetricRssDelta(const BenchmarkRecord *r) {
  return r->rssDeltaBytes;
}
static long long MetricPeakRss(const BenchmarkRecord *r) {
  return r->peakRssBytes;
}
static long long MetricSimilarityReport(const BenchmarkRecord *r) {
  return r->similarityReportBytes;
}

static void FillNowUtcIso(char out[64]) {
  time_t now = time(NULL);
  struct tm tm_utc;
#if defined(_WIN32)
  gmtime_s(&tm_utc, &now);
#else
  gmtime_r(&now, &tm_utc);
#endif
  strftime(out, 64, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static void FillGitCommit(char out[32]) {
  out[0] = '\0';
  FILE *pipe = popen("git rev-parse --short HEAD 2>/dev/null", "r");
  if (!pipe) {
    snprintf(out, 32, "unknown");
    return;
  }

  if (!fgets(out, 32, pipe)) {
    snprintf(out, 32, "unknown");
  }
  pclose(pipe);

  size_t len = strlen(out);
  while (len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r')) {
    out[len - 1] = '\0';
    len--;
  }
  if (out[0] == '\0') {
    snprintf(out, 32, "unknown");
  }
}

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

static bool ParseProfile(const char *profile, CacheCompressionMode *out_mode,
                         int *out_level, char *out_profile, size_t out_size,
                         char *out_error, size_t out_error_size) {
  if (!profile || !out_mode || !out_level || !out_profile || out_size == 0) {
    snprintf(out_error, out_error_size, "invalid profile arguments");
    return false;
  }

  if (strcmp(profile, "uncompressed") == 0) {
    *out_mode = CACHE_COMPRESSION_NONE;
    *out_level = 3;
    snprintf(out_profile, out_size, "uncompressed");
    return true;
  }

  if (strncmp(profile, "zstd-l", 6) == 0) {
    char *endptr = NULL;
    long lvl = strtol(profile + 6, &endptr, 10);
    if (!endptr || *endptr != '\0' || lvl < 1 || lvl > 19) {
      snprintf(out_error, out_error_size,
               "invalid zstd profile level (expected 1..19)");
      return false;
    }

    *out_mode = CACHE_COMPRESSION_ZSTD;
    *out_level = (int)lvl;
    snprintf(out_profile, out_size, "zstd-l%d", *out_level);
    return true;
  }

  snprintf(out_error, out_error_size,
           "invalid profile (use uncompressed or zstd-l<level>)");
  return false;
}

static const char *WorkloadToName(BenchmarkWorkload workload) {
  switch (workload) {
  case WORKLOAD_CACHE_METADATA_ONLY:
    return "cache_metadata_only";
  case WORKLOAD_CACHE_FULL_WITH_EMBEDDINGS:
    return "cache_full_with_embeddings";
  case WORKLOAD_SIMILARITY_SEARCH:
    return "similarity_search";
  default:
    return "unknown";
  }
}

static bool ParseWorkload(const char *arg, BenchmarkWorkload *out_workload,
                          int *run_all) {
  if (!arg || !out_workload || !run_all) {
    return false;
  }
  if (strcmp(arg, "all") == 0) {
    *run_all = 1;
    return true;
  }
  *run_all = 0;

  if (strcmp(arg, "cache_metadata_only") == 0) {
    *out_workload = WORKLOAD_CACHE_METADATA_ONLY;
    return true;
  }
  if (strcmp(arg, "cache_full_with_embeddings") == 0) {
    *out_workload = WORKLOAD_CACHE_FULL_WITH_EMBEDDINGS;
    return true;
  }
  if (strcmp(arg, "similarity_search") == 0) {
    *out_workload = WORKLOAD_SIMILARITY_SEARCH;
    return true;
  }

  return false;
}

static void ProfileSlug(const char *profile, char *out, size_t out_size) {
  size_t j = 0;
  for (size_t i = 0; profile[i] != '\0' && j + 1 < out_size; i++) {
    unsigned char c = (unsigned char)profile[i];
    if (isalnum(c) || c == '-' || c == '_') {
      out[j++] = (char)c;
    } else {
      out[j++] = '_';
    }
  }
  out[j] = '\0';
}

static bool AppendBenchmarkRecord(const char *history_path,
                                  const BenchmarkRecord *record) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return false;
  }

  cJSON_AddStringToObject(root, "timestampUtc", record->timestampUtc);
  cJSON_AddStringToObject(root, "gitCommit", record->gitCommit);
  cJSON_AddStringToObject(root, "profile", record->profile);
  cJSON_AddStringToObject(root, "simMemoryMode", record->simMemoryMode);
  cJSON_AddStringToObject(root, "workload", record->workload);
  cJSON_AddStringToObject(root, "datasetPath", record->datasetPath);
  cJSON_AddNumberToObject(root, "datasetFileCount", record->datasetFileCount);
  cJSON_AddStringToObject(root, "cachePath", record->cachePath);
  cJSON_AddNumberToObject(root, "cacheBytes", (double)record->cacheBytes);
  cJSON_AddNumberToObject(root, "timeMs", (double)record->timeMs);
  cJSON_AddNumberToObject(root, "rssStartBytes", (double)record->rssStartBytes);
  cJSON_AddNumberToObject(root, "rssEndBytes", (double)record->rssEndBytes);
  cJSON_AddNumberToObject(root, "rssDeltaBytes", (double)record->rssDeltaBytes);
  cJSON_AddNumberToObject(root, "peakRssBytes", (double)record->peakRssBytes);
  cJSON_AddNumberToObject(root, "runIndex", record->runIndex);
  cJSON_AddNumberToObject(root, "runCount", record->runCount);
  cJSON_AddNumberToObject(root, "warmupRuns", record->warmupRuns);
  cJSON_AddBoolToObject(root, "success", record->success != 0);
  if (record->error[0] != '\0') {
    cJSON_AddStringToObject(root, "error", record->error);
  } else {
    cJSON_AddNullToObject(root, "error");
  }
  if (record->similarityReportBytes >= 0) {
    cJSON_AddNumberToObject(root, "similarityReportBytes",
                            (double)record->similarityReportBytes);
  }

  char *line = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (!line) {
    return false;
  }

  FILE *f = fopen(history_path, "a");
  if (!f) {
    free(line);
    return false;
  }
  fputs(line, f);
  fputc('\n', f);
  fclose(f);
  free(line);
  return true;
}

static cJSON *StatsToJson(const MetricStats *stats) {
  cJSON *obj = cJSON_CreateObject();
  if (!obj) {
    return NULL;
  }
  cJSON_AddBoolToObject(obj, "valid", stats->valid);
  if (stats->valid) {
    cJSON_AddNumberToObject(obj, "min", stats->min);
    cJSON_AddNumberToObject(obj, "max", stats->max);
    cJSON_AddNumberToObject(obj, "median", stats->median);
    cJSON_AddNumberToObject(obj, "p95", stats->p95);
    cJSON_AddNumberToObject(obj, "mean", stats->mean);
    cJSON_AddNumberToObject(obj, "stddev", stats->stddev);
  }
  return obj;
}

static bool WriteLastSummary(const char *last_path,
                             const WorkloadSummary *summaries,
                             int summary_count) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return false;
  }

  cJSON *arr = cJSON_CreateArray();
  if (!arr) {
    cJSON_Delete(root);
    return false;
  }

  for (int i = 0; i < summary_count; i++) {
    cJSON *entry = cJSON_CreateObject();
    if (!entry) {
      cJSON_Delete(arr);
      cJSON_Delete(root);
      return false;
    }

    cJSON_AddStringToObject(entry, "profile", summaries[i].profile);
    cJSON_AddStringToObject(entry, "simMemoryMode", summaries[i].simMemoryMode);
    cJSON_AddStringToObject(entry, "workload", summaries[i].workloadName);
    cJSON_AddNumberToObject(entry, "runs", summaries[i].runs);
    cJSON_AddNumberToObject(entry, "warmupRuns", summaries[i].warmupRuns);
    cJSON_AddNumberToObject(entry, "successCount", summaries[i].successCount);
    cJSON_AddNumberToObject(entry, "failureCount", summaries[i].failureCount);

    cJSON_AddItemToObject(entry, "timeMs", StatsToJson(&summaries[i].timeMs));
    cJSON_AddItemToObject(entry, "cacheBytes",
                          StatsToJson(&summaries[i].cacheBytes));
    cJSON_AddItemToObject(entry, "rssDeltaBytes",
                          StatsToJson(&summaries[i].rssDeltaBytes));
    cJSON_AddItemToObject(entry, "peakRssBytes",
                          StatsToJson(&summaries[i].peakRssBytes));
    cJSON_AddItemToObject(entry, "similarityReportBytes",
                          StatsToJson(&summaries[i].similarityReportBytes));

    cJSON_AddItemToArray(arr, entry);
  }

  cJSON_AddItemToObject(root, "workloads", arr);

  char *json = cJSON_Print(root);
  cJSON_Delete(root);
  if (!json) {
    return false;
  }

  FILE *f = fopen(last_path, "w");
  if (!f) {
    free(json);
    return false;
  }
  fputs(json, f);
  fclose(f);
  free(json);
  return true;
}

static bool RunWorkload(BenchmarkWorkload workload, const char *dataset_path,
                        const char *models_root, CacheCompressionMode mode,
                        int compression_level, const char *profile_name,
                        SimilarityMemoryMode sim_memory_mode,
                        BenchmarkRecord *record) {
  char profile_slug[64] = {0};
  ProfileSlug(profile_name, profile_slug, sizeof(profile_slug));

  char env_dir[1024];
  snprintf(env_dir, sizeof(env_dir), "build/benchmark/%s/%s", profile_slug,
           WorkloadToName(workload));
  if (!FsMakeDirRecursive(env_dir)) {
    snprintf(record->error, sizeof(record->error),
             "failed to create benchmark env dir");
    return false;
  }

  snprintf(record->workload, sizeof(record->workload), "%s",
           WorkloadToName(workload));
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

  long long start_ms = NowMs();
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
  record->timeMs = NowMs() - start_ms;
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

static void InitRecord(BenchmarkRecord *rec, const char *profile_name,
                       SimilarityMemoryMode sim_memory_mode,
                       const char *dataset_path, int dataset_file_count,
                       int run_index, int run_count, int warmup_runs) {
  memset(rec, 0, sizeof(*rec));
  FillNowUtcIso(rec->timestampUtc);
  FillGitCommit(rec->gitCommit);
  snprintf(rec->profile, sizeof(rec->profile), "%s", profile_name);
  snprintf(rec->simMemoryMode, sizeof(rec->simMemoryMode), "%s",
           sim_memory_mode == SIM_MEMORY_MODE_EAGER ? "eager" : "chunked");
  snprintf(rec->datasetPath, sizeof(rec->datasetPath), "%s", dataset_path);
  rec->datasetFileCount = dataset_file_count;
  rec->cacheBytes = -1;
  rec->similarityReportBytes = -1;
  rec->rssStartBytes = -1;
  rec->rssEndBytes = -1;
  rec->rssDeltaBytes = -1;
  rec->peakRssBytes = -1;
  rec->runIndex = run_index;
  rec->runCount = run_count;
  rec->warmupRuns = warmup_runs;
}

static bool SummarizeWorkload(BenchmarkWorkload workload, const char *profile_name,
                              SimilarityMemoryMode sim_memory_mode,
                              BenchmarkRecord *records, int runs,
                              int warmup_runs, WorkloadSummary *out_summary) {
  if (!records || runs <= 0 || !out_summary) {
    return false;
  }

  memset(out_summary, 0, sizeof(*out_summary));
  out_summary->workload = workload;
  snprintf(out_summary->workloadName, sizeof(out_summary->workloadName), "%s",
           WorkloadToName(workload));
  snprintf(out_summary->profile, sizeof(out_summary->profile), "%s", profile_name);
  snprintf(out_summary->simMemoryMode, sizeof(out_summary->simMemoryMode), "%s",
           sim_memory_mode == SIM_MEMORY_MODE_EAGER ? "eager" : "chunked");
  out_summary->runs = runs;
  out_summary->warmupRuns = warmup_runs;

  for (int i = 0; i < runs; i++) {
    if (records[i].success) {
      out_summary->successCount++;
    } else {
      out_summary->failureCount++;
    }
  }

  if (!BuildMetricStats(records, runs, MetricTime, &out_summary->timeMs) ||
      !BuildMetricStats(records, runs, MetricCache, &out_summary->cacheBytes) ||
      !BuildMetricStats(records, runs, MetricRssDelta,
                        &out_summary->rssDeltaBytes) ||
      !BuildMetricStats(records, runs, MetricPeakRss,
                        &out_summary->peakRssBytes) ||
      !BuildMetricStats(records, runs, MetricSimilarityReport,
                        &out_summary->similarityReportBytes)) {
    return false;
  }

  return true;
}

static int RunSuite(BenchmarkWorkload *workloads, int workload_count,
                    const char *dataset_path, int dataset_file_count,
                    const char *models_root, CacheCompressionMode mode,
                    int compression_level, const char *profile_name,
                    SimilarityMemoryMode sim_memory_mode, int runs,
                    int warmup_runs, const char *history_path,
                    WorkloadSummary *out_summaries) {
  int failures = 0;

  for (int i = 0; i < workload_count; i++) {
    for (int w = 0; w < warmup_runs; w++) {
      BenchmarkRecord warmup = {0};
      InitRecord(&warmup, profile_name, sim_memory_mode, dataset_path,
                 dataset_file_count, 0, runs, warmup_runs);
      RunWorkload(workloads[i], dataset_path, models_root, mode,
                  compression_level, profile_name, sim_memory_mode, &warmup);
    }

    BenchmarkRecord *records = calloc((size_t)runs, sizeof(BenchmarkRecord));
    if (!records) {
      fprintf(stderr, "Error: failed to allocate benchmark records.\n");
      return workload_count;
    }

    int workload_failures = 0;
    for (int r = 0; r < runs; r++) {
      InitRecord(&records[r], profile_name, sim_memory_mode, dataset_path,
                 dataset_file_count, r + 1, runs, warmup_runs);
      bool ok = RunWorkload(workloads[i], dataset_path, models_root, mode,
                            compression_level, profile_name, sim_memory_mode,
                            &records[r]);

      if (!AppendBenchmarkRecord(history_path, &records[r])) {
        fprintf(stderr, "Error: failed to append benchmark history.\n");
        free(records);
        return workload_count;
      }

      printf("[%s][sim:%s][run %d/%d] %s => %s | time=%lldms cache=%lldB rssDelta=%lldB peakRss=%lldB\n",
             records[r].profile, records[r].simMemoryMode, r + 1, runs,
             records[r].workload, ok ? "OK" : "FAIL", records[r].timeMs,
             records[r].cacheBytes, records[r].rssDeltaBytes,
             records[r].peakRssBytes);

      if (!ok) {
        workload_failures++;
        printf("  error: %s\n", records[r].error);
      }
    }

    if (!SummarizeWorkload(workloads[i], profile_name, sim_memory_mode, records,
                           runs, warmup_runs, &out_summaries[i])) {
      fprintf(stderr, "Error: failed to summarize workload stats.\n");
      free(records);
      return workload_count;
    }

    if (out_summaries[i].timeMs.valid) {
      printf("  stats: median=%.0fms p95=%.0fms min=%.0fms max=%.0fms stddev=%.2f\n",
             out_summaries[i].timeMs.median, out_summaries[i].timeMs.p95,
             out_summaries[i].timeMs.min, out_summaries[i].timeMs.max,
             out_summaries[i].timeMs.stddev);
    }

    failures += workload_failures;
    free(records);
  }

  return failures;
}

static cJSON *MetricDeltaJson(const MetricStats *base, const MetricStats *other) {
  cJSON *obj = cJSON_CreateObject();
  if (!obj) {
    return NULL;
  }

  if (!base->valid || !other->valid || base->median == 0.0) {
    cJSON_AddBoolToObject(obj, "valid", false);
    return obj;
  }

  double delta = ((other->median - base->median) / base->median) * 100.0;
  cJSON_AddBoolToObject(obj, "valid", true);
  cJSON_AddNumberToObject(obj, "baselineMedian", base->median);
  cJSON_AddNumberToObject(obj, "candidateMedian", other->median);
  cJSON_AddNumberToObject(obj, "deltaPct", delta);
  return obj;
}

static bool WriteComparisonReport(const char *path, const char *baseline_profile,
                                  const char *candidate_profile,
                                  const WorkloadSummary *baseline,
                                  const WorkloadSummary *candidate,
                                  int count) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return false;
  }

  cJSON_AddStringToObject(root, "baselineProfile", baseline_profile);
  cJSON_AddStringToObject(root, "candidateProfile", candidate_profile);

  cJSON *arr = cJSON_CreateArray();
  if (!arr) {
    cJSON_Delete(root);
    return false;
  }

  for (int i = 0; i < count; i++) {
    cJSON *entry = cJSON_CreateObject();
    if (!entry) {
      cJSON_Delete(arr);
      cJSON_Delete(root);
      return false;
    }

    cJSON_AddStringToObject(entry, "workload", baseline[i].workloadName);
    cJSON_AddItemToObject(entry, "timeMs",
                          MetricDeltaJson(&baseline[i].timeMs,
                                          &candidate[i].timeMs));
    cJSON_AddItemToObject(entry, "peakRssBytes",
                          MetricDeltaJson(&baseline[i].peakRssBytes,
                                          &candidate[i].peakRssBytes));
    cJSON_AddItemToObject(entry, "cacheBytes",
                          MetricDeltaJson(&baseline[i].cacheBytes,
                                          &candidate[i].cacheBytes));
    cJSON_AddItemToArray(arr, entry);
  }

  cJSON_AddItemToObject(root, "workloads", arr);

  char *json = cJSON_Print(root);
  cJSON_Delete(root);
  if (!json) {
    return false;
  }

  FILE *f = fopen(path, "w");
  if (!f) {
    free(json);
    return false;
  }
  fputs(json, f);
  fclose(f);
  free(json);
  return true;
}

int main(int argc, char **argv) {
  const char *profile_arg = "uncompressed";
  const char *workload_arg = "all";
  const char *compare_profile_arg = NULL;
  const char *comparison_path = NULL;
  int runs = 1;
  int warmup_runs = 0;
  SimilarityMemoryMode sim_memory_mode = SIM_MEMORY_MODE_CHUNKED;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--profile") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --profile requires a value.\n");
        return 1;
      }
      profile_arg = argv[++i];
    } else if (strcmp(argv[i], "--workload") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --workload requires a value.\n");
        return 1;
      }
      workload_arg = argv[++i];
    } else if (strcmp(argv[i], "--sim-memory-mode") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --sim-memory-mode requires eager|chunked.\n");
        return 1;
      }
      const char *mode = argv[++i];
      if (strcmp(mode, "eager") == 0) {
        sim_memory_mode = SIM_MEMORY_MODE_EAGER;
      } else if (strcmp(mode, "chunked") == 0) {
        sim_memory_mode = SIM_MEMORY_MODE_CHUNKED;
      } else {
        fprintf(stderr, "Error: --sim-memory-mode must be eager|chunked.\n");
        return 1;
      }
    } else if (strcmp(argv[i], "--runs") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --runs requires a positive integer.\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed <= 0 || parsed > 1000) {
        fprintf(stderr, "Error: --runs must be in [1,1000].\n");
        return 1;
      }
      runs = (int)parsed;
    } else if (strcmp(argv[i], "--warmup-runs") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr,
                "Error: --warmup-runs requires a non-negative integer.\n");
        return 1;
      }
      char *endptr = NULL;
      long parsed = strtol(argv[++i], &endptr, 10);
      if (!endptr || *endptr != '\0' || parsed < 0 || parsed > 1000) {
        fprintf(stderr, "Error: --warmup-runs must be in [0,1000].\n");
        return 1;
      }
      warmup_runs = (int)parsed;
    } else if (strcmp(argv[i], "--compare-profile") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --compare-profile requires a value.\n");
        return 1;
      }
      compare_profile_arg = argv[++i];
    } else if (strcmp(argv[i], "--comparison-path") == 0) {
      if (i + 1 >= argc) {
        fprintf(stderr, "Error: --comparison-path requires a value.\n");
        return 1;
      }
      comparison_path = argv[++i];
    } else {
      fprintf(stderr, "Error: unknown option '%s'.\n", argv[i]);
      return 1;
    }
  }

  CacheCompressionMode compression_mode = CACHE_COMPRESSION_NONE;
  int compression_level = 3;
  char profile_name[32] = {0};
  char parse_error[128] = {0};
  if (!ParseProfile(profile_arg, &compression_mode, &compression_level,
                    profile_name, sizeof(profile_name), parse_error,
                    sizeof(parse_error))) {
    fprintf(stderr, "Error: %s\n", parse_error);
    return 1;
  }

  CacheCompressionMode compare_mode = CACHE_COMPRESSION_NONE;
  int compare_level = 3;
  char compare_profile_name[32] = {0};
  if (compare_profile_arg &&
      !ParseProfile(compare_profile_arg, &compare_mode, &compare_level,
                    compare_profile_name, sizeof(compare_profile_name),
                    parse_error, sizeof(parse_error))) {
    fprintf(stderr, "Error: %s\n", parse_error);
    return 1;
  }

  const char *dataset_path = getenv("BENCHMARK_DATASET");
  if (!dataset_path || dataset_path[0] == '\0') {
    dataset_path = "build/stress_data";
  }

  const char *models_root = getenv("BENCHMARK_MODELS_ROOT");
  if (!models_root || models_root[0] == '\0') {
    models_root = "build/models";
  }

  const char *history_path = getenv("BENCHMARK_HISTORY_PATH");
  if (!history_path || history_path[0] == '\0') {
    history_path = "build/benchmark_history.jsonl";
  }

  const char *last_path = getenv("BENCHMARK_LAST_PATH");
  if (!last_path || last_path[0] == '\0') {
    last_path = "build/benchmark_last.json";
  }

  if (!comparison_path || comparison_path[0] == '\0') {
    comparison_path = "build/benchmark_compare.json";
  }

  if (!FsMakeDirRecursive("build")) {
    fprintf(stderr, "Error: failed to initialize build directory.\n");
    return 1;
  }

  int dataset_file_count = CountMediaFiles(dataset_path);
  if (dataset_file_count < 0) {
    fprintf(stderr, "Error: failed to scan dataset path '%s'.\n", dataset_path);
    return 1;
  }

  BenchmarkWorkload selected_workload = WORKLOAD_CACHE_METADATA_ONLY;
  int run_all = 1;
  if (!ParseWorkload(workload_arg, &selected_workload, &run_all)) {
    fprintf(stderr, "Error: invalid workload '%s'.\n", workload_arg);
    return 1;
  }

  BenchmarkWorkload workloads[3] = {WORKLOAD_CACHE_METADATA_ONLY,
                                    WORKLOAD_CACHE_FULL_WITH_EMBEDDINGS,
                                    WORKLOAD_SIMILARITY_SEARCH};
  int workload_count = run_all ? 3 : 1;
  if (!run_all) {
    workloads[0] = selected_workload;
  }

  WorkloadSummary baseline[3];
  memset(baseline, 0, sizeof(baseline));

  int failures = RunSuite(workloads, workload_count, dataset_path,
                          dataset_file_count, models_root, compression_mode,
                          compression_level, profile_name, sim_memory_mode,
                          runs, warmup_runs, history_path, baseline);

  if (!WriteLastSummary(last_path, baseline, workload_count)) {
    fprintf(stderr, "Error: failed to write benchmark last summary.\n");
    return 1;
  }

  if (compare_profile_arg) {
    WorkloadSummary candidate[3];
    memset(candidate, 0, sizeof(candidate));

    int compare_failures =
        RunSuite(workloads, workload_count, dataset_path, dataset_file_count,
                 models_root, compare_mode, compare_level, compare_profile_name,
                 sim_memory_mode, runs, warmup_runs, history_path, candidate);
    failures += compare_failures;

    if (!WriteComparisonReport(comparison_path, profile_name,
                               compare_profile_name, baseline, candidate,
                               workload_count)) {
      fprintf(stderr, "Error: failed to write comparison report.\n");
      return 1;
    }
  }

  return (failures == 0) ? 0 : 1;
}
