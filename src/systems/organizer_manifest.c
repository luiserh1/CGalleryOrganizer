#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON.h"
#include "organizer.h"
#include "systems/organizer_internal.h"

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

void OrganizerRemoveEmptyParents(const char *file_path, const char *stop_dir) {
  char dir[1024];
  OrganizerCopyStringBounded(dir, sizeof(dir), file_path);

  while (true) {
    char *slash = strrchr(dir, '/');
    if (!slash)
      break;
    *slash = '\0';

    if (strncmp(dir, stop_dir, strlen(stop_dir)) != 0) {
      break;
    }

    if (strlen(dir) <= strlen(stop_dir))
      break;

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

  for (int i = 0; i < array_size; i++) {
    cJSON *item = cJSON_GetArrayItem(json, i);
    cJSON *orig = cJSON_GetObjectItem(item, "original");
    cJSON *new_p = cJSON_GetObjectItem(item, "new");

    if (cJSON_IsString(orig) && cJSON_IsString(new_p)) {
      if (FsRenameFile(new_p->valuestring, orig->valuestring)) {
        success_count++;
        printf("  Restored: %s\n", orig->valuestring);
        OrganizerRemoveEmptyParents(new_p->valuestring, env_dir);
      } else {
        printf("  Failed to restore: %s (from %s)\n", orig->valuestring,
               new_p->valuestring);
      }
    }
  }

  cJSON_Delete(json);

  if (success_count == array_size) {
    FsDeleteFile(path);
  }

  return success_count;
}
