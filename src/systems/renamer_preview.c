#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "gallery_cache.h"
#include "metadata_parser.h"
#include "systems/renamer_pattern.h"
#include "systems/renamer_preview.h"
#include "systems/renamer_tags.h"

typedef struct {
  char **paths;
  int count;
  int capacity;
} PathList;

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

static void PathListFree(PathList *list) {
  if (!list) {
    return;
  }
  for (int i = 0; i < list->count; i++) {
    free(list->paths[i]);
  }
  free(list->paths);
  list->paths = NULL;
  list->count = 0;
  list->capacity = 0;
}

static bool PathListAppend(PathList *list, const char *path) {
  if (!list || !path || path[0] == '\0') {
    return false;
  }

  if (list->count >= list->capacity) {
    int next = list->capacity == 0 ? 128 : list->capacity * 2;
    char **resized =
        realloc(list->paths, (size_t)next * sizeof(list->paths[0]));
    if (!resized) {
      return false;
    }
    list->paths = resized;
    list->capacity = next;
  }

  list->paths[list->count] = strdup(path);
  if (!list->paths[list->count]) {
    return false;
  }

  list->count++;
  return true;
}

static bool CollectSupportedMedia(const char *absolute_path, void *user_data) {
  PathList *list = (PathList *)user_data;
  if (!list) {
    return false;
  }

  if (!FsIsSupportedMedia(absolute_path)) {
    return true;
  }

  return PathListAppend(list, absolute_path);
}

static int ComparePaths(const void *left, const void *right) {
  const char *const *a = (const char *const *)left;
  const char *const *b = (const char *const *)right;
  return strcmp(*a, *b);
}

static bool CollectFilesRecursive(const char *root, PathList *out_list,
                                  char *out_error, size_t out_error_size) {
  if (!root || !out_list) {
    SetError(out_error, out_error_size,
             "invalid arguments for recursive file collection");
    return false;
  }

  if (!FsWalkDirectory(root, CollectSupportedMedia, out_list)) {
    SetError(out_error, out_error_size,
             "failed to walk target directory '%s'", root);
    return false;
  }

  if (out_list->count > 1) {
    qsort(out_list->paths, (size_t)out_list->count, sizeof(out_list->paths[0]),
          ComparePaths);
  }
  return true;
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

static bool EnsureRenameCachePaths(const char *env_dir, char *out_config_path,
                                   size_t out_config_path_size,
                                   char *out_preview_dir,
                                   size_t out_preview_dir_size) {
  if (!env_dir || env_dir[0] == '\0') {
    return false;
  }

  char cache_dir[MAX_PATH_LENGTH] = {0};
  snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", env_dir);
  if (!FsMakeDirRecursive(cache_dir)) {
    return false;
  }

  if (out_config_path && out_config_path_size > 0) {
    snprintf(out_config_path, out_config_path_size, "%s/rename_config.json",
             cache_dir);
  }

  if (out_preview_dir && out_preview_dir_size > 0) {
    snprintf(out_preview_dir, out_preview_dir_size, "%s/rename_previews",
             cache_dir);
    if (!FsMakeDirRecursive(out_preview_dir)) {
      return false;
    }
  }

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

bool RenamerPreviewLoadDefaultPattern(const char *env_dir, char *out_pattern,
                                      size_t out_pattern_size,
                                      char *out_error,
                                      size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_pattern || out_pattern_size == 0) {
    SetError(out_error, out_error_size,
             "env_dir and output buffer are required for default pattern load");
    return false;
  }

  out_pattern[0] = '\0';
  char config_path[MAX_PATH_LENGTH] = {0};
  if (!EnsureRenameCachePaths(env_dir, config_path, sizeof(config_path), NULL,
                              0)) {
    SetError(out_error, out_error_size,
             "failed to ensure rename cache paths under '%s'", env_dir);
    return false;
  }

  char *json_text = NULL;
  if (!LoadFileText(config_path, &json_text)) {
    strncpy(out_pattern, RenamerPatternDefault(), out_pattern_size - 1);
    out_pattern[out_pattern_size - 1] = '\0';
    return true;
  }

  cJSON *json = cJSON_Parse(json_text);
  free(json_text);
  if (!json) {
    strncpy(out_pattern, RenamerPatternDefault(), out_pattern_size - 1);
    out_pattern[out_pattern_size - 1] = '\0';
    return true;
  }

  cJSON *value = cJSON_GetObjectItem(json, "defaultPattern");
  if (cJSON_IsString(value) && value->valuestring && value->valuestring[0] != '\0') {
    strncpy(out_pattern, value->valuestring, out_pattern_size - 1);
    out_pattern[out_pattern_size - 1] = '\0';
  } else {
    strncpy(out_pattern, RenamerPatternDefault(), out_pattern_size - 1);
    out_pattern[out_pattern_size - 1] = '\0';
  }

  cJSON_Delete(json);
  return true;
}

bool RenamerPreviewSaveDefaultPattern(const char *env_dir, const char *pattern,
                                      char *out_error,
                                      size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !pattern || pattern[0] == '\0') {
    SetError(out_error, out_error_size,
             "env_dir and pattern are required for default pattern save");
    return false;
  }

  char config_path[MAX_PATH_LENGTH] = {0};
  if (!EnsureRenameCachePaths(env_dir, config_path, sizeof(config_path), NULL,
                              0)) {
    SetError(out_error, out_error_size,
             "failed to ensure rename cache paths under '%s'", env_dir);
    return false;
  }

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    SetError(out_error, out_error_size,
             "failed to allocate rename config JSON object");
    return false;
  }

  cJSON_AddNumberToObject(json, "version", 1);
  cJSON_AddStringToObject(json, "defaultPattern", pattern);
  char updated_at[32] = {0};
  NowUtc(updated_at, sizeof(updated_at));
  cJSON_AddStringToObject(json, "updatedAtUtc", updated_at);

  char *text = cJSON_Print(json);
  cJSON_Delete(json);
  if (!text) {
    SetError(out_error, out_error_size,
             "failed to serialize rename config JSON");
    return false;
  }

  bool ok = SaveFileText(config_path, text);
  cJSON_free(text);
  if (!ok) {
    SetError(out_error, out_error_size,
             "failed to write rename config '%s'", config_path);
    return false;
  }

  return true;
}

static void FingerprintInit(uint64_t *h1, uint64_t *h2) {
  if (!h1 || !h2) {
    return;
  }
  *h1 = 1469598103934665603ULL;
  *h2 = 1099511628211ULL;
}

static void FingerprintUpdate(uint64_t *h1, uint64_t *h2, const char *text) {
  if (!h1 || !h2 || !text) {
    return;
  }

  const unsigned char *p = (const unsigned char *)text;
  while (*p) {
    *h1 ^= (uint64_t)(*p);
    *h1 *= 1099511628211ULL;

    *h2 += (uint64_t)(*p);
    *h2 *= 14029467366897019727ULL;
    *h2 ^= (*h2 >> 33);
    p++;
  }
}

static void FingerprintFinalize(uint64_t h1, uint64_t h2, char *out_text,
                                size_t out_text_size) {
  if (!out_text || out_text_size == 0) {
    return;
  }
  snprintf(out_text, out_text_size, "%016llx%016llx",
           (unsigned long long)h1, (unsigned long long)h2);
}

static bool BuildFingerprint(const char *target_dir, const char *pattern,
                             const RenamerPreviewItem *items, int item_count,
                             char *out_fingerprint,
                             size_t out_fingerprint_size) {
  if (!target_dir || !pattern || !out_fingerprint || out_fingerprint_size == 0 ||
      item_count < 0) {
    return false;
  }

  uint64_t h1 = 0;
  uint64_t h2 = 0;
  FingerprintInit(&h1, &h2);
  FingerprintUpdate(&h1, &h2, target_dir);
  FingerprintUpdate(&h1, &h2, "|");
  FingerprintUpdate(&h1, &h2, pattern);

  for (int i = 0; i < item_count; i++) {
    char line[1280] = {0};
    snprintf(line, sizeof(line), "|%s|%.0f|%ld|%s",
             items[i].source_path, items[i].source_mod_time,
             items[i].source_size, items[i].candidate_path);
    FingerprintUpdate(&h1, &h2, line);
  }

  FingerprintFinalize(h1, h2, out_fingerprint, out_fingerprint_size);
  return true;
}

static void BuildPreviewId(const char *fingerprint, char *out_preview_id,
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

static bool BuildParentDir(const char *path, char *out_dir, size_t out_dir_size) {
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

static void ExtractTokensFromMetadata(const ImageMetadata *md, char *out_date,
                                      size_t out_date_size, char *out_time,
                                      size_t out_time_size,
                                      char *out_datetime,
                                      size_t out_datetime_size,
                                      char *out_camera,
                                      size_t out_camera_size,
                                      char *out_make,
                                      size_t out_make_size,
                                      char *out_model,
                                      size_t out_model_size,
                                      char *out_format,
                                      size_t out_format_size) {
  if (!md) {
    return;
  }

  if (out_date && out_date_size > 0) {
    out_date[0] = '\0';
  }
  if (out_time && out_time_size > 0) {
    out_time[0] = '\0';
  }
  if (out_datetime && out_datetime_size > 0) {
    out_datetime[0] = '\0';
  }
  if (out_camera && out_camera_size > 0) {
    out_camera[0] = '\0';
  }
  if (out_make && out_make_size > 0) {
    out_make[0] = '\0';
  }
  if (out_model && out_model_size > 0) {
    out_model[0] = '\0';
  }
  if (out_format && out_format_size > 0) {
    out_format[0] = '\0';
  }

  if (md->dateTaken[0] != '\0' && strlen(md->dateTaken) >= 19) {
    if (out_date && out_date_size > 0) {
      snprintf(out_date, out_date_size, "%.4s%.2s%.2s", md->dateTaken,
               md->dateTaken + 5, md->dateTaken + 8);
    }
    if (out_time && out_time_size > 0) {
      snprintf(out_time, out_time_size, "%.2s%.2s%.2s", md->dateTaken + 11,
               md->dateTaken + 14, md->dateTaken + 17);
    }
    if (out_datetime && out_datetime_size > 0) {
      snprintf(out_datetime, out_datetime_size, "%.4s%.2s%.2s-%.2s%.2s%.2s",
               md->dateTaken, md->dateTaken + 5, md->dateTaken + 8,
               md->dateTaken + 11, md->dateTaken + 14, md->dateTaken + 17);
    }
  }

  if (out_make && out_make_size > 0) {
    strncpy(out_make, md->cameraMake, out_make_size - 1);
    out_make[out_make_size - 1] = '\0';
  }

  if (out_model && out_model_size > 0) {
    strncpy(out_model, md->cameraModel, out_model_size - 1);
    out_model[out_model_size - 1] = '\0';
  }

  if (out_camera && out_camera_size > 0) {
    if (md->cameraMake[0] != '\0' && md->cameraModel[0] != '\0') {
      snprintf(out_camera, out_camera_size, "%s-%s", md->cameraMake,
               md->cameraModel);
    } else if (md->cameraModel[0] != '\0') {
      strncpy(out_camera, md->cameraModel, out_camera_size - 1);
      out_camera[out_camera_size - 1] = '\0';
    } else if (md->cameraMake[0] != '\0') {
      strncpy(out_camera, md->cameraMake, out_camera_size - 1);
      out_camera[out_camera_size - 1] = '\0';
    }
  }

  if (out_format && out_format_size > 0 && md->path) {
    const char *dot = strrchr(md->path, '.');
    if (dot && dot[1] != '\0') {
      for (size_t i = 1; dot[i] != '\0' && i < out_format_size; i++) {
        out_format[i - 1] = (char)tolower((unsigned char)dot[i]);
        out_format[i] = '\0';
      }
    }
  }
}

static bool LoadMetadataForPath(const char *absolute_path,
                                bool *io_cache_dirty,
                                ImageMetadata *out_metadata,
                                char *out_error, size_t out_error_size) {
  if (!absolute_path || !io_cache_dirty || !out_metadata) {
    SetError(out_error, out_error_size,
             "invalid metadata load arguments for renamer preview");
    return false;
  }

  memset(out_metadata, 0, sizeof(*out_metadata));

  double mod_date = 0.0;
  long file_size = 0;
  if (!ExtractBasicMetadata(absolute_path, &mod_date, &file_size)) {
    SetError(out_error, out_error_size,
             "failed to read file metadata for '%s'", absolute_path);
    return false;
  }

  ImageMetadata cached = {0};
  if (CacheGetValidEntry(absolute_path, mod_date, file_size, &cached)) {
    bool needs_refresh = false;
    if (cached.dateTaken[0] == '\0') {
      needs_refresh = true;
    }
    if (cached.cameraMake[0] == '\0' && cached.cameraModel[0] == '\0') {
      needs_refresh = true;
    }
    if (!cached.allMetadataJson || cached.allMetadataJson[0] == '\0') {
      needs_refresh = true;
    }

    if (!needs_refresh) {
      *out_metadata = cached;
      return true;
    }

    CacheFreeMetadata(&cached);
  }

  ImageMetadata fresh = ExtractMetadata(absolute_path, true);
  fresh.path = strdup(absolute_path);
  if (!fresh.path) {
    CacheFreeMetadata(&fresh);
    SetError(out_error, out_error_size,
             "out of memory while preparing metadata for '%s'", absolute_path);
    return false;
  }
  fresh.modificationDate = mod_date;
  fresh.fileSize = file_size;

  if (!CacheUpdateEntry(&fresh)) {
    CacheFreeMetadata(&fresh);
    SetError(out_error, out_error_size,
             "failed to refresh cache entry for '%s'", absolute_path);
    return false;
  }

  *io_cache_dirty = true;
  *out_metadata = fresh;
  return true;
}

static bool DetectCollisions(RenamerPreviewArtifact *preview) {
  if (!preview || !preview->items || preview->file_count <= 0) {
    return true;
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

  return true;
}

static cJSON *BuildPreviewJson(const RenamerPreviewArtifact *preview) {
  if (!preview) {
    return NULL;
  }

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return NULL;
  }

  cJSON_AddNumberToObject(root, "version", 1);
  cJSON_AddStringToObject(root, "previewId", preview->preview_id);
  cJSON_AddStringToObject(root, "createdAtUtc", preview->created_at_utc);
  cJSON_AddStringToObject(root, "envDir", preview->env_dir);
  cJSON_AddStringToObject(root, "targetDir", preview->target_dir);
  cJSON_AddStringToObject(root, "pattern", preview->pattern);
  cJSON_AddStringToObject(root, "fingerprint", preview->fingerprint);
  cJSON_AddNumberToObject(root, "fileCount", preview->file_count);
  cJSON_AddNumberToObject(root, "collisionGroupCount",
                          preview->collision_group_count);
  cJSON_AddNumberToObject(root, "collisionCount", preview->collision_count);
  cJSON_AddNumberToObject(root, "truncationCount", preview->truncation_count);
  cJSON_AddBoolToObject(root, "requiresAutoSuffixAcceptance",
                        preview->requires_auto_suffix_acceptance);

  cJSON *items = cJSON_CreateArray();
  if (!items) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *collision_groups = cJSON_CreateArray();
  if (!collision_groups) {
    cJSON_Delete(items);
    cJSON_Delete(root);
    return NULL;
  }

  for (int i = 0; i < preview->file_count; i++) {
    cJSON *item = cJSON_CreateObject();
    if (!item) {
      continue;
    }

    cJSON_AddStringToObject(item, "source", preview->items[i].source_path);
    cJSON_AddStringToObject(item, "candidate", preview->items[i].candidate_path);
    cJSON_AddStringToObject(item, "candidateName",
                            preview->items[i].candidate_filename);
    cJSON_AddStringToObject(item, "tagsManual", preview->items[i].tags_manual);
    cJSON_AddStringToObject(item, "tagsMeta", preview->items[i].tags_meta);
    cJSON_AddStringToObject(item, "tags", preview->items[i].tags_merged);
    cJSON_AddBoolToObject(item, "collisionInBatch",
                          preview->items[i].collision_in_batch);
    cJSON_AddBoolToObject(item, "collisionOnDisk",
                          preview->items[i].collision_on_disk);
    cJSON_AddBoolToObject(item, "truncated", preview->items[i].truncated);
    cJSON_AddNumberToObject(item, "sourceModTime", preview->items[i].source_mod_time);
    cJSON_AddNumberToObject(item, "sourceSize", preview->items[i].source_size);

    cJSON_AddItemToArray(items, item);
  }

  for (int i = 0; i < preview->file_count; i++) {
    if (!(preview->items[i].collision_in_batch ||
          preview->items[i].collision_on_disk)) {
      continue;
    }

    bool seen = false;
    int existing_count = cJSON_GetArraySize(collision_groups);
    for (int j = 0; j < existing_count; j++) {
      cJSON *group = cJSON_GetArrayItem(collision_groups, j);
      cJSON *candidate = cJSON_GetObjectItem(group, "candidate");
      if (!cJSON_IsString(candidate) || !candidate->valuestring) {
        continue;
      }
      if (strcmp(candidate->valuestring, preview->items[i].candidate_path) == 0) {
        seen = true;
        break;
      }
    }

    if (seen) {
      continue;
    }

    cJSON *group = cJSON_CreateObject();
    if (!group) {
      continue;
    }

    int members = 0;
    bool on_disk_conflict = false;
    for (int j = 0; j < preview->file_count; j++) {
      if (strcmp(preview->items[j].candidate_path,
                 preview->items[i].candidate_path) == 0) {
        members++;
        if (preview->items[j].collision_on_disk) {
          on_disk_conflict = true;
        }
      }
    }

    cJSON_AddStringToObject(group, "candidate", preview->items[i].candidate_path);
    cJSON_AddNumberToObject(group, "members", members);
    cJSON_AddBoolToObject(group, "onDiskConflict", on_disk_conflict);
    cJSON_AddItemToArray(collision_groups, group);
  }

  cJSON *warnings = cJSON_CreateArray();
  if (!warnings) {
    cJSON_Delete(collision_groups);
    cJSON_Delete(items);
    cJSON_Delete(root);
    return NULL;
  }

  if (preview->truncation_count > 0) {
    cJSON_AddItemToArray(
        warnings,
        cJSON_CreateString(
            "One or more filenames exceeded max length and were truncate+hash adjusted"));
  }
  if (preview->requires_auto_suffix_acceptance) {
    cJSON_AddItemToArray(
        warnings,
        cJSON_CreateString(
            "Collisions detected. Apply requires explicit auto-suffix acceptance"));
  }

  cJSON_AddItemToObject(root, "items", items);
  cJSON_AddItemToObject(root, "collisionGroups", collision_groups);
  cJSON_AddItemToObject(root, "warnings", warnings);
  return root;
}

static bool ParsePreviewItem(const cJSON *node, RenamerPreviewItem *out_item) {
  if (!cJSON_IsObject((cJSON *)node) || !out_item) {
    return false;
  }

  memset(out_item, 0, sizeof(*out_item));

  cJSON *source = cJSON_GetObjectItem((cJSON *)node, "source");
  cJSON *candidate = cJSON_GetObjectItem((cJSON *)node, "candidate");
  cJSON *candidate_name = cJSON_GetObjectItem((cJSON *)node, "candidateName");

  if (!cJSON_IsString(source) || !source->valuestring || !cJSON_IsString(candidate) ||
      !candidate->valuestring) {
    return false;
  }

  strncpy(out_item->source_path, source->valuestring,
          sizeof(out_item->source_path) - 1);
  out_item->source_path[sizeof(out_item->source_path) - 1] = '\0';
  strncpy(out_item->candidate_path, candidate->valuestring,
          sizeof(out_item->candidate_path) - 1);
  out_item->candidate_path[sizeof(out_item->candidate_path) - 1] = '\0';

  if (cJSON_IsString(candidate_name) && candidate_name->valuestring) {
    strncpy(out_item->candidate_filename, candidate_name->valuestring,
            sizeof(out_item->candidate_filename) - 1);
    out_item->candidate_filename[sizeof(out_item->candidate_filename) - 1] =
        '\0';
  }

  cJSON *tags_manual = cJSON_GetObjectItem((cJSON *)node, "tagsManual");
  cJSON *tags_meta = cJSON_GetObjectItem((cJSON *)node, "tagsMeta");
  cJSON *tags = cJSON_GetObjectItem((cJSON *)node, "tags");
  if (cJSON_IsString(tags_manual) && tags_manual->valuestring) {
    strncpy(out_item->tags_manual, tags_manual->valuestring,
            sizeof(out_item->tags_manual) - 1);
    out_item->tags_manual[sizeof(out_item->tags_manual) - 1] = '\0';
  }
  if (cJSON_IsString(tags_meta) && tags_meta->valuestring) {
    strncpy(out_item->tags_meta, tags_meta->valuestring,
            sizeof(out_item->tags_meta) - 1);
    out_item->tags_meta[sizeof(out_item->tags_meta) - 1] = '\0';
  }
  if (cJSON_IsString(tags) && tags->valuestring) {
    strncpy(out_item->tags_merged, tags->valuestring,
            sizeof(out_item->tags_merged) - 1);
    out_item->tags_merged[sizeof(out_item->tags_merged) - 1] = '\0';
  }

  cJSON *collision_batch = cJSON_GetObjectItem((cJSON *)node, "collisionInBatch");
  cJSON *collision_disk = cJSON_GetObjectItem((cJSON *)node, "collisionOnDisk");
  cJSON *truncated = cJSON_GetObjectItem((cJSON *)node, "truncated");
  cJSON *mod_time = cJSON_GetObjectItem((cJSON *)node, "sourceModTime");
  cJSON *source_size = cJSON_GetObjectItem((cJSON *)node, "sourceSize");

  out_item->collision_in_batch = cJSON_IsBool(collision_batch)
                                     ? cJSON_IsTrue(collision_batch)
                                     : false;
  out_item->collision_on_disk =
      cJSON_IsBool(collision_disk) ? cJSON_IsTrue(collision_disk) : false;
  out_item->truncated = cJSON_IsBool(truncated) ? cJSON_IsTrue(truncated) : false;
  out_item->source_mod_time =
      cJSON_IsNumber(mod_time) ? mod_time->valuedouble : 0.0;
  out_item->source_size = cJSON_IsNumber(source_size) ? (long)source_size->valuedouble
                                                       : 0;

  if (out_item->candidate_filename[0] == '\0') {
    const char *slash = strrchr(out_item->candidate_path, '/');
    const char *base = slash ? slash + 1 : out_item->candidate_path;
    strncpy(out_item->candidate_filename, base,
            sizeof(out_item->candidate_filename) - 1);
    out_item->candidate_filename[sizeof(out_item->candidate_filename) - 1] =
        '\0';
  }

  return true;
}

bool RenamerPreviewBuild(const RenamerPreviewRequest *request,
                         RenamerPreviewArtifact *out_preview,
                         char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!request || !out_preview || !request->env_dir || request->env_dir[0] == '\0' ||
      !request->target_dir || request->target_dir[0] == '\0') {
    SetError(out_error, out_error_size,
             "rename preview requires env_dir and target_dir");
    return false;
  }

  (void)request->recursive;

  memset(out_preview, 0, sizeof(*out_preview));

  char target_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(request->target_dir, target_abs, sizeof(target_abs))) {
    SetError(out_error, out_error_size,
             "failed to resolve target directory '%s'", request->target_dir);
    return false;
  }

  char effective_pattern[256] = {0};
  if (request->pattern && request->pattern[0] != '\0') {
    strncpy(effective_pattern, request->pattern, sizeof(effective_pattern) - 1);
    effective_pattern[sizeof(effective_pattern) - 1] = '\0';
  } else if (!RenamerPreviewLoadDefaultPattern(request->env_dir, effective_pattern,
                                               sizeof(effective_pattern), out_error,
                                               out_error_size)) {
    return false;
  }

  char pattern_error[256] = {0};
  if (!RenamerPatternValidate(effective_pattern, pattern_error,
                              sizeof(pattern_error))) {
    SetError(out_error, out_error_size, "%s", pattern_error);
    return false;
  }

  if (!RenamerPreviewSaveDefaultPattern(request->env_dir, effective_pattern,
                                        out_error, out_error_size)) {
    return false;
  }

  PathList files = {0};
  if (!CollectFilesRecursive(target_abs, &files, out_error, out_error_size)) {
    PathListFree(&files);
    return false;
  }

  out_preview->items =
      calloc((size_t)(files.count > 0 ? files.count : 1), sizeof(out_preview->items[0]));
  if (!out_preview->items) {
    PathListFree(&files);
    SetError(out_error, out_error_size,
             "out of memory while allocating rename preview items");
    return false;
  }

  out_preview->file_count = files.count;
  strncpy(out_preview->env_dir, request->env_dir, sizeof(out_preview->env_dir) - 1);
  out_preview->env_dir[sizeof(out_preview->env_dir) - 1] = '\0';
  strncpy(out_preview->target_dir, target_abs, sizeof(out_preview->target_dir) - 1);
  out_preview->target_dir[sizeof(out_preview->target_dir) - 1] = '\0';
  strncpy(out_preview->pattern, effective_pattern, sizeof(out_preview->pattern) - 1);
  out_preview->pattern[sizeof(out_preview->pattern) - 1] = '\0';
  NowUtc(out_preview->created_at_utc, sizeof(out_preview->created_at_utc));

  cJSON *tags_root = NULL;
  if (!RenamerTagsLoadSidecar(request->env_dir, &tags_root, out_error,
                              out_error_size)) {
    PathListFree(&files);
    RenamerPreviewFree(out_preview);
    return false;
  }

  if (!RenamerTagsApplyMapFile(tags_root, request->tags_map_path,
                               (const char **)files.paths, files.count,
                               out_error, out_error_size)) {
    cJSON_Delete(tags_root);
    PathListFree(&files);
    RenamerPreviewFree(out_preview);
    return false;
  }

  if (!RenamerTagsApplyBulkCsv(tags_root, (const char **)files.paths, files.count,
                               request->tag_add_csv, request->tag_remove_csv,
                               out_error, out_error_size)) {
    cJSON_Delete(tags_root);
    PathListFree(&files);
    RenamerPreviewFree(out_preview);
    return false;
  }

  if (!RenamerTagsSaveSidecar(request->env_dir, tags_root, out_error,
                              out_error_size)) {
    cJSON_Delete(tags_root);
    PathListFree(&files);
    RenamerPreviewFree(out_preview);
    return false;
  }

  bool cache_dirty = false;
  for (int i = 0; i < files.count; i++) {
    ImageMetadata md = {0};
    if (!LoadMetadataForPath(files.paths[i], &cache_dirty, &md, out_error,
                             out_error_size)) {
      cJSON_Delete(tags_root);
      PathListFree(&files);
      RenamerPreviewFree(out_preview);
      return false;
    }

    char date[64] = {0};
    char time_text[64] = {0};
    char datetime[96] = {0};
    char camera[128] = {0};
    char make[128] = {0};
    char model[128] = {0};
    char format[32] = {0};

    ExtractTokensFromMetadata(&md, date, sizeof(date), time_text,
                              sizeof(time_text), datetime, sizeof(datetime),
                              camera, sizeof(camera), make, sizeof(make), model,
                              sizeof(model), format, sizeof(format));

    RenamerResolvedTags resolved_tags = {0};
    if (!RenamerTagsResolve(tags_root, files.paths[i], md.allMetadataJson,
                            &resolved_tags, out_error, out_error_size)) {
      CacheFreeMetadata(&md);
      cJSON_Delete(tags_root);
      PathListFree(&files);
      RenamerPreviewFree(out_preview);
      return false;
    }

    RenamerPatternContext ctx = {
        .date = date,
        .time = time_text,
        .datetime = datetime,
        .camera = camera,
        .make = make,
        .model = model,
        .format = format,
        .tags_manual = resolved_tags.manual,
        .tags_meta = resolved_tags.meta,
        .tags = resolved_tags.merged,
        .index = i + 1,
    };

    bool truncated = false;
    char warning[128] = {0};
    char render_error[256] = {0};
    if (!RenamerPatternRender(effective_pattern, &ctx,
                              out_preview->items[i].candidate_filename,
                              sizeof(out_preview->items[i].candidate_filename),
                              &truncated, warning, sizeof(warning), render_error,
                              sizeof(render_error))) {
      CacheFreeMetadata(&md);
      cJSON_Delete(tags_root);
      PathListFree(&files);
      RenamerPreviewFree(out_preview);
      SetError(out_error, out_error_size, "%s", render_error);
      return false;
    }

    if (truncated) {
      out_preview->truncation_count++;
    }

    char parent_dir[MAX_PATH_LENGTH] = {0};
    if (!BuildParentDir(files.paths[i], parent_dir, sizeof(parent_dir))) {
      CacheFreeMetadata(&md);
      cJSON_Delete(tags_root);
      PathListFree(&files);
      RenamerPreviewFree(out_preview);
      SetError(out_error, out_error_size,
               "failed to resolve parent directory for '%s'", files.paths[i]);
      return false;
    }

    snprintf(out_preview->items[i].candidate_path,
             sizeof(out_preview->items[i].candidate_path), "%s/%s", parent_dir,
             out_preview->items[i].candidate_filename);

    strncpy(out_preview->items[i].source_path, files.paths[i],
            sizeof(out_preview->items[i].source_path) - 1);
    out_preview->items[i].source_path[sizeof(out_preview->items[i].source_path) - 1] =
        '\0';
    strncpy(out_preview->items[i].tags_manual, resolved_tags.manual,
            sizeof(out_preview->items[i].tags_manual) - 1);
    out_preview->items[i].tags_manual[sizeof(out_preview->items[i].tags_manual) - 1] =
        '\0';
    strncpy(out_preview->items[i].tags_meta, resolved_tags.meta,
            sizeof(out_preview->items[i].tags_meta) - 1);
    out_preview->items[i].tags_meta[sizeof(out_preview->items[i].tags_meta) - 1] =
        '\0';
    strncpy(out_preview->items[i].tags_merged, resolved_tags.merged,
            sizeof(out_preview->items[i].tags_merged) - 1);
    out_preview->items[i].tags_merged[sizeof(out_preview->items[i].tags_merged) - 1] =
        '\0';
    out_preview->items[i].truncated = truncated;
    out_preview->items[i].source_mod_time = md.modificationDate;
    out_preview->items[i].source_size = md.fileSize;

    CacheFreeMetadata(&md);
  }

  if (cache_dirty && !CacheSave()) {
    cJSON_Delete(tags_root);
    PathListFree(&files);
    RenamerPreviewFree(out_preview);
    SetError(out_error, out_error_size,
             "failed to persist cache while refreshing rename metadata");
    return false;
  }

  DetectCollisions(out_preview);

  if (!BuildFingerprint(out_preview->target_dir, out_preview->pattern,
                        out_preview->items, out_preview->file_count,
                        out_preview->fingerprint,
                        sizeof(out_preview->fingerprint))) {
    cJSON_Delete(tags_root);
    PathListFree(&files);
    RenamerPreviewFree(out_preview);
    SetError(out_error, out_error_size,
             "failed to generate rename preview fingerprint");
    return false;
  }

  BuildPreviewId(out_preview->fingerprint, out_preview->preview_id,
                 sizeof(out_preview->preview_id));

  cJSON_Delete(tags_root);
  PathListFree(&files);
  return true;
}

bool RenamerPreviewSaveArtifact(const RenamerPreviewArtifact *preview,
                                char *out_artifact_path,
                                size_t out_artifact_path_size,
                                char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!preview || !preview->env_dir[0] || !preview->preview_id[0]) {
    SetError(out_error, out_error_size,
             "preview artifact save requires populated preview object");
    return false;
  }

  char preview_dir[MAX_PATH_LENGTH] = {0};
  if (!EnsureRenameCachePaths(preview->env_dir, NULL, 0, preview_dir,
                              sizeof(preview_dir))) {
    SetError(out_error, out_error_size,
             "failed to ensure rename preview directory");
    return false;
  }

  char artifact_path[MAX_PATH_LENGTH] = {0};
  snprintf(artifact_path, sizeof(artifact_path), "%s/%s.json", preview_dir,
           preview->preview_id);

  cJSON *json = BuildPreviewJson(preview);
  if (!json) {
    SetError(out_error, out_error_size,
             "failed to serialize rename preview artifact");
    return false;
  }

  char *text = cJSON_Print(json);
  cJSON_Delete(json);
  if (!text) {
    SetError(out_error, out_error_size,
             "failed to encode rename preview artifact JSON");
    return false;
  }

  bool ok = SaveFileText(artifact_path, text);
  cJSON_free(text);
  if (!ok) {
    SetError(out_error, out_error_size,
             "failed to write rename preview artifact '%s'", artifact_path);
    return false;
  }

  if (out_artifact_path && out_artifact_path_size > 0) {
    strncpy(out_artifact_path, artifact_path, out_artifact_path_size - 1);
    out_artifact_path[out_artifact_path_size - 1] = '\0';
  }

  return true;
}

bool RenamerPreviewLoadArtifact(const char *env_dir, const char *preview_id,
                                RenamerPreviewArtifact *out_preview,
                                char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !preview_id || preview_id[0] == '\0' ||
      !out_preview) {
    SetError(out_error, out_error_size,
             "env_dir and preview_id are required for artifact load");
    return false;
  }

  memset(out_preview, 0, sizeof(*out_preview));

  char preview_dir[MAX_PATH_LENGTH] = {0};
  if (!EnsureRenameCachePaths(env_dir, NULL, 0, preview_dir, sizeof(preview_dir))) {
    SetError(out_error, out_error_size,
             "failed to ensure rename preview path under '%s'", env_dir);
    return false;
  }

  char artifact_path[MAX_PATH_LENGTH] = {0};
  snprintf(artifact_path, sizeof(artifact_path), "%s/%s.json", preview_dir,
           preview_id);

  char *json_text = NULL;
  if (!LoadFileText(artifact_path, &json_text)) {
    SetError(out_error, out_error_size,
             "rename preview artifact '%s' was not found", artifact_path);
    return false;
  }

  cJSON *json = cJSON_Parse(json_text);
  free(json_text);
  if (!json) {
    SetError(out_error, out_error_size,
             "rename preview artifact '%s' is malformed", artifact_path);
    return false;
  }

  cJSON *id = cJSON_GetObjectItem(json, "previewId");
  cJSON *created = cJSON_GetObjectItem(json, "createdAtUtc");
  cJSON *stored_env = cJSON_GetObjectItem(json, "envDir");
  cJSON *target = cJSON_GetObjectItem(json, "targetDir");
  cJSON *pattern = cJSON_GetObjectItem(json, "pattern");
  cJSON *fingerprint = cJSON_GetObjectItem(json, "fingerprint");
  cJSON *file_count = cJSON_GetObjectItem(json, "fileCount");
  cJSON *collision_groups = cJSON_GetObjectItem(json, "collisionGroupCount");
  cJSON *collision_count = cJSON_GetObjectItem(json, "collisionCount");
  cJSON *truncation_count = cJSON_GetObjectItem(json, "truncationCount");
  cJSON *requires_suffix =
      cJSON_GetObjectItem(json, "requiresAutoSuffixAcceptance");
  cJSON *items = cJSON_GetObjectItem(json, "items");

  if (!cJSON_IsString(id) || !id->valuestring || !cJSON_IsString(target) ||
      !target->valuestring || !cJSON_IsString(pattern) || !pattern->valuestring ||
      !cJSON_IsString(fingerprint) || !fingerprint->valuestring ||
      !cJSON_IsArray(items)) {
    cJSON_Delete(json);
    SetError(out_error, out_error_size,
             "rename preview artifact '%s' is missing required fields",
             artifact_path);
    return false;
  }

  int count = cJSON_GetArraySize(items);
  out_preview->items =
      calloc((size_t)(count > 0 ? count : 1), sizeof(out_preview->items[0]));
  if (!out_preview->items) {
    cJSON_Delete(json);
    SetError(out_error, out_error_size,
             "out of memory while loading rename preview items");
    return false;
  }

  out_preview->file_count = count;
  strncpy(out_preview->preview_id, id->valuestring,
          sizeof(out_preview->preview_id) - 1);
  out_preview->preview_id[sizeof(out_preview->preview_id) - 1] = '\0';
  if (cJSON_IsString(created) && created->valuestring) {
    strncpy(out_preview->created_at_utc, created->valuestring,
            sizeof(out_preview->created_at_utc) - 1);
    out_preview->created_at_utc[sizeof(out_preview->created_at_utc) - 1] = '\0';
  }
  if (cJSON_IsString(stored_env) && stored_env->valuestring) {
    strncpy(out_preview->env_dir, stored_env->valuestring,
            sizeof(out_preview->env_dir) - 1);
    out_preview->env_dir[sizeof(out_preview->env_dir) - 1] = '\0';
  } else {
    strncpy(out_preview->env_dir, env_dir, sizeof(out_preview->env_dir) - 1);
    out_preview->env_dir[sizeof(out_preview->env_dir) - 1] = '\0';
  }
  strncpy(out_preview->target_dir, target->valuestring,
          sizeof(out_preview->target_dir) - 1);
  out_preview->target_dir[sizeof(out_preview->target_dir) - 1] = '\0';
  strncpy(out_preview->pattern, pattern->valuestring,
          sizeof(out_preview->pattern) - 1);
  out_preview->pattern[sizeof(out_preview->pattern) - 1] = '\0';
  strncpy(out_preview->fingerprint, fingerprint->valuestring,
          sizeof(out_preview->fingerprint) - 1);
  out_preview->fingerprint[sizeof(out_preview->fingerprint) - 1] = '\0';

  out_preview->collision_group_count =
      cJSON_IsNumber(collision_groups) ? (int)collision_groups->valuedouble : 0;
  out_preview->collision_count =
      cJSON_IsNumber(collision_count) ? (int)collision_count->valuedouble : 0;
  out_preview->truncation_count =
      cJSON_IsNumber(truncation_count) ? (int)truncation_count->valuedouble : 0;
  out_preview->requires_auto_suffix_acceptance =
      cJSON_IsBool(requires_suffix) ? cJSON_IsTrue(requires_suffix)
                                    : (out_preview->collision_count > 0);

  if (cJSON_IsNumber(file_count)) {
    int declared = (int)file_count->valuedouble;
    if (declared >= 0 && declared != count) {
      out_preview->file_count = declared < count ? declared : count;
    }
  }

  for (int i = 0; i < count; i++) {
    cJSON *node = cJSON_GetArrayItem(items, i);
    if (!ParsePreviewItem(node, &out_preview->items[i])) {
      cJSON_Delete(json);
      RenamerPreviewFree(out_preview);
      SetError(out_error, out_error_size,
               "failed to parse rename preview item index %d", i);
      return false;
    }
  }

  cJSON_Delete(json);
  return true;
}

bool RenamerPreviewRecomputeFingerprint(const RenamerPreviewArtifact *preview,
                                        char *out_fingerprint,
                                        size_t out_fingerprint_size,
                                        char *out_error,
                                        size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!preview || !out_fingerprint || out_fingerprint_size == 0) {
    SetError(out_error, out_error_size,
             "invalid arguments for preview fingerprint revalidation");
    return false;
  }

  RenamerPreviewItem *recomputed =
      calloc((size_t)(preview->file_count > 0 ? preview->file_count : 1),
             sizeof(RenamerPreviewItem));
  if (!recomputed) {
    SetError(out_error, out_error_size,
             "out of memory during fingerprint revalidation");
    return false;
  }

  for (int i = 0; i < preview->file_count; i++) {
    strncpy(recomputed[i].source_path, preview->items[i].source_path,
            sizeof(recomputed[i].source_path) - 1);
    recomputed[i].source_path[sizeof(recomputed[i].source_path) - 1] = '\0';
    strncpy(recomputed[i].candidate_path, preview->items[i].candidate_path,
            sizeof(recomputed[i].candidate_path) - 1);
    recomputed[i].candidate_path[sizeof(recomputed[i].candidate_path) - 1] = '\0';

    double mod_date = 0.0;
    long file_size = 0;
    if (!ExtractBasicMetadata(preview->items[i].source_path, &mod_date, &file_size)) {
      free(recomputed);
      SetError(out_error, out_error_size,
               "preview fingerprint mismatch: source '%s' is missing or changed",
               preview->items[i].source_path);
      return false;
    }

    recomputed[i].source_mod_time = mod_date;
    recomputed[i].source_size = file_size;
  }

  bool ok = BuildFingerprint(preview->target_dir, preview->pattern, recomputed,
                             preview->file_count, out_fingerprint,
                             out_fingerprint_size);
  free(recomputed);

  if (!ok) {
    SetError(out_error, out_error_size,
             "failed to compute revalidation fingerprint");
    return false;
  }

  return true;
}

void RenamerPreviewFree(RenamerPreviewArtifact *preview) {
  if (!preview) {
    return;
  }

  free(preview->items);
  preview->items = NULL;
  preview->file_count = 0;
  preview->collision_group_count = 0;
  preview->collision_count = 0;
  preview->truncation_count = 0;
  preview->requires_auto_suffix_acceptance = false;

  preview->preview_id[0] = '\0';
  preview->created_at_utc[0] = '\0';
  preview->env_dir[0] = '\0';
  preview->target_dir[0] = '\0';
  preview->pattern[0] = '\0';
  preview->fingerprint[0] = '\0';
}
