#include <stdio.h>

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

  if (ActionButton(state, ToRayRect(layout.analyze_button), "Analyze Duplicates")) {
    GuiUiStartTask(state, GUI_TASK_FIND_DUPLICATES);
  }

  if (ActionButton(state, ToRayRect(layout.move_button), "Move Duplicates to Env")) {
    GuiUiStartTask(state, GUI_TASK_MOVE_DUPLICATES);
  }

  char info[128] = {0};
  snprintf(info, sizeof(info), "Last duplicate groups: %d",
           state->worker_snapshot.duplicate_group_count);
  GuiLabel(ToRayRect(layout.info_bottom), info);
}
