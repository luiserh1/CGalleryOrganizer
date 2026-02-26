#include <math.h>
#include <stdlib.h>

#include "runner_internal.h"

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

bool BenchBuildMetricStats(const BenchmarkRecord *records, int record_count,
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
    out->valid = false;
    out->min = 0.0;
    out->max = 0.0;
    out->median = 0.0;
    out->p95 = 0.0;
    out->mean = 0.0;
    out->stddev = 0.0;
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

long long BenchMetricTime(const BenchmarkRecord *r) { return r->timeMs; }
long long BenchMetricCache(const BenchmarkRecord *r) { return r->cacheBytes; }
long long BenchMetricRssDelta(const BenchmarkRecord *r) {
  return r->rssDeltaBytes;
}
long long BenchMetricPeakRss(const BenchmarkRecord *r) {
  return r->peakRssBytes;
}
long long BenchMetricSimilarityReport(const BenchmarkRecord *r) {
  return r->similarityReportBytes;
}
