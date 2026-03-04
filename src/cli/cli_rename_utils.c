#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "cJSON.h"
#include "cli/cli_rename_common.h"
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
    CliRenameCommonSetError(out_error, out_error_size,
             "target directory is required for media collection");
    return false;
  }

  memset(out_paths, 0, sizeof(*out_paths));
  if (!FsWalkDirectory(root_dir, CollectMediaPathCallback, out_paths)) {
    PathListFree(out_paths);
    CliRenameCommonSetError(out_error, out_error_size,
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
    CliRenameCommonSetError(out_error, out_error_size, "environment directory is required");
    return false;
  }

  if (!FsMakeDirRecursive(env_dir) || !CliRenameCommonIsExistingDirectory(env_dir)) {
    CliRenameCommonSetError(out_error, out_error_size,
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
      !FsMakeDirRecursive(history_dir) || !CliRenameCommonIsExistingDirectory(preview_dir) ||
      !CliRenameCommonIsExistingDirectory(history_dir)) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to create rename cache layout under '%s'", env_dir);
    return false;
  }

  if (out_env_abs && out_env_abs_size > 0) {
    if (!FsGetAbsolutePath(env_dir, out_env_abs, out_env_abs_size)) {
      CliRenameCommonSetError(out_error, out_error_size,
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
    CliRenameCommonSetError(out_error, out_error_size,
             "target and environment directories are required");
    return false;
  }

  memset(out_result, 0, sizeof(*out_result));

  if (!CliRenameCommonIsExistingDirectory(target_dir)) {
    CliRenameCommonSetError(out_error, out_error_size,
             "target directory '%s' does not exist or is not a directory",
             target_dir);
    return false;
  }

  if (!FsGetAbsolutePath(target_dir, out_result->target_abs,
                         sizeof(out_result->target_abs))) {
    CliRenameCommonSetError(out_error, out_error_size,
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
    CliRenameCommonSetError(out_error, out_error_size,
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

  if (leaf[0] == '\0' || !CliRenameCommonIsExistingDirectory(parent)) {
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

    if (!CliRenameCommonIsExistingDirectory(full)) {
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
    CliRenameCommonSetError(out_error, out_error_size,
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
    CliRenameCommonSetError(out_error, out_error_size,
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
      CliRenameCommonSetError(out_error, out_error_size,
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
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to serialize bootstrap tags map");
    return false;
  }

  bool save_ok = CliRenameCommonSaveTextFile(map_path, json_text);
  free(json_text);
  cJSON_Delete(root);
  PathListFree(&paths);

  if (!save_ok) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to write bootstrap tags map '%s'", map_path);
    return false;
  }

  strncpy(out_result->map_path, map_path, sizeof(out_result->map_path) - 1);
  out_result->map_path[sizeof(out_result->map_path) - 1] = '\0';
  out_result->files_scanned = init.media_files_in_scope;
  out_result->files_with_tags = tagged_files;
  return true;
}
