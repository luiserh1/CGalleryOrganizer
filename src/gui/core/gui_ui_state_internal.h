#ifndef GUI_UI_STATE_INTERNAL_H
#define GUI_UI_STATE_INTERNAL_H

#include <stdbool.h>

#include "gui/gui_common.h"

bool GuiUiTryParseIntStrict(const char *text, int *out_value);
bool GuiUiTryParseFloatStrict(const char *text, float *out_value);
void GuiUiSyncNumericInputBuffers(GuiUiState *state);
void GuiUiBuildPersistedStateFromUi(const GuiUiState *state,
                                    GuiState *out_state);
void GuiUiApplyPersistedStateToUi(GuiUiState *state, const GuiState *saved);

#endif // GUI_UI_STATE_INTERNAL_H
