#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "metadata_parser.h"
#include "systems/renamer_preview.h"
#include "systems/renamer_preview_internal.h"

bool RenamerPreviewSaveArtifact(const RenamerPreviewArtifact *preview,
                                char *out_artifact_path,
                                size_t out_artifact_path_size,
                                char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!preview || !preview->env_dir[0] || !preview->preview_id[0]) {
    RenamerPreviewSetError(out_error, out_error_size,
             "preview artifact save requires populated preview object");
    return false;
  }

  char preview_dir[MAX_PATH_LENGTH] = {0};
  if (!RenamerPreviewEnsureRenameCachePaths(preview->env_dir, NULL, 0, preview_dir,
                              sizeof(preview_dir))) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to ensure rename preview directory");
    return false;
  }

  char artifact_path[MAX_PATH_LENGTH] = {0};
  snprintf(artifact_path, sizeof(artifact_path), "%s/%s.json", preview_dir,
           preview->preview_id);

  cJSON *json = RenamerPreviewBuildArtifactJson(preview);
  if (!json) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to serialize rename preview artifact");
    return false;
  }

  char *text = cJSON_Print(json);
  cJSON_Delete(json);
  if (!text) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to encode rename preview artifact JSON");
    return false;
  }

  bool ok = RenamerPreviewSaveFileText(artifact_path, text);
  cJSON_free(text);
  if (!ok) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to write rename preview artifact '%s'", artifact_path);
    return false;
  }

  if (out_artifact_path && out_artifact_path_size > 0) {
    strncpy(out_artifact_path, artifact_path, out_artifact_path_size - 1);
    out_artifact_path[out_artifact_path_size - 1] = '\0';
  }

  return true;
}

bool RenamerPreviewLoadArtifact(const char *env_dir, const char *preview_id,
                                RenamerPreviewArtifact *out_preview,
                                char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !preview_id || preview_id[0] == '\0' ||
      !out_preview) {
    RenamerPreviewSetError(out_error, out_error_size,
             "env_dir and preview_id are required for artifact load");
    return false;
  }

  memset(out_preview, 0, sizeof(*out_preview));

  char preview_dir[MAX_PATH_LENGTH] = {0};
  if (!RenamerPreviewEnsureRenameCachePaths(env_dir, NULL, 0, preview_dir, sizeof(preview_dir))) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to ensure rename preview path under '%s'", env_dir);
    return false;
  }

  char artifact_path[MAX_PATH_LENGTH] = {0};
  snprintf(artifact_path, sizeof(artifact_path), "%s/%s.json", preview_dir,
           preview_id);

  char *json_text = NULL;
  if (!RenamerPreviewLoadFileText(artifact_path, &json_text)) {
    RenamerPreviewSetError(out_error, out_error_size,
             "rename preview artifact '%s' was not found", artifact_path);
    return false;
  }

  cJSON *json = cJSON_Parse(json_text);
  free(json_text);
  if (!json) {
    RenamerPreviewSetError(out_error, out_error_size,
             "rename preview artifact '%s' is malformed", artifact_path);
    return false;
  }

  bool ok = RenamerPreviewParseArtifactJson(json, env_dir, artifact_path,
                                            out_preview, out_error,
                                            out_error_size);
  cJSON_Delete(json);
  return ok;
}

bool RenamerPreviewRecomputeFingerprint(const RenamerPreviewArtifact *preview,
                                        char *out_fingerprint,
                                        size_t out_fingerprint_size,
                                        char *out_error,
                                        size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!preview || !out_fingerprint || out_fingerprint_size == 0) {
    RenamerPreviewSetError(out_error, out_error_size,
             "invalid arguments for preview fingerprint revalidation");
    return false;
  }

  RenamerPreviewItem *recomputed =
      calloc((size_t)(preview->file_count > 0 ? preview->file_count : 1),
             sizeof(RenamerPreviewItem));
  if (!recomputed) {
    RenamerPreviewSetError(out_error, out_error_size,
             "out of memory during fingerprint revalidation");
    return false;
  }

  for (int i = 0; i < preview->file_count; i++) {
    strncpy(recomputed[i].source_path, preview->items[i].source_path,
            sizeof(recomputed[i].source_path) - 1);
    recomputed[i].source_path[sizeof(recomputed[i].source_path) - 1] = '\0';
    strncpy(recomputed[i].candidate_path, preview->items[i].candidate_path,
            sizeof(recomputed[i].candidate_path) - 1);
    recomputed[i].candidate_path[sizeof(recomputed[i].candidate_path) - 1] = '\0';

    double mod_date = 0.0;
    long file_size = 0;
    if (!ExtractBasicMetadata(preview->items[i].source_path, &mod_date, &file_size)) {
      free(recomputed);
      RenamerPreviewSetError(out_error, out_error_size,
               "preview fingerprint mismatch: source '%s' is missing or changed",
               preview->items[i].source_path);
      return false;
    }

    recomputed[i].source_mod_time = mod_date;
    recomputed[i].source_size = file_size;
  }

  bool ok = RenamerPreviewBuildFingerprint(preview->target_dir, preview->pattern, recomputed,
                             preview->file_count, out_fingerprint,
                             out_fingerprint_size);
  free(recomputed);

  if (!ok) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to compute revalidation fingerprint");
    return false;
  }

  return true;
}

void RenamerPreviewFree(RenamerPreviewArtifact *preview) {
  if (!preview) {
    return;
  }

  free(preview->items);
  preview->items = NULL;
  preview->file_count = 0;
  preview->collision_group_count = 0;
  preview->collision_count = 0;
  preview->truncation_count = 0;
  preview->requires_auto_suffix_acceptance = false;

  preview->preview_id[0] = '\0';
  preview->created_at_utc[0] = '\0';
  preview->env_dir[0] = '\0';
  preview->target_dir[0] = '\0';
  preview->pattern[0] = '\0';
  preview->fingerprint[0] = '\0';
}
