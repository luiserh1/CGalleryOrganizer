#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui/gui_state.h"
#include "test_framework.h"

void test_gui_state_roundtrip_with_override(void) {
  system("rm -rf build/test_gui_state");
  system("mkdir -p build/test_gui_state");
  ASSERT_EQ(0, setenv("CGO_GUI_STATE_PATH", "build/test_gui_state/state.json", 1));

  GuiState in = {0};
  strncpy(in.gallery_dir, "/tmp/gallery_a", sizeof(in.gallery_dir) - 1);
  strncpy(in.env_dir, "/tmp/env_a", sizeof(in.env_dir) - 1);
  in.ui_scale_percent = 130;
  in.window_width = 1440;
  in.window_height = 900;

  ASSERT_TRUE(GuiStateSave(&in));

  GuiState out = {0};
  ASSERT_TRUE(GuiStateLoad(&out));
  ASSERT_STR_EQ(in.gallery_dir, out.gallery_dir);
  ASSERT_STR_EQ(in.env_dir, out.env_dir);
  ASSERT_EQ(130, out.ui_scale_percent);
  ASSERT_EQ(1440, out.window_width);
  ASSERT_EQ(900, out.window_height);

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
  fputs("{\"galleryDir\":\"/tmp/g\",\"envDir\":\"/tmp/e\"}", f);
  fclose(f);

  GuiState loaded = {0};
  ASSERT_TRUE(GuiStateLoad(&loaded));
  ASSERT_STR_EQ("/tmp/g", loaded.gallery_dir);
  ASSERT_STR_EQ("/tmp/e", loaded.env_dir);
  ASSERT_EQ(GUI_STATE_DEFAULT_UI_SCALE_PERCENT, loaded.ui_scale_percent);
  ASSERT_EQ(GUI_STATE_DEFAULT_WINDOW_WIDTH, loaded.window_width);
  ASSERT_EQ(GUI_STATE_DEFAULT_WINDOW_HEIGHT, loaded.window_height);

  unsetenv("CGO_GUI_STATE_PATH");
  system("rm -rf build/test_gui_state");
}

void test_gui_state_clamps_invalid_values(void) {
  system("rm -rf build/test_gui_state");
  system("mkdir -p build/test_gui_state");
  ASSERT_EQ(0, setenv("CGO_GUI_STATE_PATH", "build/test_gui_state/state.json", 1));

  GuiState in = {0};
  strncpy(in.gallery_dir, "/tmp/gallery_invalid", sizeof(in.gallery_dir) - 1);
  strncpy(in.env_dir, "/tmp/env_invalid", sizeof(in.env_dir) - 1);
  in.ui_scale_percent = 999;
  in.window_width = 0;
  in.window_height = -20;
  ASSERT_TRUE(GuiStateSave(&in));

  GuiState out = {0};
  ASSERT_TRUE(GuiStateLoad(&out));
  ASSERT_EQ(GUI_STATE_MAX_UI_SCALE_PERCENT, out.ui_scale_percent);
  ASSERT_EQ(GUI_STATE_DEFAULT_WINDOW_WIDTH, out.window_width);
  ASSERT_EQ(GUI_STATE_DEFAULT_WINDOW_HEIGHT, out.window_height);

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
  register_test("test_gui_state_clamps_invalid_values",
                test_gui_state_clamps_invalid_values, "unit");
}
