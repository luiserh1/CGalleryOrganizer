#ifndef GUI_UI_STATE_INTERNAL_H
#define GUI_UI_STATE_INTERNAL_H

#include <stdbool.h>

bool GuiUiTryParseIntStrict(const char *text, int *out_value);
bool GuiUiTryParseFloatStrict(const char *text, float *out_value);

#endif // GUI_UI_STATE_INTERNAL_H
