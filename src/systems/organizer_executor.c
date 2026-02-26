#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "organizer.h"
#include "systems/organizer_internal.h"

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

    snprintf(probe_path, sizeof(probe_path), "%s%s", exact_new_path, ext);

    while (stat(probe_path, &st) == 0) {
      snprintf(probe_path, sizeof(probe_path), "%s_%d%s", exact_new_path,
               collision_idx, ext);
      collision_idx++;
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
