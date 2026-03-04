#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "systems/renamer_preview_internal.h"

cJSON *RenamerPreviewBuildArtifactJson(const RenamerPreviewArtifact *preview) {
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

static bool RenamerPreviewParseItem(const cJSON *node, RenamerPreviewItem *out_item) {
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

bool RenamerPreviewParseArtifactJson(const cJSON *json, const char *env_dir,
                                     const char *artifact_path,
                                     RenamerPreviewArtifact *out_preview,
                                     char *out_error, size_t out_error_size) {
  if (!json || !env_dir || !artifact_path || !out_preview) {
    RenamerPreviewSetError(out_error, out_error_size,
                           "invalid arguments while parsing rename preview artifact");
    return false;
  }

  cJSON *id = cJSON_GetObjectItem((cJSON *)json, "previewId");
  cJSON *created = cJSON_GetObjectItem((cJSON *)json, "createdAtUtc");
  cJSON *stored_env = cJSON_GetObjectItem((cJSON *)json, "envDir");
  cJSON *target = cJSON_GetObjectItem((cJSON *)json, "targetDir");
  cJSON *pattern = cJSON_GetObjectItem((cJSON *)json, "pattern");
  cJSON *fingerprint = cJSON_GetObjectItem((cJSON *)json, "fingerprint");
  cJSON *file_count = cJSON_GetObjectItem((cJSON *)json, "fileCount");
  cJSON *collision_groups = cJSON_GetObjectItem((cJSON *)json, "collisionGroupCount");
  cJSON *collision_count = cJSON_GetObjectItem((cJSON *)json, "collisionCount");
  cJSON *truncation_count = cJSON_GetObjectItem((cJSON *)json, "truncationCount");
  cJSON *metadata_field_count =
      cJSON_GetObjectItem((cJSON *)json, "metadataTagFieldCount");
  cJSON *metadata_fields = cJSON_GetObjectItem((cJSON *)json, "metadataTagFields");
  cJSON *requires_suffix =
      cJSON_GetObjectItem((cJSON *)json, "requiresAutoSuffixAcceptance");
  cJSON *items = cJSON_GetObjectItem((cJSON *)json, "items");

  if (!cJSON_IsString(id) || !id->valuestring || !cJSON_IsString(target) ||
      !target->valuestring || !cJSON_IsString(pattern) || !pattern->valuestring ||
      !cJSON_IsString(fingerprint) || !fingerprint->valuestring ||
      !cJSON_IsArray(items)) {
    RenamerPreviewSetError(out_error, out_error_size,
                           "rename preview artifact '%s' is missing required fields",
                           artifact_path);
    return false;
  }

  int count = cJSON_GetArraySize(items);
  out_preview->items =
      calloc((size_t)(count > 0 ? count : 1), sizeof(out_preview->items[0]));
  if (!out_preview->items) {
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
    if (!RenamerPreviewParseItem(node, &out_preview->items[i])) {
      RenamerPreviewFree(out_preview);
      RenamerPreviewSetError(out_error, out_error_size,
                             "failed to parse rename preview item index %d", i);
      return false;
    }
  }

  return true;
}
