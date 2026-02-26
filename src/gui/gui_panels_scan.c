#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/gui_common.h"

static Rectangle ToRayRect(GuiLayoutRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
}

static float LabelWidth(const char *text, const GuiLayoutMetrics *metrics) {
  return GuiLayoutMeasureTextWidth(text, metrics) + (float)(metrics->gap * 2);
}

static float ActionButtonWidth(const char *text, const GuiLayoutMetrics *metrics) {
  return GuiLayoutButtonWidth(text, metrics, 120.0f * metrics->effective_scale);
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
  state->ui_scale_percent = GUI_STATE_DEFAULT_UI_SCALE_PERCENT;
  state->window_width = GUI_STATE_DEFAULT_WINDOW_WIDTH;
  state->window_height = GUI_STATE_DEFAULT_WINDOW_HEIGHT;
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
  state->ui_scale_percent = loaded.ui_scale_percent;
  state->window_width = loaded.window_width;
  state->window_height = loaded.window_height;
  state->persisted_state = loaded;
}

void GuiUiPersistState(const GuiUiState *state) {
  if (!state) {
    return;
  }

  GuiState to_save = {0};
  strncpy(to_save.gallery_dir, state->gallery_dir, sizeof(to_save.gallery_dir) - 1);
  strncpy(to_save.env_dir, state->env_dir, sizeof(to_save.env_dir) - 1);
  to_save.ui_scale_percent = GuiLayoutClampUiScalePercent(state->ui_scale_percent);
  to_save.window_width = state->window_width;
  to_save.window_height = state->window_height;
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
    GuiUiPersistState(state);
    strncpy(state->banner_message, "Task started", sizeof(state->banner_message) - 1);
    return true;
  }

  strncpy(state->banner_message, "Failed to start task",
          sizeof(state->banner_message) - 1);
  return false;
}

void GuiDrawScanPanel(GuiUiState *state, GuiLayoutRect panel_bounds,
                      const GuiLayoutMetrics *metrics) {
  if (!state || !metrics) {
    return;
  }

  GuiLayoutContext ctx = {0};
  GuiLayoutInit(&ctx, panel_bounds, metrics);

  GuiLayoutRect label_gallery =
      GuiLayoutPlaceFixed(&ctx, LabelWidth("Gallery Directory", metrics),
                          (float)metrics->label_h);
  GuiLabel(ToRayRect(label_gallery), "Gallery Directory");
  GuiLayoutRect input_gallery =
      GuiLayoutPlaceFlex(&ctx, (float)metrics->min_text_input_w, (float)metrics->input_h);
  GuiTextBox(ToRayRect(input_gallery), state->gallery_dir,
             (int)sizeof(state->gallery_dir), true);

  GuiLayoutNextLine(&ctx);

  GuiLayoutRect label_env =
      GuiLayoutPlaceFixed(&ctx, LabelWidth("Environment Dir", metrics),
                          (float)metrics->label_h);
  GuiLabel(ToRayRect(label_env), "Environment Dir");
  GuiLayoutRect input_env =
      GuiLayoutPlaceFlex(&ctx, (float)metrics->min_text_input_w, (float)metrics->input_h);
  GuiTextBox(ToRayRect(input_env), state->env_dir, (int)sizeof(state->env_dir), true);

  GuiLayoutNextLine(&ctx);

  GuiLayoutRect exhaustive_rect =
      GuiLayoutPlaceFixed(&ctx, GuiLayoutButtonWidth("Exhaustive metadata", metrics,
                                                     210.0f * metrics->effective_scale),
                          (float)metrics->label_h);
  if (GuiCheckBox(ToRayRect(exhaustive_rect), "Exhaustive metadata",
                  state->exhaustive)) {
    state->exhaustive = !state->exhaustive;
  }

  GuiLayoutNextLine(&ctx);

  GuiLayoutRect jobs_label =
      GuiLayoutPlaceFixed(&ctx, LabelWidth("Jobs", metrics), (float)metrics->label_h);
  GuiLabel(ToRayRect(jobs_label), "Jobs");
  {
    char jobs_buf[16] = {0};
    snprintf(jobs_buf, sizeof(jobs_buf), "%d", state->jobs);
    GuiLayoutRect jobs_input = GuiLayoutPlaceFixed(
        &ctx, (float)metrics->min_numeric_input_w, (float)metrics->input_h);
    if (GuiTextBox(ToRayRect(jobs_input), jobs_buf, sizeof(jobs_buf), true)) {
      int parsed = atoi(jobs_buf);
      if (parsed > 0 && parsed <= 256) {
        state->jobs = parsed;
      }
    }
  }

  GuiLayoutRect cache_label =
      GuiLayoutPlaceFixed(&ctx, LabelWidth("Cache compression", metrics),
                          (float)metrics->label_h);
  GuiLabel(ToRayRect(cache_label), "Cache compression");

  float mode_min_width = 64.0f * metrics->effective_scale;
  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, GuiLayoutButtonWidth("none", metrics, mode_min_width),
                    (float)metrics->button_h)),
                "none")) {
    state->cache_mode = APP_CACHE_COMPRESSION_NONE;
  }
  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, GuiLayoutButtonWidth("zstd", metrics, mode_min_width),
                    (float)metrics->button_h)),
                "zstd")) {
    state->cache_mode = APP_CACHE_COMPRESSION_ZSTD;
  }
  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, GuiLayoutButtonWidth("auto", metrics, mode_min_width),
                    (float)metrics->button_h)),
                "auto")) {
    state->cache_mode = APP_CACHE_COMPRESSION_AUTO;
  }

  GuiLayoutRect level_label =
      GuiLayoutPlaceFixed(&ctx, LabelWidth("Level", metrics), (float)metrics->label_h);
  GuiLabel(ToRayRect(level_label), "Level");
  {
    char lvl_buf[8] = {0};
    snprintf(lvl_buf, sizeof(lvl_buf), "%d", state->cache_level);
    GuiLayoutRect level_input = GuiLayoutPlaceFixed(
        &ctx, (float)metrics->min_numeric_input_w, (float)metrics->input_h);
    if (GuiTextBox(ToRayRect(level_input), lvl_buf, sizeof(lvl_buf), true)) {
      int parsed = atoi(lvl_buf);
      if (parsed >= 1 && parsed <= 19) {
        state->cache_level = parsed;
      }
    }
  }

  GuiLayoutNextLine(&ctx);

  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, ActionButtonWidth("Scan/Cache", metrics),
                    (float)metrics->button_h)),
                "Scan/Cache")) {
    GuiUiStartTask(state, GUI_TASK_SCAN);
  }
  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, ActionButtonWidth("ML Enrich", metrics),
                    (float)metrics->button_h)),
                "ML Enrich")) {
    GuiUiStartTask(state, GUI_TASK_ML_ENRICH);
  }
  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, ActionButtonWidth("Save Paths", metrics),
                    (float)metrics->button_h)),
                "Save Paths")) {
    GuiUiPersistState(state);
    strncpy(state->banner_message, "Paths saved", sizeof(state->banner_message) - 1);
  }
  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, ActionButtonWidth("Reset Saved Paths", metrics),
                    (float)metrics->button_h)),
                "Reset Saved Paths")) {
    GuiStateReset();
    state->gallery_dir[0] = '\0';
    state->env_dir[0] = '\0';
    state->ui_scale_percent = GUI_STATE_DEFAULT_UI_SCALE_PERCENT;
    state->window_width = GUI_STATE_DEFAULT_WINDOW_WIDTH;
    state->window_height = GUI_STATE_DEFAULT_WINDOW_HEIGHT;
    strncpy(state->banner_message, "Saved GUI state reset",
            sizeof(state->banner_message) - 1);
  }

#if defined(CGO_GUI_LAYOUT_DEBUG)
  if (!GuiLayoutContextIsValid(&ctx)) {
    strncpy(state->banner_message, "Layout warning: scan panel overlap/bounds",
            sizeof(state->banner_message) - 1);
  }
#endif
}
