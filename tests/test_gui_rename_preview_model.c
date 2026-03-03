#include <string.h>

#include "gui/core/gui_rename_preview_model.h"
#include "test_framework.h"

void test_gui_rename_preview_model_filters_visible_rows(void) {
  GuiRenamePreviewRow rows[4] = {0};
  rows[0].collision = false;
  rows[0].warning = false;
  rows[1].collision = true;
  rows[1].warning = true;
  rows[2].collision = false;
  rows[2].warning = true;
  rows[3].collision = true;
  rows[3].warning = true;

  int visible[4] = {0};

  int count = GuiRenameBuildVisibleRowIndexList(rows, 4, false, false, visible, 4);
  ASSERT_EQ(4, count);
  ASSERT_EQ(0, visible[0]);
  ASSERT_EQ(1, visible[1]);
  ASSERT_EQ(2, visible[2]);
  ASSERT_EQ(3, visible[3]);

  memset(visible, 0, sizeof(visible));
  count = GuiRenameBuildVisibleRowIndexList(rows, 4, true, false, visible, 4);
  ASSERT_EQ(2, count);
  ASSERT_EQ(1, visible[0]);
  ASSERT_EQ(3, visible[1]);

  memset(visible, 0, sizeof(visible));
  count = GuiRenameBuildVisibleRowIndexList(rows, 4, false, true, visible, 4);
  ASSERT_EQ(3, count);
  ASSERT_EQ(1, visible[0]);
  ASSERT_EQ(2, visible[1]);
  ASSERT_EQ(3, visible[2]);

  memset(visible, 0, sizeof(visible));
  count = GuiRenameBuildVisibleRowIndexList(rows, 4, true, true, visible, 4);
  ASSERT_EQ(2, count);
  ASSERT_EQ(1, visible[0]);
  ASSERT_EQ(3, visible[1]);
}

void test_gui_rename_preview_model_resolves_selection(void) {
  int visible[3] = {2, 5, 9};

  ASSERT_EQ(5, GuiRenameResolveSelectedRow(visible, 3, 5));
  ASSERT_EQ(2, GuiRenameResolveSelectedRow(visible, 3, 1));
  ASSERT_EQ(-1, GuiRenameResolveSelectedRow(visible, 0, 1));
  ASSERT_EQ(-1, GuiRenameResolveSelectedRow(NULL, 3, 1));
}

void register_gui_rename_preview_model_tests(void) {
  register_test("test_gui_rename_preview_model_filters_visible_rows",
                test_gui_rename_preview_model_filters_visible_rows, "unit");
  register_test("test_gui_rename_preview_model_resolves_selection",
                test_gui_rename_preview_model_resolves_selection, "unit");
}
