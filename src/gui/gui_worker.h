#ifndef GUI_WORKER_H
#define GUI_WORKER_H

#include <stdbool.h>

#include "app_api.h"
#include "gui/gui_state.h"

typedef enum {
  GUI_TASK_NONE = 0,
  GUI_TASK_SCAN = 1,
  GUI_TASK_ML_ENRICH = 2,
  GUI_TASK_SIMILARITY = 3,
  GUI_TASK_PREVIEW_ORGANIZE = 4,
  GUI_TASK_EXECUTE_ORGANIZE = 5,
  GUI_TASK_ROLLBACK = 6,
  GUI_TASK_FIND_DUPLICATES = 7,
  GUI_TASK_MOVE_DUPLICATES = 8,
  GUI_TASK_DOWNLOAD_MODELS = 9,
  GUI_TASK_RENAME_PREVIEW = 10,
  GUI_TASK_RENAME_APPLY = 11,
  GUI_TASK_RENAME_HISTORY = 12,
  GUI_TASK_RENAME_ROLLBACK = 13
} GuiTaskType;

typedef struct {
  GuiTaskType type;
  char gallery_dir[GUI_STATE_MAX_PATH];
  char env_dir[GUI_STATE_MAX_PATH];
  bool exhaustive;
  int jobs;
  AppCacheCompressionMode cache_mode;
  int cache_level;
  bool sim_incremental;
  float sim_threshold;
  int sim_topk;
  AppSimilarityMemoryMode sim_memory_mode;
  char group_by[256];
  char rename_pattern[256];
  char rename_tags_map_path[GUI_STATE_MAX_PATH];
  char rename_tag_add_csv[256];
  char rename_tag_remove_csv[256];
  char rename_preview_id[64];
  char rename_operation_id[64];
  bool rename_accept_auto_suffix;
} GuiTaskInput;

typedef struct {
  bool busy;
  bool has_result;
  bool success;
  AppStatus status;
  char message[APP_MAX_ERROR];
  int progress_current;
  int progress_total;
  char progress_stage[64];
  AppScanResult scan_result;
  AppSimilarityResult similarity_result;
  AppModelInstallResult model_install_result;
  AppOrganizeExecuteResult organize_execute_result;
  AppRollbackResult rollback_result;
  AppDuplicateMoveResult duplicate_move_result;
  AppRenameApplyResult rename_apply_result;
  AppRenameRollbackResult rename_rollback_result;
  char rename_preview_id[64];
  int rename_preview_file_count;
  int rename_preview_collision_count;
  bool rename_preview_requires_auto_suffix;
  int rename_history_count;
  int duplicate_group_count;
  bool duplicate_report_ready;
  char detail_text[32768];
} GuiTaskSnapshot;

bool GuiWorkerInit(AppContext *app);
void GuiWorkerShutdown(void);

bool GuiWorkerStartTask(const GuiTaskInput *input);
void GuiWorkerRequestCancel(void);

void GuiWorkerGetSnapshot(GuiTaskSnapshot *out_snapshot);
void GuiWorkerClearResult(void);

bool GuiWorkerInspectRuntimeState(const AppRuntimeStateRequest *request,
                                  AppRuntimeState *out_state, char *out_error,
                                  size_t out_error_size);

bool GuiWorkerInspectScanProfile(const AppScanRequest *request,
                                 AppScanProfileDecision *out_decision,
                                 char *out_error, size_t out_error_size);

#endif // GUI_WORKER_H
