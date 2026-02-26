#ifndef GUI_COMMON_H
#define GUI_COMMON_H

#include <stdbool.h>

#include "app_api.h"

#include "gui/gui_layout.h"
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
  int ui_scale_percent;
  int window_width;
  int window_height;

  int active_tab;
  GuiTaskSnapshot worker_snapshot;

  char banner_message[APP_MAX_ERROR];
  char detail_text[32768];
} GuiUiState;

void GuiUiInitDefaults(GuiUiState *state);
void GuiUiSyncFromStateFile(GuiUiState *state);
void GuiUiPersistState(const GuiUiState *state);

bool GuiUiStartTask(GuiUiState *state, GuiTaskType task_type);

void GuiDrawScanPanel(GuiUiState *state, GuiLayoutRect panel_bounds,
                      const GuiLayoutMetrics *metrics);
void GuiDrawSimilarityPanel(GuiUiState *state, GuiLayoutRect panel_bounds,
                            const GuiLayoutMetrics *metrics);
void GuiDrawOrganizePanel(GuiUiState *state, GuiLayoutRect panel_bounds,
                          const GuiLayoutMetrics *metrics);
void GuiDrawDuplicatesPanel(GuiUiState *state, GuiLayoutRect panel_bounds,
                            const GuiLayoutMetrics *metrics);

#endif // GUI_COMMON_H
