#include "raygui.h"

#include "gui/frontends/functional/gui_fixed_layout.h"
#include "gui/gui_common.h"

static Rectangle ToRayRect(GuiRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
}

static bool ActionButton(const GuiUiState *state, Rectangle bounds, const char *text) {
  bool enabled = !state->worker_snapshot.busy;
  return GuiButtonStyled(bounds, text, enabled, false);
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

  if (ActionButton(state, ToRayRect(layout.preview_button), "Preview Organize")) {
    GuiUiStartTask(state, GUI_TASK_PREVIEW_ORGANIZE);
  }
  if (ActionButton(state, ToRayRect(layout.execute_button), "Execute Organize")) {
    GuiUiStartTask(state, GUI_TASK_EXECUTE_ORGANIZE);
  }
  if (ActionButton(state, ToRayRect(layout.rollback_button), "Rollback")) {
    GuiUiStartTask(state, GUI_TASK_ROLLBACK);
  }

  GuiLabel(ToRayRect(layout.info_label),
           "Use comma-separated keys from: date,camera,format,orientation,resolution");
}
