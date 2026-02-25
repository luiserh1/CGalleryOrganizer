#include "duplicate_finder.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "metadata_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

long long timeInMilliseconds(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (((long long)tv.tv_sec) * 1000) + (tv.tv_usec / 1000);
}

static bool ScanCallback(const char *absolute_path, void *user_data) {
  (void)user_data;
  if (FsIsSupportedMedia(absolute_path)) {
    ImageMetadata meta = ExtractMetadata(absolute_path);
    CacheUpdateEntry(&meta);
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
  printf("    -> Cold run took: %lld ms\n\n", end_cold - start);

  CacheShutdown();

  // 2. Warm Run
  printf("[*] WARM RUN Benchmark (Loading purely from JSON cache)...\n");
  start = timeInMilliseconds();

  if (!CacheInit(cache_file)) {
    printf("Error: Failed to initialize cache.\n");
    return 1;
  }

  long long end_warm = timeInMilliseconds();
  printf("    -> Warm run took: %lld ms\n\n", end_warm - start);

  // 3. Duplicate Grouping (Algorithmic stress)
  printf("[*] ALGORITHMIC STRESS (Finding exact duplicates)...\n");
  start = timeInMilliseconds();
  DuplicateReport report = FindExactDuplicates();
  FreeDuplicateReport(&report);
  long long end_algo = timeInMilliseconds();
  printf("    -> Duplicate grouping took: %lld ms\n\n", end_algo - start);

  CacheShutdown();

  printf("========================================\n");
  printf(" SUCCESS. Performance logs ready.\n");
  printf("========================================\n");

  return 0;
}
