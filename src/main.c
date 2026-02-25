#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "duplicate_finder.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "hash_utils.h"
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

      bool exhaustive = (user_data != NULL) ? *(bool *)user_data : false;

      if (CacheGetValidEntry(absolute_path, mod_date, size, &md)) {
        if (md.exactHashMd5[0] == '\0' ||
            (exhaustive && md.allMetadataJson == NULL)) {
          // If hash is missing OR we want exhaustive but don't have it, re-scan
          ImageMetadata full = ExtractMetadata(absolute_path, exhaustive);
          md.width = full.width;
          md.height = full.height;
          strncpy(md.dateTaken, full.dateTaken, METADATA_MAX_STRING - 1);
          strncpy(md.cameraMake, full.cameraMake, METADATA_MAX_STRING - 1);
          strncpy(md.cameraModel, full.cameraModel, METADATA_MAX_STRING - 1);
          md.orientation = full.orientation;
          md.hasGps = full.hasGps;
          md.gpsLatitude = full.gpsLatitude;
          md.gpsLongitude = full.gpsLongitude;
          if (full.allMetadataJson) {
            if (md.allMetadataJson)
              free(md.allMetadataJson);
            md.allMetadataJson = strdup(full.allMetadataJson);
          }
          if (md.exactHashMd5[0] == '\0')
            ComputeFileMd5(absolute_path, md.exactHashMd5);
          CacheUpdateEntry(&md);
          CacheFreeMetadata(&full);
        }
        g_files_cached++;
        CacheFreeMetadata(&md);
        return true;
      }

      md.path = strdup(absolute_path);
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

      if (extracted.allMetadataJson) {
        md.allMetadataJson = strdup(extracted.allMetadataJson);
      }

      ComputeFileMd5(absolute_path, md.exactHashMd5);

      if (CacheUpdateEntry(&md)) {
        g_files_cached++;
      }
      CacheFreeMetadata(&md);
      CacheFreeMetadata(&extracted);
    }
  }

  return true;
}

int main(int argc, char **argv) {
  printf("CGalleryOrganizer v0.1.6-dev\n");

  bool exhaustive = false;
  const char *target_dir = NULL;
  const char *duplicates_target_dir = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      printf(
          "Usage: %s <directory_to_scan> [duplicates_target_dir] [options]\n",
          argv[0]);
      printf("\n");
      printf("Options:\n");
      printf("  -h, --help        Show this help message and exit\n");
      printf("  -e, --exhaustive  Extract all metadata tags (larger cache)\n");
      return 0;
    } else if (strcmp(argv[i], "-e") == 0 ||
               strcmp(argv[i], "--exhaustive") == 0) {
      exhaustive = true;
    } else if (target_dir == NULL) {
      target_dir = argv[i];
    } else if (duplicates_target_dir == NULL) {
      duplicates_target_dir = argv[i];
    }
  }

  if (target_dir == NULL) {
    printf("Usage: %s <directory_to_scan> [duplicates_target_dir] [options]\n",
           argv[0]);
    return 1;
  }

  mkdir(".cache", 0755);
  if (!CacheInit(".cache/gallery_cache.json")) {
    printf("Error: Failed to initialize cache.\n");
    return 1;
  }

  printf("Scanning directory: %s (Exhaustive: %s)\n", target_dir,
         exhaustive ? "ON" : "OFF");

  if (!FsWalkDirectory(target_dir, ScanCallback, &exhaustive)) {
    printf("Error: Failed to walk directory '%s'.\n", target_dir);
    CacheShutdown();
    return 1;
  }

  if (!CacheSave()) {
    printf("Error: Failed to save cache.\n");
  }

  printf("Scan complete.\n");
  printf("Files scanned: %d\n", g_files_scanned);
  printf("Media files cached: %d\n\n", g_files_cached);

  DuplicateReport rep = FindExactDuplicates();
  int moved_count = 0;

  if (rep.group_count > 0) {
    if (duplicates_target_dir) {
      printf("Found %d groups of exact duplicates. Moving copies to '%s'...\n",
             rep.group_count, duplicates_target_dir);
      for (int i = 0; i < rep.group_count; i++) {
        for (int j = 0; j < rep.groups[i].duplicate_count; j++) {
          char moved_path[1024] = {0};
          if (FsMoveFile(rep.groups[i].duplicate_paths[j],
                         duplicates_target_dir, moved_path,
                         sizeof(moved_path))) {
            printf("  Moved: %s -> %s\n", rep.groups[i].duplicate_paths[j],
                   moved_path);
            moved_count++;
          } else {
            printf("  Failed to move: %s\n", rep.groups[i].duplicate_paths[j]);
          }
        }
      }
      printf("Successfully moved %d duplicate files.\n", moved_count);
    } else {
      printf("Found %d groups of exact duplicates (dry-run):\n",
             rep.group_count);
      for (int i = 0; i < rep.group_count; i++) {
        printf("  Group %d (Original: %s):\n", i + 1,
               rep.groups[i].original_path);
        for (int j = 0; j < rep.groups[i].duplicate_count; j++) {
          printf("    Duplicate: %s\n", rep.groups[i].duplicate_paths[j]);
        }
      }
      printf("\nTo automatically move these duplicates, provide a target "
             "directory as the second argument.\n");
    }
  } else {
    printf("No exact duplicates found.\n");
  }

  FreeDuplicateReport(&rep);
  CacheShutdown();

  return 0;
}
