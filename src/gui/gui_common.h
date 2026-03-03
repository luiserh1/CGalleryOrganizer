#ifndef GUI_COMMON_H
#define GUI_COMMON_H

#include <stdbool.h>

#include "app_api.h"
#include "raylib.h"

#include "gui/gui_state.h"
#include "gui/gui_worker.h"

typedef struct {
  GuiState persisted_state;

  char gallery_dir[GUI_STATE_MAX_PATH];
  char env_dir[GUI_STATE_MAX_PATH];
  char group_by[GUI_STATE_GROUP_BY_MAX];
  char jobs_input[16];
  char cache_level_input[8];
  char sim_threshold_input[32];
  char sim_topk_input[16];
  char rename_pattern_input[256];
  char rename_tags_map_path[GUI_STATE_MAX_PATH];
  char rename_tag_add_csv[256];
  char rename_tag_remove_csv[256];
  char rename_meta_tag_add_csv[256];
  char rename_meta_tag_remove_csv[256];
  char rename_selected_tags_csv[256];
  char rename_selected_meta_tags_csv[256];
  char rename_preview_id_input[64];
  char rename_operation_id_input[64];

  bool exhaustive;
  int jobs;
  AppCacheCompressionMode cache_mode;
  int cache_level;

  bool sim_incremental;
  float sim_threshold;
  int sim_topk;
  AppSimilarityMemoryMode sim_memory_mode;
  bool rename_accept_auto_suffix;
  bool rename_filter_collisions_only;
  bool rename_filter_warnings_only;
  int rename_table_scroll;
  int rename_selected_row;
  int rename_preview_row_count;
  int rename_preview_warning_count;
  GuiRenamePreviewRow rename_preview_rows[GUI_RENAME_PREVIEW_ROWS_MAX];

  int active_tab;
  GuiTaskSnapshot worker_snapshot;
  AppRuntimeState runtime_state;
  bool runtime_state_valid;

  char banner_message[APP_MAX_ERROR];
  bool banner_is_error;
  char runtime_error[APP_MAX_ERROR];
  char detail_text[32768];

  bool rebuild_confirm_pending;
  GuiTaskType rebuild_confirm_task;
  char rebuild_confirm_reason[APP_MAX_ERROR];
} GuiUiState;

void GuiUiInitDefaults(GuiUiState *state);
void GuiUiSyncFromStateFile(GuiUiState *state);
bool GuiUiPersistState(GuiUiState *state);
bool GuiUiHasUnsavedChanges(const GuiUiState *state);
void GuiUiRefreshRuntimeState(GuiUiState *state);
void GuiUiSetBannerInfo(GuiUiState *state, const char *message);
void GuiUiSetBannerError(GuiUiState *state, const char *message);

bool GuiUiStartTask(GuiUiState *state, GuiTaskType task_type);
bool GuiUiStartPendingRebuildTask(GuiUiState *state);
void GuiUiCancelPendingRebuildTask(GuiUiState *state);

void GuiDrawScanPanel(GuiUiState *state, Rectangle panel_bounds);
void GuiDrawSimilarityPanel(GuiUiState *state, Rectangle panel_bounds);
void GuiDrawOrganizePanel(GuiUiState *state, Rectangle panel_bounds);
void GuiDrawDuplicatesPanel(GuiUiState *state, Rectangle panel_bounds);
void GuiDrawRenamePanel(GuiUiState *state, Rectangle panel_bounds);

#endif // GUI_COMMON_H
