#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui/gui_state.h"
#include "integration_test_helpers.h"
#include "test_framework.h"

static int TestSetEnv(const char *key, const char *value) {
#if defined(_WIN32)
  return _putenv_s(key, value);
#else
  return setenv(key, value, 1);
#endif
}

static void TestUnsetEnv(const char *key) {
#if defined(_WIN32)
  (void)_putenv_s(key, "");
#else
  unsetenv(key);
#endif
}

static void ReadFileText(const char *path, char *out, size_t out_size) {
  ASSERT_TRUE(path != NULL);
  ASSERT_TRUE(out != NULL);
  ASSERT_TRUE(out_size > 0);

  FILE *f = fopen(path, "rb");
  ASSERT_TRUE(f != NULL);
  size_t read = fread(out, 1, out_size - 1, f);
  fclose(f);
  out[read] = '\0';
}

void test_gui_state_roundtrip_with_override(void) {
  ASSERT_TRUE(ResetDirForTest("build/test_gui_state"));
  ASSERT_EQ(0, TestSetEnv("CGO_GUI_STATE_PATH", "build/test_gui_state/state.json"));

  GuiState in = {0};
  strncpy(in.gallery_dir, "/tmp/gallery_a", sizeof(in.gallery_dir) - 1);
  strncpy(in.env_dir, "/tmp/env_a", sizeof(in.env_dir) - 1);
  in.scan_exhaustive = true;
  in.scan_jobs = 6;
  in.scan_cache_mode = APP_CACHE_COMPRESSION_ZSTD;
  in.scan_cache_level = 7;
  in.sim_incremental = false;
  in.sim_threshold = 0.77f;
  in.sim_topk = 15;
  in.sim_memory_mode = APP_SIM_MEMORY_EAGER;
  strncpy(in.organize_group_by, "camera,date",
          sizeof(in.organize_group_by) - 1);

  ASSERT_TRUE(GuiStateSave(&in));

  GuiState out = {0};
  ASSERT_TRUE(GuiStateLoad(&out));
  ASSERT_STR_EQ(in.gallery_dir, out.gallery_dir);
  ASSERT_STR_EQ(in.env_dir, out.env_dir);
  ASSERT_EQ((int)in.scan_exhaustive, (int)out.scan_exhaustive);
  ASSERT_EQ(in.scan_jobs, out.scan_jobs);
  ASSERT_EQ(in.scan_cache_mode, out.scan_cache_mode);
  ASSERT_EQ(in.scan_cache_level, out.scan_cache_level);
  ASSERT_EQ((int)in.sim_incremental, (int)out.sim_incremental);
  ASSERT_TRUE(out.sim_threshold > 0.769f && out.sim_threshold < 0.771f);
  ASSERT_EQ(in.sim_topk, out.sim_topk);
  ASSERT_EQ(in.sim_memory_mode, out.sim_memory_mode);
  ASSERT_STR_EQ(in.organize_group_by, out.organize_group_by);

  char saved_json[4096] = {0};
  ReadFileText("build/test_gui_state/state.json", saved_json, sizeof(saved_json));
  ASSERT_TRUE(strstr(saved_json, "\"schemaVersion\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"scanExhaustive\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"scanJobs\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"scanCacheCompressionMode\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"scanCacheCompressionLevel\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"simIncremental\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"simThreshold\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"simTopK\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"simMemoryMode\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"organizeGroupBy\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "uiScalePercent") == NULL);
  ASSERT_TRUE(strstr(saved_json, "windowWidth") == NULL);
  ASSERT_TRUE(strstr(saved_json, "windowHeight") == NULL);

  ASSERT_TRUE(GuiStateReset());
  ASSERT_FALSE(GuiStateLoad(&out));

  TestUnsetEnv("CGO_GUI_STATE_PATH");
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_gui_state"));
}

void test_gui_state_resolve_default_path(void) {
  TestUnsetEnv("CGO_GUI_STATE_PATH");

  char resolved[GUI_STATE_MAX_PATH] = {0};
  ASSERT_TRUE(GuiStateResolvePath(resolved, sizeof(resolved)));
  ASSERT_TRUE(strstr(resolved, "CGalleryOrganizer") != NULL);
}

void test_gui_state_legacy_defaults(void) {
  ASSERT_TRUE(ResetDirForTest("build/test_gui_state"));
  ASSERT_EQ(0, TestSetEnv("CGO_GUI_STATE_PATH", "build/test_gui_state/state.json"));

  FILE *f = fopen("build/test_gui_state/state.json", "wb");
  ASSERT_TRUE(f != NULL);
  fputs("{\"galleryDir\":\"/tmp/g\",\"envDir\":\"/tmp/e\","
        "\"uiScalePercent\":140,\"windowWidth\":1500,\"windowHeight\":900}",
        f);
  fclose(f);

  GuiState loaded = {0};
  ASSERT_TRUE(GuiStateLoad(&loaded));
  ASSERT_STR_EQ("/tmp/g", loaded.gallery_dir);
  ASSERT_STR_EQ("/tmp/e", loaded.env_dir);
  ASSERT_FALSE(loaded.scan_exhaustive);
  ASSERT_EQ(1, loaded.scan_jobs);
  ASSERT_EQ(APP_CACHE_COMPRESSION_NONE, loaded.scan_cache_mode);
  ASSERT_EQ(3, loaded.scan_cache_level);
  ASSERT_TRUE(loaded.sim_incremental);
  ASSERT_TRUE(loaded.sim_threshold > 0.919f && loaded.sim_threshold < 0.921f);
  ASSERT_EQ(5, loaded.sim_topk);
  ASSERT_EQ(APP_SIM_MEMORY_CHUNKED, loaded.sim_memory_mode);
  ASSERT_STR_EQ("date", loaded.organize_group_by);

  ASSERT_TRUE(GuiStateSave(&loaded));

  char rewritten_json[4096] = {0};
  ReadFileText("build/test_gui_state/state.json", rewritten_json,
               sizeof(rewritten_json));
  ASSERT_TRUE(strstr(rewritten_json, "uiScalePercent") == NULL);
  ASSERT_TRUE(strstr(rewritten_json, "windowWidth") == NULL);
  ASSERT_TRUE(strstr(rewritten_json, "windowHeight") == NULL);

  TestUnsetEnv("CGO_GUI_STATE_PATH");
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_gui_state"));
}

void test_gui_state_invalid_values_are_clamped(void) {
  ASSERT_TRUE(ResetDirForTest("build/test_gui_state"));
  ASSERT_EQ(0, TestSetEnv("CGO_GUI_STATE_PATH", "build/test_gui_state/state.json"));

  FILE *f = fopen("build/test_gui_state/state.json", "wb");
  ASSERT_TRUE(f != NULL);
  fputs("{"
        "\"galleryDir\":\"/tmp/g\","
        "\"envDir\":\"/tmp/e\","
        "\"scanJobs\":999,"
        "\"scanCacheCompressionMode\":\"auto\","
        "\"scanCacheCompressionLevel\":25,"
        "\"simThreshold\":1.7,"
        "\"simTopK\":0,"
        "\"simMemoryMode\":\"bad\","
        "\"organizeGroupBy\":\"\""
        "}",
        f);
  fclose(f);

  GuiState loaded = {0};
  ASSERT_TRUE(GuiStateLoad(&loaded));
  ASSERT_EQ(256, loaded.scan_jobs);
  ASSERT_EQ(APP_CACHE_COMPRESSION_NONE, loaded.scan_cache_mode);
  ASSERT_EQ(19, loaded.scan_cache_level);
  ASSERT_TRUE(loaded.sim_threshold > 0.999f && loaded.sim_threshold <= 1.0f);
  ASSERT_EQ(1, loaded.sim_topk);
  ASSERT_EQ(APP_SIM_MEMORY_CHUNKED, loaded.sim_memory_mode);
  ASSERT_STR_EQ("date", loaded.organize_group_by);

  TestUnsetEnv("CGO_GUI_STATE_PATH");
  ASSERT_TRUE(RemovePathRecursiveForTest("build/test_gui_state"));
}

void register_gui_state_tests(void) {
  register_test("test_gui_state_roundtrip_with_override",
                test_gui_state_roundtrip_with_override, "unit");
  register_test("test_gui_state_resolve_default_path",
                test_gui_state_resolve_default_path, "unit");
  register_test("test_gui_state_legacy_defaults", test_gui_state_legacy_defaults,
                "unit");
  register_test("test_gui_state_invalid_values_are_clamped",
                test_gui_state_invalid_values_are_clamped, "unit");
}
