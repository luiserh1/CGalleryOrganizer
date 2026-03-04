#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "systems/renamer_preview_internal.h"

void RenamerPreviewBuildPreviewId(const char *fingerprint, char *out_preview_id,
                                  size_t out_preview_id_size) {
  if (!out_preview_id || out_preview_id_size == 0) {
    return;
  }

  char timestamp[32] = {0};
  time_t now = time(NULL);
  struct tm tm_utc;
#if defined(_WIN32)
  gmtime_s(&tm_utc, &now);
#else
  gmtime_r(&now, &tm_utc);
#endif
  strftime(timestamp, sizeof(timestamp), "%Y%m%d%H%M%S", &tm_utc);

  const char *suffix = "00000000";
  if (fingerprint && strlen(fingerprint) >= 8) {
    suffix = fingerprint;
  }
  snprintf(out_preview_id, out_preview_id_size, "rnp-%s-%.8s", timestamp,
           suffix);
}

bool RenamerPreviewBuildParentDir(const char *path, char *out_dir,
                                  size_t out_dir_size) {
  if (!path || !out_dir || out_dir_size == 0) {
    return false;
  }

  strncpy(out_dir, path, out_dir_size - 1);
  out_dir[out_dir_size - 1] = '\0';
  char *slash = strrchr(out_dir, '/');
  if (!slash) {
    return false;
  }

  if (slash == out_dir) {
    slash[1] = '\0';
  } else {
    *slash = '\0';
  }
  return true;
}

void RenamerPreviewDetectCollisions(RenamerPreviewArtifact *preview) {
  if (!preview || !preview->items || preview->file_count <= 0) {
    return;
  }

  preview->collision_group_count = 0;
  preview->collision_count = 0;

  char **group_paths = calloc((size_t)preview->file_count, sizeof(char *));
  int group_count = 0;

  for (int i = 0; i < preview->file_count; i++) {
    preview->items[i].collision_in_batch = false;
    preview->items[i].collision_on_disk = false;

    if (access(preview->items[i].candidate_path, F_OK) == 0 &&
        strcmp(preview->items[i].source_path, preview->items[i].candidate_path) !=
            0) {
      preview->items[i].collision_on_disk = true;
    }
  }

  for (int i = 0; i < preview->file_count; i++) {
    for (int j = i + 1; j < preview->file_count; j++) {
      if (strcmp(preview->items[i].candidate_path,
                 preview->items[j].candidate_path) == 0) {
        preview->items[i].collision_in_batch = true;
        preview->items[j].collision_in_batch = true;
      }
    }
  }

  for (int i = 0; i < preview->file_count; i++) {
    if (!(preview->items[i].collision_in_batch ||
          preview->items[i].collision_on_disk)) {
      continue;
    }

    preview->collision_count++;

    bool seen = false;
    for (int j = 0; j < group_count; j++) {
      if (strcmp(group_paths[j], preview->items[i].candidate_path) == 0) {
        seen = true;
        break;
      }
    }
    if (!seen) {
      group_paths[group_count] = strdup(preview->items[i].candidate_path);
      if (group_paths[group_count]) {
        group_count++;
      }
    }
  }

  preview->collision_group_count = group_count;
  preview->requires_auto_suffix_acceptance = preview->collision_count > 0;

  for (int i = 0; i < group_count; i++) {
    free(group_paths[i]);
  }
  free(group_paths);
}
