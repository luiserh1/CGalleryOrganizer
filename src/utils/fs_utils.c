#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "fs_utils.h"

static const char *SUPPORTED_EXTS[] = {
    ".jpg", ".jpeg", ".png", ".gif", ".webp", ".heic", ".bmp", // Images
    ".mp4", ".mov",  ".mkv", ".avi", ".webm",                  // Videos
    NULL};

bool FsIsSupportedMedia(const char *path) {
  if (!path)
    return false;

  // Find the last dot in the path
  const char *ext = strrchr(path, '.');
  if (!ext || ext == path)
    return false; // No extension or hidden file without extension

  // Convert extension to lowercase for comparison
  char lower_ext[16] = {0};
  size_t ext_len = strlen(ext);
  if (ext_len >= sizeof(lower_ext))
    return false; // Extension too long

  for (size_t i = 0; i < ext_len; i++) {
    lower_ext[i] = tolower((unsigned char)ext[i]);
  }

  // Check against supported list
  for (int i = 0; SUPPORTED_EXTS[i] != NULL; i++) {
    if (strcmp(lower_ext, SUPPORTED_EXTS[i]) == 0) {
      return true;
    }
  }

  return false;
}

bool FsGetAbsolutePath(const char *path, char *out_path, size_t out_size) {
  if (!path || !out_path || out_size == 0)
    return false;

  char *real_p = realpath(path, NULL);
  if (!real_p)
    return false;

  size_t len = strlen(real_p);
  if (len >= out_size) {
    free(real_p);
    return false;
  }

  strcpy(out_path, real_p);
  free(real_p);
  return true;
}

bool FsWalkDirectory(const char *root_path, FsWalkCallback callback,
                     void *user_data) {
  if (!root_path || !callback)
    return false;

  DIR *dir = opendir(root_path);
  if (!dir)
    return false;

  struct dirent *entry;
  bool continue_walk = true;

  while (continue_walk && (entry = readdir(dir)) != NULL) {
    // Skip . and ..
    if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
      continue;
    }

    // Build full path
    char full_path[MAX_PATH_LENGTH];
    int written = snprintf(full_path, sizeof(full_path), "%s/%s", root_path,
                           entry->d_name);
    if (written < 0 || (size_t)written >= sizeof(full_path)) {
      continue; // Path too long, skip this entry
    }

    // Check if directory or file
    struct stat st;
    if (stat(full_path, &st) == 0) {
      if (S_ISDIR(st.st_mode)) {
        // Recursive call
        continue_walk = FsWalkDirectory(full_path, callback, user_data);
      } else if (S_ISREG(st.st_mode)) {
        // Get absolute path to pass to callback
        char abs_path[MAX_PATH_LENGTH];
        if (FsGetAbsolutePath(full_path, abs_path, sizeof(abs_path))) {
          continue_walk = callback(abs_path, user_data);
        }
      }
    }
  }

  closedir(dir);
  return continue_walk;
}
