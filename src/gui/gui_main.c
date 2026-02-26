#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/gui_common.h"

static Font g_gui_font = {0};
static float g_gui_font_size = 20.0f;
static bool g_gui_font_loaded = false;

static Rectangle ToRayRect(GuiLayoutRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
}

static float LabelWidth(const char *text, const GuiLayoutMetrics *metrics) {
  return GuiLayoutMeasureTextWidth(text, metrics) + (float)(metrics->gap * 2);
}

Font GuiGetFont(void) {
  if (g_gui_font_loaded && g_gui_font.texture.id > 0) {
    return g_gui_font;
  }
  return GetFontDefault();
}

float GuiGetFontSize(void) {
  return g_gui_font_size;
}

static bool TryLoadGuiFont(const char *path) {
  if (!path || !FileExists(path)) {
    return false;
  }

  Font font = LoadFontEx(path, 48, NULL, 0);
  if (font.texture.id == 0) {
    return false;
  }

  SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
  g_gui_font = font;
  g_gui_font_loaded = true;
  return true;
}

static void InitGuiFont(void) {
  const char *candidates[] = {
      "/System/Library/Fonts/Supplemental/Arial.ttf",
      "/System/Library/Fonts/Supplemental/Helvetica.ttf",
      "/System/Library/Fonts/Supplemental/Menlo.ttc",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/dejavu/DejaVuSans.ttf",
      "C:\\Windows\\Fonts\\segoeui.ttf",
      "C:\\Windows\\Fonts\\arial.ttf"};
  int count = (int)(sizeof(candidates) / sizeof(candidates[0]));
  for (int i = 0; i < count; i++) {
    if (TryLoadGuiFont(candidates[i])) {
      return;
    }
  }

  g_gui_font = GetFontDefault();
  g_gui_font_loaded = false;
}

static void ShutdownGuiFont(void) {
  if (g_gui_font_loaded && g_gui_font.texture.id > 0) {
    UnloadFont(g_gui_font);
    g_gui_font = (Font){0};
    g_gui_font_loaded = false;
  }
}

static bool DrawMultilineTextInRect(const char *text, GuiLayoutRect area, int font_size,
                                    Color color) {
  if (!text || text[0] == '\0' || area.width < 2.0f || area.height < 2.0f) {
    return false;
  }

  int draw_size = font_size;
  if (draw_size < 12) {
    draw_size = 12;
  }
  int line_stride = draw_size + 6;
  int max_lines = (int)(area.height / (float)line_stride);
  if (max_lines < 1) {
    return false;
  }

  int line = 0;
  const char *cursor = text;
  const char *line_start = text;
  bool truncated = false;

  while (*cursor != '\0') {
    if (line >= max_lines) {
      truncated = true;
      break;
    }
    if (*cursor == '\n') {
      int len = (int)(cursor - line_start);
      if (len > 0) {
        char buffer[1024] = {0};
        if (len >= (int)sizeof(buffer)) {
          len = (int)sizeof(buffer) - 1;
        }
        memcpy(buffer, line_start, (size_t)len);
        buffer[len] = '\0';
        DrawTextEx(GuiGetFont(), buffer,
                   (Vector2){area.x, area.y + (float)(line * line_stride)},
                   (float)draw_size, 1.0f, color);
      }
      line++;
      line_start = cursor + 1;
    }
    cursor++;
  }

  if (!truncated && line < max_lines && line_start && *line_start != '\0') {
    DrawTextEx(GuiGetFont(), line_start,
               (Vector2){area.x, area.y + (float)(line * line_stride)},
               (float)draw_size, 1.0f, color);
  } else if (!truncated && line_start && *line_start != '\0' && line >= max_lines) {
    truncated = true;
  }
  return truncated;
}

static void HandleZoomControls(GuiUiState *state, const GuiLayoutMetrics *metrics,
                               float x, float y) {
  float minus_w = GuiLayoutButtonWidth("A-", metrics, (float)metrics->small_button_w);
  float plus_w = GuiLayoutButtonWidth("A+", metrics, (float)metrics->small_button_w);
  float reset_w = GuiLayoutButtonWidth("Reset", metrics, 72.0f * metrics->effective_scale);
  float value_w = (float)metrics->zoom_value_w;

  GuiLayoutRect minus = {x, y, minus_w, (float)metrics->button_h};
  GuiLayoutRect value = {minus.x + minus.width + (float)metrics->gap, y, value_w,
                         (float)metrics->button_h};
  GuiLayoutRect plus = {value.x + value.width + (float)metrics->gap, y, plus_w,
                        (float)metrics->button_h};
  GuiLayoutRect reset = {plus.x + plus.width + (float)metrics->gap, y, reset_w,
                         (float)metrics->button_h};

  if (GuiButton(ToRayRect(minus), "A-")) {
    state->ui_scale_percent -= 10;
  }
  if (GuiButton(ToRayRect(plus), "A+")) {
    state->ui_scale_percent += 10;
  }
  if (GuiButton(ToRayRect(reset), "Reset")) {
    state->ui_scale_percent = GUI_STATE_DEFAULT_UI_SCALE_PERCENT;
  }

  state->ui_scale_percent = GuiLayoutClampUiScalePercent(state->ui_scale_percent);

  DrawRectangleRec(ToRayRect(value), (Color){242, 242, 242, 255});
  DrawRectangleLinesEx(ToRayRect(value), 1.0f, GRAY);
  char zoom_text[16] = {0};
  snprintf(zoom_text, sizeof(zoom_text), "%d%%", state->ui_scale_percent);
  GuiLabel(ToRayRect(value), zoom_text);
}

int main(int argc, char **argv) {
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--reset-state") == 0) {
      if (GuiStateReset()) {
        printf("GUI state reset.\n");
        return 0;
      }
      printf("Failed to reset GUI state.\n");
      return 1;
    }
  }

  AppRuntimeOptions runtime_options = {0};
  AppContext *app = AppContextCreate(&runtime_options);
  if (!app) {
    fprintf(stderr, "Failed to initialize app context.\n");
    return 1;
  }

  if (!GuiWorkerInit(app)) {
    fprintf(stderr, "Failed to initialize GUI worker.\n");
    AppContextDestroy(app);
    return 1;
  }

  GuiUiState state;
  GuiUiInitDefaults(&state);
  GuiUiSyncFromStateFile(&state);

  int min_window_width = 0;
  int min_window_height = 0;
  GuiLayoutComputeMinWindowSize(&min_window_width, &min_window_height);

  int initial_width = state.window_width > 0 ? state.window_width
                                              : GUI_STATE_DEFAULT_WINDOW_WIDTH;
  int initial_height = state.window_height > 0 ? state.window_height
                                                : GUI_STATE_DEFAULT_WINDOW_HEIGHT;
  if (initial_width < min_window_width) {
    initial_width = min_window_width;
  }
  if (initial_height < min_window_height) {
    initial_height = min_window_height;
  }

  SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  InitWindow(initial_width, initial_height, "CGalleryOrganizer v0.5.1 GUI");
  SetWindowMinSize(min_window_width, min_window_height);
  InitGuiFont();
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    state.window_width = GetScreenWidth();
    state.window_height = GetScreenHeight();

    GuiLayoutMetrics metrics = {0};
    GuiLayoutComputeMetrics(&metrics, state.window_width, state.window_height,
                            state.ui_scale_percent);
    g_gui_font_size = (float)metrics.font_size;

    GuiWorkerGetSnapshot(&state.worker_snapshot);
    if (state.worker_snapshot.has_result) {
      if (state.worker_snapshot.message[0] != '\0') {
        strncpy(state.banner_message, state.worker_snapshot.message,
                sizeof(state.banner_message) - 1);
      } else {
        strncpy(state.banner_message,
                state.worker_snapshot.success ? "Task completed" : "Task failed",
                sizeof(state.banner_message) - 1);
      }

      if (state.worker_snapshot.detail_text[0] != '\0') {
        strncpy(state.detail_text, state.worker_snapshot.detail_text,
                sizeof(state.detail_text) - 1);
      }

      GuiWorkerClearResult();
    }

    BeginDrawing();
    ClearBackground((Color){248, 248, 246, 255});

    GuiLayoutRect canvas = {(float)metrics.pad,
                            (float)metrics.pad,
                            (float)(state.window_width - metrics.pad * 2),
                            (float)(state.window_height - metrics.pad * 2)};
    if (canvas.width < 10.0f || canvas.height < 10.0f) {
      EndDrawing();
      continue;
    }

    GuiLayoutRect title_rect = {canvas.x,
                                canvas.y,
                                canvas.width,
                                (float)metrics.label_h};
    GuiLabel(ToRayRect(title_rect), "CGalleryOrganizer v0.5.1 - GUI");

    float tabs_y = title_rect.y + title_rect.height + (float)metrics.gap;
    float tab_labels_width[4];
    const char *tab_labels[4] = {"Scan", "Similarity", "Organize", "Duplicates"};
    for (int i = 0; i < 4; i++) {
      tab_labels_width[i] =
          GuiLayoutButtonWidth(tab_labels[i], &metrics, (float)metrics.tab_button_min_w);
    }

    float zoom_minus_w =
        GuiLayoutButtonWidth("A-", &metrics, (float)metrics.small_button_w);
    float zoom_plus_w = GuiLayoutButtonWidth("A+", &metrics, (float)metrics.small_button_w);
    float zoom_reset_w =
        GuiLayoutButtonWidth("Reset", &metrics, 72.0f * metrics.effective_scale);
    float zoom_total_w = zoom_minus_w + (float)metrics.zoom_value_w + zoom_plus_w +
                         zoom_reset_w + (float)(metrics.gap * 3);
    float zoom_start_x = canvas.x + canvas.width - zoom_total_w;

    float tab_x = canvas.x;
    for (int i = 0; i < 4; i++) {
      GuiLayoutRect tab_rect = {tab_x, tabs_y, tab_labels_width[i],
                                (float)metrics.button_h};
      bool selected = (state.active_tab == i);
      if (GuiButtonStyled(ToRayRect(tab_rect), tab_labels[i], true, selected)) {
        state.active_tab = i;
      }
      tab_x += tab_rect.width + (float)metrics.gap;
    }

    float zoom_y = tabs_y;
    if (zoom_start_x <= tab_x + (float)metrics.gap) {
      zoom_start_x = canvas.x;
      zoom_y = tabs_y + (float)metrics.button_h + (float)metrics.gap;
    }
    HandleZoomControls(&state, &metrics, zoom_start_x, zoom_y);

    float header_bottom = tabs_y + (float)metrics.button_h;
    if (zoom_y + (float)metrics.button_h > header_bottom) {
      header_bottom = zoom_y + (float)metrics.button_h;
    }

    float content_y = header_bottom + (float)metrics.gap;
    float available_h = canvas.y + canvas.height - content_y;
    float status_min_h = (float)(metrics.label_h + metrics.gap + metrics.pad * 2 +
                                 metrics.line_height * 6);
    float panel_min_h = 240.0f * metrics.effective_scale;
    float status_h = available_h / 3.0f;
    if (status_h < status_min_h) {
      status_h = status_min_h;
    }
    if (available_h - status_h - (float)metrics.gap < panel_min_h) {
      status_h = available_h - panel_min_h - (float)metrics.gap;
    }
    if (status_h < 120.0f) {
      status_h = 120.0f;
    }

    GuiLayoutRect panel_rect = {canvas.x, content_y, canvas.width,
                                available_h - status_h - (float)metrics.gap};
    if (panel_rect.height < panel_min_h) {
      panel_rect.height = panel_min_h;
    }
    GuiLayoutRect status_rect = {canvas.x,
                                 panel_rect.y + panel_rect.height + (float)metrics.gap,
                                 canvas.width,
                                 canvas.y + canvas.height -
                                     (panel_rect.y + panel_rect.height + (float)metrics.gap)};

    DrawRectangleLinesEx(ToRayRect(panel_rect), 1.0f, GRAY);
    GuiLayoutRect panel_inner = {panel_rect.x + 1.0f, panel_rect.y + 1.0f,
                                 panel_rect.width - 2.0f, panel_rect.height - 2.0f};

    if (state.active_tab == 0) {
      GuiDrawScanPanel(&state, panel_inner, &metrics);
    } else if (state.active_tab == 1) {
      GuiDrawSimilarityPanel(&state, panel_inner, &metrics);
    } else if (state.active_tab == 2) {
      GuiDrawOrganizePanel(&state, panel_inner, &metrics);
    } else {
      GuiDrawDuplicatesPanel(&state, panel_inner, &metrics);
    }

    DrawRectangleLinesEx(ToRayRect(status_rect), 1.0f, LIGHTGRAY);
    GuiLayoutContext status_ctx = {0};
    GuiLayoutRect status_inner = {status_rect.x + 1.0f, status_rect.y + 1.0f,
                                  status_rect.width - 2.0f, status_rect.height - 2.0f};
    GuiLayoutInit(&status_ctx, status_inner, &metrics);

    GuiLayoutRect status_label =
        GuiLayoutPlaceFixed(&status_ctx, LabelWidth("Task status:", &metrics),
                            (float)metrics.label_h);
    GuiLabel(ToRayRect(status_label), "Task status:");

    Color status_color = DARKGRAY;
    const char *status_text =
        state.banner_message[0] != '\0' ? state.banner_message : "Idle";

    if (state.worker_snapshot.busy) {
      char progress[256] = {0};
      snprintf(progress, sizeof(progress), "Running [%s] %d/%d",
               state.worker_snapshot.progress_stage,
               state.worker_snapshot.progress_current,
               state.worker_snapshot.progress_total);
      float cancel_w =
          GuiLayoutButtonWidth("Cancel", &metrics, 110.0f * metrics.effective_scale);
      if (GuiLayoutRemainingWidth(&status_ctx) <
          cancel_w + 160.0f * metrics.effective_scale + (float)metrics.gap) {
        GuiLayoutNextLine(&status_ctx);
      }

      GuiLayoutRect progress_rect = GuiLayoutPlaceFlex(
          &status_ctx, 160.0f * metrics.effective_scale, (float)metrics.label_h);
      DrawTextEx(GuiGetFont(), progress,
                 (Vector2){progress_rect.x + 4.0f, progress_rect.y + 6.0f},
                 (float)metrics.font_size, 1.0f, (Color){40, 80, 140, 255});

      GuiLayoutRect cancel_rect =
          GuiLayoutPlaceFixed(&status_ctx, cancel_w, (float)metrics.button_h);
      if (GuiButton(ToRayRect(cancel_rect), "Cancel")) {
        GuiWorkerRequestCancel();
      }
    } else {
      if (state.worker_snapshot.status == APP_STATUS_CANCELLED) {
        status_color = (Color){175, 120, 30, 255};
      } else if (state.worker_snapshot.success) {
        status_color = (Color){35, 120, 55, 255};
      } else if (state.worker_snapshot.has_result ||
                 state.worker_snapshot.status != APP_STATUS_OK) {
        status_color = (Color){160, 50, 40, 255};
      }

      GuiLayoutRect idle_rect = GuiLayoutPlaceFlex(
          &status_ctx, 180.0f * metrics.effective_scale, (float)metrics.label_h);
      DrawTextEx(GuiGetFont(), status_text,
                 (Vector2){idle_rect.x + 4.0f, idle_rect.y + 6.0f},
                 (float)metrics.font_size, 1.0f, status_color);
    }

    GuiLayoutNextLine(&status_ctx);

    GuiLayoutRect log_rect = {status_ctx.row_start_x, status_ctx.cursor_y,
                              status_ctx.row_right_x - status_ctx.row_start_x,
                              status_inner.y + status_inner.height - (float)metrics.pad -
                                  status_ctx.cursor_y};
    if (log_rect.height < 60.0f) {
      log_rect.height = 60.0f;
    }
    DrawRectangleLinesEx(ToRayRect(log_rect), 1.0f, (Color){200, 200, 200, 255});

    GuiLayoutRect log_text = {log_rect.x + (float)metrics.gap,
                              log_rect.y + (float)metrics.gap,
                              log_rect.width - (float)(metrics.gap * 2),
                              log_rect.height - (float)(metrics.gap * 2)};
    bool truncated =
        DrawMultilineTextInRect(state.detail_text, log_text, metrics.font_size - 3,
                                (Color){64, 64, 64, 255});
    if (truncated) {
      const char *truncated_text = "... (truncated)";
      Vector2 size = MeasureTextEx(GuiGetFont(), truncated_text,
                                   (float)(metrics.font_size - 3), 1.0f);
      DrawTextEx(
          GuiGetFont(), truncated_text,
          (Vector2){log_text.x + log_text.width - size.x - 2.0f,
                    log_text.y + log_text.height - size.y - 2.0f},
          (float)(metrics.font_size - 3), 1.0f, (Color){120, 120, 120, 255});
    }

#if defined(CGO_GUI_LAYOUT_DEBUG)
    if (!GuiLayoutContextIsValid(&status_ctx)) {
      strncpy(state.banner_message, "Layout warning: status panel overlap/bounds",
              sizeof(state.banner_message) - 1);
    }
#endif

    EndDrawing();
  }

  GuiUiPersistState(&state);
  GuiWorkerShutdown();
  AppContextDestroy(app);
  ShutdownGuiFont();
  CloseWindow();

  return 0;
}
