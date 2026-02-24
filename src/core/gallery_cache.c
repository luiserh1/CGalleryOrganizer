#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gallery_cache.h"

#include "cJSON.h"

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

  char *json_str = cJSON_PrintUnformatted(g_cache_root);
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

  // EXIF fields
  if (data->dateTaken[0] != '\0')
    cJSON_AddStringToObject(entry, "dateTaken", data->dateTaken);
  if (data->width > 0)
    cJSON_AddNumberToObject(entry, "width", data->width);
  if (data->height > 0)
    cJSON_AddNumberToObject(entry, "height", data->height);
  if (data->cameraMake[0] != '\0')
    cJSON_AddStringToObject(entry, "cameraMake", data->cameraMake);
  if (data->cameraModel[0] != '\0')
    cJSON_AddStringToObject(entry, "cameraModel", data->cameraModel);
  if (data->orientation > 0)
    cJSON_AddNumberToObject(entry, "orientation", data->orientation);
  if (data->hasGps) {
    cJSON_AddNumberToObject(entry, "gpsLatitude", data->gpsLatitude);
    cJSON_AddNumberToObject(entry, "gpsLongitude", data->gpsLongitude);
  }

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

  cJSON *mod_date_node = cJSON_GetObjectItem(entry, "modificationDate");
  cJSON *file_size_node = cJSON_GetObjectItem(entry, "fileSize");

  if (!mod_date_node || !file_size_node)
    return false;

  if (mod_date_node->valuedouble != current_mod_date)
    return false;
  if ((long)file_size_node->valuedouble != current_size)
    return false;

  // Populate fields
  memset(out_md, 0, sizeof(ImageMetadata));
  out_md->path = strdup(absolute_path);
  out_md->modificationDate = current_mod_date;
  out_md->fileSize = current_size;

  // EXIF fields
  cJSON *node;
  node = cJSON_GetObjectItem(entry, "dateTaken");
  if (node && cJSON_IsString(node))
    strncpy(out_md->dateTaken, node->valuestring, METADATA_MAX_STRING - 1);
  node = cJSON_GetObjectItem(entry, "width");
  if (node)
    out_md->width = (int)node->valuedouble;
  node = cJSON_GetObjectItem(entry, "height");
  if (node)
    out_md->height = (int)node->valuedouble;
  node = cJSON_GetObjectItem(entry, "cameraMake");
  if (node && cJSON_IsString(node))
    strncpy(out_md->cameraMake, node->valuestring, METADATA_MAX_STRING - 1);
  node = cJSON_GetObjectItem(entry, "cameraModel");
  if (node && cJSON_IsString(node))
    strncpy(out_md->cameraModel, node->valuestring, METADATA_MAX_STRING - 1);
  node = cJSON_GetObjectItem(entry, "orientation");
  if (node)
    out_md->orientation = (int)node->valuedouble;
  node = cJSON_GetObjectItem(entry, "gpsLatitude");
  if (node) {
    out_md->hasGps = true;
    out_md->gpsLatitude = node->valuedouble;
    cJSON *lon = cJSON_GetObjectItem(entry, "gpsLongitude");
    if (lon)
      out_md->gpsLongitude = lon->valuedouble;
  }

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

  // st_mtime is the POSIX standard for modification time in seconds.
  // macOS also provides st_mtimespec for nanosecond precision.
  *out_mod_date = (double)st.st_mtime;

  return true;
}
