#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cJSON.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "organizer.h"

static cJSON *g_manifest_array = NULL;
static char g_manifest_path[MAX_PATH_LENGTH] = {0};

static void CopyStringBounded(char *dst, size_t dst_size, const char *src) {
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

bool OrganizerInit(const char *env_dir) {
  if (!env_dir)
    return false;

  snprintf(g_manifest_path, sizeof(g_manifest_path), "%s/manifest.json",
           env_dir);

  g_manifest_array = cJSON_CreateArray();
  if (!g_manifest_array)
    return false;

  return true;
}

bool OrganizerRecordMove(const char *original_path, const char *new_path) {
  if (!g_manifest_array || !original_path || !new_path)
    return false;

  cJSON *entry = cJSON_CreateObject();
  cJSON_AddStringToObject(entry, "original", original_path);
  cJSON_AddStringToObject(entry, "new", new_path);

  cJSON_AddItemToArray(g_manifest_array, entry);
  return true;
}

bool OrganizerSaveManifest(void) {
  if (!g_manifest_array)
    return false;

  char *json_string = cJSON_Print(g_manifest_array);
  if (!json_string)
    return false;

  FILE *file = fopen(g_manifest_path, "w");
  if (!file) {
    cJSON_free(json_string);
    return false;
  }

  fputs(json_string, file);
  fclose(file);
  cJSON_free(json_string);
  return true;
}

void OrganizerShutdown(void) {
  if (g_manifest_array) {
    cJSON_Delete(g_manifest_array);
    g_manifest_array = NULL;
  }
}

static void RemoveEmptyParents(const char *file_path, const char *stop_dir) {
  char dir[1024];
  CopyStringBounded(dir, sizeof(dir), file_path);

  while (true) {
    char *slash = strrchr(dir, '/');
    if (!slash)
      break;
    *slash = '\0';

    if (strncmp(dir, stop_dir, strlen(stop_dir)) != 0) {
      break;
    }

    // Don't traverse up past the stop_dir (env_dir)
    if (strlen(dir) <= strlen(stop_dir))
      break;

    // Try to remove. rmdir only succeeds if the directory is empty.
    if (rmdir(dir) != 0) {
      break;
    }
  }
}

int OrganizerRollback(const char *env_dir) {
  if (!env_dir)
    return -1;

  char path[MAX_PATH_LENGTH];
  snprintf(path, sizeof(path), "%s/manifest.json", env_dir);

  FILE *file = fopen(path, "r");
  if (!file) {
    printf("Error: Could not open %s for rollback.\n", path);
    return -1;
  }

  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (length <= 0) {
    fclose(file);
    return 0;
  }

  char *data = (char *)malloc(length + 1);
  if (!data) {
    fclose(file);
    return -1;
  }

  size_t read_bytes = fread(data, 1, length, file);
  data[read_bytes] = '\0';
  fclose(file);

  cJSON *json = cJSON_Parse(data);
  free(data);

  if (!json) {
    printf("Error: Failed to parse %s.\n", path);
    return -1;
  }
  if (!cJSON_IsArray(json)) {
    printf("Error: Invalid manifest format in %s. Expected JSON array.\n", path);
    cJSON_Delete(json);
    return -1;
  }

  int success_count = 0;
  int array_size = cJSON_GetArraySize(json);

  // Validate the full manifest before applying any move.
  for (int i = 0; i < array_size; i++) {
    cJSON *item = cJSON_GetArrayItem(json, i);
    if (!cJSON_IsObject(item)) {
      printf("Error: Invalid manifest entry at index %d.\n", i);
      cJSON_Delete(json);
      return -1;
    }
    cJSON *orig = cJSON_GetObjectItem(item, "original");
    cJSON *new_p = cJSON_GetObjectItem(item, "new");
    if (!cJSON_IsString(orig) || !cJSON_IsString(new_p)) {
      printf("Error: Malformed manifest entry at index %d.\n", i);
      cJSON_Delete(json);
      return -1;
    }
  }

  // We should ideally rollback in reverse order if directories were created,
  // but since we are just moving files back to their absolute original paths,
  // any order works as long as the parent directories still exist.
  for (int i = 0; i < array_size; i++) {
    cJSON *item = cJSON_GetArrayItem(json, i);
    cJSON *orig = cJSON_GetObjectItem(item, "original");
    cJSON *new_p = cJSON_GetObjectItem(item, "new");

    if (cJSON_IsString(orig) && cJSON_IsString(new_p)) {
      if (FsRenameFile(new_p->valuestring, orig->valuestring)) {
        success_count++;
        printf("  Restored: %s\n", orig->valuestring);
        RemoveEmptyParents(new_p->valuestring, env_dir);
      } else {
        printf("  Failed to restore: %s (from %s)\n", orig->valuestring,
               new_p->valuestring);
      }
    }
  }

  cJSON_Delete(json);

  // Optional: delete the manifest after successful rollback
  if (success_count == array_size) {
    FsDeleteFile(path);
  }

  return success_count;
}

static void SanitizeFilename(char *name) {
  for (int i = 0; name[i]; i++) {
    if (name[i] == ' ' || name[i] == '/' || name[i] == '\\' || name[i] == ':') {
      name[i] = '_';
    }
  }
}

static void ResolveGroupCriteria(ImageMetadata *md, const char *key,
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
      SanitizeFilename(combined);
      snprintf(out_buffer, out_size, "%s", combined);
    }
  } else if (strcmp(key, "format") == 0) {
    const char *ext = strrchr(md->path, '.');
    snprintf(out_buffer, out_size, "_%s", ext ? ext + 1 : "Unknown_Format");
    SanitizeFilename(out_buffer);
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
    // Fallback for unknown valid keys
    snprintf(out_buffer, out_size, "_%s", key);
  }
}

static void BuildNewFilename(ImageMetadata *md, char *out_buffer,
                             size_t out_size) {
  // Target: YYYYMMDD_HHMMSS_CameraModel.ext
  char timestamp[64] = "UnknownDate";
  if (md->dateTaken[0] != '\0' && strlen(md->dateTaken) >= 19) {
    // 2024:08:15 14:30:00 -> 20240815_143000
    snprintf(timestamp, sizeof(timestamp), "%.4s%.2s%.2s_%.2s%.2s%.2s",
             md->dateTaken, md->dateTaken + 5, md->dateTaken + 8,
             md->dateTaken + 11, md->dateTaken + 14, md->dateTaken + 17);
  }

  char camera[128] = "UnknownCamera";
  if (md->cameraModel[0] != '\0') {
    strncpy(camera, md->cameraModel, sizeof(camera) - 1);
    SanitizeFilename(camera);
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
  // Initialize with a reasonable capacity
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
      CopyStringBounded(target_dir, sizeof(target_dir), env_dir);

      // Build compound path
      for (int k = 0; k < key_count; k++) {
        char criteria_buf[256];
        ResolveGroupCriteria(&md, group_keys[k], criteria_buf,
                             sizeof(criteria_buf));

        // Append to target_dir if not "date" which might return nested Output
        // (e.g. "_2022/_10") Ensure no trailing slashes overlap
        int cur_len = strlen(target_dir);
        if (target_dir[cur_len - 1] != '/') {
          strncat(target_dir, "/", sizeof(target_dir) - cur_len - 1);
        }

        // remove leading slash from criteria_buf if it has one (our builder
        // functions inject it, but just in case)
        if (criteria_buf[0] == '/') {
          strncat(target_dir, criteria_buf + 1,
                  sizeof(target_dir) - strlen(target_dir) - 1);
        } else {
          strncat(target_dir, criteria_buf,
                  sizeof(target_dir) - strlen(target_dir) - 1);
        }
      }

      // Build target File Name
      char target_filename[256];
      BuildNewFilename(&md, target_filename, sizeof(target_filename));

      // In the plan "new_path" we store the full absolute path of the new file
      char full_new_path[MAX_PATH_LENGTH];
      int cur_len = strlen(target_dir);
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

      CopyStringBounded(plan->moves[plan->count].original_path,
                        sizeof(plan->moves[plan->count].original_path), md.path);
      CopyStringBounded(plan->moves[plan->count].new_path,
                        sizeof(plan->moves[plan->count].new_path), full_new_path);
      plan->count++;

      CacheFreeMetadata(&md);
    }
  }

  if (keys) {
    CacheFreeKeys(keys, cache_key_count);
  }

  return plan;
}

void OrganizerFreePlan(OrganizerPlan *plan) {
  if (plan) {
    if (plan->moves)
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
  // Simple tree approximation for preview. A full tree would use a trie or hash
  // map. We will print the unique target directories and their file counts.

  // Sort the moves by target directory for grouping
  // Since new_path is now the full file path, we need to extract the dir
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
      // Find the relative segment (e.g., _2024/_01 or _Unknown)
      // Skip extracting leading slash of env_dir, show it relative
      const char *rel_start = strstr(current_dir, "/_");
      const char *display_path = rel_start ? rel_start + 1 : current_dir;

      printf("  - %-40s [%d files]\n", display_path, current_count);

      current_dir = dirs[i];
      current_count = 1;
    }
  }
  // Print the last group
  const char *rel_start = strstr(current_dir, "/_");
  const char *display_path = rel_start ? rel_start + 1 : current_dir;

  printf("  - %-40s [%d files]\n", display_path, current_count);
}

bool OrganizerExecutePlan(OrganizerPlan *plan) {
  if (!plan)
    return false;

  int success_count = 0;
  printf("\n[*] Executing Plan...\n");
  for (int i = 0; i < plan->count; i++) {
    char exact_new_path[MAX_PATH_LENGTH];
    CopyStringBounded(exact_new_path, sizeof(exact_new_path),
                      plan->moves[i].new_path);

    // Determine the base filename and extension
    char *dot = strrchr(exact_new_path, '.');
    char ext[32] = {0};
    if (dot) {
      strncpy(ext, dot, sizeof(ext) - 1);
      *dot = '\0'; // Temporarily truncate extension
    }

    // Check for collisions and increment counter (_1, _2, etc)
    struct stat st;
    int collision_idx = 1;
    char probe_path[MAX_PATH_LENGTH];

    // Re-attach extension for initial check
    snprintf(probe_path, sizeof(probe_path), "%s%s", exact_new_path, ext);

    while (stat(probe_path, &st) == 0) {
      // File exists, bump the suffix
      snprintf(probe_path, sizeof(probe_path), "%s_%d%s", exact_new_path,
               collision_idx, ext);
      collision_idx++;
    }

    // Now probe_path holds a collision-free absolute path. We need to create
    // the parent dir and move.
    char target_dir[MAX_PATH_LENGTH];
    CopyStringBounded(target_dir, sizeof(target_dir), probe_path);
    char *last_slash = strrchr(target_dir, '/');
    if (last_slash)
      *last_slash = '\0';
    FsMakeDirRecursive(target_dir);

    char moved_dir_path[MAX_PATH_LENGTH];
    if (FsMoveFile(plan->moves[i].original_path, target_dir, moved_dir_path,
                   sizeof(moved_dir_path))) {

      // Ensure the moved file takes the probe_path (FsMoveFile drops it in the
      // dir with its current name)
      if (FsRenameFile(moved_dir_path, probe_path)) {
        OrganizerRecordMove(plan->moves[i].original_path, probe_path);
        success_count++;
      } else {
        printf("  [!] Failed to finalize structured rename: %s -> %s\n",
               moved_dir_path, probe_path);
      }
    } else {
      printf("  [!] Failed to move: %s\n", plan->moves[i].original_path);
    }
  }

  OrganizerSaveManifest();
  printf("[*] Successfully moved %d out of %d files.\n", success_count,
         plan->count);
  return success_count > 0;
}
