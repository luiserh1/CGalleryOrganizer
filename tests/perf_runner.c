#define _DEFAULT_SOURCE
#include "duplicate_finder.h"
extern char *strdup(const char *s);
#include "fs_utils.h"
#include "gallery_cache.h"
#include "hash_utils.h"
#include "metadata_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

long long timeInMilliseconds(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (((long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

static bool ScanCallback(const char *absolute_path, void *user_data) {
  (void)user_data;
  if (FsIsSupportedMedia(absolute_path)) {
    double mod_date = 0;
    long size = 0;
    ExtractBasicMetadata(absolute_path, &mod_date, &size);

    // In stress tests, we always run exhaustive extraction as per user request
    ImageMetadata meta = ExtractMetadata(absolute_path, true);
    meta.path = strdup(absolute_path);
    meta.modificationDate = mod_date;
    meta.fileSize = size;

    // Compute hashes for stress testing duplicate detection
    ComputeFileMd5(absolute_path, meta.exactHashMd5);

    CacheUpdateEntry(&meta);
    CacheFreeMetadata(&meta);
  }
  return true;
}

int main(void) {
  const char *target_dir = "build/stress_data";
  const char *cache_file = "build/perf_cache.json";

  printf("========================================\n");
  printf(" CGalleryOrganizer Stress Test Suite\n");
  printf("========================================\n\n");

  // 1. Cold Run
  printf("[*] COLD RUN Benchmark (Extracting Exiv2 from raw files)...\n");
  long long start = timeInMilliseconds();

  if (!CacheInit(cache_file)) {
    printf("Error: Failed to initialize cache.\n");
    return 1;
  }

  FsWalkDirectory(target_dir, ScanCallback, NULL);
  long long end_cold = timeInMilliseconds();
  long long duration_cold = end_cold - start;
  printf("    -> Cold run took: %lld ms\n\n", duration_cold);

  CacheSave();
  CacheShutdown();

  // 2. Warm Run
  printf("[*] WARM RUN Benchmark (Loading purely from JSON cache)...\n");
  start = timeInMilliseconds();

  if (!CacheInit(cache_file)) {
    printf("Error: Failed to initialize cache.\n");
    return 1;
  }

  long long end_warm = timeInMilliseconds();
  long long duration_warm = end_warm - start;
  printf("    -> Warm run took: %lld ms\n\n", duration_warm);

  // 3. Duplicate Grouping (Algorithmic stress)
  printf("[*] ALGORITHMIC STRESS (Finding exact duplicates)...\n");
  start = timeInMilliseconds();
  DuplicateReport report = FindExactDuplicates();
  FreeDuplicateReport(&report);
  long long end_algo = timeInMilliseconds();
  long long duration_algo = end_algo - start;
  printf("    -> Duplicate grouping took: %lld ms\n\n", duration_algo);

  CacheSave();
  CacheShutdown();

  printf("========================================\n");
  printf(" SUCCESS. Performance logs ready.\n");
  printf("========================================\n");

  // Log to history file for long-term tracking
  FILE *log = fopen("build/benchmark_history.log", "a");
  if (log) {
    time_t now = time(NULL);
    char *date_str = ctime(&now);
    if (date_str[strlen(date_str) - 1] == '\n')
      date_str[strlen(date_str) - 1] = '\0';

    fprintf(
        log,
        "[%s] Cold: %lld ms | Warm: %lld ms | Algo: %lld ms | Images: 5000\n",
        date_str, duration_cold, duration_warm, duration_algo);
    fclose(log);
    printf("[*] Results appended to build/benchmark_history.log\n");
  }

  return 0;
}
