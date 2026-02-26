#ifndef GUI_HELP_H
#define GUI_HELP_H

#include "raylib.h"

void GuiHelpBeginFrame(void);
void GuiHelpRegister(Rectangle bounds, const char *message);
void GuiHelpDrawHintLabel(Rectangle bounds, const char *default_message);
void GuiHelpDrawTooltip(void);

#endif // GUI_HELP_H
