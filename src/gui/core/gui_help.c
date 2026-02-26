#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_help.h"

static char g_hover_message[512] = {0};
static bool g_has_hover = false;
static Font g_help_font = {0};
static float g_help_font_size = 18.0f;
static bool g_help_font_set = false;

void GuiHelpBeginFrame(void) {
  g_hover_message[0] = '\0';
  g_has_hover = false;
}

void GuiHelpRegister(Rectangle bounds, const char *message) {
  if (!message || message[0] == '\0') {
    return;
  }

  if (!CheckCollisionPointRec(GetMousePosition(), bounds)) {
    return;
  }

  strncpy(g_hover_message, message, sizeof(g_hover_message) - 1);
  g_hover_message[sizeof(g_hover_message) - 1] = '\0';
  g_has_hover = true;
}

void GuiHelpSetFont(Font font, float font_size) {
  g_help_font = font;
  g_help_font_set = (font.texture.id != 0);
  if (font_size >= 10.0f) {
    g_help_font_size = font_size;
  }
}

void GuiHelpDrawHintLabel(Rectangle bounds, const char *default_message) {
  (void)bounds;
  (void)default_message;
  // 0.5.3 UI preference: avoid duplicated hint text and keep tooltip-only help.
}

void GuiHelpDrawTooltip(void) {
  if (!g_has_hover || g_hover_message[0] == '\0') {
    return;
  }

  Font font = g_help_font_set ? g_help_font : GetFontDefault();
  float font_size = g_help_font_size;
  if (font_size < 14.0f) {
    font_size = 14.0f;
  }

  Vector2 mouse = GetMousePosition();
  Vector2 text_size = MeasureTextEx(font, g_hover_message, font_size, 1.0f);
  float box_w = text_size.x + 16.0f;
  if (box_w < 210.0f) {
    box_w = 210.0f;
  }
  float box_h = text_size.y + 12.0f;
  if (box_h < 34.0f) {
    box_h = 34.0f;
  }
  float x = mouse.x + 12.0f;
  float y = mouse.y + 12.0f;

  if (x + box_w > (float)GetScreenWidth() - 8.0f) {
    x = (float)GetScreenWidth() - box_w - 8.0f;
  }
  if (y + box_h > (float)GetScreenHeight() - 8.0f) {
    y = (float)GetScreenHeight() - box_h - 8.0f;
  }
  if (x < 8.0f) {
    x = 8.0f;
  }
  if (y < 8.0f) {
    y = 8.0f;
  }

  Rectangle box = {x, y, box_w, box_h};
  DrawRectangleRec(box, (Color){250, 250, 235, 245});
  DrawRectangleLinesEx(box, 1.0f, (Color){156, 156, 120, 255});
  DrawTextEx(font, g_hover_message, (Vector2){x + 8.0f, y + 6.0f}, font_size,
             1.0f, (Color){45, 45, 45, 255});
}
