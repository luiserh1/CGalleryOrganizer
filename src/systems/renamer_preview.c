#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "gallery_cache.h"
#include "systems/renamer_pattern.h"
#include "systems/renamer_preview.h"
#include "systems/renamer_preview_internal.h"
#include "systems/renamer_tags.h"

bool RenamerPreviewLoadDefaultPattern(const char *env_dir, char *out_pattern,
                                      size_t out_pattern_size,
                                      char *out_error,
                                      size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_pattern || out_pattern_size == 0) {
    RenamerPreviewSetError(out_error, out_error_size,
             "env_dir and output buffer are required for default pattern load");
    return false;
  }

  out_pattern[0] = '\0';
  char config_path[MAX_PATH_LENGTH] = {0};
  if (!RenamerPreviewEnsureRenameCachePaths(env_dir, config_path, sizeof(config_path), NULL,
                              0)) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to ensure rename cache paths under '%s'", env_dir);
    return false;
  }

  char *json_text = NULL;
  if (!RenamerPreviewLoadFileText(config_path, &json_text)) {
    strncpy(out_pattern, RenamerPatternDefault(), out_pattern_size - 1);
    out_pattern[out_pattern_size - 1] = '\0';
    return true;
  }

  cJSON *json = cJSON_Parse(json_text);
  free(json_text);
  if (!json) {
    strncpy(out_pattern, RenamerPatternDefault(), out_pattern_size - 1);
    out_pattern[out_pattern_size - 1] = '\0';
    return true;
  }

  cJSON *value = cJSON_GetObjectItem(json, "defaultPattern");
  if (cJSON_IsString(value) && value->valuestring && value->valuestring[0] != '\0') {
    strncpy(out_pattern, value->valuestring, out_pattern_size - 1);
    out_pattern[out_pattern_size - 1] = '\0';
  } else {
    strncpy(out_pattern, RenamerPatternDefault(), out_pattern_size - 1);
    out_pattern[out_pattern_size - 1] = '\0';
  }

  cJSON_Delete(json);
  return true;
}

bool RenamerPreviewSaveDefaultPattern(const char *env_dir, const char *pattern,
                                      char *out_error,
                                      size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !pattern || pattern[0] == '\0') {
    RenamerPreviewSetError(out_error, out_error_size,
             "env_dir and pattern are required for default pattern save");
    return false;
  }

  char config_path[MAX_PATH_LENGTH] = {0};
  if (!RenamerPreviewEnsureRenameCachePaths(env_dir, config_path, sizeof(config_path), NULL,
                              0)) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to ensure rename cache paths under '%s'", env_dir);
    return false;
  }

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to allocate rename config JSON object");
    return false;
  }

  cJSON_AddNumberToObject(json, "version", 1);
  cJSON_AddStringToObject(json, "defaultPattern", pattern);
  char updated_at[32] = {0};
  RenamerPreviewNowUtc(updated_at, sizeof(updated_at));
  cJSON_AddStringToObject(json, "updatedAtUtc", updated_at);

  char *text = cJSON_Print(json);
  cJSON_Delete(json);
  if (!text) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to serialize rename config JSON");
    return false;
  }

  bool ok = RenamerPreviewSaveFileText(config_path, text);
  cJSON_free(text);
  if (!ok) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to write rename config '%s'", config_path);
    return false;
  }

  return true;
}

bool RenamerPreviewBuild(const RenamerPreviewRequest *request,
                         RenamerPreviewArtifact *out_preview,
                         char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!request || !out_preview || !request->env_dir || request->env_dir[0] == '\0' ||
      !request->target_dir || request->target_dir[0] == '\0') {
    RenamerPreviewSetError(out_error, out_error_size,
             "rename preview requires env_dir and target_dir");
    return false;
  }

  (void)request->recursive;

  memset(out_preview, 0, sizeof(*out_preview));

  char target_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(request->target_dir, target_abs, sizeof(target_abs))) {
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to resolve target directory '%s'", request->target_dir);
    return false;
  }

  char effective_pattern[256] = {0};
  if (request->pattern && request->pattern[0] != '\0') {
    strncpy(effective_pattern, request->pattern, sizeof(effective_pattern) - 1);
    effective_pattern[sizeof(effective_pattern) - 1] = '\0';
  } else if (!RenamerPreviewLoadDefaultPattern(request->env_dir, effective_pattern,
                                               sizeof(effective_pattern), out_error,
                                               out_error_size)) {
    return false;
  }

  char pattern_error[256] = {0};
  if (!RenamerPatternValidate(effective_pattern, pattern_error,
                              sizeof(pattern_error))) {
    RenamerPreviewSetError(out_error, out_error_size, "%s", pattern_error);
    return false;
  }

  if (!RenamerPreviewSaveDefaultPattern(request->env_dir, effective_pattern,
                                        out_error, out_error_size)) {
    return false;
  }

  RenamerPreviewPathList files = {0};
  if (!RenamerPreviewCollectFilesRecursive(target_abs, &files, out_error,
                                           out_error_size)) {
    RenamerPreviewPathListFree(&files);
    return false;
  }

  out_preview->items =
      calloc((size_t)(files.count > 0 ? files.count : 1), sizeof(out_preview->items[0]));
  if (!out_preview->items) {
    RenamerPreviewPathListFree(&files);
    RenamerPreviewSetError(out_error, out_error_size,
             "out of memory while allocating rename preview items");
    return false;
  }

  out_preview->file_count = files.count;
  strncpy(out_preview->env_dir, request->env_dir, sizeof(out_preview->env_dir) - 1);
  out_preview->env_dir[sizeof(out_preview->env_dir) - 1] = '\0';
  strncpy(out_preview->target_dir, target_abs, sizeof(out_preview->target_dir) - 1);
  out_preview->target_dir[sizeof(out_preview->target_dir) - 1] = '\0';
  strncpy(out_preview->pattern, effective_pattern, sizeof(out_preview->pattern) - 1);
  out_preview->pattern[sizeof(out_preview->pattern) - 1] = '\0';
  RenamerPreviewNowUtc(out_preview->created_at_utc, sizeof(out_preview->created_at_utc));

  cJSON *tags_root = NULL;
  if (!RenamerTagsLoadSidecar(request->env_dir, &tags_root, out_error,
                              out_error_size)) {
    RenamerPreviewPathListFree(&files);
    RenamerPreviewFree(out_preview);
    return false;
  }

  if (!RenamerTagsApplyMapFile(tags_root, request->tags_map_path,
                               (const char **)files.paths, files.count,
                               out_error, out_error_size)) {
    cJSON_Delete(tags_root);
    RenamerPreviewPathListFree(&files);
    RenamerPreviewFree(out_preview);
    return false;
  }

  if (!RenamerTagsApplyBulkCsv(tags_root, (const char **)files.paths, files.count,
                               request->tag_add_csv, request->tag_remove_csv,
                               request->meta_tag_add_csv,
                               request->meta_tag_remove_csv, out_error,
                               out_error_size)) {
    cJSON_Delete(tags_root);
    RenamerPreviewPathListFree(&files);
    RenamerPreviewFree(out_preview);
    return false;
  }

  if (!RenamerTagsSaveSidecar(request->env_dir, tags_root, out_error,
                              out_error_size)) {
    cJSON_Delete(tags_root);
    RenamerPreviewPathListFree(&files);
    RenamerPreviewFree(out_preview);
    return false;
  }

  bool cache_dirty = false;
  for (int i = 0; i < files.count; i++) {
    ImageMetadata md = {0};
    if (!RenamerPreviewLoadMetadataForPath(files.paths[i], &cache_dirty, &md, out_error,
                             out_error_size)) {
      cJSON_Delete(tags_root);
      RenamerPreviewPathListFree(&files);
      RenamerPreviewFree(out_preview);
      return false;
    }

    char date[64] = {0};
    char time_text[64] = {0};
    char datetime[96] = {0};
    char camera[128] = {0};
    char make[128] = {0};
    char model[128] = {0};
    char gps_lat[64] = {0};
    char gps_lon[64] = {0};
    char location[160] = {0};
    char format[32] = {0};

    RenamerPreviewExtractTokensFromMetadata(&md, date, sizeof(date), time_text,
                              sizeof(time_text), datetime, sizeof(datetime),
                              camera, sizeof(camera), make, sizeof(make), model,
                              sizeof(model), gps_lat, sizeof(gps_lat), gps_lon,
                              sizeof(gps_lon), location, sizeof(location),
                              format, sizeof(format));

    if (!RenamerTagsCollectMetadataFields(
            md.allMetadataJson, out_preview->metadata_fields,
            RENAMER_META_FIELD_MAX, &out_preview->metadata_field_count, out_error,
            out_error_size)) {
      CacheFreeMetadata(&md);
      cJSON_Delete(tags_root);
      RenamerPreviewPathListFree(&files);
      RenamerPreviewFree(out_preview);
      return false;
    }

    RenamerResolvedTags resolved_tags = {0};
    if (!RenamerTagsResolve(tags_root, files.paths[i], md.allMetadataJson,
                            &resolved_tags, out_error, out_error_size)) {
      CacheFreeMetadata(&md);
      cJSON_Delete(tags_root);
      RenamerPreviewPathListFree(&files);
      RenamerPreviewFree(out_preview);
      return false;
    }

    RenamerPatternContext ctx = {
        .date = date,
        .time = time_text,
        .datetime = datetime,
        .camera = camera,
        .make = make,
        .model = model,
        .format = format,
        .gps_lat = gps_lat,
        .gps_lon = gps_lon,
        .location = location,
        .tags_manual = resolved_tags.manual,
        .tags_meta = resolved_tags.meta,
        .tags = resolved_tags.merged,
        .index = i + 1,
    };

    bool truncated = false;
    char warning[128] = {0};
    char render_error[256] = {0};
    if (!RenamerPatternRender(effective_pattern, &ctx,
                              out_preview->items[i].candidate_filename,
                              sizeof(out_preview->items[i].candidate_filename),
                              &truncated, warning, sizeof(warning), render_error,
                              sizeof(render_error))) {
      CacheFreeMetadata(&md);
      cJSON_Delete(tags_root);
      RenamerPreviewPathListFree(&files);
      RenamerPreviewFree(out_preview);
      RenamerPreviewSetError(out_error, out_error_size, "%s", render_error);
      return false;
    }

    if (truncated) {
      out_preview->truncation_count++;
    }

    char parent_dir[MAX_PATH_LENGTH] = {0};
    if (!RenamerPreviewBuildParentDir(files.paths[i], parent_dir,
                                      sizeof(parent_dir))) {
      CacheFreeMetadata(&md);
      cJSON_Delete(tags_root);
      RenamerPreviewPathListFree(&files);
      RenamerPreviewFree(out_preview);
      RenamerPreviewSetError(out_error, out_error_size,
               "failed to resolve parent directory for '%s'", files.paths[i]);
      return false;
    }

    snprintf(out_preview->items[i].candidate_path,
             sizeof(out_preview->items[i].candidate_path), "%s/%s", parent_dir,
             out_preview->items[i].candidate_filename);

    strncpy(out_preview->items[i].source_path, files.paths[i],
            sizeof(out_preview->items[i].source_path) - 1);
    out_preview->items[i].source_path[sizeof(out_preview->items[i].source_path) - 1] =
        '\0';
    strncpy(out_preview->items[i].tags_manual, resolved_tags.manual,
            sizeof(out_preview->items[i].tags_manual) - 1);
    out_preview->items[i].tags_manual[sizeof(out_preview->items[i].tags_manual) - 1] =
        '\0';
    strncpy(out_preview->items[i].tags_meta, resolved_tags.meta,
            sizeof(out_preview->items[i].tags_meta) - 1);
    out_preview->items[i].tags_meta[sizeof(out_preview->items[i].tags_meta) - 1] =
        '\0';
    strncpy(out_preview->items[i].tags_merged, resolved_tags.merged,
            sizeof(out_preview->items[i].tags_merged) - 1);
    out_preview->items[i].tags_merged[sizeof(out_preview->items[i].tags_merged) - 1] =
        '\0';
    out_preview->items[i].truncated = truncated;
    out_preview->items[i].source_mod_time = md.modificationDate;
    out_preview->items[i].source_size = md.fileSize;

    CacheFreeMetadata(&md);
  }

  if (cache_dirty && !CacheSave()) {
    cJSON_Delete(tags_root);
    RenamerPreviewPathListFree(&files);
    RenamerPreviewFree(out_preview);
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to persist cache while refreshing rename metadata");
    return false;
  }

  RenamerPreviewDetectCollisions(out_preview);

  if (!RenamerPreviewBuildFingerprint(out_preview->target_dir, out_preview->pattern,
                        out_preview->items, out_preview->file_count,
                        out_preview->fingerprint,
                        sizeof(out_preview->fingerprint))) {
    cJSON_Delete(tags_root);
    RenamerPreviewPathListFree(&files);
    RenamerPreviewFree(out_preview);
    RenamerPreviewSetError(out_error, out_error_size,
             "failed to generate rename preview fingerprint");
    return false;
  }

  RenamerPreviewBuildPreviewId(out_preview->fingerprint, out_preview->preview_id,
                               sizeof(out_preview->preview_id));

  cJSON_Delete(tags_root);
  RenamerPreviewPathListFree(&files);
  return true;
}
