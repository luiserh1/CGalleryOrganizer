#include <stdio.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/gui_common.h"

static Rectangle ToRayRect(GuiLayoutRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
}

static bool ActionButton(const GuiUiState *state, Rectangle bounds, const char *text) {
  bool enabled = !state->worker_snapshot.busy;
  return GuiButtonStyled(bounds, text, enabled, false);
}

void GuiDrawDuplicatesPanel(GuiUiState *state, GuiLayoutRect panel_bounds,
                            const GuiLayoutMetrics *metrics) {
  if (!state || !metrics) {
    return;
  }

  GuiLayoutContext ctx = {0};
  GuiLayoutInit(&ctx, panel_bounds, metrics);

  GuiLayoutRect info_top = GuiLayoutPlaceFlex(
      &ctx, 320.0f * metrics->effective_scale, (float)metrics->label_h);
  GuiLabel(ToRayRect(info_top),
           "Duplicate operations use the current environment cache");

  GuiLayoutNextLine(&ctx);

  if (ActionButton(
          state,
          ToRayRect(GuiLayoutPlaceFixed(
              &ctx, GuiLayoutButtonWidth("Analyze Duplicates", metrics,
                                         185.0f * metrics->effective_scale),
              (float)metrics->button_h)),
          "Analyze Duplicates")) {
    GuiUiStartTask(state, GUI_TASK_FIND_DUPLICATES);
  }

  if (ActionButton(
          state,
          ToRayRect(GuiLayoutPlaceFixed(
              &ctx, GuiLayoutButtonWidth("Move Duplicates to Env", metrics,
                                         220.0f * metrics->effective_scale),
              (float)metrics->button_h)),
          "Move Duplicates to Env")) {
    GuiUiStartTask(state, GUI_TASK_MOVE_DUPLICATES);
  }

  GuiLayoutNextLine(&ctx);

  char info[128] = {0};
  snprintf(info, sizeof(info), "Last duplicate groups: %d",
           state->worker_snapshot.duplicate_group_count);
  GuiLayoutRect info_bottom = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth(info, metrics, 240.0f * metrics->effective_scale),
      (float)metrics->label_h);
  GuiLabel(ToRayRect(info_bottom), info);

#if defined(CGO_GUI_LAYOUT_DEBUG)
  if (!GuiLayoutContextIsValid(&ctx)) {
    strncpy(state->banner_message,
            "Layout warning: duplicates panel overlap/bounds",
            sizeof(state->banner_message) - 1);
  }
#endif
}
