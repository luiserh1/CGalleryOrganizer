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

static bool ActionButton(const GuiUiState *state, Rectangle bounds, const char *text) {
  bool enabled = !state->worker_snapshot.busy;
  return GuiButtonStyled(bounds, text, enabled, false);
}

void GuiDrawOrganizePanel(GuiUiState *state, GuiLayoutRect panel_bounds,
                          const GuiLayoutMetrics *metrics) {
  if (!state || !metrics) {
    return;
  }

  GuiLayoutContext ctx = {0};
  GuiLayoutInit(&ctx, panel_bounds, metrics);

  GuiLayoutRect group_label = GuiLayoutPlaceFixed(
      &ctx, LabelWidth("Group-by keys", metrics), (float)metrics->label_h);
  GuiLabel(ToRayRect(group_label), "Group-by keys");
  GuiLayoutRect group_input = GuiLayoutPlaceFlex(
      &ctx, 260.0f * metrics->effective_scale, (float)metrics->input_h);
  GuiTextBox(ToRayRect(group_input), state->group_by, (int)sizeof(state->group_by),
             true);

  GuiLayoutNextLine(&ctx);

  if (ActionButton(
          state,
          ToRayRect(GuiLayoutPlaceFixed(
              &ctx, GuiLayoutButtonWidth("Preview Organize", metrics,
                                         170.0f * metrics->effective_scale),
              (float)metrics->button_h)),
          "Preview Organize")) {
    GuiUiStartTask(state, GUI_TASK_PREVIEW_ORGANIZE);
  }

  if (ActionButton(
          state,
          ToRayRect(GuiLayoutPlaceFixed(
              &ctx, GuiLayoutButtonWidth("Execute Organize", metrics,
                                         170.0f * metrics->effective_scale),
              (float)metrics->button_h)),
          "Execute Organize")) {
    GuiUiStartTask(state, GUI_TASK_EXECUTE_ORGANIZE);
  }

  if (ActionButton(
          state,
          ToRayRect(GuiLayoutPlaceFixed(
              &ctx, GuiLayoutButtonWidth("Rollback", metrics,
                                         130.0f * metrics->effective_scale),
              (float)metrics->button_h)),
          "Rollback")) {
    GuiUiStartTask(state, GUI_TASK_ROLLBACK);
  }

  GuiLayoutNextLine(&ctx);
  GuiLayoutRect info = GuiLayoutPlaceFlex(&ctx, 300.0f * metrics->effective_scale,
                                          (float)metrics->label_h);
  GuiLabel(ToRayRect(info),
           "Use comma-separated keys from: date,camera,format,orientation,resolution");

#if defined(CGO_GUI_LAYOUT_DEBUG)
  if (!GuiLayoutContextIsValid(&ctx)) {
    strncpy(state->banner_message, "Layout warning: organize panel overlap/bounds",
            sizeof(state->banner_message) - 1);
  }
#endif
}
