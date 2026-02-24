#include "fs_utils.h"
#include "gallery_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_files_scanned = 0;
static int g_files_cached = 0;

static bool ScanCallback(const char *absolute_path, void *user_data) {
  (void)user_data;
  g_files_scanned++;

  if (FsIsSupportedMedia(absolute_path)) {
    double mod_date = 0;
    long size = 0;

    if (ExtractBasicMetadata(absolute_path, &mod_date, &size)) {
      // Setup the DTO
      ImageMetadata md = {0};
      md.path = strdup(absolute_path);
      md.modificationDate = mod_date;
      md.fileSize = size;

      // For v0.1.0 we just mock the ML values to 0/false by default
      if (CacheUpdateEntry(&md)) {
        g_files_cached++;
      }
    }
  }

  return true; // Continue walking
}

int main(int argc, char **argv) {
  printf("CGalleryOrganizer v0.1.0-dev\n");

  if (argc < 2) {
    printf("Usage: %s <directory_to_scan>\n", argv[0]);
    return 1;
  }

  const char *target_dir = argv[1];

  // Initialize cache
  if (!CacheInit("gallery_cache.json")) {
    printf("Error: Failed to initialize cache.\n");
    return 1;
  }

  printf("Scanning directory: %s\n", target_dir);

  // Run the directory walker
  if (!FsWalkDirectory(target_dir, ScanCallback, NULL)) {
    printf("Error: Failed to walk directory '%s'.\n", target_dir);
    CacheShutdown();
    return 1;
  }

  // Save and shutdown
  if (!CacheSave()) {
    printf("Error: Failed to save cache.\n");
  }
  CacheShutdown();

  printf("Scan complete.\n");
  printf("Files scanned: %d\n", g_files_scanned);
  printf("Media files cached: %d\n", g_files_cached);

  return 0;
}
