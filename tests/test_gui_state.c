#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui/gui_state.h"
#include "test_framework.h"

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
  system("rm -rf build/test_gui_state");
  system("mkdir -p build/test_gui_state");
  ASSERT_EQ(0, setenv("CGO_GUI_STATE_PATH", "build/test_gui_state/state.json", 1));

  GuiState in = {0};
  strncpy(in.gallery_dir, "/tmp/gallery_a", sizeof(in.gallery_dir) - 1);
  strncpy(in.env_dir, "/tmp/env_a", sizeof(in.env_dir) - 1);

  ASSERT_TRUE(GuiStateSave(&in));

  GuiState out = {0};
  ASSERT_TRUE(GuiStateLoad(&out));
  ASSERT_STR_EQ(in.gallery_dir, out.gallery_dir);
  ASSERT_STR_EQ(in.env_dir, out.env_dir);

  char saved_json[2048] = {0};
  ReadFileText("build/test_gui_state/state.json", saved_json, sizeof(saved_json));
  ASSERT_TRUE(strstr(saved_json, "\"galleryDir\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "\"envDir\"") != NULL);
  ASSERT_TRUE(strstr(saved_json, "uiScalePercent") == NULL);
  ASSERT_TRUE(strstr(saved_json, "windowWidth") == NULL);
  ASSERT_TRUE(strstr(saved_json, "windowHeight") == NULL);

  ASSERT_TRUE(GuiStateReset());
  ASSERT_FALSE(GuiStateLoad(&out));

  unsetenv("CGO_GUI_STATE_PATH");
  system("rm -rf build/test_gui_state");
}

void test_gui_state_resolve_default_path(void) {
  unsetenv("CGO_GUI_STATE_PATH");

  char resolved[GUI_STATE_MAX_PATH] = {0};
  ASSERT_TRUE(GuiStateResolvePath(resolved, sizeof(resolved)));
  ASSERT_TRUE(strstr(resolved, "CGalleryOrganizer") != NULL);
}

void test_gui_state_legacy_defaults(void) {
  system("rm -rf build/test_gui_state");
  system("mkdir -p build/test_gui_state");
  ASSERT_EQ(0, setenv("CGO_GUI_STATE_PATH", "build/test_gui_state/state.json", 1));

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

  ASSERT_TRUE(GuiStateSave(&loaded));

  char rewritten_json[2048] = {0};
  ReadFileText("build/test_gui_state/state.json", rewritten_json,
               sizeof(rewritten_json));
  ASSERT_TRUE(strstr(rewritten_json, "uiScalePercent") == NULL);
  ASSERT_TRUE(strstr(rewritten_json, "windowWidth") == NULL);
  ASSERT_TRUE(strstr(rewritten_json, "windowHeight") == NULL);

  unsetenv("CGO_GUI_STATE_PATH");
  system("rm -rf build/test_gui_state");
}

void register_gui_state_tests(void) {
  register_test("test_gui_state_roundtrip_with_override",
                test_gui_state_roundtrip_with_override, "unit");
  register_test("test_gui_state_resolve_default_path",
                test_gui_state_resolve_default_path, "unit");
  register_test("test_gui_state_legacy_defaults",
                test_gui_state_legacy_defaults, "unit");
}
