#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gallery_cache.h"
#include "organizer.h"
#include "systems/organizer_internal.h"

void OrganizerCopyStringBounded(char *dst, size_t dst_size, const char *src) {
  if (!dst || dst_size == 0) {
    return;
  }
  if (!src) {
    dst[0] = '\0';
    return;
  }
  strncpy(dst, src, dst_size - 1);
  dst[dst_size - 1] = '\0';
}

void OrganizerSanitizeFilename(char *name) {
  for (int i = 0; name[i]; i++) {
    if (name[i] == ' ' || name[i] == '/' || name[i] == '\\' || name[i] == ':') {
      name[i] = '_';
    }
  }
}

void OrganizerResolveGroupCriteria(ImageMetadata *md, const char *key,
                                   char *out_buffer, size_t out_size) {
  if (strcmp(key, "date") == 0) {
    if (md->dateTaken[0] == '\0' || strlen(md->dateTaken) < 7) {
      snprintf(out_buffer, out_size, "_Unknown");
    } else {
      char year[5] = {0};
      char month[3] = {0};
      strncpy(year, md->dateTaken, 4);
      strncpy(month, md->dateTaken + 5, 2);
      snprintf(out_buffer, out_size, "_%s/_%s", year, month);
    }
  } else if (strcmp(key, "camera") == 0) {
    if (md->cameraMake[0] == '\0' && md->cameraModel[0] == '\0') {
      snprintf(out_buffer, out_size, "_Unknown_Camera");
    } else {
      char combined[256];
      snprintf(combined, sizeof(combined), "_%s_%s", md->cameraMake,
               md->cameraModel);
      OrganizerSanitizeFilename(combined);
      snprintf(out_buffer, out_size, "%s", combined);
    }
  } else if (strcmp(key, "format") == 0) {
    const char *ext = strrchr(md->path, '.');
    snprintf(out_buffer, out_size, "_%s", ext ? ext + 1 : "Unknown_Format");
    OrganizerSanitizeFilename(out_buffer);
  } else if (strcmp(key, "orientation") == 0) {
    if (md->width > md->height) {
      snprintf(out_buffer, out_size, "_Landscape");
    } else if (md->height > md->width) {
      snprintf(out_buffer, out_size, "_Portrait");
    } else if (md->width > 0 && md->width == md->height) {
      snprintf(out_buffer, out_size, "_Square");
    } else {
      snprintf(out_buffer, out_size, "_Unknown_Orientation");
    }
  } else if (strcmp(key, "resolution") == 0) {
    int max_dim = md->width > md->height ? md->width : md->height;
    if (max_dim >= 3840)
      snprintf(out_buffer, out_size, "_4K_UHD");
    else if (max_dim >= 1920)
      snprintf(out_buffer, out_size, "_1080p_FHD");
    else if (max_dim > 0)
      snprintf(out_buffer, out_size, "_Web");
    else
      snprintf(out_buffer, out_size, "_Unknown_Resolution");
  } else {
    snprintf(out_buffer, out_size, "_%s", key);
  }
}

void OrganizerBuildNewFilename(ImageMetadata *md, char *out_buffer,
                               size_t out_size) {
  char timestamp[64] = "UnknownDate";
  if (md->dateTaken[0] != '\0' && strlen(md->dateTaken) >= 19) {
    snprintf(timestamp, sizeof(timestamp), "%.4s%.2s%.2s_%.2s%.2s%.2s",
             md->dateTaken, md->dateTaken + 5, md->dateTaken + 8,
             md->dateTaken + 11, md->dateTaken + 14, md->dateTaken + 17);
  }

  char camera[128] = "UnknownCamera";
  if (md->cameraModel[0] != '\0') {
    strncpy(camera, md->cameraModel, sizeof(camera) - 1);
    OrganizerSanitizeFilename(camera);
  }

  const char *ext = strrchr(md->path, '.');
  if (!ext)
    ext = "";

  snprintf(out_buffer, out_size, "%s_%s%s", timestamp, camera, ext);
}

OrganizerPlan *OrganizerComputePlan(const char *env_dir,
                                    const char **group_keys, int key_count) {
  if (!env_dir || !group_keys || key_count <= 0)
    return NULL;

  OrganizerPlan *plan = (OrganizerPlan *)malloc(sizeof(OrganizerPlan));
  if (!plan)
    return NULL;

  plan->count = 0;
  int max_keys = CacheGetEntryCount();
  plan->capacity = max_keys > 0 ? max_keys : 100;
  plan->moves = (OrganizerMove *)malloc(sizeof(OrganizerMove) * plan->capacity);
  if (!plan->moves) {
    free(plan);
    return NULL;
  }

  int cache_key_count = 0;
  char **keys = CacheGetAllKeys(&cache_key_count);
  if (!keys || cache_key_count <= 0) {
    plan->count = 0;
    return plan;
  }

  for (int i = 0; i < cache_key_count; i++) {
    ImageMetadata md = {0};
    if (CacheGetRawEntry(keys[i], &md)) {
      char target_dir[MAX_PATH_LENGTH];
      OrganizerCopyStringBounded(target_dir, sizeof(target_dir), env_dir);

      for (int k = 0; k < key_count; k++) {
        char criteria_buf[256];
        OrganizerResolveGroupCriteria(&md, group_keys[k], criteria_buf,
                                      sizeof(criteria_buf));

        int cur_len = (int)strlen(target_dir);
        if (target_dir[cur_len - 1] != '/') {
          strncat(target_dir, "/", sizeof(target_dir) - cur_len - 1);
        }

        if (criteria_buf[0] == '/') {
          strncat(target_dir, criteria_buf + 1,
                  sizeof(target_dir) - strlen(target_dir) - 1);
        } else {
          strncat(target_dir, criteria_buf,
                  sizeof(target_dir) - strlen(target_dir) - 1);
        }
      }

      char target_filename[256];
      OrganizerBuildNewFilename(&md, target_filename, sizeof(target_filename));

      char full_new_path[MAX_PATH_LENGTH];
      int cur_len = (int)strlen(target_dir);
      if (target_dir[cur_len - 1] != '/') {
        snprintf(full_new_path, sizeof(full_new_path), "%s/%s", target_dir,
                 target_filename);
      } else {
        snprintf(full_new_path, sizeof(full_new_path), "%s%s", target_dir,
                 target_filename);
      }

      if (plan->count >= plan->capacity) {
        plan->capacity *= 2;
        OrganizerMove *new_moves =
            (OrganizerMove *)realloc(plan->moves,
                                     sizeof(OrganizerMove) * plan->capacity);
        if (!new_moves) {
          CacheFreeMetadata(&md);
          CacheFreeKeys(keys, cache_key_count);
          free(plan->moves);
          free(plan);
          return NULL;
        }
        plan->moves = new_moves;
      }

      OrganizerCopyStringBounded(plan->moves[plan->count].original_path,
                                 sizeof(plan->moves[plan->count].original_path),
                                 md.path);
      OrganizerCopyStringBounded(plan->moves[plan->count].new_path,
                                 sizeof(plan->moves[plan->count].new_path),
                                 full_new_path);
      plan->count++;

      CacheFreeMetadata(&md);
    }
  }

  CacheFreeKeys(keys, cache_key_count);
  return plan;
}

void OrganizerFreePlan(OrganizerPlan *plan) {
  if (plan) {
    free(plan->moves);
    free(plan);
  }
}

void OrganizerPrintPlan(OrganizerPlan *plan) {
  if (!plan || plan->count == 0) {
    printf("Plan is empty.\n");
    return;
  }

  printf("Target Environment Tree Preview:\n");

  char dirs[plan->count][1024];
  for (int i = 0; i < plan->count; i++) {
    strncpy(dirs[i], plan->moves[i].new_path, 1023);
    char *slash = strrchr(dirs[i], '/');
    if (slash)
      *slash = '\0';
  }

  for (int i = 0; i < plan->count - 1; i++) {
    for (int j = i + 1; j < plan->count; j++) {
      if (strcmp(dirs[i], dirs[j]) > 0) {
        OrganizerMove temp_m = plan->moves[i];
        plan->moves[i] = plan->moves[j];
        plan->moves[j] = temp_m;

        char temp_d[1024];
        strcpy(temp_d, dirs[i]);
        strcpy(dirs[i], dirs[j]);
        strcpy(dirs[j], temp_d);
      }
    }
  }

  const char *current_dir = dirs[0];
  int current_count = 0;

  for (int i = 0; i < plan->count; i++) {
    if (strcmp(dirs[i], current_dir) == 0) {
      current_count++;
    } else {
      const char *rel_start = strstr(current_dir, "/_");
      const char *display_path = rel_start ? rel_start + 1 : current_dir;
      printf("  - %-40s [%d files]\n", display_path, current_count);
      current_dir = dirs[i];
      current_count = 1;
    }
  }

  const char *rel_start = strstr(current_dir, "/_");
  const char *display_path = rel_start ? rel_start + 1 : current_dir;
  printf("  - %-40s [%d files]\n", display_path, current_count);
}
