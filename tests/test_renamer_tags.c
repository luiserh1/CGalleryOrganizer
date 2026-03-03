#include <string.h>

#include "cJSON.h"
#include "systems/renamer_tags.h"
#include "test_framework.h"

static cJSON *CreateTagSidecarRoot(void) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return NULL;
  }
  cJSON_AddNumberToObject(root, "version", 1);
  cJSON_AddItemToObject(root, "files", cJSON_CreateObject());
  return root;
}

static bool FieldListContains(
    char fields[][RENAMER_META_FIELD_KEY_MAX], int count, const char *candidate) {
  if (!fields || count <= 0 || !candidate || candidate[0] == '\0') {
    return false;
  }
  for (int i = 0; i < count; i++) {
    if (strcmp(fields[i], candidate) == 0) {
      return true;
    }
  }
  return false;
}

void test_renamer_tags_metadata_add_remove_and_resolve(void) {
  cJSON *root = CreateTagSidecarRoot();
  ASSERT_TRUE(root != NULL);

  const char *paths[] = {"/tmp/renamer_tags_a.jpg"};
  char error[256] = {0};
  ASSERT_TRUE(RenamerTagsApplyBulkCsv(root, paths, 1, "manual-a", NULL,
                                      "meta-a,meta-b", NULL, error,
                                      sizeof(error)));

  const char *metadata_json =
      "{\"Iptc.Application2.Keywords\":[\"meta-z\"],"
      "\"Xmp.dc.subject\":\"meta-solo\"}";
  RenamerResolvedTags tags = {0};
  ASSERT_TRUE(RenamerTagsResolve(root, paths[0], metadata_json, &tags, error,
                                 sizeof(error)));
  ASSERT_STR_EQ("manual-a", tags.manual);
  ASSERT_STR_EQ("meta-z-meta-solo-meta-a-meta-b", tags.meta);
  ASSERT_STR_EQ("manual-a-meta-z-meta-solo-meta-a-meta-b", tags.merged);

  ASSERT_TRUE(RenamerTagsApplyBulkCsv(root, paths, 1, NULL, NULL, NULL, "meta-a",
                                      error, sizeof(error)));
  memset(&tags, 0, sizeof(tags));
  ASSERT_TRUE(RenamerTagsResolve(root, paths[0], metadata_json, &tags, error,
                                 sizeof(error)));
  ASSERT_STR_EQ("meta-z-meta-solo-meta-b", tags.meta);
  ASSERT_STR_EQ("manual-a-meta-z-meta-solo-meta-b", tags.merged);

  cJSON_Delete(root);
}

void test_renamer_tags_collect_metadata_fields_whitelist_only(void) {
  char fields[RENAMER_META_FIELD_MAX][RENAMER_META_FIELD_KEY_MAX] = {{0}};
  int field_count = 0;
  char error[256] = {0};

  ASSERT_TRUE(RenamerTagsCollectMetadataFields(
      "{\"Iptc.Application2.Keywords\":[\"a\"],"
      "\"Xmp.lr.hierarchicalSubject\":[\"b\"],"
      "\"Custom.Field\":\"ignore\"}",
      fields, RENAMER_META_FIELD_MAX, &field_count, error, sizeof(error)));
  ASSERT_EQ(2, field_count);
  ASSERT_TRUE(FieldListContains(fields, field_count, "Iptc.Application2.Keywords"));
  ASSERT_TRUE(FieldListContains(fields, field_count, "Xmp.lr.hierarchicalSubject"));
  ASSERT_FALSE(FieldListContains(fields, field_count, "Custom.Field"));

  ASSERT_TRUE(RenamerTagsCollectMetadataFields(
      "{\"Iptc.Application2.Keywords\":[\"x\"],"
      "\"Xmp.dc.subject\":[\"y\"]}",
      fields, RENAMER_META_FIELD_MAX, &field_count, error, sizeof(error)));
  ASSERT_EQ(3, field_count);
  ASSERT_TRUE(FieldListContains(fields, field_count, "Xmp.dc.subject"));
}

void register_renamer_tags_tests(void) {
  register_test("test_renamer_tags_metadata_add_remove_and_resolve",
                test_renamer_tags_metadata_add_remove_and_resolve, "unit");
  register_test("test_renamer_tags_collect_metadata_fields_whitelist_only",
                test_renamer_tags_collect_metadata_fields_whitelist_only, "unit");
}
