#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"

#include "runner_internal.h"

long long BenchNowMs(void) {
#if defined(CLOCK_MONOTONIC)
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (long long)ts.tv_sec * 1000LL + (long long)(ts.tv_nsec / 1000000LL);
  }
#endif
  time_t now = time(NULL);
  if (now < 0) {
    return 0;
  }
  return (long long)now * 1000LL;
}

void BenchFillNowUtcIso(char out[64]) {
  time_t now = time(NULL);
  struct tm tm_utc;
#if defined(_WIN32)
  gmtime_s(&tm_utc, &now);
#else
  gmtime_r(&now, &tm_utc);
#endif
  strftime(out, 64, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

void BenchFillGitCommit(char out[32]) {
  out[0] = '\0';
#if defined(_WIN32)
  FILE *pipe = popen("git rev-parse --short HEAD 2>nul", "r");
#else
  FILE *pipe = popen("git rev-parse --short HEAD 2>/dev/null", "r");
#endif
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

void BenchProfileSlug(const char *profile, char *out, size_t out_size) {
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

bool BenchAppendBenchmarkRecord(const char *history_path,
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

bool BenchWriteLastSummary(const char *last_path,
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

bool BenchWriteComparisonReport(const char *path, const char *baseline_profile,
                                const char *candidate_profile,
                                const WorkloadSummary *baseline,
                                const WorkloadSummary *candidate, int count) {
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
