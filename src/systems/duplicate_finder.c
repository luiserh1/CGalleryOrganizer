#include "duplicate_finder.h"
#include "hash_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper to sort ImageMetadata array by MD5 hash
static int CompareImageMd5(const void *a, const void *b) {
  const ImageMetadata *imgA = *(const ImageMetadata **)a;
  const ImageMetadata *imgB = *(const ImageMetadata **)b;
  return strcmp(imgA->exactHashMd5, imgB->exactHashMd5);
}

DuplicateReport FindExactDuplicates(void) {
  DuplicateReport report = {NULL, 0};

  int key_count = 0;
  char **keys = CacheGetAllKeys(&key_count);
  printf("[DEBUG] CacheGetAllKeys returned %d keys\n", key_count);
  if (!keys || key_count == 0) {
    return report;
  }

  // 1. Load all elements with valid MD5s
  ImageMetadata **entries = malloc(key_count * sizeof(ImageMetadata *));
  int valid_entries = 0;

  for (int i = 0; i < key_count; i++) {
    ImageMetadata *md = malloc(sizeof(ImageMetadata));
    if (CacheGetRawEntry(keys[i], md)) {
      if (md->exactHashMd5[0] != '\0') {
        entries[valid_entries++] = md;
      } else {
        free(md->path);
        free(md);
      }
    } else {
      free(md);
    }
  }
  CacheFreeKeys(keys, key_count);
  printf(
      "[DEBUG] Found %d valid entries with MD5 out of %d total cache keys.\n",
      valid_entries, key_count);

  if (valid_entries < 2) {
    for (int i = 0; i < valid_entries; i++) {
      free(entries[i]->path);
      free(entries[i]);
    }
    free(entries);
    return report;
  }

  // 2. Sort entries by MD5 hash to group exact matches in O(N log N)
  qsort(entries, valid_entries, sizeof(ImageMetadata *), CompareImageMd5);

  // 3. Iterate through sorted array and group duplicates
  int capacity = 10;
  report.groups = malloc(capacity * sizeof(DuplicateGroup));

  for (int i = 0; i < valid_entries - 1;) {
    int j = i + 1;

    // Find extent of the adjacent MD5 matches
    while (j < valid_entries &&
           strcmp(entries[i]->exactHashMd5, entries[j]->exactHashMd5) == 0) {
      j++;
    }

    int group_size = j - i;
    if (group_size > 1) {
      printf("[DEBUG] Found MD5 collision group of size %d starting at index "
             "%d (MD5: %s)\n",
             group_size, i, entries[i]->exactHashMd5);
      // We have an MD5 collision group. Now we must verify with SHA-256.
      // In theory, there could be an MD5 collision that is NOT a SHA-256
      // collision. For simplicity, we compute SHA-256 for the group base.
      char base_sha256[SHA256_HASH_LEN + 1] = {0};
      if (ComputeFileSha256(entries[i]->path, base_sha256)) {

        int dup_capacity = group_size;
        char **dup_paths = malloc(dup_capacity * sizeof(char *));
        int dup_count = 0;

        // The first valid file is our original
        int original_idx = i;

        for (int k = i + 1; k < j; k++) {
          char current_sha256[SHA256_HASH_LEN + 1] = {0};
          if (ComputeFileSha256(entries[k]->path, current_sha256)) {
            if (strcmp(base_sha256, current_sha256) == 0) {
              // Exact match verified!
              dup_paths[dup_count++] = strdup(entries[k]->path);
            }
          }
        }

        if (dup_count > 0) {
          if (report.group_count >= capacity) {
            capacity *= 2;
            report.groups =
                realloc(report.groups, capacity * sizeof(DuplicateGroup));
          }
          strncpy(report.groups[report.group_count].original_path,
                  entries[original_idx]->path, 1023);
          report.groups[report.group_count].duplicate_paths = dup_paths;
          report.groups[report.group_count].duplicate_count = dup_count;
          report.group_count++;
        } else {
          free(dup_paths);
        }
      }
    }

    i = j; // Move to next distinct MD5 block
  }

  // Free all allocated entries
  for (int i = 0; i < valid_entries; i++) {
    free(entries[i]->path);
    free(entries[i]);
  }
  free(entries);

  return report;
}

void FreeDuplicateReport(DuplicateReport *report) {
  if (!report || !report->groups)
    return;

  for (int i = 0; i < report->group_count; i++) {
    for (int j = 0; j < report->groups[i].duplicate_count; j++) {
      free(report->groups[i].duplicate_paths[j]);
    }
    free(report->groups[i].duplicate_paths);
  }
  free(report->groups);
  report->groups = NULL;
  report->group_count = 0;
}
