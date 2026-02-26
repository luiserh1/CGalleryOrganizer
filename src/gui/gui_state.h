#ifndef GUI_STATE_H
#define GUI_STATE_H

#include <stdbool.h>
#include <stddef.h>

#define GUI_STATE_MAX_PATH 1024
#define GUI_STATE_UPDATED_AT_MAX 64

#define GUI_STATE_DEFAULT_UI_SCALE_PERCENT 100
#define GUI_STATE_MIN_UI_SCALE_PERCENT 80
#define GUI_STATE_MAX_UI_SCALE_PERCENT 160
#define GUI_STATE_DEFAULT_WINDOW_WIDTH 1000
#define GUI_STATE_DEFAULT_WINDOW_HEIGHT 760

typedef struct {
  char gallery_dir[GUI_STATE_MAX_PATH];
  char env_dir[GUI_STATE_MAX_PATH];
  int ui_scale_percent;
  int window_width;
  int window_height;
  char updated_at[GUI_STATE_UPDATED_AT_MAX];
} GuiState;

bool GuiStateResolvePath(char *out_path, size_t out_size);
bool GuiStateLoad(GuiState *out_state);
bool GuiStateSave(const GuiState *state);
bool GuiStateReset(void);

#endif // GUI_STATE_H
