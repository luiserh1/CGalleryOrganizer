#include "gui/frontends/functional/gui_fixed_layout.h"
#include "test_framework.h"

static void AssertRectsInPanel(GuiRect panel, const GuiRect *rects, int count) {
  ASSERT_TRUE(rects != NULL);
  ASSERT_TRUE(count > 0);
  ASSERT_TRUE(GuiValidateNoOverlap(panel, rects, count));
}

void test_gui_fixed_shell_layout_in_bounds(void) {
  GuiShellLayout shell = {0};
  GuiBuildShellLayout(&shell);

  GuiRect window = {0.0f, 0.0f, (float)GUI_FIXED_WINDOW_WIDTH,
                    (float)GUI_FIXED_WINDOW_HEIGHT};

  ASSERT_TRUE(GuiRectInBounds(window, shell.canvas));
  ASSERT_TRUE(GuiRectInBounds(shell.canvas, shell.title));
  ASSERT_TRUE(GuiRectInBounds(shell.canvas, shell.panel_outer));
  ASSERT_TRUE(GuiRectInBounds(shell.canvas, shell.status_outer));

  for (int i = 0; i < 5; i++) {
    ASSERT_TRUE(GuiRectInBounds(shell.canvas, shell.tabs[i]));
  }

  ASSERT_FALSE(GuiRectsOverlap(shell.panel_outer, shell.status_outer));
  ASSERT_TRUE(GuiRectInBounds(shell.status_inner, shell.status_label));
  ASSERT_TRUE(GuiRectInBounds(shell.status_inner, shell.status_value));
  ASSERT_TRUE(GuiRectInBounds(shell.status_inner, shell.cancel_button));
  ASSERT_TRUE(GuiRectInBounds(shell.status_inner, shell.log_box));
}

void test_gui_fixed_scan_layout_no_overlap(void) {
  GuiShellLayout shell = {0};
  GuiBuildShellLayout(&shell);

  GuiScanPanelLayout scan = {0};
  GuiBuildScanPanelLayout(shell.panel_inner, &scan);

  GuiRect rects[] = {
      scan.gallery_label,      scan.gallery_input,  scan.gallery_pick_button,
      scan.env_label,          scan.env_input,      scan.env_pick_button,
      scan.exhaustive,         scan.jobs_label,     scan.jobs_input,
      scan.cache_label,        scan.cache_none,     scan.cache_zstd,
      scan.level_label,        scan.level_input,    scan.scan_button,
      scan.ml_button,
      scan.download_models_button, scan.save_paths_button,
      scan.reset_paths_button, scan.info_label,
  };

  AssertRectsInPanel(shell.panel_inner, rects,
                     (int)(sizeof(rects) / sizeof(rects[0])));
}

void test_gui_fixed_similarity_layout_no_overlap(void) {
  GuiShellLayout shell = {0};
  GuiBuildShellLayout(&shell);

  GuiSimilarityPanelLayout sim = {0};
  GuiBuildSimilarityPanelLayout(shell.panel_inner, &sim);

  GuiRect rects[] = {
      sim.threshold_label, sim.threshold_input, sim.topk_label,  sim.topk_input,
      sim.incremental,     sim.memory_label,    sim.mode_chunked, sim.mode_eager,
      sim.run_button,      sim.info_label,
  };

  AssertRectsInPanel(shell.panel_inner, rects,
                     (int)(sizeof(rects) / sizeof(rects[0])));
}

void test_gui_fixed_organize_layout_no_overlap(void) {
  GuiShellLayout shell = {0};
  GuiBuildShellLayout(&shell);

  GuiOrganizePanelLayout organize = {0};
  GuiBuildOrganizePanelLayout(shell.panel_inner, &organize);

  GuiRect rects[] = {
      organize.group_label, organize.group_input,   organize.preview_button,
      organize.execute_button, organize.rollback_button, organize.info_label,
  };

  AssertRectsInPanel(shell.panel_inner, rects,
                     (int)(sizeof(rects) / sizeof(rects[0])));
}

void test_gui_fixed_duplicates_layout_no_overlap(void) {
  GuiShellLayout shell = {0};
  GuiBuildShellLayout(&shell);

  GuiDuplicatesPanelLayout dup = {0};
  GuiBuildDuplicatesPanelLayout(shell.panel_inner, &dup);

  GuiRect rects[] = {
      dup.info_top,
      dup.analyze_button,
      dup.move_button,
      dup.info_bottom,
  };

  AssertRectsInPanel(shell.panel_inner, rects,
                     (int)(sizeof(rects) / sizeof(rects[0])));
}

void test_gui_fixed_rename_layout_no_overlap(void) {
  GuiShellLayout shell = {0};
  GuiBuildShellLayout(&shell);

  GuiRenamePanelLayout rename = {0};
  GuiBuildRenamePanelLayout(shell.panel_inner, &rename);

  GuiRect rects[] = {
      rename.pattern_label,      rename.pattern_input,   rename.tags_map_label,
      rename.tags_map_input,     rename.tags_map_pick_button,
      rename.tags_map_bootstrap_button, rename.tag_add_label,   rename.tag_add_input,
      rename.tag_remove_label,   rename.tag_remove_input,
      rename.meta_tag_add_label, rename.meta_tag_add_input,
      rename.meta_tag_remove_label, rename.meta_tag_remove_input,
      rename.preview_id_label,   rename.preview_id_input,
      rename.preview_latest_button,
      rename.operation_id_label, rename.operation_id_input,
      rename.history_latest_button,
      rename.accept_suffix,      rename.filter_collisions,
      rename.filter_warnings,    rename.preview_button, rename.apply_button,
      rename.history_button,     rename.detail_button, rename.redo_button,
      rename.rollback_button,
      rename.selected_tags_label, rename.selected_tags_input,
      rename.selected_tags_apply_button, rename.selected_meta_tags_label,
      rename.selected_meta_tags_input, rename.selected_meta_tags_apply_button,
      rename.info_label,
      rename.preview_table,
  };

  AssertRectsInPanel(shell.panel_inner, rects,
                     (int)(sizeof(rects) / sizeof(rects[0])));
}

void register_gui_layout_tests(void) {
  register_test("test_gui_fixed_shell_layout_in_bounds",
                test_gui_fixed_shell_layout_in_bounds, "unit");
  register_test("test_gui_fixed_scan_layout_no_overlap",
                test_gui_fixed_scan_layout_no_overlap, "unit");
  register_test("test_gui_fixed_similarity_layout_no_overlap",
                test_gui_fixed_similarity_layout_no_overlap, "unit");
  register_test("test_gui_fixed_organize_layout_no_overlap",
                test_gui_fixed_organize_layout_no_overlap, "unit");
  register_test("test_gui_fixed_duplicates_layout_no_overlap",
                test_gui_fixed_duplicates_layout_no_overlap, "unit");
  register_test("test_gui_fixed_rename_layout_no_overlap",
                test_gui_fixed_rename_layout_no_overlap, "unit");
}
