#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runner_internal.h"

bool BenchParseProfile(const char *profile, CacheCompressionMode *out_mode,
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

const char *BenchWorkloadToName(BenchmarkWorkload workload) {
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

bool BenchParseWorkload(const char *arg, BenchmarkWorkload *out_workload,
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
