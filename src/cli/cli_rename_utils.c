#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "cli/cli_rename_utils.h"

#define CLI_RENAME_MAX_EDIT_LEN 255
#define CLI_RENAME_MAX_FILENAME_TAGS 64

#if !defined(NAME_MAX)
#define NAME_MAX 255
#endif

typedef struct {
  char **items;
  int count;
  int capacity;
} PathList;

typedef struct {
  int count;
} MediaCountCtx;

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

static bool IsExistingDirectory(const char *path) {
  if (!path || path[0] == '\0') {
    return false;
  }

  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static bool SaveTextFile(const char *path, const char *text) {
  if (!path || path[0] == '\0' || !text) {
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

static bool LoadTextFile(const char *path, char **out_text) {
  if (!path || path[0] == '\0' || !out_text) {
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
  char *text = calloc((size_t)size + 1, 1);
  if (!text) {
    fclose(f);
    return false;
  }

  size_t read_bytes = fread(text, 1, (size_t)size, f);
  fclose(f);
  text[read_bytes] = '\0';
  *out_text = text;
  return true;
}

static void PathListFree(PathList *list) {
  if (!list) {
    return;
  }

  for (int i = 0; i < list->count; i++) {
    free(list->items[i]);
  }
  free(list->items);
  list->items = NULL;
  list->count = 0;
  list->capacity = 0;
}

static bool PathListAppend(PathList *list, const char *path) {
  if (!list || !path || path[0] == '\0') {
    return false;
  }

  if (list->count >= list->capacity) {
    int next_capacity = list->capacity == 0 ? 64 : list->capacity * 2;
    char **next_items =
        realloc(list->items, (size_t)next_capacity * sizeof(list->items[0]));
    if (!next_items) {
      return false;
    }
    list->items = next_items;
    list->capacity = next_capacity;
  }

  list->items[list->count] = strdup(path);
  if (!list->items[list->count]) {
    return false;
  }
  list->count++;
  return true;
}

static bool CollectMediaPathCallback(const char *absolute_path, void *user_data) {
  PathList *list = (PathList *)user_data;
  if (!list) {
    return false;
  }
  if (!FsIsSupportedMedia(absolute_path)) {
    return true;
  }
  return PathListAppend(list, absolute_path);
}

static int CompareStringPtr(const void *left, const void *right) {
  const char *const *a = (const char *const *)left;
  const char *const *b = (const char *const *)right;
  return strcmp(*a, *b);
}

static bool CollectMediaPaths(const char *root_dir, PathList *out_paths,
                              char *out_error, size_t out_error_size) {
  if (!root_dir || root_dir[0] == '\0' || !out_paths) {
    SetError(out_error, out_error_size,
             "target directory is required for media collection");
    return false;
  }

  memset(out_paths, 0, sizeof(*out_paths));
  if (!FsWalkDirectory(root_dir, CollectMediaPathCallback, out_paths)) {
    PathListFree(out_paths);
    SetError(out_error, out_error_size,
             "failed to scan media files under '%s'", root_dir);
    return false;
  }

  if (out_paths->count > 1) {
    qsort(out_paths->items, (size_t)out_paths->count, sizeof(out_paths->items[0]),
          CompareStringPtr);
  }
  return true;
}

static bool CountMediaPathCallback(const char *absolute_path, void *user_data) {
  MediaCountCtx *ctx = (MediaCountCtx *)user_data;
  if (!ctx) {
    return false;
  }
  if (FsIsSupportedMedia(absolute_path)) {
    ctx->count++;
  }
  return true;
}

static bool EnsureRenameCacheLayout(const char *env_dir, char *out_env_abs,
                                    size_t out_env_abs_size, char *out_error,
                                    size_t out_error_size) {
  if (!env_dir || env_dir[0] == '\0') {
    SetError(out_error, out_error_size, "environment directory is required");
    return false;
  }

  if (!FsMakeDirRecursive(env_dir) || !IsExistingDirectory(env_dir)) {
    SetError(out_error, out_error_size,
             "failed to create environment directory '%s'", env_dir);
    return false;
  }

  char cache_dir[MAX_PATH_LENGTH] = {0};
  char preview_dir[MAX_PATH_LENGTH] = {0};
  char history_dir[MAX_PATH_LENGTH] = {0};
  snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", env_dir);
  snprintf(preview_dir, sizeof(preview_dir), "%s/rename_previews", cache_dir);
  snprintf(history_dir, sizeof(history_dir), "%s/rename_history", cache_dir);

  if (!FsMakeDirRecursive(cache_dir) || !FsMakeDirRecursive(preview_dir) ||
      !FsMakeDirRecursive(history_dir) || !IsExistingDirectory(preview_dir) ||
      !IsExistingDirectory(history_dir)) {
    SetError(out_error, out_error_size,
             "failed to create rename cache layout under '%s'", env_dir);
    return false;
  }

  if (out_env_abs && out_env_abs_size > 0) {
    if (!FsGetAbsolutePath(env_dir, out_env_abs, out_env_abs_size)) {
      SetError(out_error, out_error_size,
               "failed to resolve absolute environment path '%s'", env_dir);
      return false;
    }
  }

  return true;
}

bool CliRenameInitEnvironment(const char *target_dir, const char *env_dir,
                              CliRenameInitResult *out_result,
                              char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!target_dir || target_dir[0] == '\0' || !env_dir || env_dir[0] == '\0' ||
      !out_result) {
    SetError(out_error, out_error_size,
             "target and environment directories are required");
    return false;
  }

  memset(out_result, 0, sizeof(*out_result));

  if (!IsExistingDirectory(target_dir)) {
    SetError(out_error, out_error_size,
             "target directory '%s' does not exist or is not a directory",
             target_dir);
    return false;
  }

  if (!FsGetAbsolutePath(target_dir, out_result->target_abs,
                         sizeof(out_result->target_abs))) {
    SetError(out_error, out_error_size,
             "failed to resolve absolute target path '%s'", target_dir);
    return false;
  }

  if (!EnsureRenameCacheLayout(env_dir, out_result->env_abs,
                               sizeof(out_result->env_abs), out_error,
                               out_error_size)) {
    return false;
  }

  MediaCountCtx count_ctx = {0};
  if (!FsWalkDirectory(out_result->target_abs, CountMediaPathCallback,
                       &count_ctx)) {
    SetError(out_error, out_error_size,
             "failed to scan media files under '%s'", out_result->target_abs);
    return false;
  }
  out_result->media_files_in_scope = count_ctx.count;
  return true;
}

static void TrimTrailingSlash(char *path) {
  if (!path) {
    return;
  }

  size_t len = strlen(path);
  while (len > 1 && path[len - 1] == '/') {
    path[len - 1] = '\0';
    len--;
  }
}

static bool EndsWith(const char *text, const char *suffix) {
  if (!text || !suffix) {
    return false;
  }

  size_t text_len = strlen(text);
  size_t suffix_len = strlen(suffix);
  if (suffix_len > text_len) {
    return false;
  }
  return strcmp(text + (text_len - suffix_len), suffix) == 0;
}

static int EditDistanceCasefold(const char *left, const char *right) {
  if (!left || !right) {
    return 9999;
  }

  size_t n = strlen(left);
  size_t m = strlen(right);
  if (n > CLI_RENAME_MAX_EDIT_LEN || m > CLI_RENAME_MAX_EDIT_LEN) {
    return 9999;
  }

  int prev[CLI_RENAME_MAX_EDIT_LEN + 1] = {0};
  int curr[CLI_RENAME_MAX_EDIT_LEN + 1] = {0};
  for (size_t j = 0; j <= m; j++) {
    prev[j] = (int)j;
  }

  for (size_t i = 1; i <= n; i++) {
    curr[0] = (int)i;
    char lc = (char)tolower((unsigned char)left[i - 1]);
    for (size_t j = 1; j <= m; j++) {
      char rc = (char)tolower((unsigned char)right[j - 1]);
      int cost = lc == rc ? 0 : 1;
      int del = prev[j] + 1;
      int ins = curr[j - 1] + 1;
      int sub = prev[j - 1] + cost;
      int best = del < ins ? del : ins;
      curr[j] = sub < best ? sub : best;
    }

    for (size_t j = 0; j <= m; j++) {
      prev[j] = curr[j];
    }
  }

  return prev[m];
}

bool CliRenameSuggestPath(const char *missing_path, char *out_suggestion,
                          size_t out_suggestion_size) {
  if (!missing_path || missing_path[0] == '\0' || !out_suggestion ||
      out_suggestion_size == 0) {
    return false;
  }

  out_suggestion[0] = '\0';

  char candidate_path[MAX_PATH_LENGTH] = {0};
  strncpy(candidate_path, missing_path, sizeof(candidate_path) - 1);
  candidate_path[sizeof(candidate_path) - 1] = '\0';
  TrimTrailingSlash(candidate_path);
  if (candidate_path[0] == '\0') {
    return false;
  }

  char parent[MAX_PATH_LENGTH] = {0};
  char leaf[NAME_MAX + 1] = {0};

  char *slash = strrchr(candidate_path, '/');
  if (!slash) {
    strncpy(parent, ".", sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    strncpy(leaf, candidate_path, sizeof(leaf) - 1);
    leaf[sizeof(leaf) - 1] = '\0';
  } else {
    strncpy(leaf, slash + 1, sizeof(leaf) - 1);
    leaf[sizeof(leaf) - 1] = '\0';
    if (slash == candidate_path) {
      strncpy(parent, "/", sizeof(parent) - 1);
      parent[sizeof(parent) - 1] = '\0';
    } else {
      *slash = '\0';
      strncpy(parent, candidate_path, sizeof(parent) - 1);
      parent[sizeof(parent) - 1] = '\0';
    }
  }

  if (leaf[0] == '\0' || !IsExistingDirectory(parent)) {
    return false;
  }

  DIR *dir = opendir(parent);
  if (!dir) {
    return false;
  }

  int best_score = 9999;
  char best_name[NAME_MAX + 1] = {0};

  struct dirent *entry = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    char full[MAX_PATH_LENGTH] = {0};
    if (strcmp(parent, "/") == 0) {
      snprintf(full, sizeof(full), "/%s", entry->d_name);
    } else {
      snprintf(full, sizeof(full), "%s/%s", parent, entry->d_name);
    }

    if (!IsExistingDirectory(full)) {
      continue;
    }

    int score = EditDistanceCasefold(leaf, entry->d_name);
    if (score < best_score) {
      best_score = score;
      strncpy(best_name, entry->d_name, sizeof(best_name) - 1);
      best_name[sizeof(best_name) - 1] = '\0';
      if (best_score == 0) {
        break;
      }
    }
  }
  closedir(dir);

  if (best_name[0] == '\0') {
    return false;
  }

  int max_allowed = (int)(strlen(leaf) / 3) + 1;
  if (max_allowed < 2) {
    max_allowed = 2;
  } else if (max_allowed > 6) {
    max_allowed = 6;
  }
  if (best_score > max_allowed) {
    return false;
  }

  if (strcmp(parent, ".") == 0) {
    snprintf(out_suggestion, out_suggestion_size, "%s", best_name);
  } else if (strcmp(parent, "/") == 0) {
    snprintf(out_suggestion, out_suggestion_size, "/%s", best_name);
  } else {
    snprintf(out_suggestion, out_suggestion_size, "%s/%s", parent, best_name);
  }
  return true;
}

static void ExtractFilenameStem(const char *path, char *out_stem,
                                size_t out_stem_size) {
  if (!out_stem || out_stem_size == 0) {
    return;
  }
  out_stem[0] = '\0';
  if (!path || path[0] == '\0') {
    return;
  }

  const char *base = strrchr(path, '/');
  base = base ? base + 1 : path;

  strncpy(out_stem, base, out_stem_size - 1);
  out_stem[out_stem_size - 1] = '\0';

  char *dot = strrchr(out_stem, '.');
  if (dot && dot != out_stem) {
    *dot = '\0';
  }
}

static bool TagAlreadyCaptured(const char tags[][4], int count,
                               const char *candidate) {
  if (!candidate) {
    return false;
  }
  for (int i = 0; i < count; i++) {
    if (strcmp(tags[i], candidate) == 0) {
      return true;
    }
  }
  return false;
}

static int ExtractThreeDigitTags(const char *stem, char tags[][4],
                                 int tags_capacity) {
  if (!stem || !tags || tags_capacity <= 0) {
    return 0;
  }

  int count = 0;
  size_t i = 0;
  while (stem[i] != '\0') {
    if (!isdigit((unsigned char)stem[i])) {
      i++;
      continue;
    }

    size_t start = i;
    while (isdigit((unsigned char)stem[i])) {
      i++;
    }
    size_t len = i - start;
    if (len == 3 && count < tags_capacity) {
      char candidate[4] = {0};
      memcpy(candidate, stem + start, 3);
      candidate[3] = '\0';
      if (!TagAlreadyCaptured(tags, count, candidate)) {
        strncpy(tags[count], candidate, 4);
        count++;
      }
    }
  }

  return count;
}

bool CliRenameBootstrapTagsFromFilename(const char *target_dir,
                                        const char *env_dir,
                                        CliRenameBootstrapResult *out_result,
                                        char *out_error,
                                        size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!target_dir || target_dir[0] == '\0' || !env_dir || env_dir[0] == '\0' ||
      !out_result) {
    SetError(out_error, out_error_size,
             "target and environment directories are required");
    return false;
  }

  memset(out_result, 0, sizeof(*out_result));

  CliRenameInitResult init = {0};
  if (!CliRenameInitEnvironment(target_dir, env_dir, &init, out_error,
                                out_error_size)) {
    return false;
  }

  PathList paths = {0};
  if (!CollectMediaPaths(init.target_abs, &paths, out_error, out_error_size)) {
    return false;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON *files = cJSON_CreateObject();
  if (!root || !files) {
    cJSON_Delete(root);
    cJSON_Delete(files);
    PathListFree(&paths);
    SetError(out_error, out_error_size,
             "out of memory while building bootstrap tags map");
    return false;
  }
  cJSON_AddItemToObject(root, "files", files);

  int tagged_files = 0;
  for (int i = 0; i < paths.count; i++) {
    char stem[512] = {0};
    ExtractFilenameStem(paths.items[i], stem, sizeof(stem));

    char tags[CLI_RENAME_MAX_FILENAME_TAGS][4] = {{0}};
    int tag_count =
        ExtractThreeDigitTags(stem, tags, CLI_RENAME_MAX_FILENAME_TAGS);
    if (tag_count <= 0) {
      continue;
    }

    cJSON *entry = cJSON_CreateObject();
    cJSON *manual = cJSON_CreateArray();
    if (!entry || !manual) {
      cJSON_Delete(entry);
      cJSON_Delete(manual);
      cJSON_Delete(root);
      PathListFree(&paths);
      SetError(out_error, out_error_size,
               "out of memory while building tag entries");
      return false;
    }

    for (int j = 0; j < tag_count; j++) {
      cJSON_AddItemToArray(manual, cJSON_CreateString(tags[j]));
    }
    cJSON_AddItemToObject(entry, "manualTags", manual);
    cJSON_AddItemToObject(files, paths.items[i], entry);
    tagged_files++;
  }

  char map_path[MAX_PATH_LENGTH] = {0};
  snprintf(map_path, sizeof(map_path), "%s/.cache/rename_tags_bootstrap.json",
           init.env_abs);

  char *json_text = cJSON_Print(root);
  if (!json_text) {
    cJSON_Delete(root);
    PathListFree(&paths);
    SetError(out_error, out_error_size,
             "failed to serialize bootstrap tags map");
    return false;
  }

  bool save_ok = SaveTextFile(map_path, json_text);
  free(json_text);
  cJSON_Delete(root);
  PathListFree(&paths);

  if (!save_ok) {
    SetError(out_error, out_error_size,
             "failed to write bootstrap tags map '%s'", map_path);
    return false;
  }

  strncpy(out_result->map_path, map_path, sizeof(out_result->map_path) - 1);
  out_result->map_path[sizeof(out_result->map_path) - 1] = '\0';
  out_result->files_scanned = init.media_files_in_scope;
  out_result->files_with_tags = tagged_files;
  return true;
}

bool CliRenameResolveLatestPreviewId(const char *env_dir, char *out_preview_id,
                                     size_t out_preview_id_size,
                                     char *out_error,
                                     size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_preview_id ||
      out_preview_id_size == 0) {
    SetError(out_error, out_error_size,
             "environment directory and output buffer are required");
    return false;
  }
  out_preview_id[0] = '\0';

  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    SetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  char preview_dir[MAX_PATH_LENGTH] = {0};
  snprintf(preview_dir, sizeof(preview_dir), "%s/.cache/rename_previews",
           env_abs);
  DIR *dir = opendir(preview_dir);
  if (!dir) {
    SetError(out_error, out_error_size,
             "rename preview directory not found: '%s'", preview_dir);
    return false;
  }

  char latest_name[NAME_MAX + 1] = {0};
  struct dirent *entry = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (!EndsWith(entry->d_name, ".json")) {
      continue;
    }
    if (strncmp(entry->d_name, "rnp-", 4) != 0) {
      continue;
    }
    if (latest_name[0] == '\0' || strcmp(entry->d_name, latest_name) > 0) {
      strncpy(latest_name, entry->d_name, sizeof(latest_name) - 1);
      latest_name[sizeof(latest_name) - 1] = '\0';
    }
  }
  closedir(dir);

  if (latest_name[0] == '\0') {
    SetError(out_error, out_error_size,
             "no rename preview artifacts found under '%s'", preview_dir);
    return false;
  }

  size_t len = strlen(latest_name);
  if (len <= 5) {
    SetError(out_error, out_error_size, "invalid rename preview artifact name");
    return false;
  }
  latest_name[len - 5] = '\0';
  strncpy(out_preview_id, latest_name, out_preview_id_size - 1);
  out_preview_id[out_preview_id_size - 1] = '\0';
  return true;
}

static bool LoadRenameHistoryEntries(const char *env_dir, cJSON **out_entries,
                                     char *out_error, size_t out_error_size) {
  if (!env_dir || env_dir[0] == '\0' || !out_entries) {
    SetError(out_error, out_error_size,
             "environment directory and output payload are required");
    return false;
  }

  *out_entries = NULL;
  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    SetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  char index_path[MAX_PATH_LENGTH] = {0};
  snprintf(index_path, sizeof(index_path), "%s/.cache/rename_history/index.json",
           env_abs);

  char *index_text = NULL;
  if (!LoadTextFile(index_path, &index_text)) {
    SetError(out_error, out_error_size,
             "rename history index not found: '%s'", index_path);
    return false;
  }

  cJSON *index_json = cJSON_Parse(index_text);
  free(index_text);
  if (!index_json) {
    SetError(out_error, out_error_size,
             "rename history index is malformed: '%s'", index_path);
    return false;
  }

  cJSON *entries = cJSON_DetachItemFromObject(index_json, "entries");
  cJSON_Delete(index_json);
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(entries);
    SetError(out_error, out_error_size,
             "rename history index entries payload is invalid");
    return false;
  }

  *out_entries = entries;
  return true;
}

bool CliRenameResolveLatestOperationId(const char *env_dir, char *out_operation_id,
                                       size_t out_operation_id_size,
                                       char *out_error,
                                       size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_operation_id ||
      out_operation_id_size == 0) {
    SetError(out_error, out_error_size,
             "environment directory and output buffer are required");
    return false;
  }

  out_operation_id[0] = '\0';
  cJSON *entries = NULL;
  if (!LoadRenameHistoryEntries(env_dir, &entries, out_error, out_error_size)) {
    return false;
  }

  bool found = false;
  int count = cJSON_GetArraySize(entries);
  for (int i = count - 1; i >= 0; i--) {
    cJSON *entry = cJSON_GetArrayItem(entries, i);
    cJSON *operation_id = cJSON_GetObjectItem(entry, "operationId");
    if (!cJSON_IsString(operation_id) || !operation_id->valuestring ||
        operation_id->valuestring[0] == '\0') {
      continue;
    }
    strncpy(out_operation_id, operation_id->valuestring,
            out_operation_id_size - 1);
    out_operation_id[out_operation_id_size - 1] = '\0';
    found = true;
    break;
  }

  cJSON_Delete(entries);
  if (!found) {
    SetError(out_error, out_error_size,
             "rename history contains no operation ids");
    return false;
  }
  return true;
}

bool CliRenameResolvePreviewIdFromOperation(const char *env_dir,
                                            const char *operation_id,
                                            char *out_preview_id,
                                            size_t out_preview_id_size,
                                            char *out_error,
                                            size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !operation_id ||
      operation_id[0] == '\0' || !out_preview_id ||
      out_preview_id_size == 0) {
    SetError(out_error, out_error_size,
             "env directory, operation id, and output buffer are required");
    return false;
  }
  out_preview_id[0] = '\0';

  cJSON *entries = NULL;
  if (!LoadRenameHistoryEntries(env_dir, &entries, out_error, out_error_size)) {
    return false;
  }

  bool found = false;
  int count = cJSON_GetArraySize(entries);
  for (int i = count - 1; i >= 0; i--) {
    cJSON *entry = cJSON_GetArrayItem(entries, i);
    cJSON *entry_operation_id = cJSON_GetObjectItem(entry, "operationId");
    if (!cJSON_IsString(entry_operation_id) ||
        !entry_operation_id->valuestring ||
        strcmp(entry_operation_id->valuestring, operation_id) != 0) {
      continue;
    }

    cJSON *entry_preview_id = cJSON_GetObjectItem(entry, "previewId");
    if (!cJSON_IsString(entry_preview_id) || !entry_preview_id->valuestring ||
        entry_preview_id->valuestring[0] == '\0') {
      cJSON_Delete(entries);
      SetError(out_error, out_error_size,
               "operation '%s' does not reference a preview id", operation_id);
      return false;
    }

    strncpy(out_preview_id, entry_preview_id->valuestring,
            out_preview_id_size - 1);
    out_preview_id[out_preview_id_size - 1] = '\0';
    found = true;
    break;
  }

  cJSON_Delete(entries);
  if (!found) {
    SetError(out_error, out_error_size,
             "rename operation '%s' not found in history", operation_id);
    return false;
  }
  return true;
}

bool CliRenameResolveOperationManifestPath(const char *env_dir,
                                           const char *operation_id,
                                           char *out_manifest_path,
                                           size_t out_manifest_path_size,
                                           char *out_error,
                                           size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !operation_id ||
      operation_id[0] == '\0' || !out_manifest_path ||
      out_manifest_path_size == 0) {
    SetError(out_error, out_error_size,
             "env directory, operation id, and output buffer are required");
    return false;
  }

  out_manifest_path[0] = '\0';
  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    SetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  char manifest_path[MAX_PATH_LENGTH] = {0};
  snprintf(manifest_path, sizeof(manifest_path),
           "%s/.cache/rename_history/%s.json", env_abs, operation_id);
  if (access(manifest_path, F_OK) != 0) {
    SetError(out_error, out_error_size,
             "rename operation manifest not found: '%s'", manifest_path);
    return false;
  }

  strncpy(out_manifest_path, manifest_path, out_manifest_path_size - 1);
  out_manifest_path[out_manifest_path_size - 1] = '\0';
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

static bool IsDigits(const char *text, size_t len) {
  if (!text) {
    return false;
  }
  for (size_t i = 0; i < len; i++) {
    if (!isdigit((unsigned char)text[i])) {
      return false;
    }
  }
  return true;
}

bool CliRenameParseRollbackFilter(const char *raw_value,
                                  CliRenameRollbackFilter *out_filter,
                                  char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!out_filter) {
    SetError(out_error, out_error_size, "output rollback filter is required");
    return false;
  }

  if (!raw_value || raw_value[0] == '\0' || strcmp(raw_value, "any") == 0) {
    *out_filter = CLI_RENAME_ROLLBACK_FILTER_ANY;
    return true;
  }
  if (strcmp(raw_value, "yes") == 0 || strcmp(raw_value, "true") == 0) {
    *out_filter = CLI_RENAME_ROLLBACK_FILTER_YES;
    return true;
  }
  if (strcmp(raw_value, "no") == 0 || strcmp(raw_value, "false") == 0) {
    *out_filter = CLI_RENAME_ROLLBACK_FILTER_NO;
    return true;
  }

  SetError(out_error, out_error_size,
           "invalid rollback filter '%s' (allowed: any|yes|no)", raw_value);
  return false;
}

bool CliRenameNormalizeHistoryDateBound(const char *raw_value, bool is_upper_bound,
                                        char *out_utc, size_t out_utc_size,
                                        char *out_error,
                                        size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!out_utc || out_utc_size == 0) {
    SetError(out_error, out_error_size, "output date buffer is required");
    return false;
  }

  out_utc[0] = '\0';
  if (!raw_value || raw_value[0] == '\0') {
    return true;
  }

  size_t len = strlen(raw_value);
  if (len == 10) {
    if (!IsDigits(raw_value, 4) || raw_value[4] != '-' ||
        !IsDigits(raw_value + 5, 2) || raw_value[7] != '-' ||
        !IsDigits(raw_value + 8, 2)) {
      SetError(out_error, out_error_size,
               "invalid date '%s' (expected YYYY-MM-DD)", raw_value);
      return false;
    }
    snprintf(out_utc, out_utc_size, "%sT%sZ", raw_value,
             is_upper_bound ? "23:59:59" : "00:00:00");
    return true;
  }

  if (len == 20 && IsDigits(raw_value, 4) && raw_value[4] == '-' &&
      IsDigits(raw_value + 5, 2) && raw_value[7] == '-' &&
      IsDigits(raw_value + 8, 2) && raw_value[10] == 'T' &&
      IsDigits(raw_value + 11, 2) && raw_value[13] == ':' &&
      IsDigits(raw_value + 14, 2) && raw_value[16] == ':' &&
      IsDigits(raw_value + 17, 2) && raw_value[19] == 'Z') {
    strncpy(out_utc, raw_value, out_utc_size - 1);
    out_utc[out_utc_size - 1] = '\0';
    return true;
  }

  SetError(out_error, out_error_size,
           "invalid date '%s' (expected YYYY-MM-DD or YYYY-MM-DDTHH:MM:SSZ)",
           raw_value);
  return false;
}

static bool HistoryEntryMatchesFilter(const AppRenameHistoryEntry *entry,
                                      const CliRenameHistoryFilter *filter) {
  if (!entry || !filter) {
    return false;
  }

  if (filter->operation_id_prefix[0] != '\0') {
    size_t prefix_len = strlen(filter->operation_id_prefix);
    if (strncmp(entry->operation_id, filter->operation_id_prefix, prefix_len) !=
        0) {
      return false;
    }
  }

  if (filter->rollback_filter == CLI_RENAME_ROLLBACK_FILTER_YES &&
      !entry->rollback_performed) {
    return false;
  }
  if (filter->rollback_filter == CLI_RENAME_ROLLBACK_FILTER_NO &&
      entry->rollback_performed) {
    return false;
  }

  if (filter->created_from_utc[0] != '\0') {
    if (entry->created_at_utc[0] == '\0' ||
        strcmp(entry->created_at_utc, filter->created_from_utc) < 0) {
      return false;
    }
  }
  if (filter->created_to_utc[0] != '\0') {
    if (entry->created_at_utc[0] == '\0' ||
        strcmp(entry->created_at_utc, filter->created_to_utc) > 0) {
      return false;
    }
  }

  return true;
}

bool CliRenameBuildHistoryFilterIndex(const AppRenameHistoryEntry *entries,
                                      int entry_count,
                                      const CliRenameHistoryFilter *filter,
                                      int **out_indices,
                                      int *out_filtered_count,
                                      char *out_error,
                                      size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!entries || entry_count < 0 || !filter || !out_indices ||
      !out_filtered_count) {
    SetError(out_error, out_error_size,
             "entries, filter, and output pointers are required");
    return false;
  }

  *out_indices = NULL;
  *out_filtered_count = 0;
  if (entry_count == 0) {
    return true;
  }

  int *indices = calloc((size_t)entry_count, sizeof(int));
  if (!indices) {
    SetError(out_error, out_error_size,
             "out of memory while building filtered history index");
    return false;
  }

  int written = 0;
  for (int i = 0; i < entry_count; i++) {
    if (HistoryEntryMatchesFilter(&entries[i], filter)) {
      indices[written++] = i;
    }
  }

  if (written == 0) {
    free(indices);
    indices = NULL;
  }

  *out_indices = indices;
  *out_filtered_count = written;
  return true;
}

void CliRenameFreeHistoryFilterIndex(int *indices) { free(indices); }

static const char *RollbackFilterToString(CliRenameRollbackFilter filter) {
  switch (filter) {
  case CLI_RENAME_ROLLBACK_FILTER_YES:
    return "yes";
  case CLI_RENAME_ROLLBACK_FILTER_NO:
    return "no";
  case CLI_RENAME_ROLLBACK_FILTER_ANY:
  default:
    return "any";
  }
}

bool CliRenameExportHistoryJson(const char *env_dir,
                                const AppRenameHistoryEntry *entries,
                                const int *indices, int index_count,
                                const CliRenameHistoryFilter *filter,
                                const char *output_path, char *out_error,
                                size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !entries || !filter || !output_path ||
      output_path[0] == '\0' || index_count < 0 ||
      (index_count > 0 && !indices)) {
    SetError(out_error, out_error_size,
             "env_dir, entries, filter, output_path, and valid index set are "
             "required");
    return false;
  }

  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    SetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON *items = cJSON_CreateArray();
  cJSON *filters = cJSON_CreateObject();
  if (!root || !items || !filters) {
    cJSON_Delete(root);
    cJSON_Delete(items);
    cJSON_Delete(filters);
    SetError(out_error, out_error_size,
             "out of memory while building history export payload");
    return false;
  }

  cJSON_AddNumberToObject(root, "version", 1);
  char created_at[32] = {0};
  NowUtc(created_at, sizeof(created_at));
  cJSON_AddStringToObject(root, "createdAtUtc", created_at);
  cJSON_AddStringToObject(root, "envDir", env_abs);
  cJSON_AddNumberToObject(root, "entryCount", index_count);

  cJSON_AddStringToObject(filters, "operationIdPrefix",
                          filter->operation_id_prefix);
  cJSON_AddStringToObject(filters, "rollback",
                          RollbackFilterToString(filter->rollback_filter));
  cJSON_AddStringToObject(filters, "createdFromUtc", filter->created_from_utc);
  cJSON_AddStringToObject(filters, "createdToUtc", filter->created_to_utc);
  cJSON_AddItemToObject(root, "filters", filters);

  for (int i = 0; i < index_count; i++) {
    int idx = indices[i];
    const AppRenameHistoryEntry *entry = &entries[idx];

    cJSON *item = cJSON_CreateObject();
    if (!item) {
      continue;
    }

    cJSON_AddStringToObject(item, "operationId", entry->operation_id);
    cJSON_AddStringToObject(item, "previewId", entry->preview_id);
    cJSON_AddStringToObject(item, "createdAtUtc", entry->created_at_utc);
    cJSON_AddNumberToObject(item, "renamedCount", entry->renamed_count);
    cJSON_AddNumberToObject(item, "skippedCount", entry->skipped_count);
    cJSON_AddNumberToObject(item, "failedCount", entry->failed_count);
    cJSON_AddNumberToObject(item, "collisionResolvedCount",
                            entry->collision_resolved_count);
    cJSON_AddNumberToObject(item, "truncationCount", entry->truncation_count);
    cJSON_AddBoolToObject(item, "rollbackPerformed", entry->rollback_performed);
    cJSON_AddNumberToObject(item, "rollbackRestoredCount",
                            entry->rollback_restored_count);
    cJSON_AddNumberToObject(item, "rollbackSkippedCount",
                            entry->rollback_skipped_count);
    cJSON_AddNumberToObject(item, "rollbackFailedCount",
                            entry->rollback_failed_count);

    char manifest_path[MAX_PATH_LENGTH] = {0};
    bool manifest_available =
        CliRenameResolveOperationManifestPath(env_abs, entry->operation_id,
                                             manifest_path,
                                             sizeof(manifest_path), NULL, 0);
    cJSON_AddBoolToObject(item, "manifestAvailable", manifest_available);
    if (manifest_available) {
      cJSON_AddStringToObject(item, "manifestPath", manifest_path);
    }

    int manifest_item_count = 0;
    bool manifest_rollback_present = false;
    if (manifest_available) {
      char *manifest_text = NULL;
      if (LoadTextFile(manifest_path, &manifest_text)) {
        cJSON *manifest = cJSON_Parse(manifest_text);
        free(manifest_text);
        if (manifest) {
          cJSON *manifest_items = cJSON_GetObjectItem(manifest, "items");
          if (cJSON_IsArray(manifest_items)) {
            manifest_item_count = cJSON_GetArraySize(manifest_items);
          }
          cJSON *manifest_rollback = cJSON_GetObjectItem(manifest, "rollback");
          manifest_rollback_present = cJSON_IsObject(manifest_rollback);
          cJSON_Delete(manifest);
        }
      }
    }
    cJSON_AddNumberToObject(item, "manifestItemCount", manifest_item_count);
    cJSON_AddBoolToObject(item, "manifestRollbackPresent",
                          manifest_rollback_present);

    cJSON_AddItemToArray(items, item);
  }

  cJSON_AddItemToObject(root, "entries", items);

  char *payload = cJSON_Print(root);
  cJSON_Delete(root);
  if (!payload) {
    SetError(out_error, out_error_size,
             "failed to serialize history export payload");
    return false;
  }

  bool saved = SaveTextFile(output_path, payload);
  cJSON_free(payload);
  if (!saved) {
    SetError(out_error, out_error_size,
             "failed to write history export '%s'", output_path);
    return false;
  }
  return true;
}
