#include <stdio.h>
#include <string.h>

#include "app_api.h"
#include "cJSON.h"
#include "fs_utils.h"
#include "integration_test_helpers.h"
#include "test_framework.h"

static bool FileExists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  fclose(f);
  return true;
}

static bool FileContainsText(const char *path, const char *needle) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  char buffer[32768] = {0};
  size_t read_bytes = fread(buffer, 1, sizeof(buffer) - 1, f);
  fclose(f);
  buffer[read_bytes] = '\0';

  return strstr(buffer, needle) != NULL;
}

static bool ExtractFirstPreviewTagsMeta(const char *details_json, char *out_tags,
                                        size_t out_tags_size) {
  if (!details_json || !out_tags || out_tags_size == 0) {
    return false;
  }

  out_tags[0] = '\0';
  cJSON *root = cJSON_Parse(details_json);
  if (!root) {
    return false;
  }

  cJSON *items = cJSON_GetObjectItem(root, "items");
  if (!cJSON_IsArray(items) || cJSON_GetArraySize(items) <= 0) {
    cJSON_Delete(root);
    return false;
  }

  cJSON *first = cJSON_GetArrayItem(items, 0);
  cJSON *tags_meta = cJSON_GetObjectItem(first, "tagsMeta");
  if (!cJSON_IsString(tags_meta) || !tags_meta->valuestring) {
    cJSON_Delete(root);
    return false;
  }

  strncpy(out_tags, tags_meta->valuestring, out_tags_size - 1);
  out_tags[out_tags_size - 1] = '\0';
  cJSON_Delete(root);
  return true;
}

void test_app_rename_preview_apply_history_and_rollback(void) {
  const char *src_dir = "build/test_app_rename_src";
  const char *env_dir = "build/test_app_rename_env";

  ASSERT_TRUE(RemovePathsForTest((const char *[]){src_dir, env_dir}, 2));
  ASSERT_TRUE(FsMakeDirRecursive(src_dir));
  ASSERT_TRUE(FsMakeDirRecursive(env_dir));

  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_exif.jpg",
                              "build/test_app_rename_src/a.jpg"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_no_exif.jpg",
                              "build/test_app_rename_src/b.jpg"));

  char abs_a[1024] = {0};
  char abs_b[1024] = {0};
  ASSERT_TRUE(FsGetAbsolutePath("build/test_app_rename_src/a.jpg", abs_a,
                                sizeof(abs_a)));
  ASSERT_TRUE(FsGetAbsolutePath("build/test_app_rename_src/b.jpg", abs_b,
                                sizeof(abs_b)));

  AppContext *ctx = AppContextCreate(&(AppRuntimeOptions){0});
  ASSERT_TRUE(ctx != NULL);

  AppRenamePreviewRequest preview_request = {
      .target_dir = src_dir,
      .env_dir = env_dir,
      .pattern = "ren-[index].[format]",
      .tag_add_csv = "fragment-a",
  };
  AppRenamePreviewResult preview_result = {0};

  AppStatus status = AppPreviewRename(ctx, &preview_request, &preview_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(preview_result.preview_id[0] != '\0');
  ASSERT_EQ(2, preview_result.file_count);
  ASSERT_TRUE(preview_result.details_json != NULL);

  char preview_id[64] = {0};
  strncpy(preview_id, preview_result.preview_id, sizeof(preview_id) - 1);

  AppRenameApplyRequest apply_request = {
      .env_dir = env_dir,
      .preview_id = preview_id,
      .accept_auto_suffix = true,
  };
  AppRenameApplyResult apply_result = {0};
  status = AppApplyRename(ctx, &apply_request, &apply_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(apply_result.operation_id[0] != '\0');
  ASSERT_EQ(2, apply_result.renamed_count);

  char renamed_1[1024] = {0};
  char renamed_2[1024] = {0};
  snprintf(renamed_1, sizeof(renamed_1), "%s/ren-1.jpg", src_dir);
  snprintf(renamed_2, sizeof(renamed_2), "%s/ren-2.jpg", src_dir);
  ASSERT_TRUE(FileExists(renamed_1));
  ASSERT_TRUE(FileExists(renamed_2));

  AppRenameHistoryEntry *history = NULL;
  int history_count = 0;
  status = AppListRenameHistory(ctx, env_dir, &history, &history_count);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(history_count >= 1);
  ASSERT_STR_EQ(apply_result.operation_id, history[0].operation_id);
  AppFreeRenameHistoryEntries(history);

  char sidecar_path[1024] = {0};
  snprintf(sidecar_path, sizeof(sidecar_path),
           "%s/.cache/rename_tags.json", env_dir);
  char abs_renamed_1[1024] = {0};
  char abs_renamed_2[1024] = {0};
  ASSERT_TRUE(FsGetAbsolutePath(renamed_1, abs_renamed_1, sizeof(abs_renamed_1)));
  ASSERT_TRUE(FsGetAbsolutePath(renamed_2, abs_renamed_2, sizeof(abs_renamed_2)));
  ASSERT_TRUE(FileContainsText(sidecar_path, abs_renamed_1) ||
              FileContainsText(sidecar_path, abs_renamed_2));

  AppRenameRollbackRequest rollback_request = {
      .env_dir = env_dir,
      .operation_id = apply_result.operation_id,
  };
  AppRenameRollbackResult rollback_result = {0};
  status = AppRollbackRename(ctx, &rollback_request, &rollback_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(rollback_result.restored_count >= 1);

  ASSERT_TRUE(FileExists("build/test_app_rename_src/a.jpg"));
  ASSERT_TRUE(FileExists("build/test_app_rename_src/b.jpg"));
  ASSERT_TRUE(FileContainsText(sidecar_path, abs_a));
  ASSERT_TRUE(FileContainsText(sidecar_path, abs_b));

  AppFreeRenamePreviewResult(&preview_result);
  AppContextDestroy(ctx);
  ASSERT_TRUE(RemovePathsForTest((const char *[]){src_dir, env_dir}, 2));
}

void test_app_rename_apply_requires_collision_acceptance(void) {
  const char *src_dir = "build/test_app_rename_collision_src";
  const char *env_dir = "build/test_app_rename_collision_env";

  ASSERT_TRUE(RemovePathsForTest((const char *[]){src_dir, env_dir}, 2));
  ASSERT_TRUE(FsMakeDirRecursive(src_dir));
  ASSERT_TRUE(FsMakeDirRecursive(env_dir));

  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_app_rename_collision_src/a.png"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_app_rename_collision_src/b.png"));

  AppContext *ctx = AppContextCreate(&(AppRuntimeOptions){0});
  ASSERT_TRUE(ctx != NULL);

  AppRenamePreviewRequest preview_request = {
      .target_dir = src_dir,
      .env_dir = env_dir,
      .pattern = "same.[format]",
  };
  AppRenamePreviewResult preview_result = {0};
  AppStatus status = AppPreviewRename(ctx, &preview_request, &preview_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(preview_result.requires_auto_suffix_acceptance);

  AppRenameApplyRequest apply_request = {
      .env_dir = env_dir,
      .preview_id = preview_result.preview_id,
      .accept_auto_suffix = false,
  };
  AppRenameApplyResult apply_result = {0};
  status = AppApplyRename(ctx, &apply_request, &apply_result);
  ASSERT_EQ(APP_STATUS_INVALID_ARGUMENT, status);

  apply_request.accept_auto_suffix = true;
  status = AppApplyRename(ctx, &apply_request, &apply_result);
  ASSERT_EQ(APP_STATUS_OK, status);

  AppFreeRenamePreviewResult(&preview_result);
  AppContextDestroy(ctx);
  ASSERT_TRUE(RemovePathsForTest((const char *[]){src_dir, env_dir}, 2));
}

void test_app_rename_preview_metadata_tag_edits(void) {
  const char *src_dir = "build/test_app_rename_meta_src";
  const char *env_dir = "build/test_app_rename_meta_env";

  ASSERT_TRUE(RemovePathsForTest((const char *[]){src_dir, env_dir}, 2));
  ASSERT_TRUE(FsMakeDirRecursive(src_dir));
  ASSERT_TRUE(FsMakeDirRecursive(env_dir));

  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_no_exif.jpg",
                              "build/test_app_rename_meta_src/a.jpg"));

  AppContext *ctx = AppContextCreate(&(AppRuntimeOptions){0});
  ASSERT_TRUE(ctx != NULL);

  AppRenamePreviewRequest preview_add = {
      .target_dir = src_dir,
      .env_dir = env_dir,
      .pattern = "meta-[tags_meta].[format]",
      .meta_tag_add_csv = "frag-a,frag-b",
  };
  AppRenamePreviewResult preview_add_result = {0};
  AppStatus status = AppPreviewRename(ctx, &preview_add, &preview_add_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(preview_add_result.details_json != NULL);
  char tags_meta[256] = {0};
  ASSERT_TRUE(ExtractFirstPreviewTagsMeta(preview_add_result.details_json, tags_meta,
                                          sizeof(tags_meta)));
  ASSERT_STR_EQ("frag-a-frag-b", tags_meta);
  ASSERT_TRUE(strstr(preview_add_result.details_json, "metadataTagFields") != NULL);

  AppRenamePreviewRequest preview_remove = {
      .target_dir = src_dir,
      .env_dir = env_dir,
      .pattern = "meta-[tags_meta].[format]",
      .meta_tag_remove_csv = "frag-a",
  };
  AppRenamePreviewResult preview_remove_result = {0};
  status = AppPreviewRename(ctx, &preview_remove, &preview_remove_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  memset(tags_meta, 0, sizeof(tags_meta));
  ASSERT_TRUE(ExtractFirstPreviewTagsMeta(preview_remove_result.details_json, tags_meta,
                                          sizeof(tags_meta)));
  ASSERT_STR_EQ("frag-b", tags_meta);

  AppFreeRenamePreviewResult(&preview_add_result);
  AppFreeRenamePreviewResult(&preview_remove_result);
  AppContextDestroy(ctx);
  ASSERT_TRUE(RemovePathsForTest((const char *[]){src_dir, env_dir}, 2));
}

void register_app_rename_tests(void) {
  register_test("test_app_rename_preview_apply_history_and_rollback",
                test_app_rename_preview_apply_history_and_rollback,
                "integration");
  register_test("test_app_rename_apply_requires_collision_acceptance",
                test_app_rename_apply_requires_collision_acceptance,
                "integration");
  register_test("test_app_rename_preview_metadata_tag_edits",
                test_app_rename_preview_metadata_tag_edits, "integration");
}
