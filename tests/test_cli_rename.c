#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "fs_utils.h"
#include "integration_test_helpers.h"
#include "test_framework.h"

static bool ExtractTokenAfterPrefix(const char *text, const char *prefix,
                                    char *out_token, size_t out_size);

static bool FileExists(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  fclose(f);
  return true;
}

static bool DirExists(const char *path) {
  struct stat st;
  return path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
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

void test_cli_rename_init_bootstrap_preview_json_and_apply_latest(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_v061_src",
                       "build/test_cli_rename_v061_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_v061_src"));

  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_no_exif.jpg",
                              "build/test_cli_rename_v061_src/001_alpha.jpg"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_no_exif.jpg",
                              "build/test_cli_rename_v061_src/002_003_beta.jpg"));

  char output[32768] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_v061_src "
      "build/test_cli_rename_v061_env --rename-init 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rename init ready") != NULL);
  ASSERT_TRUE(DirExists("build/test_cli_rename_v061_env/.cache/rename_previews"));
  ASSERT_TRUE(DirExists("build/test_cli_rename_v061_env/.cache/rename_history"));

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_v061_src "
      "build/test_cli_rename_v061_env "
      "--rename-bootstrap-tags-from-filename 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rename tag bootstrap completed") != NULL);
  ASSERT_TRUE(
      FileExists("build/test_cli_rename_v061_env/.cache/rename_tags_bootstrap.json"));

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_v061_src "
      "build/test_cli_rename_v061_env --rename-preview "
      "--rename-pattern 'pic-[tags]-[camera].[format]' "
      "--rename-tags-map build/test_cli_rename_v061_env/.cache/rename_tags_bootstrap.json "
      "--rename-preview-json-out build/test_cli_rename_v061_env/preview.json 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rename preview ready") != NULL);
  ASSERT_TRUE(strstr(output, "Preview JSON saved:") != NULL);
  ASSERT_TRUE(strstr(output, "--- Preview Details (JSON) ---") == NULL);
  ASSERT_TRUE(FileContainsText("build/test_cli_rename_v061_env/preview.json",
                               "\"previewId\""));

  char preview_id[128] = {0};
  ASSERT_TRUE(ExtractTokenAfterPrefix(output, "Preview ID:", preview_id,
                                      sizeof(preview_id)));

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_v061_env "
      "--rename-apply-latest 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Resolved latest preview id:") != NULL);
  ASSERT_TRUE(strstr(output, preview_id) != NULL);
  ASSERT_TRUE(strstr(output, "Rename apply completed") != NULL);

  ASSERT_TRUE(
      FileExists("build/test_cli_rename_v061_src/pic-001-unknown-camera.jpg"));
  ASSERT_TRUE(
      FileExists("build/test_cli_rename_v061_src/pic-002-003-unknown-camera.jpg"));

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_v061_src",
                       "build/test_cli_rename_v061_env"},
      2));
}

void test_cli_rename_preview_path_suggestion(void) {
  ASSERT_TRUE(RemovePathsForTest((const char *[]){"build/test_cli_rename_hint_root",
                                                  "build/test_cli_rename_hint_env"},
                                 2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_hint_root/Destacadas"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_hint_env"));

  ASSERT_TRUE(CopyFileForTest(
      "tests/assets/jpg/sample_no_exif.jpg",
      "build/test_cli_rename_hint_root/Destacadas/001.jpg"));

  char output[8192] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer "
      "build/test_cli_rename_hint_root/Destacados "
      "build/test_cli_rename_hint_env --rename-preview 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "does not exist") != NULL);
  ASSERT_TRUE(strstr(output, "did you mean") != NULL);
  ASSERT_TRUE(strstr(output, "Destacadas") != NULL);

  ASSERT_TRUE(RemovePathsForTest((const char *[]){"build/test_cli_rename_hint_root",
                                                  "build/test_cli_rename_hint_env"},
                                 2));
}

static bool ExtractTokenAfterPrefix(const char *text, const char *prefix,
                                    char *out_token, size_t out_size) {
  if (!text || !prefix || !out_token || out_size == 0) {
    return false;
  }

  out_token[0] = '\0';
  const char *start = strstr(text, prefix);
  if (!start) {
    return false;
  }

  start += strlen(prefix);
  while (*start == ' ' || *start == '\t') {
    start++;
  }

  size_t used = 0;
  while (start[used] != '\0' && start[used] != '\n' && start[used] != '\r' &&
         start[used] != ' ' && start[used] != '\t') {
    if (used + 1 >= out_size) {
      return false;
    }
    out_token[used] = start[used];
    used++;
  }
  out_token[used] = '\0';
  return used > 0;
}

void test_cli_rename_preview_apply_handshake_and_rollback(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_src", "build/test_cli_rename_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_env"));

  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_cli_rename_src/a.png"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_cli_rename_src/b.png"));

  char output[16384] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_src "
      "build/test_cli_rename_env --rename-preview "
      "--rename-pattern 'same.[format]' 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Preview ID:") != NULL);

  char preview_id[128] = {0};
  ASSERT_TRUE(ExtractTokenAfterPrefix(output, "Preview ID:", preview_id,
                                      sizeof(preview_id)));

  memset(output, 0, sizeof(output));
  char apply_cmd_no_accept[2048] = {0};
  snprintf(apply_cmd_no_accept, sizeof(apply_cmd_no_accept),
           "./build/bin/gallery_organizer build/test_cli_rename_env "
           "--rename-apply --rename-from-preview %s 2>&1",
           preview_id);
  code = RunCommandCapture(apply_cmd_no_accept, output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "requires explicit collision acceptance flag") !=
              NULL);

  memset(output, 0, sizeof(output));
  char apply_cmd_accept[2048] = {0};
  snprintf(apply_cmd_accept, sizeof(apply_cmd_accept),
           "./build/bin/gallery_organizer build/test_cli_rename_env "
           "--rename-apply --rename-from-preview %s "
           "--rename-accept-auto-suffix 2>&1",
           preview_id);
  code = RunCommandCapture(apply_cmd_accept, output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Operation ID:") != NULL);

  char operation_id[128] = {0};
  ASSERT_TRUE(ExtractTokenAfterPrefix(output, "Operation ID:", operation_id,
                                      sizeof(operation_id)));

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_env "
      "--rename-history 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, operation_id) != NULL);

  memset(output, 0, sizeof(output));
  char rollback_cmd[2048] = {0};
  snprintf(rollback_cmd, sizeof(rollback_cmd),
           "./build/bin/gallery_organizer build/test_cli_rename_env "
           "--rename-rollback %s 2>&1",
           operation_id);
  code = RunCommandCapture(rollback_cmd, output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rename rollback complete") != NULL);

  ASSERT_TRUE(FileExists("build/test_cli_rename_src/a.png"));
  ASSERT_TRUE(FileExists("build/test_cli_rename_src/b.png"));

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_src", "build/test_cli_rename_env"},
      2));
}

void test_cli_rename_tags_map_validation_errors(void) {
  ASSERT_TRUE(RemovePathsForTest((const char *[]){"build/test_cli_rename_map_src",
                                                  "build/test_cli_rename_map_env"},
                                 2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_map_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_map_env"));

  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_no_exif.jpg",
                              "build/test_cli_rename_map_src/a.jpg"));

  FILE *f = fopen("build/test_cli_rename_map_src/bad_map.json", "wb");
  ASSERT_TRUE(f != NULL);
  fputs("{bad-json", f);
  fclose(f);

  char output[8192] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_map_src "
      "build/test_cli_rename_map_env --rename-preview "
      "--rename-tags-map build/test_cli_rename_map_src/bad_map.json 2>&1",
      output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "not valid JSON") != NULL);

  ASSERT_TRUE(RemovePathsForTest((const char *[]){"build/test_cli_rename_map_src",
                                                  "build/test_cli_rename_map_env"},
                                 2));
}

void test_cli_rename_tags_map_ingest_and_sidecar_persist(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_map_ok_src",
                       "build/test_cli_rename_map_ok_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_map_ok_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_map_ok_env"));

  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_no_exif.jpg",
                              "build/test_cli_rename_map_ok_src/a.jpg"));

  char absolute_a[1024] = {0};
  ASSERT_TRUE(FsGetAbsolutePath("build/test_cli_rename_map_ok_src/a.jpg", absolute_a,
                                sizeof(absolute_a)));

  FILE *f = fopen("build/test_cli_rename_map_ok_src/map.json", "wb");
  ASSERT_TRUE(f != NULL);
  fprintf(f, "{\"files\":{\"%s\":{\"manualTags\":[\"frag42\"]}}}",
          absolute_a);
  fclose(f);

  char output[16384] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_map_ok_src "
      "build/test_cli_rename_map_ok_env --rename-preview "
      "--rename-tags-map build/test_cli_rename_map_ok_src/map.json 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rename preview ready") != NULL);

  ASSERT_TRUE(FileContainsText(
      "build/test_cli_rename_map_ok_env/.cache/rename_tags.json", "frag42"));

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_map_ok_src",
                       "build/test_cli_rename_map_ok_env"},
      2));
}

void test_cli_rename_metadata_tag_flags_apply_in_preview(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_meta_src",
                       "build/test_cli_rename_meta_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_meta_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_meta_env"));

  ASSERT_TRUE(CopyFileForTest("tests/assets/jpg/sample_no_exif.jpg",
                              "build/test_cli_rename_meta_src/a.jpg"));

  char output[32768] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_meta_src "
      "build/test_cli_rename_meta_env --rename-preview "
      "--rename-pattern 'meta-[tags_meta].[format]' "
      "--rename-meta-tag-add 'meta-a,meta-b' "
      "--rename-meta-fields "
      "--rename-preview-json-out build/test_cli_rename_meta_env/preview.json 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rename preview ready") != NULL);
  ASSERT_TRUE(strstr(output, "Metadata tag fields in scope") != NULL);
  ASSERT_TRUE(FileContainsText("build/test_cli_rename_meta_env/preview.json",
                               "meta-a-meta-b"));
  ASSERT_TRUE(FileContainsText(
      "build/test_cli_rename_meta_env/.cache/rename_tags.json", "metaTagAdds"));

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_meta_src",
                       "build/test_cli_rename_meta_env"},
      2));
}

void test_cli_rename_history_detail_redo_and_latest_ids(void) {
  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_v068_src",
                       "build/test_cli_rename_v068_env"},
      2));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_v068_src"));
  ASSERT_TRUE(FsMakeDirRecursive("build/test_cli_rename_v068_env"));

  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_cli_rename_v068_src/a.png"));
  ASSERT_TRUE(CopyFileForTest("tests/assets/png/sample_no_exif.png",
                              "build/test_cli_rename_v068_src/b.png"));

  char output[32768] = {0};
  int code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_v068_src "
      "build/test_cli_rename_v068_env --rename-preview "
      "--rename-pattern 'pic-[index]-[camera].[format]' 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Preview ID:") != NULL);

  char preview_id[128] = {0};
  ASSERT_TRUE(ExtractTokenAfterPrefix(output, "Preview ID:", preview_id,
                                      sizeof(preview_id)));

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_v068_env "
      "--rename-preview-latest-id 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Latest preview ID:") != NULL);
  ASSERT_TRUE(strstr(output, preview_id) != NULL);

  memset(output, 0, sizeof(output));
  char apply_cmd[2048] = {0};
  snprintf(apply_cmd, sizeof(apply_cmd),
           "./build/bin/gallery_organizer build/test_cli_rename_v068_env "
           "--rename-apply --rename-from-preview %s 2>&1",
           preview_id);
  code = RunCommandCapture(apply_cmd, output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Operation ID:") != NULL);

  char operation_id[128] = {0};
  ASSERT_TRUE(ExtractTokenAfterPrefix(output, "Operation ID:", operation_id,
                                      sizeof(operation_id)));

  memset(output, 0, sizeof(output));
  code = RunCommandCapture(
      "./build/bin/gallery_organizer build/test_cli_rename_v068_env "
      "--rename-history-latest-id 2>&1",
      output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Latest operation ID:") != NULL);
  ASSERT_TRUE(strstr(output, operation_id) != NULL);

  memset(output, 0, sizeof(output));
  char detail_cmd[2048] = {0};
  snprintf(detail_cmd, sizeof(detail_cmd),
           "./build/bin/gallery_organizer build/test_cli_rename_v068_env "
           "--rename-history-detail %s 2>&1",
           operation_id);
  code = RunCommandCapture(detail_cmd, output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rename history detail:") != NULL);
  ASSERT_TRUE(strstr(output, "Manifest path:") != NULL);
  ASSERT_TRUE(strstr(output, "\"operationId\"") != NULL);

  memset(output, 0, sizeof(output));
  char rollback_cmd[2048] = {0};
  snprintf(rollback_cmd, sizeof(rollback_cmd),
           "./build/bin/gallery_organizer build/test_cli_rename_v068_env "
           "--rename-rollback %s 2>&1",
           operation_id);
  code = RunCommandCapture(rollback_cmd, output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Rename rollback complete") != NULL);

  memset(output, 0, sizeof(output));
  char invalid_redo_cmd[2048] = {0};
  snprintf(invalid_redo_cmd, sizeof(invalid_redo_cmd),
           "./build/bin/gallery_organizer build/test_cli_rename_v068_env "
           "--rename-redo %s --rename-from-preview %s 2>&1",
           operation_id, preview_id);
  code = RunCommandCapture(invalid_redo_cmd, output, sizeof(output));
  ASSERT_TRUE(code != 0);
  ASSERT_TRUE(strstr(output, "--rename-from-preview can only be used with "
                            "--rename-apply") != NULL);

  memset(output, 0, sizeof(output));
  char redo_cmd[2048] = {0};
  snprintf(redo_cmd, sizeof(redo_cmd),
           "./build/bin/gallery_organizer build/test_cli_rename_v068_env "
           "--rename-redo %s 2>&1",
           operation_id);
  code = RunCommandCapture(redo_cmd, output, sizeof(output));
  ASSERT_EQ(0, code);
  ASSERT_TRUE(strstr(output, "Resolved preview id from operation") != NULL);
  ASSERT_TRUE(strstr(output, "Rename apply completed") != NULL);

  ASSERT_FALSE(FileExists("build/test_cli_rename_v068_src/a.png"));
  ASSERT_FALSE(FileExists("build/test_cli_rename_v068_src/b.png"));

  ASSERT_TRUE(RemovePathsForTest(
      (const char *[]){"build/test_cli_rename_v068_src",
                       "build/test_cli_rename_v068_env"},
      2));
}

void register_cli_rename_tests(void) {
  register_test("test_cli_rename_preview_apply_handshake_and_rollback",
                test_cli_rename_preview_apply_handshake_and_rollback,
                "integration");
  register_test("test_cli_rename_tags_map_validation_errors",
                test_cli_rename_tags_map_validation_errors, "integration");
  register_test("test_cli_rename_tags_map_ingest_and_sidecar_persist",
                test_cli_rename_tags_map_ingest_and_sidecar_persist,
                "integration");
  register_test("test_cli_rename_metadata_tag_flags_apply_in_preview",
                test_cli_rename_metadata_tag_flags_apply_in_preview,
                "integration");
  register_test("test_cli_rename_init_bootstrap_preview_json_and_apply_latest",
                test_cli_rename_init_bootstrap_preview_json_and_apply_latest,
                "integration");
  register_test("test_cli_rename_preview_path_suggestion",
                test_cli_rename_preview_path_suggestion, "integration");
  register_test("test_cli_rename_history_detail_redo_and_latest_ids",
                test_cli_rename_history_detail_redo_and_latest_ids,
                "integration");
}
