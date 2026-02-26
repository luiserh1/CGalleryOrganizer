#include <stdio.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_action_rules.h"
#include "gui/core/gui_help.h"
#include "gui/frontends/functional/gui_fixed_layout.h"
#include "gui/gui_common.h"

static Rectangle ToRayRect(GuiRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
}

static bool ActionButton(GuiUiState *state, Rectangle bounds, const char *text,
                         GuiActionId action_id, const char *help_message) {
  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(state, action_id, &availability);
  if (availability.enabled) {
    GuiHelpRegister(bounds, help_message);
  } else {
    char blocked_help[APP_MAX_ERROR + 96] = {0};
    snprintf(blocked_help, sizeof(blocked_help), "%s (blocked: %s)",
             help_message, availability.reason);
    GuiHelpRegister(bounds, blocked_help);
  }
  bool clicked = GuiButtonStyled(bounds, text, availability.enabled, false);
  if (!availability.enabled &&
      CheckCollisionPointRec(GetMousePosition(), bounds) &&
      IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
      availability.reason[0] != '\0') {
    GuiUiSetBannerError(state, availability.reason);
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
  GuiHelpRegister(ToRayRect(layout.info_top),
                  "Analyze groups equal-content files and optionally move duplicates");

  if (ActionButton(state, ToRayRect(layout.analyze_button), "Analyze Duplicates",
                   GUI_ACTION_DUPLICATES_ANALYZE,
                   "Build duplicate groups from current cache")) {
    GuiUiStartTask(state, GUI_TASK_FIND_DUPLICATES);
  }

  if (ActionButton(state, ToRayRect(layout.move_button), "Move Duplicates to Env",
                   GUI_ACTION_DUPLICATES_MOVE,
                   "Move duplicate files into environment directory")) {
    GuiUiStartTask(state, GUI_TASK_MOVE_DUPLICATES);
  }

  char info[128] = {0};
  snprintf(info, sizeof(info), "Last duplicate groups: %d",
           state->worker_snapshot.duplicate_group_count);
  GuiHelpDrawHintLabel(ToRayRect(layout.info_bottom), info);
}
