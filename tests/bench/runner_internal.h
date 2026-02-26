#ifndef BENCH_RUNNER_INTERNAL_H
#define BENCH_RUNNER_INTERNAL_H

#include <stdbool.h>

#include "gallery_cache.h"
#include "similarity_engine.h"

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

long long BenchNowMs(void);
void BenchFillNowUtcIso(char out[64]);
void BenchFillGitCommit(char out[32]);
void BenchProfileSlug(const char *profile, char *out, size_t out_size);

const char *BenchWorkloadToName(BenchmarkWorkload workload);
bool BenchParseWorkload(const char *arg, BenchmarkWorkload *out_workload,
                        int *run_all);
bool BenchParseProfile(const char *profile, CacheCompressionMode *out_mode,
                       int *out_level, char *out_profile, size_t out_size,
                       char *out_error, size_t out_error_size);

bool BenchRunWorkload(BenchmarkWorkload workload, const char *dataset_path,
                      const char *models_root, CacheCompressionMode mode,
                      int compression_level, const char *profile_name,
                      SimilarityMemoryMode sim_memory_mode,
                      BenchmarkRecord *record);
int BenchCountDatasetFiles(const char *dataset_path);

bool BenchAppendBenchmarkRecord(const char *history_path,
                                const BenchmarkRecord *record);
bool BenchWriteLastSummary(const char *last_path,
                           const WorkloadSummary *summaries,
                           int summary_count);
bool BenchWriteComparisonReport(const char *path,
                                const char *baseline_profile,
                                const char *candidate_profile,
                                const WorkloadSummary *baseline,
                                const WorkloadSummary *candidate, int count);

bool BenchBuildMetricStats(const BenchmarkRecord *records, int record_count,
                           long long (*metric)(const BenchmarkRecord *),
                           MetricStats *out);
long long BenchMetricTime(const BenchmarkRecord *r);
long long BenchMetricCache(const BenchmarkRecord *r);
long long BenchMetricRssDelta(const BenchmarkRecord *r);
long long BenchMetricPeakRss(const BenchmarkRecord *r);
long long BenchMetricSimilarityReport(const BenchmarkRecord *r);

int BenchRunnerMain(int argc, char **argv);

#endif // BENCH_RUNNER_INTERNAL_H
