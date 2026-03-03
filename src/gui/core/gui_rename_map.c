#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "fs_utils.h"
#include "gui/core/gui_rename_map.h"

static void SetError(char *out_error, size_t out_error_size, const char *fmt,
                     ...) {
  if (!out_error || out_error_size == 0 || !fmt) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(out_error, out_error_size, fmt, args);
  va_end(args);
}

static char *LoadTextFile(const char *path) {
  if (!path || path[0] == '\0') {
    return NULL;
  }

  FILE *f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);

  char *text = calloc((size_t)size + 1, 1);
  if (!text) {
    fclose(f);
    return NULL;
  }

  size_t read = fread(text, 1, (size_t)size, f);
  fclose(f);
  text[read] = '\0';
  return text;
}

static bool SaveTextFile(const char *path, const char *text) {
  if (!path || path[0] == '\0' || !text) {
    return false;
  }

  char dir[MAX_PATH_LENGTH] = {0};
  strncpy(dir, path, sizeof(dir) - 1);
  dir[sizeof(dir) - 1] = '\0';
  char *slash = strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
    if (dir[0] != '\0' && !FsMakeDirRecursive(dir)) {
      return false;
    }
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  bool ok = fputs(text, f) >= 0;
  fclose(f);
  return ok;
}

static void TrimInPlace(char *text) {
  if (!text) {
    return;
  }

  size_t len = strlen(text);
  size_t start = 0;
  while (start < len && isspace((unsigned char)text[start])) {
    start++;
  }
  size_t end = len;
  while (end > start && isspace((unsigned char)text[end - 1])) {
    end--;
  }

  if (start > 0) {
    memmove(text, text + start, end - start);
  }
  text[end - start] = '\0';
}

static bool TagExists(cJSON *array, const char *tag) {
  if (!cJSON_IsArray(array) || !tag || tag[0] == '\0') {
    return false;
  }
  int count = cJSON_GetArraySize(array);
  for (int i = 0; i < count; i++) {
    cJSON *node = cJSON_GetArrayItem(array, i);
    if (cJSON_IsString(node) && node->valuestring &&
        strcmp(node->valuestring, tag) == 0) {
      return true;
    }
  }
  return false;
}

static bool AppendCsvTags(cJSON *array, const char *csv) {
  if (!cJSON_IsArray(array) || !csv) {
    return false;
  }

  char copy[512] = {0};
  strncpy(copy, csv, sizeof(copy) - 1);
  copy[sizeof(copy) - 1] = '\0';

  char *token = strtok(copy, ",");
  while (token) {
    TrimInPlace(token);
    if (token[0] != '\0' && !TagExists(array, token)) {
      cJSON *tag = cJSON_CreateString(token);
      if (!tag) {
        return false;
      }
      cJSON_AddItemToArray(array, tag);
    }
    token = strtok(NULL, ",");
  }
  return true;
}

bool GuiRenameMapUpsertManualTags(const char *map_path, const char *source_path,
                                  const char *manual_tags_csv,
                                  char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!map_path || map_path[0] == '\0' || !source_path ||
      source_path[0] == '\0') {
    SetError(out_error, out_error_size,
             "map path and source path are required for manual tag update");
    return false;
  }

  cJSON *root = NULL;
  char *existing = LoadTextFile(map_path);
  if (existing) {
    root = cJSON_Parse(existing);
    free(existing);
  }
  if (!root) {
    root = cJSON_CreateObject();
  }
  if (!root) {
    SetError(out_error, out_error_size,
             "out of memory while preparing rename map");
    return false;
  }

  cJSON *files = cJSON_GetObjectItem(root, "files");
  if (!cJSON_IsObject(files)) {
    cJSON *replacement = cJSON_CreateObject();
    if (!replacement) {
      cJSON_Delete(root);
      SetError(out_error, out_error_size,
               "out of memory while preparing files map");
      return false;
    }
    if (files) {
      cJSON_ReplaceItemInObject(root, "files", replacement);
    } else {
      cJSON_AddItemToObject(root, "files", replacement);
    }
    files = replacement;
  }

  cJSON *entry = cJSON_GetObjectItem(files, source_path);
  if (!cJSON_IsObject(entry)) {
    cJSON *replacement = cJSON_CreateObject();
    if (!replacement) {
      cJSON_Delete(root);
      SetError(out_error, out_error_size,
               "out of memory while preparing source entry");
      return false;
    }
    if (entry) {
      cJSON_ReplaceItemInObject(files, source_path, replacement);
    } else {
      cJSON_AddItemToObject(files, source_path, replacement);
    }
    entry = replacement;
  }

  cJSON *manual = cJSON_CreateArray();
  if (!manual) {
    cJSON_Delete(root);
    SetError(out_error, out_error_size,
             "out of memory while preparing manual tags");
    return false;
  }
  if (manual_tags_csv && manual_tags_csv[0] != '\0' &&
      !AppendCsvTags(manual, manual_tags_csv)) {
    cJSON_Delete(manual);
    cJSON_Delete(root);
    SetError(out_error, out_error_size,
             "failed to parse manual tags csv '%s'", manual_tags_csv);
    return false;
  }
  cJSON *manual_existing = cJSON_GetObjectItem(entry, "manualTags");
  if (manual_existing) {
    cJSON_ReplaceItemInObject(entry, "manualTags", manual);
  } else {
    cJSON_AddItemToObject(entry, "manualTags", manual);
  }

  if (!cJSON_GetObjectItem(entry, "suppressedMetaTags")) {
    cJSON *suppressed = cJSON_CreateArray();
    if (!suppressed) {
      cJSON_Delete(root);
      SetError(out_error, out_error_size,
               "out of memory while preparing suppressed tags");
      return false;
    }
    cJSON_AddItemToObject(entry, "suppressedMetaTags", suppressed);
  }

  char *text = cJSON_Print(root);
  cJSON_Delete(root);
  if (!text) {
    SetError(out_error, out_error_size,
             "failed to serialize rename tags map");
    return false;
  }

  bool save_ok = SaveTextFile(map_path, text);
  cJSON_free(text);
  if (!save_ok) {
    SetError(out_error, out_error_size,
             "failed to write rename tags map '%s'", map_path);
    return false;
  }
  return true;
}
