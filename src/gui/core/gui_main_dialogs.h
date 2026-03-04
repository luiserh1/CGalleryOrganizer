#ifndef GUI_MAIN_DIALOGS_H
#define GUI_MAIN_DIALOGS_H

#include "raylib.h"

typedef enum {
  EXIT_DIALOG_NONE = 0,
  EXIT_DIALOG_SAVE_AND_EXIT = 1,
  EXIT_DIALOG_DISCARD_EXIT = 2,
  EXIT_DIALOG_CANCEL = 3
} ExitDialogAction;

typedef enum {
  REBUILD_DIALOG_NONE = 0,
  REBUILD_DIALOG_CONTINUE = 1,
  REBUILD_DIALOG_CANCEL = 2
} RebuildDialogAction;

ExitDialogAction GuiDrawUnsavedExitDialog(Font font, float font_size);
RebuildDialogAction GuiDrawRebuildConfirmDialog(Font font, float font_size,
                                                const char *reason);

#endif // GUI_MAIN_DIALOGS_H
