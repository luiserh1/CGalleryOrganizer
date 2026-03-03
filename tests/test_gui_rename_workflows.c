#include <stdio.h>
#include <string.h>
#include <time.h>

#include "app_api.h"
#include "fs_utils.h"
#include "gui/core/gui_rename_map.h"
#include "gui/gui_worker.h"
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

static bool WaitForWorkerResult(GuiTaskSnapshot *out_snapshot, int timeout_ms) {
  if (!out_snapshot || timeout_ms <= 0) {
    return false;
  }

  struct timespec sleep_step = {.tv_sec = 0, .tv_nsec = 10 * 1000 * 1000};
  int waited_ms = 0;

  while (waited_ms <= timeout_ms) {
    GuiWorkerGetSnapshot(out_snapshot);
    if (out_snapshot->has_result && !out_snapshot->busy) {
      return true;
    }

    nanosleep(&sleep_step, NULL);
    waited_ms += 10;
  }

  GuiWorkerGetSnapshot(out_snapshot);
  return false;
}

static bool RunWorkerTask(const GuiTaskInput *input, GuiTaskSnapshot *out_snapshot) {
  if (!input || !out_snapshot) {
    return false;
  }

  if (!GuiWorkerStartTask(input)) {
    return false;
  }
  if (!WaitForWorkerResult(out_snapshot, 15000)) {
    GuiWorkerRequestCancel();
    return false;
  }

  GuiWorkerClearResult();
  return true;
}

static bool SetupWorker(AppContext **out_ctx) {
  if (!out_ctx) {
    return false;
  }
  *out_ctx = NULL;

  AppRuntimeOptions options = {0};
  AppContext *ctx = AppContextCreate(&options);
  if (!ctx) {
    return false;
  }
  if (!GuiWorkerInit(ctx)) {
    AppContextDestroy(ctx);
    return false;
  }

  *out_ctx = ctx;
  return true;
}

static void TeardownWorker(AppContext *ctx) {
  GuiWorkerShutdown();
  if (ctx) {
    AppContextDestroy(ctx);
  }
}

static void InitTaskInput(GuiTaskInput *input, GuiTaskType type,
                          const char *gallery_dir, const char *env_dir) {
  memset(input, 0, sizeof(*input));
  input->type = type;
  if (gallery_dir) {
    strncpy(input->gallery_dir, gallery_dir, sizeof(input->gallery_dir) - 1);
    input->gallery_dir[sizeof(input->gallery_dir) - 1] = '\0';
  }
  if (env_dir) {
    strncpy(input->env_dir, env_dir, sizeof(input->env_dir) - 1);
    input->env_dir[sizeof(input->env_dir) - 1] = '\0';
  }
}

void test_gui_rename_workflow_preview_apply_history_rollback(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_gui_rename_flow_src",
                       "build/test_gui_rename_flow_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_gui_rename_flow_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_gui_rename_flow_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_gui_rename_flow_src/a.png"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_gui_rename_flow_src/b.png"));

  AppContext *ctx = NULL;
  ASSERT_TRUE(SetupWorker(&ctx));

  GuiTaskInput preview_input = {0};
  InitTaskInput(&preview_input, GUI_TASK_RENAME_PREVIEW,
                "build/test_gui_rename_flow_src", "build/test_gui_rename_flow_env");
  strncpy(preview_input.rename_pattern, "pic-[index].[format]",
          sizeof(preview_input.rename_pattern) - 1);

  GuiTaskSnapshot preview_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&preview_input, &preview_snapshot));
  ASSERT_TRUE(preview_snapshot.success);
  ASSERT_EQ(APP_STATUS_OK, preview_snapshot.status);
  ASSERT_TRUE(preview_snapshot.rename_preview_id[0] != '\0');
  ASSERT_EQ(2, preview_snapshot.rename_preview_row_count);

  GuiTaskInput apply_input = {0};
  InitTaskInput(&apply_input, GUI_TASK_RENAME_APPLY, NULL,
                "build/test_gui_rename_flow_env");
  strncpy(apply_input.rename_preview_id, preview_snapshot.rename_preview_id,
          sizeof(apply_input.rename_preview_id) - 1);

  GuiTaskSnapshot apply_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&apply_input, &apply_snapshot));
  ASSERT_TRUE(apply_snapshot.success);
  ASSERT_EQ(APP_STATUS_OK, apply_snapshot.status);
  ASSERT_TRUE(apply_snapshot.rename_apply_result.operation_id[0] != '\0');
  ASSERT_EQ(2, apply_snapshot.rename_apply_result.renamed_count);

  char operation_id[64] = {0};
  strncpy(operation_id, apply_snapshot.rename_apply_result.operation_id,
          sizeof(operation_id) - 1);
  operation_id[sizeof(operation_id) - 1] = '\0';

  GuiTaskInput history_input = {0};
  InitTaskInput(&history_input, GUI_TASK_RENAME_HISTORY, NULL,
                "build/test_gui_rename_flow_env");

  GuiTaskSnapshot history_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&history_input, &history_snapshot));
  ASSERT_TRUE(history_snapshot.success);
  ASSERT_EQ(APP_STATUS_OK, history_snapshot.status);
  ASSERT_TRUE(history_snapshot.rename_history_count >= 1);
  ASSERT_TRUE(strstr(history_snapshot.detail_text, operation_id) != NULL);

  GuiTaskInput rollback_input = {0};
  InitTaskInput(&rollback_input, GUI_TASK_RENAME_ROLLBACK, NULL,
                "build/test_gui_rename_flow_env");
  strncpy(rollback_input.rename_operation_id, operation_id,
          sizeof(rollback_input.rename_operation_id) - 1);

  GuiTaskSnapshot rollback_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&rollback_input, &rollback_snapshot));
  ASSERT_TRUE(rollback_snapshot.success);
  ASSERT_EQ(APP_STATUS_OK, rollback_snapshot.status);
  ASSERT_EQ(2, rollback_snapshot.rename_rollback_result.restored_count);

  ASSERT_TRUE(FileExists("build/test_gui_rename_flow_src/a.png"));
  ASSERT_TRUE(FileExists("build/test_gui_rename_flow_src/b.png"));

  TeardownWorker(ctx);
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_gui_rename_flow_src",
                       "build/test_gui_rename_flow_env"},
      2));
}

void test_gui_rename_workflow_collision_guard_requires_acceptance(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_gui_rename_collision_src",
                       "build/test_gui_rename_collision_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_gui_rename_collision_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_gui_rename_collision_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_gui_rename_collision_src/a.png"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_gui_rename_collision_src/b.png"));

  AppContext *ctx = NULL;
  ASSERT_TRUE(SetupWorker(&ctx));

  GuiTaskInput preview_input = {0};
  InitTaskInput(&preview_input, GUI_TASK_RENAME_PREVIEW,
                "build/test_gui_rename_collision_src",
                "build/test_gui_rename_collision_env");
  strncpy(preview_input.rename_pattern, "same.[format]",
          sizeof(preview_input.rename_pattern) - 1);

  GuiTaskSnapshot preview_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&preview_input, &preview_snapshot));
  ASSERT_TRUE(preview_snapshot.success);
  ASSERT_TRUE(preview_snapshot.rename_preview_requires_auto_suffix);
  ASSERT_TRUE(preview_snapshot.rename_preview_collision_count > 0);

  GuiTaskInput apply_without_accept = {0};
  InitTaskInput(&apply_without_accept, GUI_TASK_RENAME_APPLY, NULL,
                "build/test_gui_rename_collision_env");
  strncpy(apply_without_accept.rename_preview_id, preview_snapshot.rename_preview_id,
          sizeof(apply_without_accept.rename_preview_id) - 1);
  apply_without_accept.rename_accept_auto_suffix = false;

  GuiTaskSnapshot apply_fail_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&apply_without_accept, &apply_fail_snapshot));
  ASSERT_FALSE(apply_fail_snapshot.success);
  ASSERT_TRUE(apply_fail_snapshot.status != APP_STATUS_OK);
  ASSERT_TRUE(strstr(apply_fail_snapshot.message, "collision") != NULL ||
              strstr(apply_fail_snapshot.message, "auto-suffix") != NULL);

  GuiTaskInput apply_with_accept = apply_without_accept;
  apply_with_accept.rename_accept_auto_suffix = true;
  GuiTaskSnapshot apply_ok_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&apply_with_accept, &apply_ok_snapshot));
  ASSERT_TRUE(apply_ok_snapshot.success);
  ASSERT_EQ(APP_STATUS_OK, apply_ok_snapshot.status);
  ASSERT_EQ(2, apply_ok_snapshot.rename_apply_result.renamed_count);

  TeardownWorker(ctx);
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_gui_rename_collision_src",
                       "build/test_gui_rename_collision_env"},
      2));
}

void test_gui_rename_workflow_invalid_tags_map_path(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_gui_rename_badmap_src",
                       "build/test_gui_rename_badmap_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_gui_rename_badmap_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_gui_rename_badmap_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_no_exif.jpg",
                              "build/test_gui_rename_badmap_src/a.jpg"));

  AppContext *ctx = NULL;
  ASSERT_TRUE(SetupWorker(&ctx));

  GuiTaskInput preview_input = {0};
  InitTaskInput(&preview_input, GUI_TASK_RENAME_PREVIEW,
                "build/test_gui_rename_badmap_src",
                "build/test_gui_rename_badmap_env");
  strncpy(preview_input.rename_tags_map_path,
          "build/test_gui_rename_badmap_env/missing_tags_map.json",
          sizeof(preview_input.rename_tags_map_path) - 1);

  GuiTaskSnapshot snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&preview_input, &snapshot));
  ASSERT_FALSE(snapshot.success);
  ASSERT_TRUE(snapshot.status != APP_STATUS_OK);

  TeardownWorker(ctx);
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_gui_rename_badmap_src",
                       "build/test_gui_rename_badmap_env"},
      2));
}

void test_gui_rename_workflow_missing_ids_fail(void) {
  ASSERT_TRUE(RemovePathsForTest((const char *[]){"build/test_gui_rename_missing_ids_env"},
                                 1));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_gui_rename_missing_ids_env"));

  AppContext *ctx = NULL;
  ASSERT_TRUE(SetupWorker(&ctx));

  GuiTaskInput apply_input = {0};
  InitTaskInput(&apply_input, GUI_TASK_RENAME_APPLY, NULL,
                "build/test_gui_rename_missing_ids_env");
  GuiTaskSnapshot apply_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&apply_input, &apply_snapshot));
  ASSERT_FALSE(apply_snapshot.success);
  ASSERT_TRUE(apply_snapshot.status != APP_STATUS_OK);

  GuiTaskInput rollback_input = {0};
  InitTaskInput(&rollback_input, GUI_TASK_RENAME_ROLLBACK, NULL,
                "build/test_gui_rename_missing_ids_env");
  GuiTaskSnapshot rollback_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&rollback_input, &rollback_snapshot));
  ASSERT_FALSE(rollback_snapshot.success);
  ASSERT_TRUE(rollback_snapshot.status != APP_STATUS_OK);

  TeardownWorker(ctx);
  ASSERT_TRUE(RemovePathsForTest((const char *[]){"build/test_gui_rename_missing_ids_env"},
                                 1));
}

void test_gui_rename_workflow_tag_update_refreshes_preview(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_gui_rename_tagrefresh_src",
                       "build/test_gui_rename_tagrefresh_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_gui_rename_tagrefresh_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_gui_rename_tagrefresh_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_gui_rename_tagrefresh_src/a.png"));

  AppContext *ctx = NULL;
  ASSERT_TRUE(SetupWorker(&ctx));

  GuiTaskInput preview_input = {0};
  InitTaskInput(&preview_input, GUI_TASK_RENAME_PREVIEW,
                "build/test_gui_rename_tagrefresh_src",
                "build/test_gui_rename_tagrefresh_env");
  strncpy(preview_input.rename_pattern, "pic-[tags]-[camera].[format]",
          sizeof(preview_input.rename_pattern) - 1);

  GuiTaskSnapshot initial_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&preview_input, &initial_snapshot));
  ASSERT_TRUE(initial_snapshot.success);
  ASSERT_EQ(1, initial_snapshot.rename_preview_row_count);
  ASSERT_TRUE(strstr(initial_snapshot.rename_preview_rows[0].tags_manual,
                     "untagged") != NULL);

  char map_path[1024] = {0};
  snprintf(map_path, sizeof(map_path),
           "build/test_gui_rename_tagrefresh_env/tags_map.json");
  char error[APP_MAX_ERROR] = {0};
  ASSERT_TRUE(GuiRenameMapUpsertManualTags(
      map_path, initial_snapshot.rename_preview_rows[0].source_path, "frag-42",
      error, sizeof(error)));

  GuiTaskInput refresh_input = preview_input;
  strncpy(refresh_input.rename_tags_map_path, map_path,
          sizeof(refresh_input.rename_tags_map_path) - 1);

  GuiTaskSnapshot refreshed_snapshot = {0};
  ASSERT_TRUE(RunWorkerTask(&refresh_input, &refreshed_snapshot));
  ASSERT_TRUE(refreshed_snapshot.success);
  ASSERT_EQ(1, refreshed_snapshot.rename_preview_row_count);
  ASSERT_TRUE(strstr(refreshed_snapshot.rename_preview_rows[0].tags_manual,
                     "frag-42") != NULL);
  ASSERT_TRUE(strstr(refreshed_snapshot.rename_preview_rows[0].candidate_name,
                     "frag-42") != NULL);

  TeardownWorker(ctx);
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_gui_rename_tagrefresh_src",
                       "build/test_gui_rename_tagrefresh_env"},
      2));
}

void register_gui_rename_workflow_tests(void) {
  register_test("test_gui_rename_workflow_preview_apply_history_rollback",
                test_gui_rename_workflow_preview_apply_history_rollback,
                "integration");
  register_test("test_gui_rename_workflow_collision_guard_requires_acceptance",
                test_gui_rename_workflow_collision_guard_requires_acceptance,
                "integration");
  register_test("test_gui_rename_workflow_invalid_tags_map_path",
                test_gui_rename_workflow_invalid_tags_map_path, "integration");
  register_test("test_gui_rename_workflow_missing_ids_fail",
                test_gui_rename_workflow_missing_ids_fail, "integration");
  register_test("test_gui_rename_workflow_tag_update_refreshes_preview",
                test_gui_rename_workflow_tag_update_refreshes_preview,
                "integration");
}
