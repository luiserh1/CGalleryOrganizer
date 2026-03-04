#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "systems/renamer_tags_internal.h"
void RenamerTagsSetError(char *out_error, size_t out_error_size, const char *fmt,
                     ...) {
  if (!out_error || out_error_size == 0 || !fmt) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(out_error, out_error_size, fmt, args);
  va_end(args);
}
bool RenamerTagsLoadFileText(const char *path, char **out_text) {
  if (!path || !out_text) {
    return false;
  }

  *out_text = NULL;
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }

  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return false;
  }
  rewind(f);

  char *buffer = calloc((size_t)size + 1, 1);
  if (!buffer) {
    fclose(f);
    return false;
  }

  size_t read_bytes = fread(buffer, 1, (size_t)size, f);
  fclose(f);
  buffer[read_bytes] = '\0';

  *out_text = buffer;
  return true;
}

bool RenamerTagsSaveFileText(const char *path, const char *text) {
  if (!path || !text) {
    return false;
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }

  bool ok = fputs(text, f) >= 0;
  fclose(f);
  return ok;
}
static bool TagIsValidChar(unsigned char c) {
  return isalnum(c) || c == '-' || c == '_';
}

static void NormalizeTag(const char *input, char *out_tag, size_t out_size) {
  if (!out_tag || out_size == 0) {
    return;
  }
  out_tag[0] = '\0';
  if (!input || input[0] == '\0') {
    return;
  }

  size_t used = 0;
  char last = '\0';
  for (size_t i = 0; input[i] != '\0'; i++) {
    unsigned char c = (unsigned char)input[i];
    char emit = '\0';

    if (isalnum(c)) {
      emit = (char)tolower(c);
    } else if (c == '-' || c == '_') {
      emit = '-';
    } else if (isspace(c) || c == '/' || c == '\\' || c == ':' || c == ';' ||
               c == ',' || c == '|') {
      emit = '-';
    } else if (TagIsValidChar(c)) {
      emit = (char)c;
    } else {
      emit = '-';
    }

    if (emit == '-' && last == '-') {
      continue;
    }
    if (used + 1 >= out_size) {
      break;
    }

    out_tag[used++] = emit;
    last = emit;
  }
  out_tag[used] = '\0';

  while (out_tag[0] == '-') {
    memmove(out_tag, out_tag + 1, strlen(out_tag));
  }
  size_t len = strlen(out_tag);
  while (len > 0 && out_tag[len - 1] == '-') {
    out_tag[len - 1] = '\0';
    len--;
  }
}

bool RenamerTagsTagListContains(char **tags, int count, const char *candidate) {
  if (!tags || count <= 0 || !candidate || candidate[0] == '\0') {
    return false;
  }

  for (int i = 0; i < count; i++) {
    if (tags[i] && strcmp(tags[i], candidate) == 0) {
      return true;
    }
  }
  return false;
}
bool RenamerTagsTagListAppend(char **tags, int *io_count, const char *tag) {
  if (!tags || !io_count || !tag || tag[0] == '\0') {
    return false;
  }
  if (*io_count >= RENAMER_TAG_LIMIT) {
    return false;
  }
  if (RenamerTagsTagListContains(tags, *io_count, tag)) {
    return true;
  }

  tags[*io_count] = strdup(tag);
  if (!tags[*io_count]) {
    return false;
  }
  (*io_count)++;
  return true;
}

void RenamerTagsTagListFree(char **tags, int count) {
  if (!tags || count <= 0) {
    return;
  }
  for (int i = 0; i < count; i++) {
    free(tags[i]);
  }
}

void RenamerTagsTagListJoin(char **tags, int count, char *out_text,
                        size_t out_text_size, const char *fallback) {
  if (!out_text || out_text_size == 0) {
    return;
  }
  out_text[0] = '\0';

  for (int i = 0; i < count; i++) {
    if (!tags[i] || tags[i][0] == '\0') {
      continue;
    }
    if (out_text[0] != '\0') {
      strncat(out_text, "-", out_text_size - strlen(out_text) - 1);
    }
    strncat(out_text, tags[i], out_text_size - strlen(out_text) - 1);
  }

  if (out_text[0] == '\0' && fallback) {
    strncpy(out_text, fallback, out_text_size - 1);
    out_text[out_text_size - 1] = '\0';
  }
}
bool RenamerTagsJSONArrayContainsTag(const cJSON *array, const char *tag) {
  if (!cJSON_IsArray(array) || !tag || tag[0] == '\0') {
    return false;
  }

  int count = cJSON_GetArraySize((cJSON *)array);
  for (int i = 0; i < count; i++) {
    cJSON *item = cJSON_GetArrayItem((cJSON *)array, i);
    if (!cJSON_IsString(item) || !item->valuestring) {
      continue;
    }
    if (strcmp(item->valuestring, tag) == 0) {
      return true;
    }
  }

  return false;
}

void RenamerTagsJSONArrayRemoveTag(cJSON *array, const char *tag) {
  if (!cJSON_IsArray(array) || !tag || tag[0] == '\0') {
    return;
  }

  int index = 0;
  while (index < cJSON_GetArraySize(array)) {
    cJSON *item = cJSON_GetArrayItem(array, index);
    if (!cJSON_IsString(item) || !item->valuestring) {
      index++;
      continue;
    }
    if (strcmp(item->valuestring, tag) == 0) {
      cJSON_DeleteItemFromArray(array, index);
      continue;
    }
    index++;
  }
}

bool RenamerTagsJSONArrayAddTag(cJSON *array, const char *tag) {
  if (!cJSON_IsArray(array) || !tag || tag[0] == '\0') {
    return false;
  }

  if (RenamerTagsJSONArrayContainsTag(array, tag)) {
    return true;
  }

  cJSON *item = cJSON_CreateString(tag);
  if (!item) {
    return false;
  }

  cJSON_AddItemToArray(array, item);
  return true;
}
bool RenamerTagsParseCsvTags(const char *csv, char **out_tags, int *out_count) {
  if (!out_tags || !out_count) {
    return false;
  }

  *out_count = 0;
  if (!csv || csv[0] == '\0') {
    return true;
  }

  char *buffer = strdup(csv);
  if (!buffer) {
    return false;
  }

  char *cursor = buffer;
  while (cursor && *cursor != '\0') {
    char *next = strpbrk(cursor, ",;");
    if (next) {
      *next = '\0';
    }

    while (*cursor != '\0' && isspace((unsigned char)*cursor)) {
      cursor++;
    }

    char *end = cursor + strlen(cursor);
    while (end > cursor && isspace((unsigned char)*(end - 1))) {
      *(end - 1) = '\0';
      end--;
    }

    char normalized[64] = {0};
    NormalizeTag(cursor, normalized, sizeof(normalized));
    if (normalized[0] != '\0') {
      if (!RenamerTagsTagListAppend(out_tags, out_count, normalized)) {
        free(buffer);
        return false;
      }
    }

    if (!next) {
      break;
    }
    cursor = next + 1;
  }

  free(buffer);
  return true;
}

bool RenamerTagsParseTagsFromNode(const cJSON *node, char **out_tags,
                              int *out_count) {
  if (!out_tags || !out_count) {
    return false;
  }

  if (!node) {
    return true;
  }

  if (cJSON_IsString((cJSON *)node) && node->valuestring) {
    return RenamerTagsParseCsvTags(node->valuestring, out_tags, out_count);
  }

  if (!cJSON_IsArray((cJSON *)node)) {
    return false;
  }

  int array_size = cJSON_GetArraySize((cJSON *)node);
  for (int i = 0; i < array_size; i++) {
    cJSON *item = cJSON_GetArrayItem((cJSON *)node, i);
    if (!cJSON_IsString(item) || !item->valuestring) {
      continue;
    }
    char normalized[64] = {0};
    NormalizeTag(item->valuestring, normalized, sizeof(normalized));
    if (normalized[0] == '\0') {
      continue;
    }
    if (!RenamerTagsTagListAppend(out_tags, out_count, normalized)) {
      return false;
    }
  }

  return true;
}
