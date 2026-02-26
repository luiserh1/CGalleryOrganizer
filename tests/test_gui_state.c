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

  ASSERT_TRUE(GuiStateSave(&in));

  GuiState out = {0};
  ASSERT_TRUE(GuiStateLoad(&out));
  ASSERT_STR_EQ(in.gallery_dir, out.gallery_dir);
  ASSERT_STR_EQ(in.env_dir, out.env_dir);

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

void register_gui_state_tests(void) {
  register_test("test_gui_state_roundtrip_with_override",
                test_gui_state_roundtrip_with_override, "unit");
  register_test("test_gui_state_resolve_default_path",
                test_gui_state_resolve_default_path, "unit");
}
