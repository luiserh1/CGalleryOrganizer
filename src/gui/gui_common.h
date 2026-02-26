#ifndef GUI_COMMON_H
#define GUI_COMMON_H

#include <stdbool.h>

#include "app_api.h"

#include "gui/gui_state.h"
#include "gui/gui_worker.h"

typedef struct {
  GuiState persisted_state;

  char gallery_dir[GUI_STATE_MAX_PATH];
  char env_dir[GUI_STATE_MAX_PATH];
  char group_by[256];

  bool exhaustive;
  int jobs;
  AppCacheCompressionMode cache_mode;
  int cache_level;

  bool sim_incremental;
  float sim_threshold;
  int sim_topk;
  AppSimilarityMemoryMode sim_memory_mode;

  int active_tab;
  GuiTaskSnapshot worker_snapshot;

  char banner_message[APP_MAX_ERROR];
  char detail_text[32768];
} GuiUiState;

void GuiUiInitDefaults(GuiUiState *state);
void GuiUiSyncFromStateFile(GuiUiState *state);
void GuiUiPersistPaths(const GuiUiState *state);

bool GuiUiStartTask(GuiUiState *state, GuiTaskType task_type);

void GuiDrawScanPanel(GuiUiState *state);
void GuiDrawSimilarityPanel(GuiUiState *state);
void GuiDrawOrganizePanel(GuiUiState *state);
void GuiDrawDuplicatesPanel(GuiUiState *state);

#endif // GUI_COMMON_H
