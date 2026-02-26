#include <stdio.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_action_rules.h"
#include "gui/frontends/functional/gui_fixed_layout.h"
#include "gui/gui_common.h"

static Rectangle ToRayRect(GuiRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
}

static bool ActionButton(GuiUiState *state, Rectangle bounds, const char *text,
                         GuiActionId action_id) {
  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(state, action_id, &availability);
  bool clicked = GuiButtonStyled(bounds, text, availability.enabled, false);
  if (!availability.enabled &&
      CheckCollisionPointRec(GetMousePosition(), bounds) &&
      IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
      availability.reason[0] != '\0') {
    strncpy(state->banner_message, availability.reason,
            sizeof(state->banner_message) - 1);
  }
  return availability.enabled && clicked;
}

void GuiDrawDuplicatesPanel(GuiUiState *state, Rectangle panel_bounds) {
  if (!state) {
    return;
  }

  GuiDuplicatesPanelLayout layout = {0};
  GuiRect panel = {panel_bounds.x, panel_bounds.y, panel_bounds.width,
                   panel_bounds.height};
  GuiBuildDuplicatesPanelLayout(panel, &layout);

  GuiLabel(ToRayRect(layout.info_top),
           "Duplicate operations use the current environment cache");

  if (ActionButton(state, ToRayRect(layout.analyze_button), "Analyze Duplicates",
                   GUI_ACTION_DUPLICATES_ANALYZE)) {
    GuiUiStartTask(state, GUI_TASK_FIND_DUPLICATES);
  }

  if (ActionButton(state, ToRayRect(layout.move_button), "Move Duplicates to Env",
                   GUI_ACTION_DUPLICATES_MOVE)) {
    GuiUiStartTask(state, GUI_TASK_MOVE_DUPLICATES);
  }

  char info[128] = {0};
  snprintf(info, sizeof(info), "Last duplicate groups: %d",
           state->worker_snapshot.duplicate_group_count);
  GuiLabel(ToRayRect(layout.info_bottom), info);
}
