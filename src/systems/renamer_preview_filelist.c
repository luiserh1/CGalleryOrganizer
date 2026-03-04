#include <stdlib.h>
#include <string.h>

#include "fs_utils.h"
#include "systems/renamer_preview_internal.h"

void RenamerPreviewPathListFree(RenamerPreviewPathList *list) {
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

static bool RenamerPreviewPathListAppend(RenamerPreviewPathList *list,
                                         const char *path) {
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

static bool RenamerPreviewCollectSupportedMedia(const char *absolute_path,
                                                void *user_data) {
  RenamerPreviewPathList *list = (RenamerPreviewPathList *)user_data;
  if (!list) {
    return false;
  }
  if (!FsIsSupportedMedia(absolute_path)) {
    return true;
  }
  return RenamerPreviewPathListAppend(list, absolute_path);
}

static int RenamerPreviewComparePaths(const void *left, const void *right) {
  const char *const *a = (const char *const *)left;
  const char *const *b = (const char *const *)right;
  return strcmp(*a, *b);
}

bool RenamerPreviewCollectFilesRecursive(const char *root,
                                         RenamerPreviewPathList *out_list,
                                         char *out_error,
                                         size_t out_error_size) {
  if (!root || !out_list) {
    RenamerPreviewSetError(out_error, out_error_size,
                           "invalid arguments for recursive file collection");
    return false;
  }

  if (!FsWalkDirectory(root, RenamerPreviewCollectSupportedMedia, out_list)) {
    RenamerPreviewSetError(out_error, out_error_size,
                           "failed to walk target directory '%s'", root);
    return false;
  }

  if (out_list->count > 1) {
    qsort(out_list->paths, (size_t)out_list->count, sizeof(out_list->paths[0]),
          RenamerPreviewComparePaths);
  }
  return true;
}
