#include <string.h>

#include "gui/core/gui_action_rules.h"
#include "test_framework.h"

void test_gui_action_rules_scan_requires_paths(void) {
  GuiUiState state;
  memset(&state, 0, sizeof(state));

  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(&state, GUI_ACTION_SCAN_CACHE, &availability);
  ASSERT_FALSE(availability.enabled);

  strncpy(state.gallery_dir, "/tmp/gallery", sizeof(state.gallery_dir) - 1);
  GuiResolveActionAvailability(&state, GUI_ACTION_SCAN_CACHE, &availability);
  ASSERT_FALSE(availability.enabled);

  strncpy(state.env_dir, "/tmp/env", sizeof(state.env_dir) - 1);
  GuiResolveActionAvailability(&state, GUI_ACTION_SCAN_CACHE, &availability);
  ASSERT_TRUE(availability.enabled);
}

void test_gui_action_rules_ml_requires_models(void) {
  GuiUiState state;
  memset(&state, 0, sizeof(state));
  strncpy(state.gallery_dir, "/tmp/gallery", sizeof(state.gallery_dir) - 1);
  strncpy(state.env_dir, "/tmp/env", sizeof(state.env_dir) - 1);
  state.runtime_state_valid = true;

  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(&state, GUI_ACTION_ML_ENRICH, &availability);
  ASSERT_FALSE(availability.enabled);

  state.runtime_state.models.classification_present = true;
  state.runtime_state.models.text_detection_present = true;
  state.runtime_state.models.embedding_present = true;
  GuiResolveActionAvailability(&state, GUI_ACTION_ML_ENRICH, &availability);
  ASSERT_TRUE(availability.enabled);
}

void test_gui_action_rules_cache_gated_actions(void) {
  GuiUiState state;
  memset(&state, 0, sizeof(state));
  strncpy(state.env_dir, "/tmp/env", sizeof(state.env_dir) - 1);
  state.runtime_state_valid = true;

  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(&state, GUI_ACTION_ORGANIZE_PREVIEW, &availability);
  ASSERT_FALSE(availability.enabled);

  state.runtime_state.cache_exists = true;
  state.runtime_state.cache_entry_count_known = false;
  state.runtime_state.cache_entry_count = 0;
  GuiResolveActionAvailability(&state, GUI_ACTION_ORGANIZE_PREVIEW, &availability);
  ASSERT_TRUE(availability.enabled);

  state.runtime_state.cache_entry_count_known = true;
  state.runtime_state.cache_entry_count = 0;
  GuiResolveActionAvailability(&state, GUI_ACTION_ORGANIZE_PREVIEW, &availability);
  ASSERT_FALSE(availability.enabled);

  state.runtime_state.cache_entry_count = 2;
  state.runtime_state.cache_entry_count_known = true;
  GuiResolveActionAvailability(&state, GUI_ACTION_ORGANIZE_PREVIEW, &availability);
  ASSERT_TRUE(availability.enabled);

  GuiResolveActionAvailability(&state, GUI_ACTION_ROLLBACK, &availability);
  ASSERT_FALSE(availability.enabled);
  state.runtime_state.rollback_manifest_exists = true;
  GuiResolveActionAvailability(&state, GUI_ACTION_ROLLBACK, &availability);
  ASSERT_TRUE(availability.enabled);
}

void test_gui_action_rules_duplicates_move_needs_report(void) {
  GuiUiState state;
  memset(&state, 0, sizeof(state));
  strncpy(state.env_dir, "/tmp/env", sizeof(state.env_dir) - 1);

  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(&state, GUI_ACTION_DUPLICATES_MOVE, &availability);
  ASSERT_FALSE(availability.enabled);

  state.worker_snapshot.duplicate_report_ready = true;
  GuiResolveActionAvailability(&state, GUI_ACTION_DUPLICATES_MOVE, &availability);
  ASSERT_TRUE(availability.enabled);
}

void test_gui_action_rules_rename_requirements(void) {
  GuiUiState state;
  memset(&state, 0, sizeof(state));

  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(&state, GUI_ACTION_RENAME_PREVIEW, &availability);
  ASSERT_FALSE(availability.enabled);

  strncpy(state.gallery_dir, "/tmp/gallery", sizeof(state.gallery_dir) - 1);
  GuiResolveActionAvailability(&state, GUI_ACTION_RENAME_PREVIEW, &availability);
  ASSERT_FALSE(availability.enabled);

  strncpy(state.env_dir, "/tmp/env", sizeof(state.env_dir) - 1);
  GuiResolveActionAvailability(&state, GUI_ACTION_RENAME_PREVIEW, &availability);
  ASSERT_TRUE(availability.enabled);

  GuiResolveActionAvailability(&state, GUI_ACTION_RENAME_APPLY, &availability);
  ASSERT_FALSE(availability.enabled);
  strncpy(state.rename_preview_id_input, "rnp-1",
          sizeof(state.rename_preview_id_input) - 1);
  GuiResolveActionAvailability(&state, GUI_ACTION_RENAME_APPLY, &availability);
  ASSERT_TRUE(availability.enabled);

  GuiResolveActionAvailability(&state, GUI_ACTION_RENAME_ROLLBACK, &availability);
  ASSERT_FALSE(availability.enabled);
  strncpy(state.rename_operation_id_input, "rop-1",
          sizeof(state.rename_operation_id_input) - 1);
  GuiResolveActionAvailability(&state, GUI_ACTION_RENAME_ROLLBACK, &availability);
  ASSERT_TRUE(availability.enabled);
}

void register_gui_action_rules_tests(void) {
  register_test("test_gui_action_rules_scan_requires_paths",
                test_gui_action_rules_scan_requires_paths, "unit");
  register_test("test_gui_action_rules_ml_requires_models",
                test_gui_action_rules_ml_requires_models, "unit");
  register_test("test_gui_action_rules_cache_gated_actions",
                test_gui_action_rules_cache_gated_actions, "unit");
  register_test("test_gui_action_rules_duplicates_move_needs_report",
                test_gui_action_rules_duplicates_move_needs_report, "unit");
  register_test("test_gui_action_rules_rename_requirements",
                test_gui_action_rules_rename_requirements, "unit");
}
