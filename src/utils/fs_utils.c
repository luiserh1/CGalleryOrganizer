#include <errno.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#if defined(_WIN32)
#include <direct.h>
#endif

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

#if defined(_WIN32)
  char resolved[MAX_PATH_LENGTH];
  if (!_fullpath(resolved, path, sizeof(resolved))) {
    return false;
  }
  size_t len = strlen(resolved);
  if (len >= out_size) {
    return false;
  }
  strcpy(out_path, resolved);
  return true;
#else
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
#endif
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

bool FsDeleteFile(const char *path) {
  if (!path)
    return false;
  return remove(path) == 0;
}

static bool FsCreateDirSingle(const char *path) {
  if (!path || path[0] == '\0') {
    return false;
  }

#if defined(_WIN32)
  if (_mkdir(path) == 0) {
    return true;
  }
#else
  if (mkdir(path, 0755) == 0) {
    return true;
  }
#endif

  if (errno != EEXIST) {
    return false;
  }

  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool FsMakeDirRecursive(const char *path) {
  if (!path || path[0] == '\0')
    return false;

  char tmp[MAX_PATH_LENGTH];
  size_t len = strlen(path);
  if (len >= sizeof(tmp)) {
    return false;
  }
  strcpy(tmp, path);

  for (size_t i = 0; i < len; i++) {
    if (tmp[i] == '\\') {
      tmp[i] = '/';
    }
  }

  while (len > 1 && tmp[len - 1] == '/') {
    tmp[len - 1] = '\0';
    len--;
  }

  if (len == 0) {
    return false;
  }

  size_t start = 0;
  if (tmp[0] == '/') {
    start = 1;
  }
#if defined(_WIN32)
  if (len >= 2 && isalpha((unsigned char)tmp[0]) && tmp[1] == ':') {
    start = 2;
    if (len >= 3 && tmp[2] == '/') {
      start = 3;
    }
  }
#endif

  for (char *p = tmp + start; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      if (!FsCreateDirSingle(tmp)) {
        return false;
      }
      *p = '/';
    }
  }

  return FsCreateDirSingle(tmp);
}

bool FsRenameFile(const char *old_path, const char *new_path) {
  if (!old_path || !new_path)
    return false;
  return rename(old_path, new_path) == 0;
}

static void FsGetFilenameAndExt(const char *path, char *filename,
                                size_t filename_size, char *ext,
                                size_t ext_size) {
  const char *last_slash = strrchr(path, '/');
  const char *base = last_slash ? last_slash + 1 : path;

  const char *last_dot = strrchr(base, '.');
  if (!last_dot || last_dot == base) {
    strncpy(filename, base, filename_size - 1);
    filename[filename_size - 1] = '\0';
    ext[0] = '\0';
  } else {
    size_t name_len = last_dot - base;
    if (name_len > filename_size - 1)
      name_len = filename_size - 1;
    strncpy(filename, base, name_len);
    filename[name_len] = '\0';

    strncpy(ext, last_dot, ext_size - 1);
    ext[ext_size - 1] = '\0';
  }
}

bool FsMoveFile(const char *source_path, const char *target_dir,
                char *out_new_path, size_t out_size) {
  if (!source_path || !target_dir)
    return false;

  char filename[256] = {0};
  char ext[32] = {0};
  FsGetFilenameAndExt(source_path, filename, sizeof(filename), ext,
                      sizeof(ext));

  char current_target[MAX_PATH_LENGTH];
  int counter = 0;

  // mkdir if it doesn't exist
  struct stat st;
  if (stat(target_dir, &st) != 0) {
    // Create missing target directory recursively
    FsMakeDirRecursive(target_dir);
  }

  while (1) {
    if (counter == 0) {
      snprintf(current_target, sizeof(current_target), "%s/%s%s", target_dir,
               filename, ext);
    } else {
      snprintf(current_target, sizeof(current_target), "%s/%s_%d%s", target_dir,
               filename, counter, ext);
    }

    if (access(current_target, F_OK) != 0) {
      // File does not exist here, safe to move
      break;
    }
    counter++;
  }

  if (FsRenameFile(source_path, current_target)) {
    if (out_new_path && out_size > 0) {
      strncpy(out_new_path, current_target, out_size - 1);
      out_new_path[out_size - 1] = '\0';
    }
    return true;
  }
  return false;
}
