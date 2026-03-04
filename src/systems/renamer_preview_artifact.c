#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "metadata_parser.h"
#include "systems/renamer_preview.h"
#include "systems/renamer_preview_internal.h"
static cJSON *BuildPreviewJson(const RenamerPreviewArtifact *preview) {
  if (!preview) {
    return NULL;
  }

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return NULL;
  }

  cJSON_AddNumberToObject(root, "version", 1);
  cJSON_AddStringToObject(root, "previewId", preview->preview_id);
  cJSON_AddStringToObject(root, "createdAtUtc", preview->created_at_utc);
  cJSON_AddStringToObject(root, "envDir", preview->env_dir);
  cJSON_AddStringToObject(root, "targetDir", preview->target_dir);
  cJSON_AddStringToObject(root, "pattern", preview->pattern);
  cJSON_AddStringToObject(root, "fingerprint", preview->fingerprint);
  cJSON_AddNumberToObject(root, "fileCount", preview->file_count);
  cJSON_AddNumberToObject(root, "collisionGroupCount",
                          preview->collision_group_count);
  cJSON_AddNumberToObject(root, "collisionCount", preview->collision_count);
  cJSON_AddNumberToObject(root, "truncationCount", preview->truncation_count);
  cJSON_AddNumberToObject(root, "metadataTagFieldCount",
                          preview->metadata_field_count);
  cJSON_AddBoolToObject(root, "requiresAutoSuffixAcceptance",
                        preview->requires_auto_suffix_acceptance);

  cJSON *items = cJSON_CreateArray();
  if (!items) {
    cJSON_Delete(root);
    return NULL;
  }

  cJSON *collision_groups = cJSON_CreateArray();
  if (!collision_groups) {
    cJSON_Delete(items);
    cJSON_Delete(root);
    return NULL;
  }
  cJSON *metadata_fields = cJSON_CreateArray();
  if (!metadata_fields) {
    cJSON_Delete(collision_groups);
    cJSON_Delete(items);
    cJSON_Delete(root);
    return NULL;
  }

  for (int i = 0; i < preview->metadata_field_count; i++) {
    if (preview->metadata_fields[i][0] == '\0') {
      continue;
    }
    cJSON_AddItemToArray(metadata_fields,
                         cJSON_CreateString(preview->metadata_fields[i]));
  }

  for (int i = 0; i < preview->file_count; i++) {
    cJSON *item = cJSON_CreateObject();
    if (!item) {
      continue;
    }

    cJSON_AddStringToObject(item, "source", preview->items[i].source_path);
    cJSON_AddStringToObject(item, "candidate", preview->items[i].candidate_path);
    cJSON_AddStringToObject(item, "candidateName",
                            preview->items[i].candidate_filename);
    cJSON_AddStringToObject(item, "tagsManual", preview->items[i].tags_manual);
    cJSON_AddStringToObject(item, "tagsMeta", preview->items[i].tags_meta);
    cJSON_AddStringToObject(item, "tags", preview->items[i].tags_merged);
    cJSON_AddBoolToObject(item, "collisionInBatch",
                          preview->items[i].collision_in_batch);
    cJSON_AddBoolToObject(item, "collisionOnDisk",
                          preview->items[i].collision_on_disk);
    cJSON_AddBoolToObject(item, "truncated", preview->items[i].truncated);
    cJSON_AddNumberToObject(item, "sourceModTime", preview->items[i].source_mod_time);
    cJSON_AddNumberToObject(item, "sourceSize", preview->items[i].source_size);

    cJSON_AddItemToArray(items, item);
  }

  for (int i = 0; i < preview->file_count; i++) {
    if (!(preview->items[i].collision_in_batch ||
          preview->items[i].collision_on_disk)) {
      continue;
    }

    bool seen = false;
    int existing_count = cJSON_GetArraySize(collision_groups);
    for (int j = 0; j < existing_count; j++) {
      cJSON *group = cJSON_GetArrayItem(collision_groups, j);
      cJSON *candidate = cJSON_GetObjectItem(group, "candidate");
      if (!cJSON_IsString(candidate) || !candidate->valuestring) {
        continue;
      }
      if (strcmp(candidate->valuestring, preview->items[i].candidate_path) == 0) {
        seen = true;
        break;
      }
    }

    if (seen) {
      continue;
    }

    cJSON *group = cJSON_CreateObject();
    if (!group) {
      continue;
    }

    int members = 0;
    bool on_disk_conflict = false;
    for (int j = 0; j < preview->file_count; j++) {
      if (strcmp(preview->items[j].candidate_path,
                 preview->items[i].candidate_path) == 0) {
        members++;
        if (preview->items[j].collision_on_disk) {
          on_disk_conflict = true;
        }
      }
    }

    cJSON_AddStringToObject(group, "candidate", preview->items[i].candidate_path);
    cJSON_AddNumberToObject(group, "members", members);
    cJSON_AddBoolToObject(group, "onDiskConflict", on_disk_conflict);
    cJSON_AddItemToArray(collision_groups, group);
  }

  cJSON *warnings = cJSON_CreateArray();
  if (!warnings) {
    cJSON_Delete(metadata_fields);
    cJSON_Delete(collision_groups);
    cJSON_Delete(items);
    cJSON_Delete(root);
    return NULL;
  }

  if (preview->truncation_count > 0) {
    cJSON_AddItemToArray(
        warnings,
        cJSON_CreateString(
            "One or more filenames exceeded max length and were truncate+hash adjusted"));
  }
  if (preview->requires_auto_suffix_acceptance) {
    cJSON_AddItemToArray(
        warnings,
        cJSON_CreateString(
            "Collisions detected. Apply requires explicit auto-suffix acceptance"));
  }

  cJSON_AddItemToObject(root, "items", items);
  cJSON_AddItemToObject(root, "metadataTagFields", metadata_fields);
  cJSON_AddItemToObject(root, "collisionGroups", collision_groups);
  cJSON_AddItemToObject(root, "warnings", warnings);
  return root;
}

static bool ParsePreviewItem(const cJSON *node, RenamerPreviewItem *out_item) {
  if (!cJSON_IsObject((cJSON *)node) || !out_item) {
    return false;
  }

  memset(out_item, 0, sizeof(*out_item));

  cJSON *source = cJSON_GetObjectItem((cJSON *)node, "source");
  cJSON *candidate = cJSON_GetObjectItem((cJSON *)node, "candidate");
  cJSON *candidate_name = cJSON_GetObjectItem((cJSON *)node, "candidateName");

  if (!cJSON_IsString(source) || !source->valuestring || !cJSON_IsString(candidate) ||
      !candidate->valuestring) {
    return false;
  }

  strncpy(out_item->source_path, source->valuestring,
          sizeof(out_item->source_path) - 1);
  out_item->source_path[sizeof(out_item->source_path) - 1] = '\0';
  strncpy(out_item->candidate_path, candidate->valuestring,
          sizeof(out_item->candidate_path) - 1);
  out_item->candidate_path[sizeof(out_item->candidate_path) - 1] = '\0';

  if (cJSON_IsString(candidate_name) && candidate_name->valuestring) {
    strncpy(out_item->candidate_filename, candidate_name->valuestring,
            sizeof(out_item->candidate_filename) - 1);
    out_item->candidate_filename[sizeof(out_item->candidate_filename) - 1] =
        '\0';
  }

  cJSON *tags_manual = cJSON_GetObjectItem((cJSON *)node, "tagsManual");
  cJSON *tags_meta = cJSON_GetObjectItem((cJSON *)node, "tagsMeta");
  cJSON *tags = cJSON_GetObjectItem((cJSON *)node, "tags");
  if (cJSON_IsString(tags_manual) && tags_manual->valuestring) {
    strncpy(out_item->tags_manual, tags_manual->valuestring,
            sizeof(out_item->tags_manual) - 1);
    out_item->tags_manual[sizeof(out_item->tags_manual) - 1] = '\0';
  }
  if (cJSON_IsString(tags_meta) && tags_meta->valuestring) {
    strncpy(out_item->tags_meta, tags_meta->valuestring,
            sizeof(out_item->tags_meta) - 1);
    out_item->tags_meta[sizeof(out_item->tags_meta) - 1] = '\0';
  }
  if (cJSON_IsString(tags) && tags->valuestring) {
    strncpy(out_item->tags_merged, tags->valuestring,
            sizeof(out_item->tags_merged) - 1);
    out_item->tags_merged[sizeof(out_item->tags_merged) - 1] = '\0';
  }

  cJSON *collision_batch = cJSON_GetObjectItem((cJSON *)node, "collisionInBatch");
  cJSON *collision_disk = cJSON_GetObjectItem((cJSON *)node, "collisionOnDisk");
  cJSON *truncated = cJSON_GetObjectItem((cJSON *)node, "truncated");
  cJSON *mod_time = cJSON_GetObjectItem((cJSON *)node, "sourceModTime");
  cJSON *source_size = cJSON_GetObjectItem((cJSON *)node, "sourceSize");

  out_item->collision_in_batch = cJSON_IsBool(collision_batch)
                                     ? cJSON_IsTrue(collision_batch)
                                     : false;
  out_item->collision_on_disk =
      cJSON_IsBool(collision_disk) ? cJSON_IsTrue(collision_disk) : false;
  out_item->truncated = cJSON_IsBool(truncated) ? cJSON_IsTrue(truncated) : false;
  out_item->source_mod_time =
      cJSON_IsNumber(mod_time) ? mod_time->valuedouble : 0.0;
  out_item->source_size = cJSON_IsNumber(source_size) ? (long)source_size->valuedouble
                                                       : 0;

  if (out_item->candidate_filename[0] == '\0') {
    const char *slash = strrchr(out_item->candidate_path, '/');
    const char *base = slash ? slash + 1 : out_item->candidate_path;
    strncpy(out_item->candidate_filename, base,
            sizeof(out_item->candidate_filename) - 1);
    out_item->candidate_filename[sizeof(out_item->candidate_filename) - 1] =
        '\0';
  }

  return true;
}

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

  cJSON *json = BuildPreviewJson(preview);
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

  cJSON *id = cJSON_GetObjectItem(json, "previewId");
  cJSON *created = cJSON_GetObjectItem(json, "createdAtUtc");
  cJSON *stored_env = cJSON_GetObjectItem(json, "envDir");
  cJSON *target = cJSON_GetObjectItem(json, "targetDir");
  cJSON *pattern = cJSON_GetObjectItem(json, "pattern");
  cJSON *fingerprint = cJSON_GetObjectItem(json, "fingerprint");
  cJSON *file_count = cJSON_GetObjectItem(json, "fileCount");
  cJSON *collision_groups = cJSON_GetObjectItem(json, "collisionGroupCount");
  cJSON *collision_count = cJSON_GetObjectItem(json, "collisionCount");
  cJSON *truncation_count = cJSON_GetObjectItem(json, "truncationCount");
  cJSON *metadata_field_count =
      cJSON_GetObjectItem(json, "metadataTagFieldCount");
  cJSON *metadata_fields = cJSON_GetObjectItem(json, "metadataTagFields");
  cJSON *requires_suffix =
      cJSON_GetObjectItem(json, "requiresAutoSuffixAcceptance");
  cJSON *items = cJSON_GetObjectItem(json, "items");

  if (!cJSON_IsString(id) || !id->valuestring || !cJSON_IsString(target) ||
      !target->valuestring || !cJSON_IsString(pattern) || !pattern->valuestring ||
      !cJSON_IsString(fingerprint) || !fingerprint->valuestring ||
      !cJSON_IsArray(items)) {
    cJSON_Delete(json);
    RenamerPreviewSetError(out_error, out_error_size,
             "rename preview artifact '%s' is missing required fields",
             artifact_path);
    return false;
  }

  int count = cJSON_GetArraySize(items);
  out_preview->items =
      calloc((size_t)(count > 0 ? count : 1), sizeof(out_preview->items[0]));
  if (!out_preview->items) {
    cJSON_Delete(json);
    RenamerPreviewSetError(out_error, out_error_size,
             "out of memory while loading rename preview items");
    return false;
  }

  out_preview->file_count = count;
  strncpy(out_preview->preview_id, id->valuestring,
          sizeof(out_preview->preview_id) - 1);
  out_preview->preview_id[sizeof(out_preview->preview_id) - 1] = '\0';
  if (cJSON_IsString(created) && created->valuestring) {
    strncpy(out_preview->created_at_utc, created->valuestring,
            sizeof(out_preview->created_at_utc) - 1);
    out_preview->created_at_utc[sizeof(out_preview->created_at_utc) - 1] = '\0';
  }
  if (cJSON_IsString(stored_env) && stored_env->valuestring) {
    strncpy(out_preview->env_dir, stored_env->valuestring,
            sizeof(out_preview->env_dir) - 1);
    out_preview->env_dir[sizeof(out_preview->env_dir) - 1] = '\0';
  } else {
    strncpy(out_preview->env_dir, env_dir, sizeof(out_preview->env_dir) - 1);
    out_preview->env_dir[sizeof(out_preview->env_dir) - 1] = '\0';
  }
  strncpy(out_preview->target_dir, target->valuestring,
          sizeof(out_preview->target_dir) - 1);
  out_preview->target_dir[sizeof(out_preview->target_dir) - 1] = '\0';
  strncpy(out_preview->pattern, pattern->valuestring,
          sizeof(out_preview->pattern) - 1);
  out_preview->pattern[sizeof(out_preview->pattern) - 1] = '\0';
  strncpy(out_preview->fingerprint, fingerprint->valuestring,
          sizeof(out_preview->fingerprint) - 1);
  out_preview->fingerprint[sizeof(out_preview->fingerprint) - 1] = '\0';

  out_preview->collision_group_count =
      cJSON_IsNumber(collision_groups) ? (int)collision_groups->valuedouble : 0;
  out_preview->collision_count =
      cJSON_IsNumber(collision_count) ? (int)collision_count->valuedouble : 0;
  out_preview->truncation_count =
      cJSON_IsNumber(truncation_count) ? (int)truncation_count->valuedouble : 0;
  out_preview->metadata_field_count =
      cJSON_IsNumber(metadata_field_count)
          ? (int)metadata_field_count->valuedouble
          : 0;
  out_preview->requires_auto_suffix_acceptance =
      cJSON_IsBool(requires_suffix) ? cJSON_IsTrue(requires_suffix)
                                    : (out_preview->collision_count > 0);

  if (out_preview->metadata_field_count < 0) {
    out_preview->metadata_field_count = 0;
  }
  if (out_preview->metadata_field_count > RENAMER_META_FIELD_MAX) {
    out_preview->metadata_field_count = RENAMER_META_FIELD_MAX;
  }
  if (cJSON_IsArray(metadata_fields)) {
    out_preview->metadata_field_count = 0;
    int metadata_count = cJSON_GetArraySize(metadata_fields);
    for (int i = 0; i < metadata_count &&
                    out_preview->metadata_field_count < RENAMER_META_FIELD_MAX;
         i++) {
      cJSON *field = cJSON_GetArrayItem(metadata_fields, i);
      if (!cJSON_IsString(field) || !field->valuestring ||
          field->valuestring[0] == '\0') {
        continue;
      }
      strncpy(out_preview->metadata_fields[out_preview->metadata_field_count],
              field->valuestring, RENAMER_META_FIELD_KEY_MAX - 1);
      out_preview
          ->metadata_fields[out_preview->metadata_field_count]
                          [RENAMER_META_FIELD_KEY_MAX - 1] = '\0';
      out_preview->metadata_field_count++;
    }
  }

  if (cJSON_IsNumber(file_count)) {
    int declared = (int)file_count->valuedouble;
    if (declared >= 0 && declared != count) {
      out_preview->file_count = declared < count ? declared : count;
    }
  }

  for (int i = 0; i < count; i++) {
    cJSON *node = cJSON_GetArrayItem(items, i);
    if (!ParsePreviewItem(node, &out_preview->items[i])) {
      cJSON_Delete(json);
      RenamerPreviewFree(out_preview);
      RenamerPreviewSetError(out_error, out_error_size,
               "failed to parse rename preview item index %d", i);
      return false;
    }
  }

  cJSON_Delete(json);
  return true;
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
