#ifndef GUI_ACTION_RULES_H
#define GUI_ACTION_RULES_H

#include <stdbool.h>
#include <stddef.h>

#include "gui/gui_common.h"

typedef enum {
  GUI_ACTION_SCAN_CACHE = 0,
  GUI_ACTION_ML_ENRICH = 1,
  GUI_ACTION_SIMILARITY_REPORT = 2,
  GUI_ACTION_ORGANIZE_PREVIEW = 3,
  GUI_ACTION_ORGANIZE_EXECUTE = 4,
  GUI_ACTION_ROLLBACK = 5,
  GUI_ACTION_DUPLICATES_ANALYZE = 6,
  GUI_ACTION_DUPLICATES_MOVE = 7,
  GUI_ACTION_SAVE_PATHS = 8,
  GUI_ACTION_RESET_PATHS = 9
} GuiActionId;

typedef struct {
  bool enabled;
  char reason[APP_MAX_ERROR];
} GuiActionAvailability;

void GuiResolveActionAvailability(const GuiUiState *state, GuiActionId action_id,
                                  GuiActionAvailability *out);

bool GuiActionCanStartTask(const GuiUiState *state, GuiTaskType task_type,
                           char *out_reason, size_t out_reason_size);

#endif // GUI_ACTION_RULES_H
