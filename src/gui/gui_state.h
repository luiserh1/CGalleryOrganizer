#ifndef GUI_STATE_H
#define GUI_STATE_H

#include <stdbool.h>
#include <stddef.h>

#define GUI_STATE_MAX_PATH 1024

typedef struct {
  char gallery_dir[GUI_STATE_MAX_PATH];
  char env_dir[GUI_STATE_MAX_PATH];
} GuiState;

bool GuiStateResolvePath(char *out_path, size_t out_size);
bool GuiStateLoad(GuiState *out_state);
bool GuiStateSave(const GuiState *state);
bool GuiStateReset(void);

#endif // GUI_STATE_H
