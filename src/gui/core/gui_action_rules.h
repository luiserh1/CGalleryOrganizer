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
  GUI_ACTION_DOWNLOAD_MODELS = 8,
  GUI_ACTION_SAVE_PATHS = 9,
  GUI_ACTION_RESET_PATHS = 10,
  GUI_ACTION_RENAME_PREVIEW = 11,
  GUI_ACTION_RENAME_BOOTSTRAP_TAGS = 12,
  GUI_ACTION_RENAME_APPLY = 13,
  GUI_ACTION_RENAME_HISTORY = 14,
  GUI_ACTION_RENAME_ROLLBACK = 15
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
