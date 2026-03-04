#include <string.h>

#include "gui/core/gui_ui_state_internal.h"
#include "gui/gui_common.h"

static bool GuiUiNormalizeAllNumericInputs(GuiUiState *state) {
  if (!state) {
    return false;
  }

  int jobs = 0;
  if (!GuiUiTryParseIntStrict(state->jobs_input, &jobs) || jobs < 1 || jobs > 256) {
    GuiUiSetBannerError(state, "Jobs must be an integer between 1 and 256");
    return false;
  }
  state->jobs = jobs;

  int cache_level = 0;
  if (!GuiUiTryParseIntStrict(state->cache_level_input, &cache_level) ||
      cache_level < 1 || cache_level > 19) {
    GuiUiSetBannerError(state,
                        "Compression level must be an integer between 1 and 19");
    return false;
  }
  state->cache_level = cache_level;

  float threshold = 0.0f;
  if (!GuiUiTryParseFloatStrict(state->sim_threshold_input, &threshold) ||
      threshold < 0.0f || threshold > 1.0f) {
    GuiUiSetBannerError(
        state,
        "Similarity threshold must be a number between 0.000 and 1.000");
    return false;
  }
  state->sim_threshold = threshold;

  int topk = 0;
  if (!GuiUiTryParseIntStrict(state->sim_topk_input, &topk) || topk < 1 ||
      topk > 1000) {
    GuiUiSetBannerError(state, "TopK must be an integer between 1 and 1000");
    return false;
  }
  state->sim_topk = topk;

  GuiUiSyncNumericInputBuffers(state);
  return true;
}

static bool GuiUiNumericInputBuffersEqualCurrentValues(const GuiUiState *state) {
  if (!state) {
    return true;
  }

  int jobs = 0;
  if (!GuiUiTryParseIntStrict(state->jobs_input, &jobs) || jobs != state->jobs) {
    return false;
  }

  int cache_level = 0;
  if (!GuiUiTryParseIntStrict(state->cache_level_input, &cache_level) ||
      cache_level != state->cache_level) {
    return false;
  }

  float threshold = 0.0f;
  if (!GuiUiTryParseFloatStrict(state->sim_threshold_input, &threshold)) {
    return false;
  }
  if (threshold < state->sim_threshold - 0.0005f ||
      threshold > state->sim_threshold + 0.0005f) {
    return false;
  }

  int topk = 0;
  if (!GuiUiTryParseIntStrict(state->sim_topk_input, &topk) || topk != state->sim_topk) {
    return false;
  }

  return true;
}

void GuiUiInitDefaults(GuiUiState *state) {
  if (!state) {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->jobs = 1;
  state->cache_mode = APP_CACHE_COMPRESSION_NONE;
  state->cache_level = 3;
  state->sim_incremental = true;
  state->sim_threshold = 0.92f;
  state->sim_topk = 5;
  state->sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
  strncpy(state->group_by, "date", sizeof(state->group_by) - 1);
  state->group_by[sizeof(state->group_by) - 1] = '\0';
  strncpy(state->rename_pattern_input, "pic-[datetime]-[index].[format]",
          sizeof(state->rename_pattern_input) - 1);
  state->rename_pattern_input[sizeof(state->rename_pattern_input) - 1] = '\0';
  state->rename_tags_map_path[0] = '\0';
  state->rename_tag_add_csv[0] = '\0';
  state->rename_tag_remove_csv[0] = '\0';
  state->rename_meta_tag_add_csv[0] = '\0';
  state->rename_meta_tag_remove_csv[0] = '\0';
  state->rename_selected_tags_csv[0] = '\0';
  state->rename_selected_meta_tags_csv[0] = '\0';
  state->rename_preview_id_input[0] = '\0';
  state->rename_operation_id_input[0] = '\0';
  state->rename_history_id_prefix_input[0] = '\0';
  state->rename_history_from_input[0] = '\0';
  state->rename_history_to_input[0] = '\0';
  state->rename_history_export_path[0] = '\0';
  strncpy(state->rename_history_prune_keep_input, "200",
          sizeof(state->rename_history_prune_keep_input) - 1);
  state->rename_history_prune_keep_input
      [sizeof(state->rename_history_prune_keep_input) - 1] = '\0';
  state->rename_history_rollback_filter_mode = 0;
  state->rename_accept_auto_suffix = false;
  state->rename_filter_collisions_only = false;
  state->rename_filter_warnings_only = false;
  state->rename_table_scroll = 0;
  state->rename_selected_row = -1;
  state->rename_preview_row_count = 0;
  state->rename_preview_warning_count = 0;
  state->runtime_state_valid = false;
  state->rebuild_confirm_pending = false;
  state->rebuild_confirm_task = GUI_TASK_NONE;
  state->rebuild_confirm_reason[0] = '\0';
  GuiUiSyncNumericInputBuffers(state);

  GuiUiBuildPersistedStateFromUi(state, &state->persisted_state);
}

void GuiUiSyncFromStateFile(GuiUiState *state) {
  if (!state) {
    return;
  }

  GuiState loaded = {0};
  if (!GuiStateLoad(&loaded)) {
    return;
  }

  GuiUiApplyPersistedStateToUi(state, &loaded);
  state->persisted_state = loaded;
}

bool GuiUiPersistState(GuiUiState *state) {
  if (!state) {
    return false;
  }

  if (!GuiUiNormalizeAllNumericInputs(state)) {
    return false;
  }

  GuiState to_save = {0};
  GuiUiBuildPersistedStateFromUi(state, &to_save);
  if (!GuiStateSave(&to_save)) {
    return false;
  }

  state->persisted_state = to_save;
  return true;
}

bool GuiUiHasUnsavedChanges(const GuiUiState *state) {
  if (!state) {
    return false;
  }

  if (!GuiUiNumericInputBuffersEqualCurrentValues(state)) {
    return true;
  }

  GuiState current_state = {0};
  GuiUiBuildPersistedStateFromUi(state, &current_state);

  if (strncmp(current_state.gallery_dir, state->persisted_state.gallery_dir,
              sizeof(current_state.gallery_dir)) != 0) {
    return true;
  }
  if (strncmp(current_state.env_dir, state->persisted_state.env_dir,
              sizeof(current_state.env_dir)) != 0) {
    return true;
  }

  if (current_state.scan_exhaustive != state->persisted_state.scan_exhaustive ||
      current_state.scan_jobs != state->persisted_state.scan_jobs ||
      current_state.scan_cache_mode != state->persisted_state.scan_cache_mode ||
      current_state.scan_cache_level != state->persisted_state.scan_cache_level ||
      current_state.sim_incremental != state->persisted_state.sim_incremental ||
      current_state.sim_topk != state->persisted_state.sim_topk ||
      current_state.sim_memory_mode != state->persisted_state.sim_memory_mode) {
    return true;
  }
  if (current_state.sim_threshold <
          state->persisted_state.sim_threshold - 0.0005f ||
      current_state.sim_threshold >
          state->persisted_state.sim_threshold + 0.0005f) {
    return true;
  }

  if (strncmp(current_state.organize_group_by,
              state->persisted_state.organize_group_by,
              sizeof(current_state.organize_group_by)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_pattern, state->persisted_state.rename_pattern,
              sizeof(current_state.rename_pattern)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_tags_map_path,
              state->persisted_state.rename_tags_map_path,
              sizeof(current_state.rename_tags_map_path)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_tag_add_csv,
              state->persisted_state.rename_tag_add_csv,
              sizeof(current_state.rename_tag_add_csv)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_tag_remove_csv,
              state->persisted_state.rename_tag_remove_csv,
              sizeof(current_state.rename_tag_remove_csv)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_meta_tag_add_csv,
              state->persisted_state.rename_meta_tag_add_csv,
              sizeof(current_state.rename_meta_tag_add_csv)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_meta_tag_remove_csv,
              state->persisted_state.rename_meta_tag_remove_csv,
              sizeof(current_state.rename_meta_tag_remove_csv)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_preview_id,
              state->persisted_state.rename_preview_id,
              sizeof(current_state.rename_preview_id)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_operation_id,
              state->persisted_state.rename_operation_id,
              sizeof(current_state.rename_operation_id)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_history_id_prefix,
              state->persisted_state.rename_history_id_prefix,
              sizeof(current_state.rename_history_id_prefix)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_history_from,
              state->persisted_state.rename_history_from,
              sizeof(current_state.rename_history_from)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_history_to,
              state->persisted_state.rename_history_to,
              sizeof(current_state.rename_history_to)) != 0) {
    return true;
  }
  if (strncmp(current_state.rename_history_export_path,
              state->persisted_state.rename_history_export_path,
              sizeof(current_state.rename_history_export_path)) != 0) {
    return true;
  }
  if (current_state.rename_history_prune_keep_count !=
      state->persisted_state.rename_history_prune_keep_count) {
    return true;
  }
  if (current_state.rename_history_rollback_filter_mode !=
      state->persisted_state.rename_history_rollback_filter_mode) {
    return true;
  }
  if (current_state.rename_accept_auto_suffix !=
      state->persisted_state.rename_accept_auto_suffix) {
    return true;
  }

  return false;
}
