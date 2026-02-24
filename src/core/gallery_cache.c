#include "gallery_cache.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static cJSON *g_cache_root = NULL;
static char g_cache_path[1024] = {0};

bool CacheInit(const char *cache_path) {
  if (!cache_path)
    return false;

  strncpy(g_cache_path, cache_path, sizeof(g_cache_path) - 1);

  FILE *f = fopen(cache_path, "r");
  if (f) {
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (length > 0) {
      char *content = malloc(length + 1);
      if (content) {
        fread(content, 1, length, f);
        content[length] = '\0';
        g_cache_root = cJSON_Parse(content);
        free(content);
      }
    }
    fclose(f);
  }

  if (!g_cache_root) {
    g_cache_root = cJSON_CreateObject(); // Empty cache
  }
  return true;
}

void CacheShutdown(void) {
  if (g_cache_root) {
    cJSON_Delete(g_cache_root);
    g_cache_root = NULL;
  }
  g_cache_path[0] = '\0';
}

bool CacheSave(void) {
  if (!g_cache_root || g_cache_path[0] == '\0')
    return false;

  char *json_str =
      cJSON_PrintUnformatted(g_cache_root); // or cJSON_Print for pretty
  if (!json_str)
    return false;

  FILE *f = fopen(g_cache_path, "w");
  if (!f) {
    free(json_str);
    return false;
  }

  fputs(json_str, f);
  fclose(f);
  free(json_str);

  return true;
}

bool CacheUpdateEntry(const ImageMetadata *data) {
  if (!g_cache_root || !data || !data->path)
    return false;

  cJSON *entry = cJSON_CreateObject();

  cJSON_AddStringToObject(entry, "path", data->path);
  cJSON_AddNumberToObject(entry, "modificationDate", data->modificationDate);
  cJSON_AddNumberToObject(entry, "fileSize", (double)data->fileSize);

  // Replace if exists, otherwise add
  if (cJSON_GetObjectItem(g_cache_root, data->path)) {
    cJSON_ReplaceItemInObject(g_cache_root, data->path, entry);
  } else {
    cJSON_AddItemToObject(g_cache_root, data->path, entry);
  }

  return true;
}

bool CacheGetValidEntry(const char *absolute_path, double current_mod_date,
                        long current_size, ImageMetadata *out_md) {
  if (!g_cache_root || !absolute_path || !out_md)
    return false;

  cJSON *entry = cJSON_GetObjectItem(g_cache_root, absolute_path);
  if (!entry)
    return false;

  cJSON *modDateNode = cJSON_GetObjectItem(entry, "modificationDate");
  cJSON *fileSizeNode = cJSON_GetObjectItem(entry, "fileSize");

  if (!modDateNode || !fileSizeNode)
    return false;

  if (modDateNode->valuedouble != current_mod_date)
    return false;
  if ((long)fileSizeNode->valuedouble != current_size)
    return false;

  // Populate fields
  memset(out_md, 0, sizeof(ImageMetadata));
  out_md->path = strdup(absolute_path);
  out_md->modificationDate = current_mod_date;
  out_md->fileSize = current_size;

  return true;
}

bool ExtractBasicMetadata(const char *absolute_path, double *out_mod_date,
                          long *out_size) {
  if (!absolute_path || !out_mod_date || !out_size)
    return false;

  struct stat st;
  if (stat(absolute_path, &st) != 0) {
    return false; // File doesn't exist or permissions error
  }

  *out_size = (long)st.st_size;

  // st_mtimespec (macOS struct stat) vs st_mtim (linux)
  // Actually POSIX uses st_mtime for seconds. Using simply seconds is safer for
  // cross-compilation or we can use the microsecond parts if available via
  // #ifdefs. For now we use st_mtime (unix timestamp in seconds) which is
  // standard C POSIX
  *out_mod_date = (double)st.st_mtime;

  return true;
}
