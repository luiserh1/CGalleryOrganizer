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

void GuiDrawOrganizePanel(GuiUiState *state, Rectangle panel_bounds) {
  if (!state) {
    return;
  }

  GuiOrganizePanelLayout layout = {0};
  GuiRect panel = {panel_bounds.x, panel_bounds.y, panel_bounds.width,
                   panel_bounds.height};
  GuiBuildOrganizePanelLayout(panel, &layout);

  GuiLabel(ToRayRect(layout.group_label), "Group-by keys");
  GuiTextBox(ToRayRect(layout.group_input), state->group_by,
             (int)sizeof(state->group_by), true);

  if (ActionButton(state, ToRayRect(layout.preview_button), "Preview Organize",
                   GUI_ACTION_ORGANIZE_PREVIEW)) {
    GuiUiStartTask(state, GUI_TASK_PREVIEW_ORGANIZE);
  }
  if (ActionButton(state, ToRayRect(layout.execute_button), "Execute Organize",
                   GUI_ACTION_ORGANIZE_EXECUTE)) {
    GuiUiStartTask(state, GUI_TASK_EXECUTE_ORGANIZE);
  }
  if (ActionButton(state, ToRayRect(layout.rollback_button), "Rollback",
                   GUI_ACTION_ROLLBACK)) {
    GuiUiStartTask(state, GUI_TASK_ROLLBACK);
  }

  GuiLabel(ToRayRect(layout.info_label),
           "Use comma-separated keys from: date,camera,format,orientation,resolution");
}
