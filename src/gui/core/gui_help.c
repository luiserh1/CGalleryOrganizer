#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_help.h"

static char g_hover_message[512] = {0};
static bool g_has_hover = false;

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

void GuiHelpDrawHintLabel(Rectangle bounds, const char *default_message) {
  const char *text = default_message;
  if (g_has_hover && g_hover_message[0] != '\0') {
    text = g_hover_message;
  }
  if (!text) {
    return;
  }
  GuiLabel(bounds, text);
}

void GuiHelpDrawTooltip(void) {
  if (!g_has_hover || g_hover_message[0] == '\0') {
    return;
  }

  Vector2 mouse = GetMousePosition();
  int font_size = 18;
  int text_width = MeasureText(g_hover_message, font_size);
  float box_w = (float)text_width + 16.0f;
  if (box_w < 180.0f) {
    box_w = 180.0f;
  }
  float box_h = 30.0f;
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
  DrawText(g_hover_message, (int)(x + 8.0f), (int)(y + 7.0f), font_size,
           (Color){45, 45, 45, 255});
}
