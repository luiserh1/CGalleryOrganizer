#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gui/core/gui_ui_state_internal.h"
#include "gui/gui_common.h"

static void GuiUiSetBannerMessage(GuiUiState *state, const char *message,
                                  bool is_error) {
  if (!state) {
    return;
  }

  state->banner_message[0] = '\0';
  state->banner_is_error = is_error;

  if (!message) {
    return;
  }

  strncpy(state->banner_message, message, sizeof(state->banner_message) - 1);
  state->banner_message[sizeof(state->banner_message) - 1] = '\0';
}

void GuiUiSetBannerInfo(GuiUiState *state, const char *message) {
  GuiUiSetBannerMessage(state, message, false);
}

void GuiUiSetBannerError(GuiUiState *state, const char *message) {
  GuiUiSetBannerMessage(state, message, true);
}

bool GuiUiTryParseIntStrict(const char *text, int *out_value) {
  if (!text || !out_value) {
    return false;
  }

  char *endptr = NULL;
  long parsed = strtol(text, &endptr, 10);
  if (!endptr || *endptr != '\0') {
    return false;
  }
  if (parsed < INT_MIN || parsed > INT_MAX) {
    return false;
  }
  *out_value = (int)parsed;
  return true;
}

bool GuiUiTryParseFloatStrict(const char *text, float *out_value) {
  if (!text || !out_value) {
    return false;
  }

  char *endptr = NULL;
  float parsed = strtof(text, &endptr);
  if (!endptr || *endptr != '\0') {
    return false;
  }
  *out_value = parsed;
  return true;
}

static void SyncNumericInputBuffers(GuiUiState *state) {
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

static void BuildPersistedStateFromUi(const GuiUiState *state,
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
}

static void ApplyPersistedStateToUi(GuiUiState *state, const GuiState *saved) {
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

  SyncNumericInputBuffers(state);
}

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

  SyncNumericInputBuffers(state);
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
  state->runtime_state_valid = false;
  state->rebuild_confirm_pending = false;
  state->rebuild_confirm_task = GUI_TASK_NONE;
  state->rebuild_confirm_reason[0] = '\0';
  SyncNumericInputBuffers(state);

  BuildPersistedStateFromUi(state, &state->persisted_state);
}

void GuiUiSyncFromStateFile(GuiUiState *state) {
  if (!state) {
    return;
  }

  GuiState loaded = {0};
  if (!GuiStateLoad(&loaded)) {
    return;
  }

  ApplyPersistedStateToUi(state, &loaded);
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
  BuildPersistedStateFromUi(state, &to_save);
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
  BuildPersistedStateFromUi(state, &current_state);

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

  return false;
}
