#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON.h"
#include "fs_utils.h"
#include "gallery_cache.h"
#include "organizer.h"

static cJSON *g_manifest_array = NULL;
static char g_manifest_path[MAX_PATH_LENGTH] = {0};

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
  strncpy(dir, file_path, sizeof(dir) - 1);
  dir[sizeof(dir) - 1] = '\0';

  while (true) {
    char *slash = strrchr(dir, '/');
    if (!slash)
      break;
    *slash = '\0';

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

  int success_count = 0;
  int array_size = cJSON_GetArraySize(json);

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

static void ExtractYearMonth(const char *dateTaken, char *out_year,
                             char *out_month) {
  // Expected format: YYYY:MM:DD HH:MM:SS
  if (!dateTaken || strlen(dateTaken) < 7) {
    strcpy(out_year, "Unknown");
    strcpy(out_month, "Unknown");
    return;
  }

  // YYYY
  strncpy(out_year, dateTaken, 4);
  out_year[4] = '\0';

  // MM
  strncpy(out_month, dateTaken + 5, 2);
  out_month[2] = '\0';
}

OrganizerPlan *OrganizerComputePlan(const char *env_dir) {
  if (!env_dir)
    return NULL;

  OrganizerPlan *plan = (OrganizerPlan *)malloc(sizeof(OrganizerPlan));
  if (!plan)
    return NULL;

  plan->count = 0;
  // Initialize with a reasonable capacity
  int max_keys = CacheGetEntryCount();
  plan->capacity = max_keys > 0 ? max_keys : 100;
  plan->moves = (OrganizerMove *)malloc(sizeof(OrganizerMove) * plan->capacity);

  int key_count = 0;
  char **keys = CacheGetAllKeys(&key_count);

  for (int i = 0; i < key_count; i++) {
    ImageMetadata md = {0};
    if (CacheGetRawEntry(keys[i], &md)) {
      char year[16] = {0};
      char month[16] = {0};
      ExtractYearMonth(md.dateTaken, year, month);

      char target_dir[MAX_PATH_LENGTH];
      if (strcmp(year, "Unknown") == 0) {
        snprintf(target_dir, sizeof(target_dir), "%s/_Unknown", env_dir);
      } else {
        snprintf(target_dir, sizeof(target_dir), "%s/_%s/_%s", env_dir, year,
                 month);
      }

      if (plan->count >= plan->capacity) {
        plan->capacity *= 2;
        plan->moves = (OrganizerMove *)realloc(
            plan->moves, sizeof(OrganizerMove) * plan->capacity);
      }

      strncpy(plan->moves[plan->count].original_path, md.path, 1023);
      strncpy(plan->moves[plan->count].new_path, target_dir,
              1023); // Storing the target directory here
      plan->count++;

      CacheFreeMetadata(&md);
    }
  }

  if (keys) {
    CacheFreeKeys(keys, key_count);
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
  for (int i = 0; i < plan->count - 1; i++) {
    for (int j = i + 1; j < plan->count; j++) {
      if (strcmp(plan->moves[i].new_path, plan->moves[j].new_path) > 0) {
        OrganizerMove temp = plan->moves[i];
        plan->moves[i] = plan->moves[j];
        plan->moves[j] = temp;
      }
    }
  }

  const char *current_dir = plan->moves[0].new_path;
  int current_count = 0;

  for (int i = 0; i < plan->count; i++) {
    if (strcmp(plan->moves[i].new_path, current_dir) == 0) {
      current_count++;
    } else {
      // Find the relative segment (e.g., _2024/_01 or _Unknown)
      const char *rel_start = strstr(current_dir, "/_");
      const char *display_path =
          rel_start ? rel_start + 1 : current_dir; // skip the leading slash
      printf("  - %-20s [%d files]\n", display_path, current_count);

      current_dir = plan->moves[i].new_path;
      current_count = 1;
    }
  }
  // Print the last group
  const char *rel_start = strstr(current_dir, "/_");
  const char *display_path = rel_start ? rel_start + 1 : current_dir;
  printf("  - %-20s [%d files]\n", display_path, current_count);
}

bool OrganizerExecutePlan(OrganizerPlan *plan) {
  if (!plan)
    return false;

  int success_count = 0;
  printf("\n[*] Executing Plan...\n");
  for (int i = 0; i < plan->count; i++) {
    char exact_new_path[MAX_PATH_LENGTH] = {0};

    if (FsMoveFile(plan->moves[i].original_path, plan->moves[i].new_path,
                   exact_new_path, sizeof(exact_new_path))) {
      OrganizerRecordMove(plan->moves[i].original_path, exact_new_path);
      success_count++;
    } else {
      printf("  [!] Failed to move: %s\n", plan->moves[i].original_path);
    }
  }

  OrganizerSaveManifest();
  printf("[*] Successfully moved %d out of %d files.\n", success_count,
         plan->count);
  return success_count > 0;
}
