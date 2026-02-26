#ifndef RAYGUI_H
#define RAYGUI_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

Font GuiGetFont(void);
float GuiGetFontSize(void);

static inline Font GuiFontResolved(void) {
  Font font = GuiGetFont();
  if (font.texture.id == 0) {
    return GetFontDefault();
  }
  return font;
}

static inline float GuiFontSizeResolved(void) {
  float size = GuiGetFontSize();
  if (size <= 0.0f) {
    return 16.0f;
  }
  return size;
}

static inline void GuiLabel(Rectangle bounds, const char *text) {
  Font font = GuiFontResolved();
  float font_size = GuiFontSizeResolved();
  DrawTextEx(font, text ? text : "", (Vector2){bounds.x + 4, bounds.y + 6},
             font_size, 1.0f, DARKGRAY);
}

static inline bool GuiButton(Rectangle bounds, const char *text) {
  Vector2 mouse = GetMousePosition();
  bool hover = CheckCollisionPointRec(mouse, bounds);
  bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

  Color fill = hover ? (Color){220, 220, 220, 255} : (Color){240, 240, 240, 255};
  DrawRectangleRec(bounds, fill);
  DrawRectangleLinesEx(bounds, 1, GRAY);

  Font font = GuiFontResolved();
  float font_size = GuiFontSizeResolved();
  Vector2 text_size = MeasureTextEx(font, text ? text : "", font_size, 1.0f);
  DrawTextEx(
      font, text ? text : "",
      (Vector2){bounds.x + (bounds.width - text_size.x) / 2.0f,
                bounds.y + (bounds.height - text_size.y) / 2.0f},
      font_size, 1.0f, BLACK);

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
    Font font = GuiFontResolved();
    float font_size = GuiFontSizeResolved();
    DrawTextEx(font, text, (Vector2){bounds.x + 24, bounds.y + 4}, font_size,
               1.0f, DARKGRAY);
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
    Font font = GuiFontResolved();
    float font_size = GuiFontSizeResolved();
    DrawTextEx(font, text, (Vector2){bounds.x + 6, bounds.y + 6}, font_size,
               1.0f, BLACK);
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
