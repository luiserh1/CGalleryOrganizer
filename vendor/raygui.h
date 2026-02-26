#ifndef RAYGUI_H
#define RAYGUI_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void GuiLabel(Rectangle bounds, const char *text) {
  DrawText(text ? text : "", (int)bounds.x + 4, (int)bounds.y + 6, 16,
           DARKGRAY);
}

static inline bool GuiButton(Rectangle bounds, const char *text) {
  Vector2 mouse = GetMousePosition();
  bool hover = CheckCollisionPointRec(mouse, bounds);
  bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

  Color fill = hover ? (Color){220, 220, 220, 255} : (Color){240, 240, 240, 255};
  DrawRectangleRec(bounds, fill);
  DrawRectangleLinesEx(bounds, 1, GRAY);

  int text_w = MeasureText(text ? text : "", 16);
  DrawText(text ? text : "", (int)(bounds.x + (bounds.width - text_w) / 2),
           (int)(bounds.y + (bounds.height - 16) / 2), 16, BLACK);

  return pressed;
}

static inline bool GuiCheckBox(Rectangle bounds, const char *text, bool checked) {
  Rectangle box = {bounds.x, bounds.y + 2, 18, 18};
  Vector2 mouse = GetMousePosition();
  bool hover = CheckCollisionPointRec(mouse, box);
  bool toggle = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

  DrawRectangleRec(box, WHITE);
  DrawRectangleLinesEx(box, 1, GRAY);
  if (checked) {
    DrawLine((int)box.x + 3, (int)box.y + 10, (int)box.x + 8, (int)box.y + 15,
             DARKGREEN);
    DrawLine((int)box.x + 8, (int)box.y + 15, (int)box.x + 15,
             (int)box.y + 4, DARKGREEN);
  }

  if (text) {
    DrawText(text, (int)bounds.x + 24, (int)bounds.y + 4, 16, DARKGRAY);
  }

  return toggle;
}

static inline bool GuiTextBox(Rectangle bounds, char *text, int text_size,
                              bool edit_mode) {
  static char *active_text = NULL;
  Vector2 mouse = GetMousePosition();
  bool hover = CheckCollisionPointRec(mouse, bounds);
  bool clicked = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
  bool active = (active_text == text);

  if (clicked && edit_mode) {
    active_text = text;
    active = true;
  }

  if (!hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && active) {
    active_text = NULL;
    active = false;
  }

  DrawRectangleRec(bounds, WHITE);
  DrawRectangleLinesEx(bounds, 1, active ? BLUE : GRAY);

  if (text) {
    DrawText(text, (int)bounds.x + 6, (int)bounds.y + 6, 16, BLACK);
  }

  bool submitted = false;
  if (active && edit_mode && text && text_size > 1) {
    int key = GetCharPressed();
    while (key > 0) {
      int len = (int)strlen(text);
      if (key >= 32 && key <= 126 && len < text_size - 1) {
        text[len] = (char)key;
        text[len + 1] = '\0';
      }
      key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
      int len = (int)strlen(text);
      if (len > 0) {
        text[len - 1] = '\0';
      }
    }

    if (IsKeyPressed(KEY_ENTER)) {
      submitted = true;
      active_text = NULL;
    }
  }

  return submitted;
}

#ifdef __cplusplus
}
#endif

#endif // RAYGUI_H
