#include <stdio.h>
#include <string.h>

#include "cli/cli_rename_common.h"
#include "cli/cli_rename_utils.h"

typedef struct {
  int count;
} MediaCountCtx;

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
