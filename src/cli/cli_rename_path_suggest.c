#include <ctype.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "cli/cli_rename_common.h"
#include "cli/cli_rename_utils.h"

#define CLI_RENAME_MAX_EDIT_LEN 255

#if !defined(NAME_MAX)
#define NAME_MAX 255
#endif

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
