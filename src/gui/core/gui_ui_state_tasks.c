#include <string.h>

#include "gui/core/gui_action_rules.h"
#include "gui/core/gui_ui_state_internal.h"
#include "gui/gui_common.h"

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

static bool TaskRequiresScanProfilePreflight(GuiTaskType task_type) {
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
    if (!GuiUiTryParseIntStrict(state->jobs_input, &jobs) || jobs < 1 ||
        jobs > 256) {
      GuiUiSetBannerError(state, "Jobs must be an integer between 1 and 256");
      return false;
    }
    state->jobs = jobs;
  }

  if (TaskUsesCacheCompressionSettings(task_type) &&
      state->cache_mode == APP_CACHE_COMPRESSION_ZSTD) {
    int level = 0;
    if (!GuiUiTryParseIntStrict(state->cache_level_input, &level) || level < 1 ||
        level > 19) {
      GuiUiSetBannerError(state,
                          "Compression level must be an integer between 1 and 19");
      return false;
    }
    state->cache_level = level;
  }

  if (task_type == GUI_TASK_SIMILARITY) {
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
  }

  return true;
}

static bool BuildScanProfileRequestForTask(const GuiUiState *state,
                                           GuiTaskType task_type,
                                           AppScanRequest *out_request) {
  if (!state || !out_request || !TaskRequiresScanProfilePreflight(task_type)) {
    return false;
  }

  memset(out_request, 0, sizeof(*out_request));
  out_request->target_dir = state->gallery_dir;
  out_request->env_dir = state->env_dir;
  out_request->exhaustive = state->exhaustive;
  out_request->ml_enrich = task_type == GUI_TASK_ML_ENRICH;
  out_request->similarity_report = task_type == GUI_TASK_SIMILARITY;
  out_request->sim_incremental = state->sim_incremental;
  out_request->jobs = state->jobs > 0 ? state->jobs : 1;
  out_request->cache_compression_mode = state->cache_mode;
  out_request->cache_compression_level = state->cache_level;
  out_request->models_root_override = NULL;
  return true;
}

static bool GuiUiHandleScanProfilePreflight(GuiUiState *state,
                                            GuiTaskType task_type,
                                            bool *out_requires_confirmation) {
  if (!state || !out_requires_confirmation) {
    return false;
  }

  *out_requires_confirmation = false;

  if (!TaskRequiresScanProfilePreflight(task_type)) {
    return true;
  }

  AppScanRequest preflight_request = {0};
  if (!BuildScanProfileRequestForTask(state, task_type, &preflight_request)) {
    GuiUiSetBannerError(state, "Failed to prepare scan profile preflight request");
    return false;
  }

  AppScanProfileDecision decision = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!GuiWorkerInspectScanProfile(&preflight_request, &decision, error,
                                   sizeof(error))) {
    if (error[0] != '\0') {
      GuiUiSetBannerError(state, error);
    } else {
      GuiUiSetBannerError(state, "Scan profile preflight failed");
    }
    return false;
  }

  if (!decision.will_rebuild_cache) {
    return true;
  }

  if (!(state->runtime_state_valid && state->runtime_state.cache_exists)) {
    return true;
  }

  state->rebuild_confirm_pending = true;
  state->rebuild_confirm_task = task_type;
  strncpy(state->rebuild_confirm_reason, decision.reason,
          sizeof(state->rebuild_confirm_reason) - 1);
  state->rebuild_confirm_reason[sizeof(state->rebuild_confirm_reason) - 1] = '\0';

  if (state->rebuild_confirm_reason[0] == '\0') {
    strncpy(state->rebuild_confirm_reason,
            "scan profile mismatch requires cache rebuild",
            sizeof(state->rebuild_confirm_reason) - 1);
    state->rebuild_confirm_reason[sizeof(state->rebuild_confirm_reason) - 1] =
        '\0';
  }

  GuiUiSetBannerError(state, "Scan profile changed. Confirm cache rebuild to continue");
  *out_requires_confirmation = true;
  return true;
}

static bool GuiUiStartTaskInternal(GuiUiState *state, GuiTaskType task_type,
                                   bool skip_rebuild_confirmation) {
  if (!state) {
    return false;
  }

  if (state->worker_snapshot.busy) {
    GuiUiSetBannerError(state, "Another task is currently running");
    return false;
  }

  if (!GuiUiValidateNumericTaskInputs(state, task_type)) {
    return false;
  }

  GuiUiRefreshRuntimeState(state);
  char reason[APP_MAX_ERROR] = {0};
  if (!GuiActionCanStartTask(state, task_type, reason, sizeof(reason))) {
    if (reason[0] != '\0') {
      GuiUiSetBannerError(state, reason);
    } else {
      GuiUiSetBannerError(state, "Task prerequisites are not satisfied");
    }
    return false;
  }

  if (!skip_rebuild_confirmation) {
    bool requires_confirmation = false;
    if (!GuiUiHandleScanProfilePreflight(state, task_type,
                                         &requires_confirmation)) {
      return false;
    }
    if (requires_confirmation) {
      return false;
    }
  }

  GuiTaskInput input = {0};
  input.type = task_type;
  strncpy(input.gallery_dir, state->gallery_dir, sizeof(input.gallery_dir) - 1);
  input.gallery_dir[sizeof(input.gallery_dir) - 1] = '\0';
  strncpy(input.env_dir, state->env_dir, sizeof(input.env_dir) - 1);
  input.env_dir[sizeof(input.env_dir) - 1] = '\0';
  input.exhaustive = state->exhaustive;
  input.jobs = state->jobs > 0 ? state->jobs : 1;
  input.cache_mode = state->cache_mode;
  input.cache_level = state->cache_level;
  input.sim_incremental = state->sim_incremental;
  input.sim_threshold = state->sim_threshold;
  input.sim_topk = state->sim_topk;
  input.sim_memory_mode = state->sim_memory_mode;
  strncpy(input.group_by, state->group_by, sizeof(input.group_by) - 1);
  input.group_by[sizeof(input.group_by) - 1] = '\0';
  strncpy(input.rename_pattern, state->rename_pattern_input,
          sizeof(input.rename_pattern) - 1);
  input.rename_pattern[sizeof(input.rename_pattern) - 1] = '\0';
  strncpy(input.rename_tags_map_path, state->rename_tags_map_path,
          sizeof(input.rename_tags_map_path) - 1);
  input.rename_tags_map_path[sizeof(input.rename_tags_map_path) - 1] = '\0';
  strncpy(input.rename_tag_add_csv, state->rename_tag_add_csv,
          sizeof(input.rename_tag_add_csv) - 1);
  input.rename_tag_add_csv[sizeof(input.rename_tag_add_csv) - 1] = '\0';
  strncpy(input.rename_tag_remove_csv, state->rename_tag_remove_csv,
          sizeof(input.rename_tag_remove_csv) - 1);
  input.rename_tag_remove_csv[sizeof(input.rename_tag_remove_csv) - 1] = '\0';
  strncpy(input.rename_preview_id, state->rename_preview_id_input,
          sizeof(input.rename_preview_id) - 1);
  input.rename_preview_id[sizeof(input.rename_preview_id) - 1] = '\0';
  strncpy(input.rename_operation_id, state->rename_operation_id_input,
          sizeof(input.rename_operation_id) - 1);
  input.rename_operation_id[sizeof(input.rename_operation_id) - 1] = '\0';
  input.rename_accept_auto_suffix = state->rename_accept_auto_suffix;

  if (GuiWorkerStartTask(&input)) {
    GuiUiSetBannerInfo(state, "Task started");
    state->rebuild_confirm_pending = false;
    state->rebuild_confirm_task = GUI_TASK_NONE;
    state->rebuild_confirm_reason[0] = '\0';
    return true;
  }

  GuiUiSetBannerError(state, "Failed to start task");
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
      state->runtime_error[sizeof(state->runtime_error) - 1] = '\0';
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
  return GuiUiStartTaskInternal(state, task_type, false);
}

bool GuiUiStartPendingRebuildTask(GuiUiState *state) {
  if (!state || !state->rebuild_confirm_pending ||
      state->rebuild_confirm_task == GUI_TASK_NONE) {
    return false;
  }

  GuiTaskType task = state->rebuild_confirm_task;
  state->rebuild_confirm_pending = false;
  state->rebuild_confirm_task = GUI_TASK_NONE;
  state->rebuild_confirm_reason[0] = '\0';
  return GuiUiStartTaskInternal(state, task, true);
}

void GuiUiCancelPendingRebuildTask(GuiUiState *state) {
  if (!state) {
    return;
  }

  state->rebuild_confirm_pending = false;
  state->rebuild_confirm_task = GUI_TASK_NONE;
  state->rebuild_confirm_reason[0] = '\0';
}
