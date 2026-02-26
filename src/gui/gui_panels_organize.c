#include <string.h>

#include "raygui.h"

#include "gui/gui_common.h"

void GuiDrawOrganizePanel(GuiUiState *state) {
  if (!state) {
    return;
  }

  GuiLabel((Rectangle){30, 120, 160, 24}, "Group-by keys");
  GuiTextBox((Rectangle){170, 116, 280, 30}, state->group_by,
             (int)sizeof(state->group_by), true);

  if (GuiButton((Rectangle){30, 166, 180, 36}, "Preview Organize")) {
    GuiUiStartTask(state, GUI_TASK_PREVIEW_ORGANIZE);
  }

  if (GuiButton((Rectangle){230, 166, 180, 36}, "Execute Organize")) {
    GuiUiStartTask(state, GUI_TASK_EXECUTE_ORGANIZE);
  }

  if (GuiButton((Rectangle){430, 166, 160, 36}, "Rollback")) {
    GuiUiStartTask(state, GUI_TASK_ROLLBACK);
  }

  GuiLabel((Rectangle){30, 220, 920, 24},
           "Use comma-separated keys from: date,camera,format,orientation,resolution");
}
