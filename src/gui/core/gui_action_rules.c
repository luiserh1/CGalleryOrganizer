#include <stdio.h>
#include <string.h>

#include "gui/core/gui_action_rules.h"

static void SetDisabled(GuiActionAvailability *out, const char *reason) {
  if (!out) {
    return;
  }
  out->enabled = false;
  out->reason[0] = '\0';
  if (reason) {
    strncpy(out->reason, reason, sizeof(out->reason) - 1);
    out->reason[sizeof(out->reason) - 1] = '\0';
  }
}

static void SetEnabled(GuiActionAvailability *out) {
  if (!out) {
    return;
  }
  out->enabled = true;
  out->reason[0] = '\0';
}

static bool HasGalleryDir(const GuiUiState *state) {
  return state && state->gallery_dir[0] != '\0';
}

static bool HasEnvDir(const GuiUiState *state) {
  return state && state->env_dir[0] != '\0';
}

static bool HasModelsReady(const GuiUiState *state, char *out_reason,
                           size_t out_reason_size) {
  if (!state) {
    return false;
  }
  if (!state->runtime_state_valid) {
    if (out_reason && out_reason_size > 0) {
      if (state->runtime_error[0] != '\0') {
        snprintf(out_reason, out_reason_size, "Runtime check failed: %s",
                 state->runtime_error);
      } else {
        snprintf(out_reason, out_reason_size, "Runtime state not available");
      }
    }
    return false;
  }

  if (!state->runtime_state.models.classification_present ||
      !state->runtime_state.models.text_detection_present ||
      !state->runtime_state.models.embedding_present) {
    if (out_reason && out_reason_size > 0) {
      char missing[128] = {0};
      int total_missing = state->runtime_state.models.missing_count;
      int printable = total_missing > 3 ? 3 : total_missing;
      for (int i = 0; i < printable; i++) {
        if (i > 0) {
          strncat(missing, ", ", sizeof(missing) - strlen(missing) - 1);
        }
        strncat(missing, state->runtime_state.models.missing_ids[i],
                sizeof(missing) - strlen(missing) - 1);
      }
      snprintf(out_reason, out_reason_size,
               "Missing models: %s. Use Download Models first",
               missing[0] != '\0' ? missing : "required artifacts");
    }
    return false;
  }
  return true;
}

void GuiResolveActionAvailability(const GuiUiState *state, GuiActionId action_id,
                                  GuiActionAvailability *out) {
  if (!state || !out) {
    return;
  }

  if (state->worker_snapshot.busy) {
    SetDisabled(out, "Another task is currently running");
    return;
  }

  switch (action_id) {
  case GUI_ACTION_SCAN_CACHE:
    if (!HasGalleryDir(state)) {
      SetDisabled(out, "Scan/Cache requires a Gallery Directory");
      return;
    }
    if (!HasEnvDir(state)) {
      SetDisabled(out, "Scan/Cache requires an Environment Dir");
      return;
    }
    SetEnabled(out);
    return;

  case GUI_ACTION_ML_ENRICH: {
    if (!HasGalleryDir(state)) {
      SetDisabled(out, "ML Enrich requires a Gallery Directory");
      return;
    }
    if (!HasEnvDir(state)) {
      SetDisabled(out, "ML Enrich requires an Environment Dir");
      return;
    }
    char reason[APP_MAX_ERROR] = {0};
    if (!HasModelsReady(state, reason, sizeof(reason))) {
      SetDisabled(out, reason);
      return;
    }
    SetEnabled(out);
    return;
  }

  case GUI_ACTION_SIMILARITY_REPORT: {
    if (!HasGalleryDir(state)) {
      SetDisabled(out, "Similarity requires a Gallery Directory");
      return;
    }
    if (!HasEnvDir(state)) {
      SetDisabled(out, "Similarity requires an Environment Dir");
      return;
    }
    char reason[APP_MAX_ERROR] = {0};
    if (!HasModelsReady(state, reason, sizeof(reason))) {
      SetDisabled(out, reason);
      return;
    }
    SetEnabled(out);
    return;
  }

  case GUI_ACTION_ORGANIZE_PREVIEW:
  case GUI_ACTION_ORGANIZE_EXECUTE:
  case GUI_ACTION_DUPLICATES_ANALYZE:
    if (!HasEnvDir(state)) {
      SetDisabled(out, "This action requires an Environment Dir");
      return;
    }
    if (!state->runtime_state_valid) {
      SetDisabled(out, "Runtime state not available for cache checks");
      return;
    }
    if (!state->runtime_state.cache_exists) {
      SetDisabled(out, "Run Scan/Cache first to populate cache");
      return;
    }
    if (state->runtime_state.cache_entry_count_known &&
        state->runtime_state.cache_entry_count <= 0) {
      SetDisabled(out, "Run Scan/Cache first to populate cache");
      return;
    }
    SetEnabled(out);
    return;

  case GUI_ACTION_ROLLBACK:
    if (!HasEnvDir(state)) {
      SetDisabled(out, "Rollback requires an Environment Dir");
      return;
    }
    if (!state->runtime_state_valid) {
      SetDisabled(out, "Runtime state not available for rollback checks");
      return;
    }
    if (!state->runtime_state.rollback_manifest_exists) {
      SetDisabled(out, "Rollback manifest not found. Execute Organize first");
      return;
    }
    SetEnabled(out);
    return;

  case GUI_ACTION_DUPLICATES_MOVE:
    if (!HasEnvDir(state)) {
      SetDisabled(out, "Move duplicates requires an Environment Dir");
      return;
    }
    if (!state->worker_snapshot.duplicate_report_ready) {
      SetDisabled(out, "Run Analyze Duplicates first");
      return;
    }
    SetEnabled(out);
    return;

  case GUI_ACTION_SAVE_PATHS:
  case GUI_ACTION_DOWNLOAD_MODELS:
  case GUI_ACTION_RESET_PATHS:
    SetEnabled(out);
    return;

  default:
    SetDisabled(out, "Unknown action");
    return;
  }
}

bool GuiActionCanStartTask(const GuiUiState *state, GuiTaskType task_type,
                           char *out_reason, size_t out_reason_size) {
  if (out_reason && out_reason_size > 0) {
    out_reason[0] = '\0';
  }

  GuiActionId action_id = GUI_ACTION_SCAN_CACHE;
  switch (task_type) {
  case GUI_TASK_SCAN:
    action_id = GUI_ACTION_SCAN_CACHE;
    break;
  case GUI_TASK_ML_ENRICH:
    action_id = GUI_ACTION_ML_ENRICH;
    break;
  case GUI_TASK_SIMILARITY:
    action_id = GUI_ACTION_SIMILARITY_REPORT;
    break;
  case GUI_TASK_PREVIEW_ORGANIZE:
    action_id = GUI_ACTION_ORGANIZE_PREVIEW;
    break;
  case GUI_TASK_EXECUTE_ORGANIZE:
    action_id = GUI_ACTION_ORGANIZE_EXECUTE;
    break;
  case GUI_TASK_ROLLBACK:
    action_id = GUI_ACTION_ROLLBACK;
    break;
  case GUI_TASK_FIND_DUPLICATES:
    action_id = GUI_ACTION_DUPLICATES_ANALYZE;
    break;
  case GUI_TASK_MOVE_DUPLICATES:
    action_id = GUI_ACTION_DUPLICATES_MOVE;
    break;
  case GUI_TASK_DOWNLOAD_MODELS:
    action_id = GUI_ACTION_DOWNLOAD_MODELS;
    break;
  default:
    if (out_reason && out_reason_size > 0) {
      snprintf(out_reason, out_reason_size, "Unknown task type");
    }
    return false;
  }

  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(state, action_id, &availability);
  if (availability.enabled) {
    return true;
  }

  if (out_reason && out_reason_size > 0 && availability.reason[0] != '\0') {
    strncpy(out_reason, availability.reason, out_reason_size - 1);
    out_reason[out_reason_size - 1] = '\0';
  }
  return false;
}
