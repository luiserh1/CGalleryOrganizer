#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void test_cli_exhaustive_flag_and_cache_enrichment(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_exhaustive_src",
                       "build/test_cli_exhaustive_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_exhaustive_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_exhaustive_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_exif.jpg",
                              "build/test_cli_exhaustive_src/a.jpg"));

  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_exhaustive_src "
      "build/test_cli_exhaustive_env --exhaustive 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Exhaustive: ON") != NULL);

  FILE *f = fopen("build/test_cli_exhaustive_env/.cache/gallery_cache.json", "r");
  ASSERT_TRUE(f != NULL);
  char cache_buf[8192] = {0};
  size_t bytes = fread(cache_buf, 1, sizeof(cache_buf) - 1, f);
  cache_buf[bytes] = '\0';
  fclose(f);
  ASSERT_TRUE(strstr(cache_buf, "\"allMetadataJson\"") != NULL);

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_exhaustive_src",
                       "build/test_cli_exhaustive_env"},
      2));
}

void test_cli_group_by_empty_rejected(void) {
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_groupby_empty_env"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_groupby_empty_env"));
  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/jpg "
      "build/test_cli_groupby_empty_env --preview --group-by '' 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "cannot include empty keys") != NULL);
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_groupby_empty_env"));
}

void test_cli_group_by_invalid_rejected(void) {
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_groupby_invalid_env"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_groupby_invalid_env"));
  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/jpg "
      "build/test_cli_groupby_invalid_env --preview --group-by foo 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "Invalid --group-by key") != NULL);
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_groupby_invalid_env"));
}

void test_cli_rollback_single_positional(void) {
  const char *env_dir = "build/test_cli_rollback_single";
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_rollback_single"));
  ASSERT_TRUE(WriteRollbackManifest(env_dir));

  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rollback_single --rollback "
      "2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rollback complete") != NULL);

  FILE *f = fopen("build/test_cli_rollback_single/orig/file.jpg", "r");
  ASSERT_TRUE(f != NULL);
  if (f) {
    fclose(f);
  }

  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_rollback_single"));
}

void test_cli_rollback_legacy_two_positional(void) {
  const char *env_dir = "build/test_cli_rollback_legacy";
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_rollback_legacy"));
  ASSERT_TRUE(WriteRollbackManifest(env_dir));

  char output[4096];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer /nonexistent/path "
      "build/test_cli_rollback_legacy --rollback 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rollback complete") != NULL);

  FILE *f = fopen("build/test_cli_rollback_legacy/orig/file.jpg", "r");
  ASSERT_TRUE(f != NULL);
  if (f) {
    fclose(f);
  }

  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_rollback_legacy"));
}

void test_cli_jobs_validation(void) {
  char output[2048];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png build/test_cli_jobs_env "
      "--jobs 0 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output,
                     "--jobs/CGO_JOBS must be 'auto' or a positive integer") !=
              NULL);
}

void test_cli_jobs_env_and_override(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_jobs_src", "build/test_cli_jobs_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_jobs_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_jobs_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_cli_jobs_src/a.png"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "CGO_JOBS=3 ./build/bin/gallery_organizer build/test_cli_jobs_src "
      "build/test_cli_jobs_env 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Jobs: 3") != NULL);

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "CGO_JOBS=3 ./build/bin/gallery_organizer build/test_cli_jobs_src "
      "build/test_cli_jobs_env --jobs 1 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Jobs: 1") != NULL);

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_jobs_src", "build/test_cli_jobs_env"},
      2));
}

void test_cli_similarity_memory_mode_validation(void) {
  char output[2048];
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer tests/assets/png build/test_cli_sim_mode "
      "--similarity-report --sim-memory-mode invalid 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "--sim-memory-mode must be eager|chunked") != NULL);
}

void test_cli_scan_does_not_move_duplicates_without_flag(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_dup_scan_src",
                       "build/test_cli_dup_scan_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_dup_scan_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_dup_scan_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird.JPG",
                              "build/test_cli_dup_scan_src/bird.JPG"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird_duplicate.JPG",
                              "build/test_cli_dup_scan_src/bird_duplicate.JPG"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_dup_scan_src "
      "build/test_cli_dup_scan_env 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Moving copies") == NULL);
  ASSERT_TRUE(FileExists("build/test_cli_dup_scan_src/bird_duplicate.JPG"));
  ASSERT_FALSE(FileExists("build/test_cli_dup_scan_env/bird_duplicate.JPG"));

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_dup_scan_src",
                       "build/test_cli_dup_scan_env"},
      2));
}

void test_cli_duplicates_report_is_non_mutating(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_dup_report_src",
                       "build/test_cli_dup_report_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_dup_report_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_dup_report_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird.JPG",
                              "build/test_cli_dup_report_src/bird.JPG"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird_duplicate.JPG",
                              "build/test_cli_dup_report_src/bird_duplicate.JPG"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_dup_report_src "
      "build/test_cli_dup_report_env --duplicates-report 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "(dry-run)") != NULL);
  ASSERT_TRUE(FileExists("build/test_cli_dup_report_src/bird_duplicate.JPG"));
  ASSERT_FALSE(FileExists("build/test_cli_dup_report_env/bird_duplicate.JPG"));

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_dup_report_src",
                       "build/test_cli_dup_report_env"},
      2));
}

void test_cli_duplicates_move_requires_env(void) {
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_dup_move_req_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_dup_move_req_src"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird.JPG",
                              "build/test_cli_dup_move_req_src/bird.JPG"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird_duplicate.JPG",
                              "build/test_cli_dup_move_req_src/bird_duplicate.JPG"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_dup_move_req_src "
      "--duplicates-move 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "--duplicates-move requires an environment directory") !=
              NULL);

  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_cli_dup_move_req_src"));
}

void test_cli_duplicates_move_executes(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_dup_move_src",
                       "build/test_cli_dup_move_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_dup_move_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_dup_move_env"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird.JPG",
                              "build/test_cli_dup_move_src/bird.JPG"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/duplicates/bird_duplicate.JPG",
                              "build/test_cli_dup_move_src/bird_duplicate.JPG"));

  char output[4096] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_dup_move_src "
      "build/test_cli_dup_move_env --duplicates-move 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Successfully moved") != NULL);
  bool source_bird_exists = FileExists("build/test_cli_dup_move_src/bird.JPG");
  bool source_dup_exists =
      FileExists("build/test_cli_dup_move_src/bird_duplicate.JPG");
  ASSERT_FALSE(source_bird_exists && source_dup_exists);
  ASSERT_TRUE(FileExists("build/test_cli_dup_move_env/bird.JPG") ||
              FileExists("build/test_cli_dup_move_env/bird_duplicate.JPG"));

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_dup_move_src",
                       "build/test_cli_dup_move_env"},
      2));
}

void register_cli_core_tests(void) {
  register_test("test_cli_exhaustive_flag_and_cache_enrichment",
                test_cli_exhaustive_flag_and_cache_enrichment, "integration");
  register_test("test_cli_group_by_empty_rejected",
                test_cli_group_by_empty_rejected, "integration");
  register_test("test_cli_group_by_invalid_rejected",
                test_cli_group_by_invalid_rejected, "integration");
  register_test("test_cli_rollback_single_positional",
                test_cli_rollback_single_positional, "integration");
  register_test("test_cli_rollback_legacy_two_positional",
                test_cli_rollback_legacy_two_positional, "integration");
  register_test("test_cli_jobs_validation", test_cli_jobs_validation,
                "integration");
  register_test("test_cli_jobs_env_and_override",
                test_cli_jobs_env_and_override, "integration");
  register_test("test_cli_similarity_memory_mode_validation",
                test_cli_similarity_memory_mode_validation, "integration");
  register_test("test_cli_scan_does_not_move_duplicates_without_flag",
                test_cli_scan_does_not_move_duplicates_without_flag,
                "integration");
  register_test("test_cli_duplicates_report_is_non_mutating",
                test_cli_duplicates_report_is_non_mutating, "integration");
  register_test("test_cli_duplicates_move_requires_env",
                test_cli_duplicates_move_requires_env, "integration");
  register_test("test_cli_duplicates_move_executes",
                test_cli_duplicates_move_executes, "integration");
}
