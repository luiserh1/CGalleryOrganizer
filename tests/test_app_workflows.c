#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_api.h"
#include "fs_utils.h"
#include "test_framework.h"
#include "integration_test_helpers.h"

static bool FileExists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  fclose(f);
  return true;
}

static int SumDuplicateCount(const AppDuplicateReport *report) {
  if (!report) {
    return 0;
  }

  int total = 0;
  for (int i = 0; i < report->group_count; i++) {
    total += report->groups[i].duplicate_count;
  }
  return total;
}

void test_app_duplicate_find_and_move_workflow(void) {
  system("rm -rf build/test_app_dup_workflow_src build/test_app_dup_workflow_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_dup_workflow_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_dup_workflow_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird.JPG",
                              "build/test_app_dup_workflow_src/bird.JPG"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird_duplicate.JPG",
                              "build/test_app_dup_workflow_src/bird_duplicate.JPG"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanRequest scan_request = {
      .target_dir = "build/test_app_dup_workflow_src",
      .env_dir = "build/test_app_dup_workflow_env",
      .jobs = 1,
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
      .sim_incremental = true,
  };
  AppScanResult scan_result = {0};
  AppStatus status = AppRunScan(ctx, &scan_request, &scan_result);
  ASSERT_EQ(APP_STATUS_OK, status);

  AppDuplicateFindRequest find_request = {
      .env_dir = "build/test_app_dup_workflow_env",
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
  };
  AppDuplicateReport report = {0};
  status = AppFindDuplicates(ctx, &find_request, &report);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(report.group_count >= 1);

  int expected_moved = SumDuplicateCount(&report);
  ASSERT_TRUE(expected_moved >= 1);

  AppDuplicateMoveRequest move_request = {
      .target_dir = "build/test_app_dup_workflow_env/duplicates",
      .report = &report,
  };
  AppDuplicateMoveResult move_result = {0};
  status = AppMoveDuplicates(ctx, &move_request, &move_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_EQ(expected_moved, move_result.moved_count);

  for (int i = 0; i < report.group_count; i++) {
    for (int j = 0; j < report.groups[i].duplicate_count; j++) {
      ASSERT_FALSE(FileExists(report.groups[i].duplicate_paths[j]));
    }
  }

  AppFreeDuplicateReport(&report);
  AppContextDestroy(ctx);
  system("rm -rf build/test_app_dup_workflow_src build/test_app_dup_workflow_env");
}

void test_app_organize_execute_and_rollback_workflow(void) {
  system("rm -rf build/test_app_org_workflow_src build/test_app_org_workflow_env");
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_org_workflow_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_app_org_workflow_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_exif.jpg",
                              "build/test_app_org_workflow_src/a.jpg"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_no_exif.jpg",
                              "build/test_app_org_workflow_src/b.jpg"));

  AppRuntimeOptions opts = {0};
  AppContext *ctx = AppContextCreate(&opts);
  ASSERT_TRUE(ctx != NULL);

  AppScanRequest scan_request = {
      .target_dir = "build/test_app_org_workflow_src",
      .env_dir = "build/test_app_org_workflow_env",
      .jobs = 1,
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
      .sim_incremental = true,
  };
  AppScanResult scan_result = {0};
  AppStatus status = AppRunScan(ctx, &scan_request, &scan_result);
  ASSERT_EQ(APP_STATUS_OK, status);

  AppOrganizePlanRequest preview_request = {
      .env_dir = "build/test_app_org_workflow_env",
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
      .group_by_arg = "date",
  };
  AppOrganizePlanResult preview_result = {0};
  status = AppPreviewOrganize(ctx, &preview_request, &preview_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_TRUE(preview_result.plan_text != NULL);
  AppFreeOrganizePlanResult(&preview_result);

  AppOrganizeExecuteRequest execute_request = {
      .env_dir = "build/test_app_org_workflow_env",
      .cache_compression_mode = APP_CACHE_COMPRESSION_NONE,
      .cache_compression_level = 3,
      .group_by_arg = "date",
  };
  AppOrganizeExecuteResult execute_result = {0};
  status = AppExecuteOrganize(ctx, &execute_request, &execute_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_EQ(execute_result.planned_moves, execute_result.executed_moves);
  ASSERT_TRUE(FileExists("build/test_app_org_workflow_env/manifest.json"));

  AppRollbackRequest rollback_request = {
      .env_dir = "build/test_app_org_workflow_env",
  };
  AppRollbackResult rollback_result = {0};
  status = AppRollback(ctx, &rollback_request, &rollback_result);
  ASSERT_EQ(APP_STATUS_OK, status);
  ASSERT_EQ(execute_result.executed_moves, rollback_result.restored_count);

  AppContextDestroy(ctx);
  system("rm -rf build/test_app_org_workflow_src build/test_app_org_workflow_env");
}

void register_app_workflow_tests(void) {
  register_test("test_app_duplicate_find_and_move_workflow",
                test_app_duplicate_find_and_move_workflow, "integration");
  register_test("test_app_organize_execute_and_rollback_workflow",
                test_app_organize_execute_and_rollback_workflow, "integration");
}
