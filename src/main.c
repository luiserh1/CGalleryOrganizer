#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_utils.h"
#include "gallery_cache.h"
#include "metadata_parser.h"

static int g_files_scanned = 0;
static int g_files_cached = 0;

static bool ScanCallback(const char *absolute_path, void *user_data) {
  (void)user_data;
  g_files_scanned++;

  if (FsIsSupportedMedia(absolute_path)) {
    double mod_date = 0;
    long size = 0;

    if (ExtractBasicMetadata(absolute_path, &mod_date, &size)) {
      ImageMetadata md = {0};
      md.path = strdup(absolute_path);
      md.modificationDate = mod_date;
      md.fileSize = size;

      ImageMetadata extracted = ExtractMetadata(absolute_path);
      if (extracted.width > 0) {
        md.width = extracted.width;
      }
      if (extracted.height > 0) {
        md.height = extracted.height;
      }
      if (extracted.dateTaken[0] != '\0') {
        strncpy(md.dateTaken, extracted.dateTaken, METADATA_MAX_STRING - 1);
      }
      if (extracted.cameraMake[0] != '\0') {
        strncpy(md.cameraMake, extracted.cameraMake, METADATA_MAX_STRING - 1);
      }
      if (extracted.cameraModel[0] != '\0') {
        strncpy(md.cameraModel, extracted.cameraModel, METADATA_MAX_STRING - 1);
      }
      md.orientation = extracted.orientation;
      md.hasGps = extracted.hasGps;
      md.gpsLatitude = extracted.gpsLatitude;
      md.gpsLongitude = extracted.gpsLongitude;

      if (CacheUpdateEntry(&md)) {
        g_files_cached++;
      }
    }
  }

  return true;
}

int main(int argc, char **argv) {
  printf("CGalleryOrganizer v0.1.6-dev\n");

  if (argc < 2) {
    printf("Usage: %s <directory_to_scan>\n", argv[0]);
    return 1;
  }

  if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    printf("Usage: %s <directory_to_scan>\n", argv[0]);
    printf("\n");
    printf("Options:\n");
    printf("  -h, --help    Show this help message and exit\n");
    return 0;
  }

  const char *target_dir = argv[1];

  if (!CacheInit("gallery_cache.json")) {
    printf("Error: Failed to initialize cache.\n");
    return 1;
  }

  printf("Scanning directory: %s\n", target_dir);

  if (!FsWalkDirectory(target_dir, ScanCallback, NULL)) {
    printf("Error: Failed to walk directory '%s'.\n", target_dir);
    CacheShutdown();
    return 1;
  }

  if (!CacheSave()) {
    printf("Error: Failed to save cache.\n");
  }
  CacheShutdown();

  printf("Scan complete.\n");
  printf("Files scanned: %d\n", g_files_scanned);
  printf("Media files cached: %d\n", g_files_cached);

  return 0;
}
