#include <stdio.h>

#include "raygui.h"
#include "raylib.h"

#include "app_api.h"
#include "gui/core/gui_main_dialogs.h"

ExitDialogAction GuiDrawUnsavedExitDialog(Font font, float font_size) {
  float screen_w = (float)GetScreenWidth();
  float screen_h = (float)GetScreenHeight();
  Rectangle overlay = {0.0f, 0.0f, screen_w, screen_h};
  DrawRectangleRec(overlay, (Color){0, 0, 0, 120});

  Rectangle dialog = {(screen_w - 480.0f) * 0.5f, (screen_h - 200.0f) * 0.5f, 480.0f,
                      200.0f};
  DrawRectangleRec(dialog, (Color){246, 246, 246, 255});
  DrawRectangleLinesEx(dialog, 1.0f, GRAY);

  Rectangle title = {dialog.x + 14.0f, dialog.y + 12.0f, dialog.width - 28.0f, 28.0f};
  GuiLabel(title, "Unsaved configuration changes");

  DrawTextEx(font, "You have unsaved GUI configuration changes.\nSave before exit?",
             (Vector2){dialog.x + 14.0f, dialog.y + 48.0f}, font_size - 2.0f, 1.0f,
             (Color){64, 64, 64, 255});

  Rectangle btn_save = {dialog.x + 14.0f, dialog.y + dialog.height - 54.0f, 140.0f,
                        38.0f};
  Rectangle btn_discard = {btn_save.x + btn_save.width + 10.0f, btn_save.y, 150.0f,
                           38.0f};
  Rectangle btn_cancel = {dialog.x + dialog.width - 124.0f, btn_save.y, 110.0f, 38.0f};

  if (GuiButton(btn_save, "Save & Exit")) {
    return EXIT_DIALOG_SAVE_AND_EXIT;
  }
  if (GuiButton(btn_discard, "Discard")) {
    return EXIT_DIALOG_DISCARD_EXIT;
  }
  if (GuiButton(btn_cancel, "Cancel")) {
    return EXIT_DIALOG_CANCEL;
  }

  return EXIT_DIALOG_NONE;
}

RebuildDialogAction GuiDrawRebuildConfirmDialog(Font font, float font_size,
                                                const char *reason) {
  float screen_w = (float)GetScreenWidth();
  float screen_h = (float)GetScreenHeight();
  Rectangle overlay = {0.0f, 0.0f, screen_w, screen_h};
  DrawRectangleRec(overlay, (Color){0, 0, 0, 120});

  Rectangle dialog = {(screen_w - 560.0f) * 0.5f, (screen_h - 230.0f) * 0.5f, 560.0f,
                      230.0f};
  DrawRectangleRec(dialog, (Color){246, 246, 246, 255});
  DrawRectangleLinesEx(dialog, 1.0f, GRAY);

  Rectangle title = {dialog.x + 14.0f, dialog.y + 12.0f, dialog.width - 28.0f, 28.0f};
  GuiLabel(title, "Cache rebuild required");

  DrawTextEx(
      font,
      "Current scan semantics differ from the saved cache profile.\n"
      "Continuing will rebuild cache before running the task.",
      (Vector2){dialog.x + 14.0f, dialog.y + 46.0f}, font_size - 2.0f, 1.0f,
      (Color){64, 64, 64, 255});

  char reason_line[APP_MAX_ERROR + 20] = {0};
  snprintf(reason_line, sizeof(reason_line), "Reason: %s",
           (reason && reason[0] != '\0') ? reason : "profile mismatch");
  DrawTextEx(font, reason_line, (Vector2){dialog.x + 14.0f, dialog.y + 106.0f},
             font_size - 3.0f, 1.0f, (Color){90, 70, 40, 255});

  Rectangle btn_continue = {dialog.x + 14.0f, dialog.y + dialog.height - 54.0f,
                            200.0f, 38.0f};
  Rectangle btn_cancel = {dialog.x + dialog.width - 124.0f,
                          dialog.y + dialog.height - 54.0f, 110.0f, 38.0f};

  if (GuiButton(btn_continue, "Continue (Rebuild)")) {
    return REBUILD_DIALOG_CONTINUE;
  }
  if (GuiButton(btn_cancel, "Cancel")) {
    return REBUILD_DIALOG_CANCEL;
  }

  return REBUILD_DIALOG_NONE;
}
