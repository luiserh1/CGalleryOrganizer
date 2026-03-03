#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "gui/core/gui_rename_map.h"
#include "integration_test_helpers.h"
#include "test_framework.h"

static char *ReadFileText(const char *path) {
  if (!path) {
    return NULL;
  }

  FILE *f = fopen(path, "rb");
  if (!f) {
    return NULL;
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return NULL;
  }
  rewind(f);

  char *text = calloc((size_t)size + 1, 1);
  if (!text) {
    fclose(f);
    return NULL;
  }
  size_t read = fread(text, 1, (size_t)size, f);
  fclose(f);
  text[read] = '\0';
  return text;
}

void test_gui_rename_map_upsert_creates_and_replaces_entry(void) {
  ASSERT_TRUE(ResetDirForTest("build/test_gui_rename_map"));

  const char *map_path = "build/test_gui_rename_map/tags_map.json";
  const char *source_path = "/tmp/source_a.jpg";
  char error[512] = {0};

  ASSERT_TRUE(GuiRenameMapUpsertManualTags(map_path, source_path, "001, 002,001",
                                           error, sizeof(error)));

  char *json_text = ReadFileText(map_path);
  ASSERT_TRUE(json_text != NULL);
  cJSON *root = cJSON_Parse(json_text);
  free(json_text);
  ASSERT_TRUE(root != NULL);

  cJSON *files = cJSON_GetObjectItem(root, "files");
  ASSERT_TRUE(cJSON_IsObject(files));
  cJSON *entry = cJSON_GetObjectItem(files, source_path);
  ASSERT_TRUE(cJSON_IsObject(entry));
  cJSON *manual_tags = cJSON_GetObjectItem(entry, "manualTags");
  ASSERT_TRUE(cJSON_IsArray(manual_tags));
  ASSERT_EQ(2, cJSON_GetArraySize(manual_tags));
  ASSERT_STR_EQ("001", cJSON_GetArrayItem(manual_tags, 0)->valuestring);
  ASSERT_STR_EQ("002", cJSON_GetArrayItem(manual_tags, 1)->valuestring);
  ASSERT_TRUE(cJSON_IsArray(cJSON_GetObjectItem(entry, "metaTagAdds")));
  ASSERT_TRUE(cJSON_IsArray(cJSON_GetObjectItem(entry, "suppressedMetaTags")));
  cJSON_Delete(root);

  ASSERT_TRUE(GuiRenameMapUpsertManualTags(map_path, source_path, "alpha", error,
                                           sizeof(error)));

  json_text = ReadFileText(map_path);
  ASSERT_TRUE(json_text != NULL);
  root = cJSON_Parse(json_text);
  free(json_text);
  ASSERT_TRUE(root != NULL);

  files = cJSON_GetObjectItem(root, "files");
  entry = cJSON_GetObjectItem(files, source_path);
  manual_tags = cJSON_GetObjectItem(entry, "manualTags");
  ASSERT_TRUE(cJSON_IsArray(manual_tags));
  ASSERT_EQ(1, cJSON_GetArraySize(manual_tags));
  ASSERT_STR_EQ("alpha", cJSON_GetArrayItem(manual_tags, 0)->valuestring);
  cJSON_Delete(root);

  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_gui_rename_map"));
}

void test_gui_rename_map_upsert_metadata_tags_preserves_manual_tags(void) {
  ASSERT_TRUE(ResetDirForTest("build/test_gui_rename_map_meta"));

  const char *map_path = "build/test_gui_rename_map_meta/tags_map.json";
  const char *source_path = "/tmp/source_meta.jpg";
  char error[512] = {0};

  ASSERT_TRUE(GuiRenameMapUpsertManualTags(map_path, source_path, "frag-a", error,
                                           sizeof(error)));
  ASSERT_TRUE(GuiRenameMapUpsertMetadataTags(map_path, source_path,
                                             "meta-a,meta-b", error,
                                             sizeof(error)));

  char *json_text = ReadFileText(map_path);
  ASSERT_TRUE(json_text != NULL);
  cJSON *root = cJSON_Parse(json_text);
  free(json_text);
  ASSERT_TRUE(root != NULL);

  cJSON *files = cJSON_GetObjectItem(root, "files");
  ASSERT_TRUE(cJSON_IsObject(files));
  cJSON *entry = cJSON_GetObjectItem(files, source_path);
  ASSERT_TRUE(cJSON_IsObject(entry));

  cJSON *manual_tags = cJSON_GetObjectItem(entry, "manualTags");
  ASSERT_TRUE(cJSON_IsArray(manual_tags));
  ASSERT_EQ(1, cJSON_GetArraySize(manual_tags));
  ASSERT_STR_EQ("frag-a", cJSON_GetArrayItem(manual_tags, 0)->valuestring);

  cJSON *meta_adds = cJSON_GetObjectItem(entry, "metaTagAdds");
  ASSERT_TRUE(cJSON_IsArray(meta_adds));
  ASSERT_EQ(2, cJSON_GetArraySize(meta_adds));
  ASSERT_STR_EQ("meta-a", cJSON_GetArrayItem(meta_adds, 0)->valuestring);
  ASSERT_STR_EQ("meta-b", cJSON_GetArrayItem(meta_adds, 1)->valuestring);

  cJSON_Delete(root);
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_gui_rename_map_meta"));
}

void test_gui_rename_map_upsert_requires_paths(void) {
  char error[256] = {0};
  ASSERT_FALSE(GuiRenameMapUpsertManualTags(NULL, "/tmp/a.jpg", "001", error,
                                            sizeof(error)));
  ASSERT_TRUE(strstr(error, "required") != NULL);
}

void register_gui_rename_map_tests(void) {
  register_test("test_gui_rename_map_upsert_creates_and_replaces_entry",
                test_gui_rename_map_upsert_creates_and_replaces_entry, "unit");
  register_test("test_gui_rename_map_upsert_requires_paths",
                test_gui_rename_map_upsert_requires_paths, "unit");
  register_test("test_gui_rename_map_upsert_metadata_tags_preserves_manual_tags",
                test_gui_rename_map_upsert_metadata_tags_preserves_manual_tags,
                "unit");
}
