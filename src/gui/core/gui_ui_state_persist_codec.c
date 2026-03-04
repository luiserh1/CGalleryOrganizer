#include <stdio.h>
#include <string.h>

#include "gui/core/gui_ui_state_internal.h"
#include "gui/gui_common.h"

void GuiUiSyncNumericInputBuffers(GuiUiState *state) {
  if (!state) {
    return;
  }

  snprintf(state->jobs_input, sizeof(state->jobs_input), "%d", state->jobs);
  snprintf(state->cache_level_input, sizeof(state->cache_level_input), "%d",
           state->cache_level);
  snprintf(state->sim_threshold_input, sizeof(state->sim_threshold_input), "%.3f",
           state->sim_threshold);
  snprintf(state->sim_topk_input, sizeof(state->sim_topk_input), "%d",
           state->sim_topk);
}

void GuiUiBuildPersistedStateFromUi(const GuiUiState *state,
                                    GuiState *out_state) {
  if (!state || !out_state) {
    return;
  }

  memset(out_state, 0, sizeof(*out_state));
  strncpy(out_state->gallery_dir, state->gallery_dir,
          sizeof(out_state->gallery_dir) - 1);
  out_state->gallery_dir[sizeof(out_state->gallery_dir) - 1] = '\0';
  strncpy(out_state->env_dir, state->env_dir, sizeof(out_state->env_dir) - 1);
  out_state->env_dir[sizeof(out_state->env_dir) - 1] = '\0';

  out_state->scan_exhaustive = state->exhaustive;
  out_state->scan_jobs = state->jobs;
  out_state->scan_cache_mode =
      state->cache_mode == APP_CACHE_COMPRESSION_ZSTD
          ? APP_CACHE_COMPRESSION_ZSTD
          : APP_CACHE_COMPRESSION_NONE;
  out_state->scan_cache_level = state->cache_level;

  out_state->sim_incremental = state->sim_incremental;
  out_state->sim_threshold = state->sim_threshold;
  out_state->sim_topk = state->sim_topk;
  out_state->sim_memory_mode = state->sim_memory_mode == APP_SIM_MEMORY_EAGER
                                   ? APP_SIM_MEMORY_EAGER
                                   : APP_SIM_MEMORY_CHUNKED;

  if (state->group_by[0] != '\0') {
    strncpy(out_state->organize_group_by, state->group_by,
            sizeof(out_state->organize_group_by) - 1);
    out_state->organize_group_by[sizeof(out_state->organize_group_by) - 1] =
        '\0';
  } else {
    strncpy(out_state->organize_group_by, "date",
            sizeof(out_state->organize_group_by) - 1);
    out_state->organize_group_by[sizeof(out_state->organize_group_by) - 1] =
        '\0';
  }

  if (state->rename_pattern_input[0] != '\0') {
    strncpy(out_state->rename_pattern, state->rename_pattern_input,
            sizeof(out_state->rename_pattern) - 1);
    out_state->rename_pattern[sizeof(out_state->rename_pattern) - 1] = '\0';
  } else {
    strncpy(out_state->rename_pattern, "pic-[datetime]-[index].[format]",
            sizeof(out_state->rename_pattern) - 1);
    out_state->rename_pattern[sizeof(out_state->rename_pattern) - 1] = '\0';
  }
  strncpy(out_state->rename_tags_map_path, state->rename_tags_map_path,
          sizeof(out_state->rename_tags_map_path) - 1);
  out_state->rename_tags_map_path[sizeof(out_state->rename_tags_map_path) - 1] =
      '\0';
  strncpy(out_state->rename_tag_add_csv, state->rename_tag_add_csv,
          sizeof(out_state->rename_tag_add_csv) - 1);
  out_state->rename_tag_add_csv[sizeof(out_state->rename_tag_add_csv) - 1] =
      '\0';
  strncpy(out_state->rename_tag_remove_csv, state->rename_tag_remove_csv,
          sizeof(out_state->rename_tag_remove_csv) - 1);
  out_state->rename_tag_remove_csv[sizeof(out_state->rename_tag_remove_csv) - 1] =
      '\0';
  strncpy(out_state->rename_meta_tag_add_csv, state->rename_meta_tag_add_csv,
          sizeof(out_state->rename_meta_tag_add_csv) - 1);
  out_state
      ->rename_meta_tag_add_csv[sizeof(out_state->rename_meta_tag_add_csv) - 1] =
      '\0';
  strncpy(out_state->rename_meta_tag_remove_csv,
          state->rename_meta_tag_remove_csv,
          sizeof(out_state->rename_meta_tag_remove_csv) - 1);
  out_state->rename_meta_tag_remove_csv
      [sizeof(out_state->rename_meta_tag_remove_csv) - 1] = '\0';
  strncpy(out_state->rename_preview_id, state->rename_preview_id_input,
          sizeof(out_state->rename_preview_id) - 1);
  out_state->rename_preview_id[sizeof(out_state->rename_preview_id) - 1] = '\0';
  strncpy(out_state->rename_operation_id, state->rename_operation_id_input,
          sizeof(out_state->rename_operation_id) - 1);
  out_state->rename_operation_id[sizeof(out_state->rename_operation_id) - 1] =
      '\0';
  strncpy(out_state->rename_history_id_prefix,
          state->rename_history_id_prefix_input,
          sizeof(out_state->rename_history_id_prefix) - 1);
  out_state->rename_history_id_prefix
      [sizeof(out_state->rename_history_id_prefix) - 1] = '\0';
  strncpy(out_state->rename_history_from, state->rename_history_from_input,
          sizeof(out_state->rename_history_from) - 1);
  out_state->rename_history_from[sizeof(out_state->rename_history_from) - 1] =
      '\0';
  strncpy(out_state->rename_history_to, state->rename_history_to_input,
          sizeof(out_state->rename_history_to) - 1);
  out_state->rename_history_to[sizeof(out_state->rename_history_to) - 1] = '\0';
  strncpy(out_state->rename_history_export_path, state->rename_history_export_path,
          sizeof(out_state->rename_history_export_path) - 1);
  out_state->rename_history_export_path
      [sizeof(out_state->rename_history_export_path) - 1] = '\0';
  int prune_keep = 200;
  if (GuiUiTryParseIntStrict(state->rename_history_prune_keep_input,
                             &prune_keep)) {
    if (prune_keep < 0) {
      prune_keep = 0;
    } else if (prune_keep > 1000000) {
      prune_keep = 1000000;
    }
  }
  out_state->rename_history_prune_keep_count = prune_keep;
  if (state->rename_history_rollback_filter_mode < 0) {
    out_state->rename_history_rollback_filter_mode = 0;
  } else if (state->rename_history_rollback_filter_mode > 2) {
    out_state->rename_history_rollback_filter_mode = 2;
  } else {
    out_state->rename_history_rollback_filter_mode =
        state->rename_history_rollback_filter_mode;
  }
  out_state->rename_accept_auto_suffix = state->rename_accept_auto_suffix;
}

void GuiUiApplyPersistedStateToUi(GuiUiState *state, const GuiState *saved) {
  if (!state || !saved) {
    return;
  }

  strncpy(state->gallery_dir, saved->gallery_dir, sizeof(state->gallery_dir) - 1);
  state->gallery_dir[sizeof(state->gallery_dir) - 1] = '\0';
  strncpy(state->env_dir, saved->env_dir, sizeof(state->env_dir) - 1);
  state->env_dir[sizeof(state->env_dir) - 1] = '\0';

  state->exhaustive = saved->scan_exhaustive;
  state->jobs = saved->scan_jobs;
  if (state->jobs < 1) {
    state->jobs = 1;
  } else if (state->jobs > 256) {
    state->jobs = 256;
  }

  state->cache_mode = saved->scan_cache_mode == APP_CACHE_COMPRESSION_ZSTD
                          ? APP_CACHE_COMPRESSION_ZSTD
                          : APP_CACHE_COMPRESSION_NONE;
  state->cache_level = saved->scan_cache_level;
  if (state->cache_level < 1) {
    state->cache_level = 1;
  } else if (state->cache_level > 19) {
    state->cache_level = 19;
  }

  state->sim_incremental = saved->sim_incremental;
  state->sim_threshold = saved->sim_threshold;
  if (state->sim_threshold < 0.0f) {
    state->sim_threshold = 0.0f;
  } else if (state->sim_threshold > 1.0f) {
    state->sim_threshold = 1.0f;
  }

  state->sim_topk = saved->sim_topk;
  if (state->sim_topk < 1) {
    state->sim_topk = 1;
  } else if (state->sim_topk > 1000) {
    state->sim_topk = 1000;
  }

  state->sim_memory_mode = saved->sim_memory_mode == APP_SIM_MEMORY_EAGER
                               ? APP_SIM_MEMORY_EAGER
                               : APP_SIM_MEMORY_CHUNKED;

  if (saved->organize_group_by[0] != '\0') {
    strncpy(state->group_by, saved->organize_group_by, sizeof(state->group_by) - 1);
    state->group_by[sizeof(state->group_by) - 1] = '\0';
  } else {
    strncpy(state->group_by, "date", sizeof(state->group_by) - 1);
    state->group_by[sizeof(state->group_by) - 1] = '\0';
  }

  if (saved->rename_pattern[0] != '\0') {
    strncpy(state->rename_pattern_input, saved->rename_pattern,
            sizeof(state->rename_pattern_input) - 1);
    state->rename_pattern_input[sizeof(state->rename_pattern_input) - 1] = '\0';
  } else {
    strncpy(state->rename_pattern_input, "pic-[datetime]-[index].[format]",
            sizeof(state->rename_pattern_input) - 1);
    state->rename_pattern_input[sizeof(state->rename_pattern_input) - 1] = '\0';
  }
  strncpy(state->rename_tags_map_path, saved->rename_tags_map_path,
          sizeof(state->rename_tags_map_path) - 1);
  state->rename_tags_map_path[sizeof(state->rename_tags_map_path) - 1] = '\0';
  strncpy(state->rename_tag_add_csv, saved->rename_tag_add_csv,
          sizeof(state->rename_tag_add_csv) - 1);
  state->rename_tag_add_csv[sizeof(state->rename_tag_add_csv) - 1] = '\0';
  strncpy(state->rename_tag_remove_csv, saved->rename_tag_remove_csv,
          sizeof(state->rename_tag_remove_csv) - 1);
  state->rename_tag_remove_csv[sizeof(state->rename_tag_remove_csv) - 1] = '\0';
  strncpy(state->rename_meta_tag_add_csv, saved->rename_meta_tag_add_csv,
          sizeof(state->rename_meta_tag_add_csv) - 1);
  state->rename_meta_tag_add_csv[sizeof(state->rename_meta_tag_add_csv) - 1] =
      '\0';
  strncpy(state->rename_meta_tag_remove_csv, saved->rename_meta_tag_remove_csv,
          sizeof(state->rename_meta_tag_remove_csv) - 1);
  state->rename_meta_tag_remove_csv
      [sizeof(state->rename_meta_tag_remove_csv) - 1] = '\0';
  strncpy(state->rename_preview_id_input, saved->rename_preview_id,
          sizeof(state->rename_preview_id_input) - 1);
  state->rename_preview_id_input[sizeof(state->rename_preview_id_input) - 1] =
      '\0';
  strncpy(state->rename_operation_id_input, saved->rename_operation_id,
          sizeof(state->rename_operation_id_input) - 1);
  state->rename_operation_id_input[sizeof(state->rename_operation_id_input) - 1] =
      '\0';
  strncpy(state->rename_history_id_prefix_input, saved->rename_history_id_prefix,
          sizeof(state->rename_history_id_prefix_input) - 1);
  state->rename_history_id_prefix_input
      [sizeof(state->rename_history_id_prefix_input) - 1] = '\0';
  strncpy(state->rename_history_from_input, saved->rename_history_from,
          sizeof(state->rename_history_from_input) - 1);
  state->rename_history_from_input[sizeof(state->rename_history_from_input) - 1] =
      '\0';
  strncpy(state->rename_history_to_input, saved->rename_history_to,
          sizeof(state->rename_history_to_input) - 1);
  state->rename_history_to_input[sizeof(state->rename_history_to_input) - 1] =
      '\0';
  strncpy(state->rename_history_export_path, saved->rename_history_export_path,
          sizeof(state->rename_history_export_path) - 1);
  state->rename_history_export_path[sizeof(state->rename_history_export_path) - 1] =
      '\0';
  int prune_keep = saved->rename_history_prune_keep_count;
  if (prune_keep < 0) {
    prune_keep = 0;
  } else if (prune_keep > 1000000) {
    prune_keep = 1000000;
  }
  snprintf(state->rename_history_prune_keep_input,
           sizeof(state->rename_history_prune_keep_input), "%d", prune_keep);
  if (saved->rename_history_rollback_filter_mode < 0) {
    state->rename_history_rollback_filter_mode = 0;
  } else if (saved->rename_history_rollback_filter_mode > 2) {
    state->rename_history_rollback_filter_mode = 2;
  } else {
    state->rename_history_rollback_filter_mode =
        saved->rename_history_rollback_filter_mode;
  }
  state->rename_accept_auto_suffix = saved->rename_accept_auto_suffix;

  GuiUiSyncNumericInputBuffers(state);
}
