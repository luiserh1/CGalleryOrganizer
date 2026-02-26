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

static inline bool GuiButtonStyled(Rectangle bounds, const char *text, bool enabled,
                                   bool selected) {
  Vector2 mouse = GetMousePosition();
  bool hover = enabled && CheckCollisionPointRec(mouse, bounds);
  bool pressed = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

  Color fill = (Color){240, 240, 240, 255};
  Color border = GRAY;
  Color text_color = enabled ? BLACK : GRAY;

  if (!enabled) {
    fill = (Color){232, 232, 232, 255};
    border = (Color){178, 178, 178, 255};
    text_color = (Color){145, 145, 145, 255};
  } else if (selected) {
    fill = (Color){210, 224, 238, 255};
    border = (Color){80, 110, 140, 255};
  } else if (hover) {
    fill = (Color){220, 220, 220, 255};
  }

  DrawRectangleRec(bounds, fill);
  DrawRectangleLinesEx(bounds, selected ? 2 : 1, border);

  Font font = GuiFontResolved();
  float font_size = GuiFontSizeResolved();
  Vector2 text_size = MeasureTextEx(font, text ? text : "", font_size, 1.0f);
  DrawTextEx(
      font, text ? text : "",
      (Vector2){bounds.x + (bounds.width - text_size.x) / 2.0f,
                bounds.y + (bounds.height - text_size.y) / 2.0f},
      font_size, 1.0f, text_color);

  return enabled && pressed;
}

static inline bool GuiButton(Rectangle bounds, const char *text) {
  return GuiButtonStyled(bounds, text, true, false);
}

static inline bool GuiCheckBox(Rectangle bounds, const char *text, bool checked) {
  Font font = GuiFontResolved();
  float font_size = GuiFontSizeResolved();
  float box_size = font_size + 4.0f;
  float max_box_size = bounds.height - 4.0f;
  if (box_size > max_box_size) {
    box_size = max_box_size;
  }
  if (box_size < 14.0f) {
    box_size = 14.0f;
  }

  float box_y = bounds.y + (bounds.height - box_size) / 2.0f;
  Rectangle box = {bounds.x, box_y, box_size, box_size};
  Vector2 text_size = {0};
  Rectangle text_rect = {0};
  Rectangle text_hit_rect = {0};
  if (text && text[0] != '\0') {
    text_size = MeasureTextEx(font, text, font_size, 1.0f);
    text_rect = (Rectangle){box.x + box_size + 8.0f,
                            bounds.y + (bounds.height - text_size.y) / 2.0f,
                            text_size.x, text_size.y};
    text_hit_rect = (Rectangle){text_rect.x - 2.0f,
                                text_rect.y - 2.0f,
                                text_rect.width + 4.0f,
                                text_rect.height + 4.0f};
  }

  Vector2 mouse = GetMousePosition();
  bool hover_box = CheckCollisionPointRec(mouse, box);
  bool hover_text = text_hit_rect.width > 0.0f &&
                    CheckCollisionPointRec(mouse, text_hit_rect);
  bool hovered = hover_box || hover_text;
  bool toggle = hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);

  if (toggle) {
    checked = !checked;
  }

  DrawRectangleRec(box, WHITE);
  DrawRectangleLinesEx(box, 1, GRAY);
  if (checked) {
    DrawLine((int)(box.x + box_size * 0.20f), (int)(box.y + box_size * 0.55f),
             (int)(box.x + box_size * 0.45f), (int)(box.y + box_size * 0.80f),
             DARKGREEN);
    DrawLine((int)(box.x + box_size * 0.45f), (int)(box.y + box_size * 0.80f),
             (int)(box.x + box_size * 0.82f), (int)(box.y + box_size * 0.22f),
             DARKGREEN);
  }

  if (text) {
    DrawTextEx(font, text, (Vector2){text_rect.x, text_rect.y}, font_size, 1.0f,
               DARKGRAY);
  }

  return checked;
}

static inline bool GuiTextBox(Rectangle bounds, char *text, int text_size,
                              bool edit_mode) {
  static char *active_text = NULL;
  static int active_cursor_pos = 0;
  Vector2 mouse = GetMousePosition();
  bool hover = CheckCollisionPointRec(mouse, bounds);
  bool clicked = hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
  bool active = (active_text == text);

  int text_len = 0;
  if (text) {
    text_len = (int)strlen(text);
    if (text_len >= text_size) {
      text_len = text_size - 1;
    }
  }

  if (clicked && edit_mode) {
    active_text = text;
    active_cursor_pos = text_len;
    if (text && text_len > 0) {
      Font click_font = GuiFontResolved();
      float click_font_size = GuiFontSizeResolved();
      float local_x = mouse.x - (bounds.x + 6.0f);
      if (local_x <= 0.0f) {
        active_cursor_pos = 0;
      } else {
        active_cursor_pos = text_len;
        for (int i = 0; i <= text_len; ++i) {
          char saved = text[i];
          text[i] = '\0';
          Vector2 prefix = MeasureTextEx(click_font, text, click_font_size, 1.0f);
          text[i] = saved;
          if (prefix.x >= local_x) {
            active_cursor_pos = i;
            break;
          }
        }
      }
    }
    active = true;
  }

  if (!hover && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && active) {
    active_text = NULL;
    active_cursor_pos = 0;
    active = false;
  }

  DrawRectangleRec(bounds, WHITE);
  DrawRectangleLinesEx(bounds, 1, active ? BLUE : GRAY);

  Font font = GuiFontResolved();
  float font_size = GuiFontSizeResolved();
  if (text) {
    DrawTextEx(font, text, (Vector2){bounds.x + 6, bounds.y + 6}, font_size,
               1.0f, BLACK);
  }

  if (active && edit_mode) {
    if (active_cursor_pos < 0) {
      active_cursor_pos = 0;
    }
    if (active_cursor_pos > text_len) {
      active_cursor_pos = text_len;
    }

    int blink = ((int)(GetTime() * 2.0)) % 2;
    if (blink == 0) {
      float caret_offset = 0.0f;
      if (text && active_cursor_pos > 0) {
        char saved = text[active_cursor_pos];
        text[active_cursor_pos] = '\0';
        Vector2 prefix = MeasureTextEx(font, text, font_size, 1.0f);
        text[active_cursor_pos] = saved;
        caret_offset = prefix.x;
      }

      float caret_x = bounds.x + 6.0f + caret_offset;
      float max_caret_x = bounds.x + bounds.width - 4.0f;
      if (caret_x > max_caret_x) {
        caret_x = max_caret_x;
      }

      DrawLine((int)caret_x, (int)(bounds.y + 6.0f), (int)caret_x,
               (int)(bounds.y + bounds.height - 6.0f), BLACK);
    }
  }

  bool submitted = false;
  if (active && edit_mode && text && text_size > 1) {
    bool modifier_down = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                         IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);

    if (modifier_down && IsKeyPressed(KEY_C)) {
      SetClipboardText(text);
    }

    if (modifier_down && IsKeyPressed(KEY_V)) {
      const char *clipboard = GetClipboardText();
      if (clipboard) {
        const unsigned char *src = (const unsigned char *)clipboard;
        while (*src != '\0' && text_len < text_size - 1) {
          unsigned char ch = *src++;
          if (ch == '\r' || ch == '\n' || ch == '\t') {
            continue;
          }
          if (ch < 32 || ch == 127) {
            continue;
          }
          memmove(text + active_cursor_pos + 1, text + active_cursor_pos,
                  (size_t)(text_len - active_cursor_pos + 1));
          text[active_cursor_pos] = (char)ch;
          active_cursor_pos += 1;
          text_len += 1;
        }
      }
    }

    int key = GetCharPressed();
    while (key > 0) {
      if (key >= 32 && key <= 126 && text_len < text_size - 1) {
        memmove(text + active_cursor_pos + 1, text + active_cursor_pos,
                (size_t)(text_len - active_cursor_pos + 1));
        text[active_cursor_pos] = (char)key;
        active_cursor_pos += 1;
        text_len += 1;
      }
      key = GetCharPressed();
    }

    if (IsKeyPressed(KEY_BACKSPACE)) {
      if (active_cursor_pos > 0 && text_len > 0) {
        memmove(text + active_cursor_pos - 1, text + active_cursor_pos,
                (size_t)(text_len - active_cursor_pos + 1));
        active_cursor_pos -= 1;
        text_len -= 1;
      }
    }

    if (IsKeyPressed(KEY_DELETE)) {
      if (active_cursor_pos < text_len) {
        memmove(text + active_cursor_pos, text + active_cursor_pos + 1,
                (size_t)(text_len - active_cursor_pos));
        text_len -= 1;
      }
    }

    if (IsKeyPressed(KEY_LEFT) && active_cursor_pos > 0) {
      active_cursor_pos -= 1;
    }

    if (IsKeyPressed(KEY_RIGHT) && active_cursor_pos < text_len) {
      active_cursor_pos += 1;
    }

    if (IsKeyPressed(KEY_HOME)) {
      active_cursor_pos = 0;
    }

    if (IsKeyPressed(KEY_END)) {
      active_cursor_pos = text_len;
    }

    if (IsKeyPressed(KEY_ENTER)) {
      submitted = true;
      active_text = NULL;
      active_cursor_pos = 0;
    }
  }

  return submitted;
}

#ifdef __cplusplus
}
#endif

#endif // RAYGUI_H
