#ifndef GUI_STATE_H
#define GUI_STATE_H

#include <stdbool.h>
#include <stddef.h>

#include "app_api_types.h"

#define GUI_STATE_MAX_PATH 1024
#define GUI_STATE_UPDATED_AT_MAX 64
#define GUI_STATE_GROUP_BY_MAX 256

typedef struct {
  char gallery_dir[GUI_STATE_MAX_PATH];
  char env_dir[GUI_STATE_MAX_PATH];
  bool scan_exhaustive;
  int scan_jobs;
  AppCacheCompressionMode scan_cache_mode;
  int scan_cache_level;
  bool sim_incremental;
  float sim_threshold;
  int sim_topk;
  AppSimilarityMemoryMode sim_memory_mode;
  char organize_group_by[GUI_STATE_GROUP_BY_MAX];
  char updated_at[GUI_STATE_UPDATED_AT_MAX];
} GuiState;

bool GuiStateResolvePath(char *out_path, size_t out_size);
bool GuiStateLoad(GuiState *out_state);
bool GuiStateSave(const GuiState *state);
bool GuiStateReset(void);

#endif // GUI_STATE_H
