#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "fs_utils.h"
#include "systems/renamer_apply.h"
#include "systems/renamer_history.h"
#include "systems/renamer_tags.h"

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

static void BuildOperationId(const RenamerPreviewArtifact *preview,
                             char *out_operation_id,
                             size_t out_operation_id_size) {
  if (!out_operation_id || out_operation_id_size == 0) {
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

  const char *suffix = (preview && preview->fingerprint[0] != '\0')
                           ? preview->fingerprint
                           : "00000000";
  snprintf(out_operation_id, out_operation_id_size, "rop-%s-%.8s", timestamp,
           suffix);
}

static bool ArrayContainsPath(char **paths, int count, const char *path) {
  if (!paths || count <= 0 || !path) {
    return false;
  }

  for (int i = 0; i < count; i++) {
    if (paths[i] && strcmp(paths[i], path) == 0) {
      return true;
    }
  }
  return false;
}

static bool SourceSetContainsOther(const RenamerPreviewArtifact *preview,
                                   const char *path,
                                   const char *current_source) {
  if (!preview || !path) {
    return false;
  }

  for (int i = 0; i < preview->file_count; i++) {
    const char *source = preview->items[i].source_path;
    if (!source || source[0] == '\0') {
      continue;
    }
    if (current_source && strcmp(source, current_source) == 0) {
      continue;
    }
    if (strcmp(source, path) == 0) {
      return true;
    }
  }

  return false;
}

static bool HasPathConflict(const RenamerPreviewArtifact *preview,
                            const char *candidate, const char *current_source,
                            char **reserved_targets, int reserved_count) {
  if (!preview || !candidate || candidate[0] == '\0') {
    return true;
  }

  if (current_source && strcmp(candidate, current_source) == 0) {
    return false;
  }

  if (ArrayContainsPath(reserved_targets, reserved_count, candidate)) {
    return true;
  }

  if (SourceSetContainsOther(preview, candidate, current_source)) {
    return true;
  }

  return access(candidate, F_OK) == 0;
}

static bool BuildSuffixedPath(const char *path, int suffix_index,
                              char *out_path, size_t out_path_size) {
  if (!path || !out_path || out_path_size == 0 || suffix_index < 1) {
    return false;
  }

  const char *slash = strrchr(path, '/');
  const char *base = slash ? slash + 1 : path;
  char directory[MAX_PATH_LENGTH] = {0};
  if (slash) {
    size_t dir_len = (size_t)(slash - path);
    if (dir_len >= sizeof(directory)) {
      return false;
    }
    memcpy(directory, path, dir_len);
    directory[dir_len] = '\0';
  }

  char stem[256] = {0};
  char ext[64] = {0};
  const char *dot = strrchr(base, '.');
  if (dot && dot != base) {
    size_t stem_len = (size_t)(dot - base);
    if (stem_len >= sizeof(stem)) {
      return false;
    }
    memcpy(stem, base, stem_len);
    stem[stem_len] = '\0';
    strncpy(ext, dot, sizeof(ext) - 1);
    ext[sizeof(ext) - 1] = '\0';
  } else {
    strncpy(stem, base, sizeof(stem) - 1);
    stem[sizeof(stem) - 1] = '\0';
  }

  if (directory[0] != '\0') {
    snprintf(out_path, out_path_size, "%s/%s_%d%s", directory, stem, suffix_index,
             ext);
  } else {
    snprintf(out_path, out_path_size, "%s_%d%s", stem, suffix_index, ext);
  }

  return true;
}

bool RenamerApplyExecute(const RenamerApplyRequest *request,
                         RenamerApplyResult *out_result,
                         char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!request || !out_result || !request->env_dir || request->env_dir[0] == '\0' ||
      !request->preview || request->preview->file_count < 0) {
    SetError(out_error, out_error_size,
             "invalid arguments for rename apply execution");
    return false;
  }

  memset(out_result, 0, sizeof(*out_result));
  out_result->truncation_count = request->preview->truncation_count;

  int file_count = request->preview->file_count;
  char **final_targets =
      calloc((size_t)(file_count > 0 ? file_count : 1), sizeof(char *));
  RenamerOperationMove *moves =
      calloc((size_t)(file_count > 0 ? file_count : 1), sizeof(RenamerOperationMove));
  if (!final_targets || !moves) {
    free(final_targets);
    free(moves);
    SetError(out_error, out_error_size,
             "out of memory while preparing rename apply plan");
    return false;
  }

  bool used_auto_suffix = false;
  int resolved_collisions = 0;

  for (int i = 0; i < file_count; i++) {
    const RenamerPreviewItem *item = &request->preview->items[i];

    char planned[MAX_PATH_LENGTH] = {0};
    strncpy(planned, item->candidate_path, sizeof(planned) - 1);
    planned[sizeof(planned) - 1] = '\0';

    bool conflict = HasPathConflict(request->preview, planned, item->source_path,
                                    final_targets, i);
    if (conflict) {
      if (!request->accept_auto_suffix) {
        for (int j = 0; j < i; j++) {
          free(final_targets[j]);
        }
        free(final_targets);
        free(moves);
        SetError(out_error, out_error_size,
                 "rename apply blocked by collisions; rerun with explicit auto-suffix acceptance");
        return false;
      }

      int suffix = 1;
      while (true) {
        char candidate[MAX_PATH_LENGTH] = {0};
        if (!BuildSuffixedPath(item->candidate_path, suffix, candidate,
                               sizeof(candidate))) {
          for (int j = 0; j < i; j++) {
            free(final_targets[j]);
          }
          free(final_targets);
          free(moves);
          SetError(out_error, out_error_size,
                   "failed to generate collision suffix for '%s'",
                   item->candidate_path);
          return false;
        }

        if (!HasPathConflict(request->preview, candidate, item->source_path,
                             final_targets, i)) {
          strncpy(planned, candidate, sizeof(planned) - 1);
          planned[sizeof(planned) - 1] = '\0';
          break;
        }
        suffix++;
      }

      used_auto_suffix = true;
      resolved_collisions++;
    }

    final_targets[i] = strdup(planned);
    if (!final_targets[i]) {
      for (int j = 0; j < i; j++) {
        free(final_targets[j]);
      }
      free(final_targets);
      free(moves);
      SetError(out_error, out_error_size,
               "out of memory while finalizing rename targets");
      return false;
    }
  }

  cJSON *tags_root = NULL;
  char tag_error[256] = {0};
  if (!RenamerTagsLoadSidecar(request->env_dir, &tags_root, tag_error,
                              sizeof(tag_error))) {
    for (int i = 0; i < file_count; i++) {
      free(final_targets[i]);
    }
    free(final_targets);
    free(moves);
    SetError(out_error, out_error_size, "%s", tag_error);
    return false;
  }

  bool tags_dirty = false;
  for (int i = 0; i < file_count; i++) {
    const char *source = request->preview->items[i].source_path;
    const char *destination = final_targets[i];

    strncpy(moves[i].source_path, source ? source : "",
            sizeof(moves[i].source_path) - 1);
    moves[i].source_path[sizeof(moves[i].source_path) - 1] = '\0';
    strncpy(moves[i].destination_path, destination ? destination : "",
            sizeof(moves[i].destination_path) - 1);
    moves[i].destination_path[sizeof(moves[i].destination_path) - 1] = '\0';
    moves[i].renamed = false;

    if (!source || !destination || source[0] == '\0' || destination[0] == '\0') {
      out_result->failed_count++;
      continue;
    }

    if (strcmp(source, destination) == 0) {
      out_result->skipped_count++;
      continue;
    }

    char parent_dir[MAX_PATH_LENGTH] = {0};
    strncpy(parent_dir, destination, sizeof(parent_dir) - 1);
    parent_dir[sizeof(parent_dir) - 1] = '\0';
    char *slash = strrchr(parent_dir, '/');
    if (slash) {
      *slash = '\0';
      if (parent_dir[0] != '\0') {
        FsMakeDirRecursive(parent_dir);
      }
    }

    if (FsRenameFile(source, destination)) {
      moves[i].renamed = true;
      out_result->renamed_count++;
      RenamerTagsMovePathKey(tags_root, source, destination);
      tags_dirty = true;
    } else {
      out_result->failed_count++;
    }
  }

  if (tags_dirty &&
      !RenamerTagsSaveSidecar(request->env_dir, tags_root, tag_error,
                              sizeof(tag_error))) {
    cJSON_Delete(tags_root);
    for (int i = 0; i < file_count; i++) {
      free(final_targets[i]);
    }
    free(final_targets);
    free(moves);
    SetError(out_error, out_error_size, "%s", tag_error);
    return false;
  }
  cJSON_Delete(tags_root);

  RenamerHistoryEntry entry = {0};
  BuildOperationId(request->preview, entry.operation_id,
                   sizeof(entry.operation_id));
  strncpy(entry.preview_id, request->preview->preview_id,
          sizeof(entry.preview_id) - 1);
  entry.preview_id[sizeof(entry.preview_id) - 1] = '\0';
  NowUtc(entry.created_at_utc, sizeof(entry.created_at_utc));
  entry.renamed_count = out_result->renamed_count;
  entry.skipped_count = out_result->skipped_count;
  entry.failed_count = out_result->failed_count;
  entry.collision_resolved_count = resolved_collisions;
  entry.truncation_count = request->preview->truncation_count;
  entry.rollback_performed = false;

  if (!RenamerHistoryPersistOperation(request->env_dir, &entry, moves, file_count,
                                      out_error, out_error_size)) {
    for (int i = 0; i < file_count; i++) {
      free(final_targets[i]);
    }
    free(final_targets);
    free(moves);
    return false;
  }

  strncpy(out_result->operation_id, entry.operation_id,
          sizeof(out_result->operation_id) - 1);
  out_result->operation_id[sizeof(out_result->operation_id) - 1] = '\0';
  strncpy(out_result->created_at_utc, entry.created_at_utc,
          sizeof(out_result->created_at_utc) - 1);
  out_result->created_at_utc[sizeof(out_result->created_at_utc) - 1] = '\0';
  out_result->collision_resolved_count = resolved_collisions;
  out_result->auto_suffix_applied = used_auto_suffix;

  for (int i = 0; i < file_count; i++) {
    free(final_targets[i]);
  }
  free(final_targets);
  free(moves);
  return true;
}
