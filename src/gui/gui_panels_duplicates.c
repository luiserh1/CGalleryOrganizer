#include <stdio.h>

#include "raygui.h"

#include "gui/gui_common.h"

void GuiDrawDuplicatesPanel(GuiUiState *state) {
  if (!state) {
    return;
  }

  GuiLabel((Rectangle){30, 120, 420, 24},
           "Duplicate operations use the current environment cache");

  if (GuiButton((Rectangle){30, 160, 190, 36}, "Analyze Duplicates")) {
    GuiUiStartTask(state, GUI_TASK_FIND_DUPLICATES);
  }

  if (GuiButton((Rectangle){240, 160, 220, 36}, "Move Duplicates to Env")) {
    GuiUiStartTask(state, GUI_TASK_MOVE_DUPLICATES);
  }

  char info[128] = {0};
  snprintf(info, sizeof(info), "Last duplicate groups: %d",
           state->worker_snapshot.duplicate_group_count);
  GuiLabel((Rectangle){30, 210, 320, 24}, info);
}
