#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"

#include "gui/gui_common.h"

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

void GuiUiPersistPaths(const GuiUiState *state) {
  if (!state) {
    return;
  }

  GuiState to_save = {0};
  strncpy(to_save.gallery_dir, state->gallery_dir, sizeof(to_save.gallery_dir) - 1);
  strncpy(to_save.env_dir, state->env_dir, sizeof(to_save.env_dir) - 1);
  GuiStateSave(&to_save);
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

  if ((task_type == GUI_TASK_SCAN || task_type == GUI_TASK_ML_ENRICH ||
       task_type == GUI_TASK_SIMILARITY) &&
      input.gallery_dir[0] == '\0') {
    strncpy(state->banner_message, "Gallery directory is required",
            sizeof(state->banner_message) - 1);
    return false;
  }

  if ((task_type == GUI_TASK_SIMILARITY || task_type == GUI_TASK_PREVIEW_ORGANIZE ||
       task_type == GUI_TASK_EXECUTE_ORGANIZE || task_type == GUI_TASK_ROLLBACK ||
       task_type == GUI_TASK_FIND_DUPLICATES || task_type == GUI_TASK_MOVE_DUPLICATES) &&
      input.env_dir[0] == '\0') {
    strncpy(state->banner_message, "Environment directory is required",
            sizeof(state->banner_message) - 1);
    return false;
  }

  if (GuiWorkerStartTask(&input)) {
    GuiUiPersistPaths(state);
    strncpy(state->banner_message, "Task started", sizeof(state->banner_message) - 1);
    return true;
  }

  strncpy(state->banner_message, "Failed to start task",
          sizeof(state->banner_message) - 1);
  return false;
}

void GuiDrawScanPanel(GuiUiState *state) {
  if (!state) {
    return;
  }

  GuiLabel((Rectangle){30, 120, 120, 24}, "Gallery Directory");
  GuiTextBox((Rectangle){170, 116, 760, 30}, state->gallery_dir,
             (int)sizeof(state->gallery_dir), true);

  GuiLabel((Rectangle){30, 160, 120, 24}, "Environment Dir");
  GuiTextBox((Rectangle){170, 156, 760, 30}, state->env_dir,
             (int)sizeof(state->env_dir), true);

  if (GuiCheckBox((Rectangle){30, 200, 240, 24}, "Exhaustive metadata",
                  state->exhaustive)) {
    state->exhaustive = !state->exhaustive;
  }

  GuiLabel((Rectangle){30, 236, 120, 24}, "Jobs");
  {
    char jobs_buf[16] = {0};
    snprintf(jobs_buf, sizeof(jobs_buf), "%d", state->jobs);
    if (GuiTextBox((Rectangle){170, 232, 90, 30}, jobs_buf, sizeof(jobs_buf),
                   true)) {
      int parsed = atoi(jobs_buf);
      if (parsed > 0 && parsed <= 256) {
        state->jobs = parsed;
      }
    }
  }

  GuiLabel((Rectangle){290, 236, 180, 24}, "Cache compression");
  if (GuiButton((Rectangle){440, 232, 70, 30}, "none")) {
    state->cache_mode = APP_CACHE_COMPRESSION_NONE;
  }
  if (GuiButton((Rectangle){520, 232, 70, 30}, "zstd")) {
    state->cache_mode = APP_CACHE_COMPRESSION_ZSTD;
  }
  if (GuiButton((Rectangle){600, 232, 70, 30}, "auto")) {
    state->cache_mode = APP_CACHE_COMPRESSION_AUTO;
  }

  GuiLabel((Rectangle){690, 236, 80, 24}, "Level");
  {
    char lvl_buf[8] = {0};
    snprintf(lvl_buf, sizeof(lvl_buf), "%d", state->cache_level);
    if (GuiTextBox((Rectangle){760, 232, 60, 30}, lvl_buf, sizeof(lvl_buf),
                   true)) {
      int parsed = atoi(lvl_buf);
      if (parsed >= 1 && parsed <= 19) {
        state->cache_level = parsed;
      }
    }
  }

  if (GuiButton((Rectangle){30, 282, 140, 36}, "Scan/Cache")) {
    GuiUiStartTask(state, GUI_TASK_SCAN);
  }
  if (GuiButton((Rectangle){190, 282, 140, 36}, "ML Enrich")) {
    GuiUiStartTask(state, GUI_TASK_ML_ENRICH);
  }
  if (GuiButton((Rectangle){350, 282, 180, 36}, "Save Paths")) {
    GuiUiPersistPaths(state);
    strncpy(state->banner_message, "Paths saved", sizeof(state->banner_message) - 1);
  }
  if (GuiButton((Rectangle){550, 282, 180, 36}, "Reset Saved Paths")) {
    GuiStateReset();
    state->gallery_dir[0] = '\0';
    state->env_dir[0] = '\0';
    strncpy(state->banner_message, "Saved GUI state reset",
            sizeof(state->banner_message) - 1);
  }
}
