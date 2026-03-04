#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs_utils.h"
#include "systems/renamer_tags.h"
#include "systems/renamer_tags_internal.h"
static bool PathInBatch(const char **paths, int path_count, const char *path) {
  if (!paths || path_count <= 0 || !path) {
    return false;
  }
  for (int i = 0; i < path_count; i++) {
    if (paths[i] && strcmp(paths[i], path) == 0) {
      return true;
    }
  }
  return false;
}
bool RenamerTagsApplyBulkCsv(cJSON *root, const char **absolute_paths,
                             int path_count, const char *tag_add_csv,
                             const char *tag_remove_csv,
                             const char *meta_tag_add_csv,
                             const char *meta_tag_remove_csv,
                             char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!root || !absolute_paths || path_count < 0) {
    RenamerTagsSetError(out_error, out_error_size,
             "invalid arguments for bulk tag operations");
    return false;
  }

  char *add_tags[RENAMER_TAG_LIMIT] = {0};
  int add_count = 0;
  char *remove_tags[RENAMER_TAG_LIMIT] = {0};
  int remove_count = 0;
  char *meta_add_tags[RENAMER_TAG_LIMIT] = {0};
  int meta_add_count = 0;
  char *meta_remove_tags[RENAMER_TAG_LIMIT] = {0};
  int meta_remove_count = 0;

  if (!RenamerTagsParseCsvTags(tag_add_csv, add_tags, &add_count) ||
      !RenamerTagsParseCsvTags(tag_remove_csv, remove_tags, &remove_count) ||
      !RenamerTagsParseCsvTags(meta_tag_add_csv, meta_add_tags, &meta_add_count) ||
      !RenamerTagsParseCsvTags(meta_tag_remove_csv, meta_remove_tags, &meta_remove_count)) {
    RenamerTagsTagListFree(add_tags, add_count);
    RenamerTagsTagListFree(remove_tags, remove_count);
    RenamerTagsTagListFree(meta_add_tags, meta_add_count);
    RenamerTagsTagListFree(meta_remove_tags, meta_remove_count);
    RenamerTagsSetError(out_error, out_error_size, "failed to parse bulk tag CSV values");
    return false;
  }

  for (int i = 0; i < path_count; i++) {
    const char *path = absolute_paths[i];
    if (!path || path[0] == '\0') {
      continue;
    }

    cJSON *entry = RenamerTagsEnsurePathEntry(root, path);
    if (!entry) {
      RenamerTagsTagListFree(add_tags, add_count);
      RenamerTagsTagListFree(remove_tags, remove_count);
      RenamerTagsTagListFree(meta_add_tags, meta_add_count);
      RenamerTagsTagListFree(meta_remove_tags, meta_remove_count);
      RenamerTagsSetError(out_error, out_error_size,
               "failed to prepare tag sidecar entry for '%s'", path);
      return false;
    }

    cJSON *manual = cJSON_GetObjectItem(entry, "manualTags");
    cJSON *meta_adds = cJSON_GetObjectItem(entry, "metaTagAdds");
    cJSON *suppressed = cJSON_GetObjectItem(entry, "suppressedMetaTags");

    for (int j = 0; j < add_count; j++) {
      if (!RenamerTagsJSONArrayAddTag(manual, add_tags[j])) {
        RenamerTagsTagListFree(add_tags, add_count);
        RenamerTagsTagListFree(remove_tags, remove_count);
        RenamerTagsTagListFree(meta_add_tags, meta_add_count);
        RenamerTagsTagListFree(meta_remove_tags, meta_remove_count);
        RenamerTagsSetError(out_error, out_error_size,
                 "failed to append manual tag '%s'", add_tags[j]);
        return false;
      }
      RenamerTagsJSONArrayRemoveTag(suppressed, add_tags[j]);
    }

    for (int j = 0; j < remove_count; j++) {
      RenamerTagsJSONArrayRemoveTag(manual, remove_tags[j]);
      if (!RenamerTagsJSONArrayAddTag(suppressed, remove_tags[j])) {
        RenamerTagsTagListFree(add_tags, add_count);
        RenamerTagsTagListFree(remove_tags, remove_count);
        RenamerTagsTagListFree(meta_add_tags, meta_add_count);
        RenamerTagsTagListFree(meta_remove_tags, meta_remove_count);
        RenamerTagsSetError(out_error, out_error_size,
                 "failed to append suppressed tag '%s'", remove_tags[j]);
        return false;
      }
    }

    for (int j = 0; j < meta_add_count; j++) {
      if (!RenamerTagsJSONArrayAddTag(meta_adds, meta_add_tags[j])) {
        RenamerTagsTagListFree(add_tags, add_count);
        RenamerTagsTagListFree(remove_tags, remove_count);
        RenamerTagsTagListFree(meta_add_tags, meta_add_count);
        RenamerTagsTagListFree(meta_remove_tags, meta_remove_count);
        RenamerTagsSetError(out_error, out_error_size,
                 "failed to append metadata add tag '%s'", meta_add_tags[j]);
        return false;
      }
      RenamerTagsJSONArrayRemoveTag(suppressed, meta_add_tags[j]);
    }

    for (int j = 0; j < meta_remove_count; j++) {
      RenamerTagsJSONArrayRemoveTag(meta_adds, meta_remove_tags[j]);
      if (!RenamerTagsJSONArrayAddTag(suppressed, meta_remove_tags[j])) {
        RenamerTagsTagListFree(add_tags, add_count);
        RenamerTagsTagListFree(remove_tags, remove_count);
        RenamerTagsTagListFree(meta_add_tags, meta_add_count);
        RenamerTagsTagListFree(meta_remove_tags, meta_remove_count);
        RenamerTagsSetError(out_error, out_error_size,
                 "failed to append metadata suppressed tag '%s'",
                 meta_remove_tags[j]);
        return false;
      }
    }

    char updated_at[64] = {0};
    RenamerTagsNowUtc(updated_at, sizeof(updated_at));
    cJSON_ReplaceItemInObject(entry, "updatedAtUtc", cJSON_CreateString(updated_at));
  }

  RenamerTagsTagListFree(add_tags, add_count);
  RenamerTagsTagListFree(remove_tags, remove_count);
  RenamerTagsTagListFree(meta_add_tags, meta_add_count);
  RenamerTagsTagListFree(meta_remove_tags, meta_remove_count);
  return true;
}
static bool ApplyMapEntry(cJSON *root, const char *path,
                          const cJSON *manual_tags_node,
                          const cJSON *meta_adds_node,
                          const cJSON *suppressed_tags_node,
                          const char **batch_paths, int batch_count,
                          char *out_error, size_t out_error_size) {
  if (!root || !path || path[0] == '\0') {
    return true;
  }

  char absolute_path[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(path, absolute_path, sizeof(absolute_path))) {
    RenamerTagsSetError(out_error, out_error_size,
             "rename tags map path '%s' is not resolvable", path);
    return false;
  }

  if (batch_paths && batch_count > 0 &&
      !PathInBatch(batch_paths, batch_count, absolute_path)) {
    return true;
  }

  cJSON *entry = RenamerTagsEnsurePathEntry(root, absolute_path);
  if (!entry) {
    RenamerTagsSetError(out_error, out_error_size,
             "failed to create tag sidecar entry for '%s'", absolute_path);
    return false;
  }

  char *manual_tags[RENAMER_TAG_LIMIT] = {0};
  int manual_count = 0;
  if (!RenamerTagsParseTagsFromNode(manual_tags_node, manual_tags, &manual_count)) {
    RenamerTagsTagListFree(manual_tags, manual_count);
    RenamerTagsSetError(out_error, out_error_size,
             "invalid manualTags in rename tags map for '%s'", absolute_path);
    return false;
  }

  char *suppressed_tags[RENAMER_TAG_LIMIT] = {0};
  int suppressed_count = 0;
  if (!RenamerTagsParseTagsFromNode(suppressed_tags_node, suppressed_tags,
                         &suppressed_count)) {
    RenamerTagsTagListFree(manual_tags, manual_count);
    RenamerTagsSetError(out_error, out_error_size,
             "invalid suppressedMetaTags in rename tags map for '%s'",
             absolute_path);
    return false;
  }

  char *meta_add_tags[RENAMER_TAG_LIMIT] = {0};
  int meta_add_count = 0;
  if (!RenamerTagsParseTagsFromNode(meta_adds_node, meta_add_tags, &meta_add_count)) {
    RenamerTagsTagListFree(manual_tags, manual_count);
    RenamerTagsTagListFree(suppressed_tags, suppressed_count);
    RenamerTagsSetError(out_error, out_error_size,
             "invalid metaTagAdds in rename tags map for '%s'",
             absolute_path);
    return false;
  }

  cJSON *manual_array = cJSON_CreateArray();
  cJSON *meta_add_array = cJSON_CreateArray();
  cJSON *suppressed_array = cJSON_CreateArray();
  if (!manual_array || !meta_add_array || !suppressed_array) {
    cJSON_Delete(manual_array);
    cJSON_Delete(meta_add_array);
    cJSON_Delete(suppressed_array);
    RenamerTagsTagListFree(manual_tags, manual_count);
    RenamerTagsTagListFree(meta_add_tags, meta_add_count);
    RenamerTagsTagListFree(suppressed_tags, suppressed_count);
    RenamerTagsSetError(out_error, out_error_size,
             "out of memory while applying tag map for '%s'", absolute_path);
    return false;
  }

  for (int i = 0; i < manual_count; i++) {
    cJSON_AddItemToArray(manual_array, cJSON_CreateString(manual_tags[i]));
  }
  for (int i = 0; i < suppressed_count; i++) {
    cJSON_AddItemToArray(suppressed_array,
                         cJSON_CreateString(suppressed_tags[i]));
  }
  for (int i = 0; i < meta_add_count; i++) {
    if (RenamerTagsTagListContains(suppressed_tags, suppressed_count, meta_add_tags[i])) {
      continue;
    }
    cJSON_AddItemToArray(meta_add_array, cJSON_CreateString(meta_add_tags[i]));
  }

  cJSON_ReplaceItemInObject(entry, "manualTags", manual_array);
  cJSON_ReplaceItemInObject(entry, "metaTagAdds", meta_add_array);
  cJSON_ReplaceItemInObject(entry, "suppressedMetaTags", suppressed_array);

  char updated_at[64] = {0};
  RenamerTagsNowUtc(updated_at, sizeof(updated_at));
  cJSON_ReplaceItemInObject(entry, "updatedAtUtc", cJSON_CreateString(updated_at));

  RenamerTagsTagListFree(manual_tags, manual_count);
  RenamerTagsTagListFree(meta_add_tags, meta_add_count);
  RenamerTagsTagListFree(suppressed_tags, suppressed_count);
  return true;
}

bool RenamerTagsApplyMapFile(cJSON *root, const char *map_json_path,
                             const char **absolute_paths, int path_count,
                             char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!root) {
    RenamerTagsSetError(out_error, out_error_size,
             "tag sidecar root is required for map ingest");
    return false;
  }
  if (!map_json_path || map_json_path[0] == '\0') {
    return true;
  }

  char *json_text = NULL;
  if (!RenamerTagsLoadFileText(map_json_path, &json_text)) {
    RenamerTagsSetError(out_error, out_error_size,
             "failed to read rename tags map '%s'", map_json_path);
    return false;
  }

  cJSON *json = cJSON_Parse(json_text);
  free(json_text);
  if (!json) {
    RenamerTagsSetError(out_error, out_error_size,
             "rename tags map '%s' is not valid JSON", map_json_path);
    return false;
  }

  bool ok = true;
  if (cJSON_IsArray(json)) {
    int count = cJSON_GetArraySize(json);
    for (int i = 0; i < count; i++) {
      cJSON *item = cJSON_GetArrayItem(json, i);
      if (!cJSON_IsObject(item)) {
        continue;
      }
      cJSON *path = cJSON_GetObjectItem(item, "path");
      if (!cJSON_IsString(path) || !path->valuestring) {
        continue;
      }
      cJSON *manual_tags = cJSON_GetObjectItem(item, "manualTags");
      cJSON *meta_add_tags = cJSON_GetObjectItem(item, "metaTagAdds");
      cJSON *suppressed_tags = cJSON_GetObjectItem(item, "suppressedMetaTags");
      ok = ApplyMapEntry(root, path->valuestring, manual_tags, meta_add_tags,
                         suppressed_tags, absolute_paths, path_count, out_error,
                         out_error_size);
      if (!ok) {
        break;
      }
    }
  } else if (cJSON_IsObject(json)) {
    cJSON *files = cJSON_GetObjectItem(json, "files");
    cJSON *iter_root = cJSON_IsObject(files) ? files : json;

    cJSON *node = NULL;
    cJSON_ArrayForEach(node, iter_root) {
      if (!node->string) {
        continue;
      }

      if (cJSON_IsObject(node)) {
        cJSON *manual_tags = cJSON_GetObjectItem(node, "manualTags");
        cJSON *meta_add_tags = cJSON_GetObjectItem(node, "metaTagAdds");
        cJSON *suppressed_tags = cJSON_GetObjectItem(node, "suppressedMetaTags");
        ok = ApplyMapEntry(root, node->string, manual_tags, meta_add_tags,
                           suppressed_tags, absolute_paths, path_count, out_error,
                           out_error_size);
      } else {
        ok = ApplyMapEntry(root, node->string, node, NULL, NULL, absolute_paths,
                           path_count, out_error, out_error_size);
      }
      if (!ok) {
        break;
      }
    }
  } else {
    ok = false;
    RenamerTagsSetError(out_error, out_error_size,
             "rename tags map must be JSON object or array");
  }

  cJSON_Delete(json);
  return ok;
}
