#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdlib.h>

#include "gui/core/gui_action_rules.h"
#include "gui/gui_common.h"

static bool TryParseIntStrict(const char *text, int *out_value) {
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

static bool TryParseFloatStrict(const char *text, float *out_value) {
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

static bool TaskUsesCacheCompressionSettings(GuiTaskType task_type) {
  switch (task_type) {
  case GUI_TASK_SCAN:
  case GUI_TASK_ML_ENRICH:
  case GUI_TASK_SIMILARITY:
  case GUI_TASK_PREVIEW_ORGANIZE:
  case GUI_TASK_EXECUTE_ORGANIZE:
  case GUI_TASK_FIND_DUPLICATES:
    return true;
  default:
    return false;
  }
}

static bool TaskUsesScanJobsSetting(GuiTaskType task_type) {
  return task_type == GUI_TASK_SCAN || task_type == GUI_TASK_ML_ENRICH ||
         task_type == GUI_TASK_SIMILARITY;
}

static bool GuiUiValidateNumericTaskInputs(GuiUiState *state,
                                           GuiTaskType task_type) {
  if (!state) {
    return false;
  }

  if (TaskUsesScanJobsSetting(task_type)) {
    int jobs = 0;
    if (!TryParseIntStrict(state->jobs_input, &jobs) || jobs < 1 || jobs > 256) {
      strncpy(state->banner_message, "Jobs must be an integer between 1 and 256",
              sizeof(state->banner_message) - 1);
      return false;
    }
    state->jobs = jobs;
  }

  if (TaskUsesCacheCompressionSettings(task_type) &&
      state->cache_mode == APP_CACHE_COMPRESSION_ZSTD) {
    int level = 0;
    if (!TryParseIntStrict(state->cache_level_input, &level) || level < 1 ||
        level > 19) {
      strncpy(state->banner_message,
              "Compression level must be an integer between 1 and 19",
              sizeof(state->banner_message) - 1);
      return false;
    }
    state->cache_level = level;
  }

  if (task_type == GUI_TASK_SIMILARITY) {
    float threshold = 0.0f;
    if (!TryParseFloatStrict(state->sim_threshold_input, &threshold) ||
        threshold < 0.0f || threshold > 1.0f) {
      strncpy(state->banner_message,
              "Similarity threshold must be a number between 0.000 and 1.000",
              sizeof(state->banner_message) - 1);
      return false;
    }
    state->sim_threshold = threshold;

    int topk = 0;
    if (!TryParseIntStrict(state->sim_topk_input, &topk) || topk < 1 ||
        topk > 1000) {
      strncpy(state->banner_message, "TopK must be an integer between 1 and 1000",
              sizeof(state->banner_message) - 1);
      return false;
    }
    state->sim_topk = topk;
  }

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
  state->runtime_state_valid = false;
  SyncNumericInputBuffers(state);
}

void GuiUiSyncFromStateFile(GuiUiState *state) {
  if (!state) {
    return;
  }

  GuiState loaded = {0};
  if (!GuiStateLoad(&loaded)) {
    return;
  }

  strncpy(state->gallery_dir, loaded.gallery_dir, sizeof(state->gallery_dir) - 1);
  strncpy(state->env_dir, loaded.env_dir, sizeof(state->env_dir) - 1);
  state->persisted_state = loaded;
}

bool GuiUiPersistState(GuiUiState *state) {
  if (!state) {
    return false;
  }

  GuiState to_save = {0};
  strncpy(to_save.gallery_dir, state->gallery_dir, sizeof(to_save.gallery_dir) - 1);
  strncpy(to_save.env_dir, state->env_dir, sizeof(to_save.env_dir) - 1);
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

  if (strncmp(state->gallery_dir, state->persisted_state.gallery_dir,
              sizeof(state->gallery_dir)) != 0) {
    return true;
  }

  if (strncmp(state->env_dir, state->persisted_state.env_dir,
              sizeof(state->env_dir)) != 0) {
    return true;
  }

  return false;
}

void GuiUiRefreshRuntimeState(GuiUiState *state) {
  if (!state) {
    return;
  }

  AppRuntimeStateRequest request = {
      .env_dir = state->env_dir[0] != '\0' ? state->env_dir : NULL,
      .models_root_override = NULL,
  };

  AppRuntimeState runtime_state = {0};
  char error[APP_MAX_ERROR] = {0};
  bool ok = GuiWorkerInspectRuntimeState(&request, &runtime_state, error,
                                         sizeof(error));
  if (!ok) {
    state->runtime_state_valid = false;
    if (error[0] != '\0') {
      strncpy(state->runtime_error, error, sizeof(state->runtime_error) - 1);
    } else {
      state->runtime_error[0] = '\0';
    }
    return;
  }

  state->runtime_state = runtime_state;
  state->runtime_state_valid = true;
  state->runtime_error[0] = '\0';
}

bool GuiUiStartTask(GuiUiState *state, GuiTaskType task_type) {
  if (!state) {
    return false;
  }

  if (state->worker_snapshot.busy) {
    strncpy(state->banner_message, "Another task is currently running",
            sizeof(state->banner_message) - 1);
    return false;
  }

  if (!GuiUiValidateNumericTaskInputs(state, task_type)) {
    return false;
  }

  GuiUiRefreshRuntimeState(state);
  char reason[APP_MAX_ERROR] = {0};
  if (!GuiActionCanStartTask(state, task_type, reason, sizeof(reason))) {
    if (reason[0] != '\0') {
      strncpy(state->banner_message, reason, sizeof(state->banner_message) - 1);
    } else {
      strncpy(state->banner_message, "Task prerequisites are not satisfied",
              sizeof(state->banner_message) - 1);
    }
    return false;
  }

  GuiTaskInput input = {0};
  input.type = task_type;
  strncpy(input.gallery_dir, state->gallery_dir, sizeof(input.gallery_dir) - 1);
  strncpy(input.env_dir, state->env_dir, sizeof(input.env_dir) - 1);
  input.exhaustive = state->exhaustive;
  input.jobs = state->jobs > 0 ? state->jobs : 1;
  input.cache_mode = state->cache_mode;
  input.cache_level = state->cache_level;
  input.sim_incremental = state->sim_incremental;
  input.sim_threshold = state->sim_threshold;
  input.sim_topk = state->sim_topk;
  input.sim_memory_mode = state->sim_memory_mode;
  strncpy(input.group_by, state->group_by, sizeof(input.group_by) - 1);

  if (GuiWorkerStartTask(&input)) {
    strncpy(state->banner_message, "Task started", sizeof(state->banner_message) - 1);
    return true;
  }

  strncpy(state->banner_message, "Failed to start task",
          sizeof(state->banner_message) - 1);
  return false;
}
