#include <stdio.h>
#include <string.h>

#include "systems/renamer_tags.h"
#include "systems/renamer_tags_internal.h"

static const char *RENAMER_META_TAG_WHITELIST[] = {
    "Iptc.Application2.Keywords",
    "Xmp.dc.subject",
    "Xmp.lr.hierarchicalSubject",
    "Xmp.photoshop.SupplementalCategories",
    "Xmp.digiKam.TagsList",
    NULL,
};
static bool FieldListContains(
    char fields[][RENAMER_META_FIELD_KEY_MAX], int count,
    const char *candidate) {
  if (!fields || count <= 0 || !candidate || candidate[0] == '\0') {
    return false;
  }

  for (int i = 0; i < count; i++) {
    if (fields[i][0] != '\0' && strcmp(fields[i], candidate) == 0) {
      return true;
    }
  }
  return false;
}
static bool KeyWhitelisted(const char *key) {
  if (!key || key[0] == '\0') {
    return false;
  }

  for (int i = 0; RENAMER_META_TAG_WHITELIST[i]; i++) {
    if (strcmp(RENAMER_META_TAG_WHITELIST[i], key) == 0) {
      return true;
    }
  }
  return false;
}

static bool CollectMetadataTags(const char *all_metadata_json, cJSON *entry,
                                char **out_meta_tags, int *out_meta_count,
                                char *out_error, size_t out_error_size) {
  if (!out_meta_tags || !out_meta_count) {
    return false;
  }

  *out_meta_count = 0;

  cJSON *suppressed_array = entry ? cJSON_GetObjectItem(entry, "suppressedMetaTags")
                                  : NULL;
  cJSON *meta_adds_array = entry ? cJSON_GetObjectItem(entry, "metaTagAdds")
                                 : NULL;
  if (all_metadata_json && all_metadata_json[0] != '\0') {
    cJSON *json = cJSON_Parse(all_metadata_json);
    if (!json) {
      RenamerTagsSetError(out_error, out_error_size,
               "failed to parse allMetadataJson while resolving tags");
      return false;
    }

    cJSON *node = NULL;
    cJSON_ArrayForEach(node, json) {
      if (!node->string || !KeyWhitelisted(node->string)) {
        continue;
      }

      char *parsed[RENAMER_TAG_LIMIT] = {0};
      int parsed_count = 0;
      bool ok = RenamerTagsParseTagsFromNode(node, parsed, &parsed_count);
      if (!ok) {
        RenamerTagsTagListFree(parsed, parsed_count);
        cJSON_Delete(json);
        RenamerTagsSetError(out_error, out_error_size,
                 "invalid metadata tags in key '%s'", node->string);
        return false;
      }

      for (int i = 0; i < parsed_count; i++) {
        if (!parsed[i] || parsed[i][0] == '\0') {
          continue;
        }
        if (RenamerTagsJSONArrayContainsTag(suppressed_array, parsed[i])) {
          continue;
        }
        if (!RenamerTagsTagListAppend(out_meta_tags, out_meta_count, parsed[i])) {
          RenamerTagsTagListFree(parsed, parsed_count);
          cJSON_Delete(json);
          RenamerTagsSetError(out_error, out_error_size,
                   "metadata tag limit exceeded while parsing '%s'",
                   node->string);
          return false;
        }
      }

      RenamerTagsTagListFree(parsed, parsed_count);
    }

    cJSON_Delete(json);
  }

  char *meta_adds[RENAMER_TAG_LIMIT] = {0};
  int meta_adds_count = 0;
  if (!RenamerTagsParseTagsFromNode(meta_adds_array, meta_adds, &meta_adds_count)) {
    RenamerTagsTagListFree(meta_adds, meta_adds_count);
    RenamerTagsSetError(out_error, out_error_size,
             "invalid metaTagAdds while resolving tags");
    return false;
  }

  for (int i = 0; i < meta_adds_count; i++) {
    if (!meta_adds[i] || meta_adds[i][0] == '\0') {
      continue;
    }
    if (RenamerTagsJSONArrayContainsTag(suppressed_array, meta_adds[i])) {
      continue;
    }
    if (!RenamerTagsTagListAppend(out_meta_tags, out_meta_count, meta_adds[i])) {
      RenamerTagsTagListFree(meta_adds, meta_adds_count);
      RenamerTagsSetError(out_error, out_error_size,
               "metadata tag limit exceeded while appending metaTagAdds");
      return false;
    }
  }

  RenamerTagsTagListFree(meta_adds, meta_adds_count);
  return true;
}
bool RenamerTagsCollectMetadataFields(
    const char *all_metadata_json,
    char out_fields[][RENAMER_META_FIELD_KEY_MAX], int max_fields,
    int *io_count, char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!out_fields || max_fields <= 0 || !io_count || *io_count < 0) {
    RenamerTagsSetError(out_error, out_error_size,
             "invalid arguments for metadata field collection");
    return false;
  }
  if (!all_metadata_json || all_metadata_json[0] == '\0') {
    return true;
  }

  cJSON *json = cJSON_Parse(all_metadata_json);
  if (!json) {
    RenamerTagsSetError(out_error, out_error_size,
             "failed to parse allMetadataJson while collecting fields");
    return false;
  }
  if (!cJSON_IsObject(json)) {
    cJSON_Delete(json);
    return true;
  }

  for (int i = 0; RENAMER_META_TAG_WHITELIST[i]; i++) {
    const char *key = RENAMER_META_TAG_WHITELIST[i];
    cJSON *node = cJSON_GetObjectItem(json, key);
    if (!node) {
      continue;
    }
    if (FieldListContains(out_fields, *io_count, key)) {
      continue;
    }
    if (*io_count >= max_fields) {
      break;
    }
    strncpy(out_fields[*io_count], key, RENAMER_META_FIELD_KEY_MAX - 1);
    out_fields[*io_count][RENAMER_META_FIELD_KEY_MAX - 1] = '\0';
    (*io_count)++;
  }

  cJSON_Delete(json);
  return true;
}

bool RenamerTagsResolve(const cJSON *root, const char *absolute_path,
                        const char *all_metadata_json,
                        RenamerResolvedTags *out_tags, char *out_error,
                        size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!root || !absolute_path || absolute_path[0] == '\0' || !out_tags) {
    RenamerTagsSetError(out_error, out_error_size,
             "invalid arguments for resolving rename tags");
    return false;
  }

  memset(out_tags, 0, sizeof(*out_tags));

  const cJSON *files = cJSON_GetObjectItem((cJSON *)root, "files");
  const cJSON *entry = cJSON_IsObject((cJSON *)files)
                           ? cJSON_GetObjectItem((cJSON *)files, absolute_path)
                           : NULL;

  char *manual_tags[RENAMER_TAG_LIMIT] = {0};
  int manual_count = 0;
  if (entry) {
    const cJSON *manual_node = cJSON_GetObjectItem((cJSON *)entry, "manualTags");
    if (!RenamerTagsParseTagsFromNode(manual_node, manual_tags, &manual_count)) {
      RenamerTagsTagListFree(manual_tags, manual_count);
      RenamerTagsSetError(out_error, out_error_size,
               "invalid manualTags for '%s' in rename tag sidecar",
               absolute_path);
      return false;
    }
  }

  char *meta_tags[RENAMER_TAG_LIMIT] = {0};
  int meta_count = 0;
  if (!CollectMetadataTags(all_metadata_json, (cJSON *)entry, meta_tags,
                           &meta_count, out_error, out_error_size)) {
    RenamerTagsTagListFree(manual_tags, manual_count);
    RenamerTagsTagListFree(meta_tags, meta_count);
    return false;
  }

  char *merged[RENAMER_TAG_LIMIT] = {0};
  int merged_count = 0;
  for (int i = 0; i < manual_count; i++) {
    if (!RenamerTagsTagListAppend(merged, &merged_count, manual_tags[i])) {
      RenamerTagsTagListFree(manual_tags, manual_count);
      RenamerTagsTagListFree(meta_tags, meta_count);
      RenamerTagsTagListFree(merged, merged_count);
      RenamerTagsSetError(out_error, out_error_size,
               "tag merge limit exceeded for '%s'", absolute_path);
      return false;
    }
  }
  for (int i = 0; i < meta_count; i++) {
    if (!RenamerTagsTagListAppend(merged, &merged_count, meta_tags[i])) {
      RenamerTagsTagListFree(manual_tags, manual_count);
      RenamerTagsTagListFree(meta_tags, meta_count);
      RenamerTagsTagListFree(merged, merged_count);
      RenamerTagsSetError(out_error, out_error_size,
               "tag merge limit exceeded for '%s'", absolute_path);
      return false;
    }
  }

  RenamerTagsTagListJoin(manual_tags, manual_count, out_tags->manual, sizeof(out_tags->manual),
              "untagged");
  RenamerTagsTagListJoin(meta_tags, meta_count, out_tags->meta, sizeof(out_tags->meta),
              "untagged");
  RenamerTagsTagListJoin(merged, merged_count, out_tags->merged, sizeof(out_tags->merged),
              "untagged");

  RenamerTagsTagListFree(manual_tags, manual_count);
  RenamerTagsTagListFree(meta_tags, meta_count);
  RenamerTagsTagListFree(merged, merged_count);
  return true;
}
