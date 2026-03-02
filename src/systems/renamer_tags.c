#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "fs_utils.h"
#include "systems/renamer_tags.h"

#define RENAMER_TAG_LIMIT 128

static const char *RENAMER_META_TAG_WHITELIST[] = {
    "Iptc.Application2.Keywords",
    "Xmp.dc.subject",
    "Xmp.lr.hierarchicalSubject",
    "Xmp.photoshop.SupplementalCategories",
    "Xmp.digiKam.TagsList",
    NULL,
};

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

static bool EnsureCacheDir(const char *env_dir, char *out_path,
                           size_t out_path_size) {
  if (!env_dir || env_dir[0] == '\0' || !out_path || out_path_size == 0) {
    return false;
  }

  char cache_dir[MAX_PATH_LENGTH] = {0};
  snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", env_dir);
  if (!FsMakeDirRecursive(cache_dir)) {
    return false;
  }

  snprintf(out_path, out_path_size, "%s/rename_tags.json", cache_dir);
  return true;
}

static bool LoadFileText(const char *path, char **out_text) {
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

static bool SaveFileText(const char *path, const char *text) {
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

static cJSON *CreateDefaultSidecar(void) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return NULL;
  }
  cJSON_AddNumberToObject(root, "version", 1);
  cJSON_AddItemToObject(root, "files", cJSON_CreateObject());
  return root;
}

static cJSON *GetFilesObject(cJSON *root) {
  if (!root) {
    return NULL;
  }

  cJSON *files = cJSON_GetObjectItem(root, "files");
  if (!cJSON_IsObject(files)) {
    cJSON *replacement = cJSON_CreateObject();
    if (!replacement) {
      return NULL;
    }
    if (files) {
      cJSON_ReplaceItemInObject(root, "files", replacement);
    } else {
      cJSON_AddItemToObject(root, "files", replacement);
    }
    files = replacement;
  }
  return files;
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

static bool TagListContains(char **tags, int count, const char *candidate) {
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

static bool TagListAppend(char **tags, int *io_count, const char *tag) {
  if (!tags || !io_count || !tag || tag[0] == '\0') {
    return false;
  }
  if (*io_count >= RENAMER_TAG_LIMIT) {
    return false;
  }
  if (TagListContains(tags, *io_count, tag)) {
    return true;
  }

  tags[*io_count] = strdup(tag);
  if (!tags[*io_count]) {
    return false;
  }
  (*io_count)++;
  return true;
}

static void TagListFree(char **tags, int count) {
  if (!tags || count <= 0) {
    return;
  }
  for (int i = 0; i < count; i++) {
    free(tags[i]);
  }
}

static void TagListJoin(char **tags, int count, char *out_text,
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

static void NowUtc(char *out_text, size_t out_text_size) {
  if (!out_text || out_text_size == 0) {
    return;
  }

  out_text[0] = '\0';
  time_t now = time(NULL);
  struct tm tm_utc;
#if defined(_WIN32)
  gmtime_s(&tm_utc, &now);
#else
  gmtime_r(&now, &tm_utc);
#endif
  strftime(out_text, out_text_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

static cJSON *EnsurePathEntry(cJSON *root, const char *absolute_path) {
  if (!root || !absolute_path || absolute_path[0] == '\0') {
    return NULL;
  }

  cJSON *files = GetFilesObject(root);
  if (!files) {
    return NULL;
  }

  cJSON *entry = cJSON_GetObjectItem(files, absolute_path);
  if (!cJSON_IsObject(entry)) {
    cJSON *replacement = cJSON_CreateObject();
    if (!replacement) {
      return NULL;
    }

    cJSON_AddItemToObject(replacement, "manualTags", cJSON_CreateArray());
    cJSON_AddItemToObject(replacement, "suppressedMetaTags", cJSON_CreateArray());
    char updated_at[64] = {0};
    NowUtc(updated_at, sizeof(updated_at));
    cJSON_AddStringToObject(replacement, "updatedAtUtc", updated_at);

    if (entry) {
      cJSON_ReplaceItemInObject(files, absolute_path, replacement);
    } else {
      cJSON_AddItemToObject(files, absolute_path, replacement);
    }
    entry = replacement;
  }

  cJSON *manual = cJSON_GetObjectItem(entry, "manualTags");
  if (!cJSON_IsArray(manual)) {
    cJSON *replacement = cJSON_CreateArray();
    if (!replacement) {
      return NULL;
    }
    if (manual) {
      cJSON_ReplaceItemInObject(entry, "manualTags", replacement);
    } else {
      cJSON_AddItemToObject(entry, "manualTags", replacement);
    }
  }

  cJSON *suppressed = cJSON_GetObjectItem(entry, "suppressedMetaTags");
  if (!cJSON_IsArray(suppressed)) {
    cJSON *replacement = cJSON_CreateArray();
    if (!replacement) {
      return NULL;
    }
    if (suppressed) {
      cJSON_ReplaceItemInObject(entry, "suppressedMetaTags", replacement);
    } else {
      cJSON_AddItemToObject(entry, "suppressedMetaTags", replacement);
    }
  }

  return entry;
}

static bool JSONArrayContainsTag(const cJSON *array, const char *tag) {
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

static void JSONArrayRemoveTag(cJSON *array, const char *tag) {
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

static bool JSONArrayAddTag(cJSON *array, const char *tag) {
  if (!cJSON_IsArray(array) || !tag || tag[0] == '\0') {
    return false;
  }

  if (JSONArrayContainsTag(array, tag)) {
    return true;
  }

  cJSON *item = cJSON_CreateString(tag);
  if (!item) {
    return false;
  }

  cJSON_AddItemToArray(array, item);
  return true;
}

static bool ParseCsvTags(const char *csv, char **out_tags, int *out_count) {
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
      if (!TagListAppend(out_tags, out_count, normalized)) {
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

static bool ParseTagsFromNode(const cJSON *node, char **out_tags,
                              int *out_count) {
  if (!out_tags || !out_count) {
    return false;
  }

  if (!node) {
    return true;
  }

  if (cJSON_IsString((cJSON *)node) && node->valuestring) {
    return ParseCsvTags(node->valuestring, out_tags, out_count);
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
    if (!TagListAppend(out_tags, out_count, normalized)) {
      return false;
    }
  }

  return true;
}

static bool PathInBatch(const char **paths, int path_count, const char *path) {
  if (!paths || path_count <= 0 || !path) {
    return false;
  }
  for (int i = 0; i < path_count; i++) {
    if (paths[i] && strcmp(paths[i], path) == 0) {
      return true;
    }
  }
  return false;
}

static bool KeyWhitelisted(const char *key) {
  if (!key || key[0] == '\0') {
    return false;
  }

  for (int i = 0; RENAMER_META_TAG_WHITELIST[i]; i++) {
    if (strcmp(RENAMER_META_TAG_WHITELIST[i], key) == 0) {
      return true;
    }
  }
  return false;
}

static bool CollectMetadataTags(const char *all_metadata_json, cJSON *entry,
                                char **out_meta_tags, int *out_meta_count,
                                char *out_error, size_t out_error_size) {
  if (!out_meta_tags || !out_meta_count) {
    return false;
  }

  *out_meta_count = 0;

  if (!all_metadata_json || all_metadata_json[0] == '\0') {
    return true;
  }

  cJSON *json = cJSON_Parse(all_metadata_json);
  if (!json) {
    SetError(out_error, out_error_size,
             "failed to parse allMetadataJson while resolving tags");
    return false;
  }

  cJSON *suppressed_array = entry ? cJSON_GetObjectItem(entry, "suppressedMetaTags")
                                  : NULL;

  cJSON *node = NULL;
  cJSON_ArrayForEach(node, json) {
    if (!node->string || !KeyWhitelisted(node->string)) {
      continue;
    }

    char *parsed[RENAMER_TAG_LIMIT] = {0};
    int parsed_count = 0;
    bool ok = ParseTagsFromNode(node, parsed, &parsed_count);
    if (!ok) {
      TagListFree(parsed, parsed_count);
      cJSON_Delete(json);
      SetError(out_error, out_error_size,
               "invalid metadata tags in key '%s'", node->string);
      return false;
    }

    for (int i = 0; i < parsed_count; i++) {
      if (!parsed[i] || parsed[i][0] == '\0') {
        continue;
      }
      if (JSONArrayContainsTag(suppressed_array, parsed[i])) {
        continue;
      }
      if (!TagListAppend(out_meta_tags, out_meta_count, parsed[i])) {
        TagListFree(parsed, parsed_count);
        cJSON_Delete(json);
        SetError(out_error, out_error_size,
                 "metadata tag limit exceeded while parsing '%s'", node->string);
        return false;
      }
    }

    TagListFree(parsed, parsed_count);
  }

  cJSON_Delete(json);
  return true;
}

bool RenamerTagsLoadSidecar(const char *env_dir, cJSON **out_root,
                            char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_root) {
    SetError(out_error, out_error_size, "env_dir is required for rename tags");
    return false;
  }

  *out_root = NULL;
  char sidecar_path[MAX_PATH_LENGTH] = {0};
  if (!EnsureCacheDir(env_dir, sidecar_path, sizeof(sidecar_path))) {
    SetError(out_error, out_error_size,
             "failed to prepare .cache directory for rename tags");
    return false;
  }

  char *text = NULL;
  if (!LoadFileText(sidecar_path, &text)) {
    *out_root = CreateDefaultSidecar();
    if (!*out_root) {
      SetError(out_error, out_error_size, "failed to initialize tag sidecar");
      return false;
    }
    return true;
  }

  cJSON *root = cJSON_Parse(text);
  free(text);
  if (!root) {
    *out_root = CreateDefaultSidecar();
    if (!*out_root) {
      SetError(out_error, out_error_size,
               "failed to recover malformed rename tag sidecar");
      return false;
    }
    return true;
  }

  cJSON *files = cJSON_GetObjectItem(root, "files");
  if (!cJSON_IsObject(files)) {
    cJSON_Delete(root);
    root = CreateDefaultSidecar();
    if (!root) {
      SetError(out_error, out_error_size, "failed to initialize tag sidecar");
      return false;
    }
  }

  *out_root = root;
  return true;
}

bool RenamerTagsSaveSidecar(const char *env_dir, const cJSON *root,
                            char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !root) {
    SetError(out_error, out_error_size,
             "env_dir and root are required for rename tag save");
    return false;
  }

  char sidecar_path[MAX_PATH_LENGTH] = {0};
  if (!EnsureCacheDir(env_dir, sidecar_path, sizeof(sidecar_path))) {
    SetError(out_error, out_error_size,
             "failed to prepare .cache directory for rename tags");
    return false;
  }

  char *json_text = cJSON_Print(root);
  if (!json_text) {
    SetError(out_error, out_error_size,
             "failed to serialize rename tag sidecar");
    return false;
  }

  bool ok = SaveFileText(sidecar_path, json_text);
  cJSON_free(json_text);
  if (!ok) {
    SetError(out_error, out_error_size,
             "failed to write rename tag sidecar '%s'", sidecar_path);
    return false;
  }

  return true;
}

bool RenamerTagsApplyBulkCsv(cJSON *root, const char **absolute_paths,
                             int path_count, const char *tag_add_csv,
                             const char *tag_remove_csv, char *out_error,
                             size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!root || !absolute_paths || path_count < 0) {
    SetError(out_error, out_error_size,
             "invalid arguments for bulk tag operations");
    return false;
  }

  char *add_tags[RENAMER_TAG_LIMIT] = {0};
  int add_count = 0;
  char *remove_tags[RENAMER_TAG_LIMIT] = {0};
  int remove_count = 0;

  if (!ParseCsvTags(tag_add_csv, add_tags, &add_count) ||
      !ParseCsvTags(tag_remove_csv, remove_tags, &remove_count)) {
    TagListFree(add_tags, add_count);
    TagListFree(remove_tags, remove_count);
    SetError(out_error, out_error_size, "failed to parse bulk tag CSV values");
    return false;
  }

  for (int i = 0; i < path_count; i++) {
    const char *path = absolute_paths[i];
    if (!path || path[0] == '\0') {
      continue;
    }

    cJSON *entry = EnsurePathEntry(root, path);
    if (!entry) {
      TagListFree(add_tags, add_count);
      TagListFree(remove_tags, remove_count);
      SetError(out_error, out_error_size,
               "failed to prepare tag sidecar entry for '%s'", path);
      return false;
    }

    cJSON *manual = cJSON_GetObjectItem(entry, "manualTags");
    cJSON *suppressed = cJSON_GetObjectItem(entry, "suppressedMetaTags");

    for (int j = 0; j < add_count; j++) {
      if (!JSONArrayAddTag(manual, add_tags[j])) {
        TagListFree(add_tags, add_count);
        TagListFree(remove_tags, remove_count);
        SetError(out_error, out_error_size,
                 "failed to append manual tag '%s'", add_tags[j]);
        return false;
      }
      JSONArrayRemoveTag(suppressed, add_tags[j]);
    }

    for (int j = 0; j < remove_count; j++) {
      JSONArrayRemoveTag(manual, remove_tags[j]);
      if (!JSONArrayAddTag(suppressed, remove_tags[j])) {
        TagListFree(add_tags, add_count);
        TagListFree(remove_tags, remove_count);
        SetError(out_error, out_error_size,
                 "failed to append suppressed tag '%s'", remove_tags[j]);
        return false;
      }
    }

    char updated_at[64] = {0};
    NowUtc(updated_at, sizeof(updated_at));
    cJSON_ReplaceItemInObject(entry, "updatedAtUtc", cJSON_CreateString(updated_at));
  }

  TagListFree(add_tags, add_count);
  TagListFree(remove_tags, remove_count);
  return true;
}

static bool ApplyMapEntry(cJSON *root, const char *path,
                          const cJSON *manual_tags_node,
                          const cJSON *suppressed_tags_node,
                          const char **batch_paths, int batch_count,
                          char *out_error, size_t out_error_size) {
  if (!root || !path || path[0] == '\0') {
    return true;
  }

  char absolute_path[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(path, absolute_path, sizeof(absolute_path))) {
    SetError(out_error, out_error_size,
             "rename tags map path '%s' is not resolvable", path);
    return false;
  }

  if (batch_paths && batch_count > 0 &&
      !PathInBatch(batch_paths, batch_count, absolute_path)) {
    return true;
  }

  cJSON *entry = EnsurePathEntry(root, absolute_path);
  if (!entry) {
    SetError(out_error, out_error_size,
             "failed to create tag sidecar entry for '%s'", absolute_path);
    return false;
  }

  char *manual_tags[RENAMER_TAG_LIMIT] = {0};
  int manual_count = 0;
  if (!ParseTagsFromNode(manual_tags_node, manual_tags, &manual_count)) {
    SetError(out_error, out_error_size,
             "invalid manualTags in rename tags map for '%s'", absolute_path);
    return false;
  }

  char *suppressed_tags[RENAMER_TAG_LIMIT] = {0};
  int suppressed_count = 0;
  if (!ParseTagsFromNode(suppressed_tags_node, suppressed_tags,
                         &suppressed_count)) {
    TagListFree(manual_tags, manual_count);
    SetError(out_error, out_error_size,
             "invalid suppressedMetaTags in rename tags map for '%s'",
             absolute_path);
    return false;
  }

  cJSON *manual_array = cJSON_CreateArray();
  cJSON *suppressed_array = cJSON_CreateArray();
  if (!manual_array || !suppressed_array) {
    cJSON_Delete(manual_array);
    cJSON_Delete(suppressed_array);
    TagListFree(manual_tags, manual_count);
    TagListFree(suppressed_tags, suppressed_count);
    SetError(out_error, out_error_size,
             "out of memory while applying tag map for '%s'", absolute_path);
    return false;
  }

  for (int i = 0; i < manual_count; i++) {
    cJSON_AddItemToArray(manual_array, cJSON_CreateString(manual_tags[i]));
  }
  for (int i = 0; i < suppressed_count; i++) {
    cJSON_AddItemToArray(suppressed_array,
                         cJSON_CreateString(suppressed_tags[i]));
  }

  cJSON_ReplaceItemInObject(entry, "manualTags", manual_array);
  cJSON_ReplaceItemInObject(entry, "suppressedMetaTags", suppressed_array);

  char updated_at[64] = {0};
  NowUtc(updated_at, sizeof(updated_at));
  cJSON_ReplaceItemInObject(entry, "updatedAtUtc", cJSON_CreateString(updated_at));

  TagListFree(manual_tags, manual_count);
  TagListFree(suppressed_tags, suppressed_count);
  return true;
}

bool RenamerTagsApplyMapFile(cJSON *root, const char *map_json_path,
                             const char **absolute_paths, int path_count,
                             char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!root) {
    SetError(out_error, out_error_size,
             "tag sidecar root is required for map ingest");
    return false;
  }
  if (!map_json_path || map_json_path[0] == '\0') {
    return true;
  }

  char *json_text = NULL;
  if (!LoadFileText(map_json_path, &json_text)) {
    SetError(out_error, out_error_size,
             "failed to read rename tags map '%s'", map_json_path);
    return false;
  }

  cJSON *json = cJSON_Parse(json_text);
  free(json_text);
  if (!json) {
    SetError(out_error, out_error_size,
             "rename tags map '%s' is not valid JSON", map_json_path);
    return false;
  }

  bool ok = true;
  if (cJSON_IsArray(json)) {
    int count = cJSON_GetArraySize(json);
    for (int i = 0; i < count; i++) {
      cJSON *item = cJSON_GetArrayItem(json, i);
      if (!cJSON_IsObject(item)) {
        continue;
      }
      cJSON *path = cJSON_GetObjectItem(item, "path");
      if (!cJSON_IsString(path) || !path->valuestring) {
        continue;
      }
      cJSON *manual_tags = cJSON_GetObjectItem(item, "manualTags");
      cJSON *suppressed_tags = cJSON_GetObjectItem(item, "suppressedMetaTags");
      ok = ApplyMapEntry(root, path->valuestring, manual_tags, suppressed_tags,
                         absolute_paths, path_count, out_error, out_error_size);
      if (!ok) {
        break;
      }
    }
  } else if (cJSON_IsObject(json)) {
    cJSON *files = cJSON_GetObjectItem(json, "files");
    cJSON *iter_root = cJSON_IsObject(files) ? files : json;

    cJSON *node = NULL;
    cJSON_ArrayForEach(node, iter_root) {
      if (!node->string) {
        continue;
      }

      if (cJSON_IsObject(node)) {
        cJSON *manual_tags = cJSON_GetObjectItem(node, "manualTags");
        cJSON *suppressed_tags = cJSON_GetObjectItem(node, "suppressedMetaTags");
        ok = ApplyMapEntry(root, node->string, manual_tags, suppressed_tags,
                           absolute_paths, path_count, out_error,
                           out_error_size);
      } else {
        ok = ApplyMapEntry(root, node->string, node, NULL, absolute_paths,
                           path_count, out_error, out_error_size);
      }
      if (!ok) {
        break;
      }
    }
  } else {
    ok = false;
    SetError(out_error, out_error_size,
             "rename tags map must be JSON object or array");
  }

  cJSON_Delete(json);
  return ok;
}

bool RenamerTagsResolve(const cJSON *root, const char *absolute_path,
                        const char *all_metadata_json,
                        RenamerResolvedTags *out_tags, char *out_error,
                        size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!root || !absolute_path || absolute_path[0] == '\0' || !out_tags) {
    SetError(out_error, out_error_size,
             "invalid arguments for resolving rename tags");
    return false;
  }

  memset(out_tags, 0, sizeof(*out_tags));

  const cJSON *files = cJSON_GetObjectItem((cJSON *)root, "files");
  const cJSON *entry = cJSON_IsObject((cJSON *)files)
                           ? cJSON_GetObjectItem((cJSON *)files, absolute_path)
                           : NULL;

  char *manual_tags[RENAMER_TAG_LIMIT] = {0};
  int manual_count = 0;
  if (entry) {
    const cJSON *manual_node = cJSON_GetObjectItem((cJSON *)entry, "manualTags");
    if (!ParseTagsFromNode(manual_node, manual_tags, &manual_count)) {
      TagListFree(manual_tags, manual_count);
      SetError(out_error, out_error_size,
               "invalid manualTags for '%s' in rename tag sidecar",
               absolute_path);
      return false;
    }
  }

  char *meta_tags[RENAMER_TAG_LIMIT] = {0};
  int meta_count = 0;
  if (!CollectMetadataTags(all_metadata_json, (cJSON *)entry, meta_tags,
                           &meta_count, out_error, out_error_size)) {
    TagListFree(manual_tags, manual_count);
    TagListFree(meta_tags, meta_count);
    return false;
  }

  char *merged[RENAMER_TAG_LIMIT] = {0};
  int merged_count = 0;
  for (int i = 0; i < manual_count; i++) {
    if (!TagListAppend(merged, &merged_count, manual_tags[i])) {
      TagListFree(manual_tags, manual_count);
      TagListFree(meta_tags, meta_count);
      TagListFree(merged, merged_count);
      SetError(out_error, out_error_size,
               "tag merge limit exceeded for '%s'", absolute_path);
      return false;
    }
  }
  for (int i = 0; i < meta_count; i++) {
    if (!TagListAppend(merged, &merged_count, meta_tags[i])) {
      TagListFree(manual_tags, manual_count);
      TagListFree(meta_tags, meta_count);
      TagListFree(merged, merged_count);
      SetError(out_error, out_error_size,
               "tag merge limit exceeded for '%s'", absolute_path);
      return false;
    }
  }

  TagListJoin(manual_tags, manual_count, out_tags->manual, sizeof(out_tags->manual),
              "untagged");
  TagListJoin(meta_tags, meta_count, out_tags->meta, sizeof(out_tags->meta),
              "untagged");
  TagListJoin(merged, merged_count, out_tags->merged, sizeof(out_tags->merged),
              "untagged");

  TagListFree(manual_tags, manual_count);
  TagListFree(meta_tags, meta_count);
  TagListFree(merged, merged_count);
  return true;
}

bool RenamerTagsMovePathKey(cJSON *root, const char *old_path,
                            const char *new_path) {
  if (!root || !old_path || old_path[0] == '\0' || !new_path ||
      new_path[0] == '\0') {
    return false;
  }

  cJSON *files = GetFilesObject(root);
  if (!files) {
    return false;
  }

  cJSON *entry = cJSON_DetachItemFromObject(files, old_path);
  if (!entry) {
    return true;
  }

  cJSON_DeleteItemFromObject(files, new_path);
  cJSON_AddItemToObject(files, new_path, entry);
  return true;
}
