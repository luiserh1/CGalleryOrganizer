#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "organizer.h"
#include "systems/organizer_internal.h"

static bool OrganizerBuildProbePath(char *out_path, size_t out_size,
                                    const char *base_path, const char *ext,
                                    int collision_idx) {
  if (!out_path || out_size == 0 || !base_path || !ext) {
    return false;
  }

  size_t base_len = strlen(base_path);
  size_t ext_len = strlen(ext);
  size_t suffix_len = 0;
  char suffix[32] = {0};

  if (collision_idx > 0) {
    int written = snprintf(suffix, sizeof(suffix), "_%d", collision_idx);
    if (written < 0 || (size_t)written >= sizeof(suffix)) {
      return false;
    }
    suffix_len = (size_t)written;
  }

  size_t total_len = base_len + suffix_len + ext_len;
  if (total_len + 1 > out_size) {
    return false;
  }

  memcpy(out_path, base_path, base_len);
  if (suffix_len > 0) {
    memcpy(out_path + base_len, suffix, suffix_len);
  }
  memcpy(out_path + base_len + suffix_len, ext, ext_len);
  out_path[total_len] = '\0';
  return true;
}

bool OrganizerExecutePlan(OrganizerPlan *plan) {
  if (!plan)
    return false;

  int success_count = 0;
  printf("\n[*] Executing Plan...\n");
  for (int i = 0; i < plan->count; i++) {
    char exact_new_path[MAX_PATH_LENGTH];
    OrganizerCopyStringBounded(exact_new_path, sizeof(exact_new_path),
                               plan->moves[i].new_path);

    char *dot = strrchr(exact_new_path, '.');
    char ext[32] = {0};
    if (dot) {
      strncpy(ext, dot, sizeof(ext) - 1);
      *dot = '\0';
    }

    struct stat st;
    int collision_idx = 1;
    char probe_path[MAX_PATH_LENGTH];
    if (!OrganizerBuildProbePath(probe_path, sizeof(probe_path), exact_new_path,
                                 ext, 0)) {
      printf("  [!] Target path too long, skipping: %s\n",
             plan->moves[i].new_path);
      continue;
    }

    while (stat(probe_path, &st) == 0) {
      if (!OrganizerBuildProbePath(probe_path, sizeof(probe_path),
                                   exact_new_path, ext, collision_idx)) {
        printf("  [!] Target path too long after collision suffix, skipping: %s\n",
               plan->moves[i].new_path);
        break;
      }
      collision_idx++;
    }

    if (stat(probe_path, &st) == 0) {
      continue;
    }

    char target_dir[MAX_PATH_LENGTH];
    OrganizerCopyStringBounded(target_dir, sizeof(target_dir), probe_path);
    char *last_slash = strrchr(target_dir, '/');
    if (last_slash)
      *last_slash = '\0';
    FsMakeDirRecursive(target_dir);

    char moved_dir_path[MAX_PATH_LENGTH];
    if (FsMoveFile(plan->moves[i].original_path, target_dir, moved_dir_path,
                   sizeof(moved_dir_path))) {
      if (FsRenameFile(moved_dir_path, probe_path)) {
        OrganizerRecordMove(plan->moves[i].original_path, probe_path);
        success_count++;
      } else {
        printf("  [!] Failed to finalize structured rename: %s -> %s\n",
               moved_dir_path, probe_path);
      }
    } else {
      printf("  [!] Failed to move: %s\n", plan->moves[i].original_path);
    }
  }

  OrganizerSaveManifest();
  printf("[*] Successfully moved %d out of %d files.\n", success_count,
         plan->count);
  return success_count > 0;
}
