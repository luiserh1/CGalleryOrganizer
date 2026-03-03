#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_help.h"
#include "gui/frontends/functional/gui_fixed_layout.h"
#include "gui/gui_common.h"

static Font g_gui_font = {0};
static float g_gui_font_size = 20.0f;
static bool g_gui_font_loaded = false;
static const double GUI_RUNTIME_IDLE_REFRESH_SEC = 3.0;

static Rectangle ToRayRect(GuiRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
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

static bool DrawMultilineTextInRect(const char *text, Rectangle area, int font_size,
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

static ExitDialogAction DrawUnsavedExitDialog(void) {
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

  DrawTextEx(GuiGetFont(),
             "You have unsaved GUI configuration changes.\nSave before exit?",
             (Vector2){dialog.x + 14.0f, dialog.y + 48.0f}, g_gui_font_size - 2.0f,
             1.0f, (Color){64, 64, 64, 255});

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

static RebuildDialogAction DrawRebuildConfirmDialog(const char *reason) {
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
      GuiGetFont(),
      "Current scan semantics differ from the saved cache profile.\n"
      "Continuing will rebuild cache before running the task.",
      (Vector2){dialog.x + 14.0f, dialog.y + 46.0f}, g_gui_font_size - 2.0f, 1.0f,
      (Color){64, 64, 64, 255});

  char reason_line[APP_MAX_ERROR + 20] = {0};
  snprintf(reason_line, sizeof(reason_line), "Reason: %s",
           (reason && reason[0] != '\0') ? reason : "profile mismatch");
  DrawTextEx(GuiGetFont(), reason_line, (Vector2){dialog.x + 14.0f, dialog.y + 106.0f},
             g_gui_font_size - 3.0f, 1.0f, (Color){90, 70, 40, 255});

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
  GuiUiRefreshRuntimeState(&state);

  InitWindow(GUI_FIXED_WINDOW_WIDTH, GUI_FIXED_WINDOW_HEIGHT,
             "CGalleryOrganizer v0.6.10 GUI");
  InitGuiFont();
  GuiHelpSetFont(GuiGetFont(), g_gui_font_size - 2.0f);
  SetTargetFPS(60);

  const char *tab_labels[5] = {"Scan", "Similarity", "Organize", "Duplicates",
                               "Rename"};

  bool exit_requested = false;
  bool show_exit_dialog = false;
  double last_runtime_refresh = GetTime();
  while (!exit_requested) {
    bool close_requested = WindowShouldClose();
    if (close_requested && !show_exit_dialog) {
      if (GuiUiHasUnsavedChanges(&state)) {
        show_exit_dialog = true;
      } else {
        exit_requested = true;
        continue;
      }
    }

    GuiWorkerGetSnapshot(&state.worker_snapshot);
    if (!state.worker_snapshot.busy) {
      if (GetTime() - last_runtime_refresh >= GUI_RUNTIME_IDLE_REFRESH_SEC) {
        GuiUiRefreshRuntimeState(&state);
        last_runtime_refresh = GetTime();
      }
    }

    if (state.worker_snapshot.has_result) {
      if (state.worker_snapshot.message[0] != '\0') {
        if (state.worker_snapshot.success) {
          GuiUiSetBannerInfo(&state, state.worker_snapshot.message);
        } else {
          GuiUiSetBannerError(&state, state.worker_snapshot.message);
        }
      } else {
        if (state.worker_snapshot.success) {
          GuiUiSetBannerInfo(&state, "Task completed");
        } else {
          GuiUiSetBannerError(&state, "Task failed");
        }
      }

      if (state.worker_snapshot.detail_text[0] != '\0') {
        strncpy(state.detail_text, state.worker_snapshot.detail_text,
                sizeof(state.detail_text) - 1);
        state.detail_text[sizeof(state.detail_text) - 1] = '\0';
      }
      if (state.worker_snapshot.rename_preview_id[0] != '\0') {
        strncpy(state.rename_preview_id_input, state.worker_snapshot.rename_preview_id,
                sizeof(state.rename_preview_id_input) - 1);
        state.rename_preview_id_input[sizeof(state.rename_preview_id_input) - 1] =
            '\0';

        state.rename_preview_row_count =
            state.worker_snapshot.rename_preview_row_count;
        if (state.rename_preview_row_count < 0) {
          state.rename_preview_row_count = 0;
        }
        if (state.rename_preview_row_count > GUI_RENAME_PREVIEW_ROWS_MAX) {
          state.rename_preview_row_count = GUI_RENAME_PREVIEW_ROWS_MAX;
        }

        state.rename_preview_warning_count =
            state.worker_snapshot.rename_preview_warning_count;
        memcpy(state.rename_preview_rows, state.worker_snapshot.rename_preview_rows,
               sizeof(state.rename_preview_rows));
        state.rename_table_scroll = 0;

        if (state.rename_preview_row_count > 0) {
          if (state.rename_selected_row < 0 ||
              state.rename_selected_row >= state.rename_preview_row_count) {
            state.rename_selected_row = 0;
          }
          strncpy(state.rename_selected_tags_csv,
                  state.rename_preview_rows[state.rename_selected_row].tags_manual,
                  sizeof(state.rename_selected_tags_csv) - 1);
          state.rename_selected_tags_csv[sizeof(state.rename_selected_tags_csv) - 1] =
              '\0';
          strncpy(state.rename_selected_meta_tags_csv,
                  state.rename_preview_rows[state.rename_selected_row].tags_meta,
                  sizeof(state.rename_selected_meta_tags_csv) - 1);
          state.rename_selected_meta_tags_csv
              [sizeof(state.rename_selected_meta_tags_csv) - 1] = '\0';
        } else {
          state.rename_selected_row = -1;
          state.rename_selected_tags_csv[0] = '\0';
          state.rename_selected_meta_tags_csv[0] = '\0';
        }
      }
      if (state.worker_snapshot.rename_bootstrap_map_path[0] != '\0') {
        strncpy(state.rename_tags_map_path,
                state.worker_snapshot.rename_bootstrap_map_path,
                sizeof(state.rename_tags_map_path) - 1);
        state.rename_tags_map_path[sizeof(state.rename_tags_map_path) - 1] = '\0';
      }
      if (state.worker_snapshot.rename_apply_result.operation_id[0] != '\0') {
        strncpy(state.rename_operation_id_input,
                state.worker_snapshot.rename_apply_result.operation_id,
                sizeof(state.rename_operation_id_input) - 1);
        state.rename_operation_id_input[sizeof(state.rename_operation_id_input) -
                                        1] = '\0';
      }
      if (state.worker_snapshot.rename_latest_operation_id[0] != '\0') {
        strncpy(state.rename_operation_id_input,
                state.worker_snapshot.rename_latest_operation_id,
                sizeof(state.rename_operation_id_input) - 1);
        state.rename_operation_id_input[sizeof(state.rename_operation_id_input) -
                                        1] = '\0';
      }
      if (state.worker_snapshot.rename_history_export_path[0] != '\0') {
        strncpy(state.rename_history_export_path,
                state.worker_snapshot.rename_history_export_path,
                sizeof(state.rename_history_export_path) - 1);
        state.rename_history_export_path
            [sizeof(state.rename_history_export_path) - 1] = '\0';
      }

      GuiWorkerClearResult();
      if (!state.worker_snapshot.busy) {
        GuiUiRefreshRuntimeState(&state);
        last_runtime_refresh = GetTime();
      }
    }

    BeginDrawing();
    ClearBackground((Color){248, 248, 246, 255});
    GuiHelpBeginFrame();

    bool modal_open = show_exit_dialog || state.rebuild_confirm_pending;

    GuiShellLayout layout = {0};
    GuiBuildShellLayout(&layout);

    GuiLabel(ToRayRect(layout.title), "CGalleryOrganizer v0.6.10 - GUI");

    for (int i = 0; i < 5; i++) {
      bool selected = (state.active_tab == i);
      if (GuiButtonStyled(ToRayRect(layout.tabs[i]), tab_labels[i], !modal_open,
                          selected) &&
          !modal_open) {
        state.active_tab = i;
      }
    }

    DrawRectangleLinesEx(ToRayRect(layout.panel_outer), 1.0f, GRAY);
    Rectangle panel_inner = ToRayRect(layout.panel_inner);
    if (!modal_open) {
      if (state.active_tab == 0) {
        GuiDrawScanPanel(&state, panel_inner);
      } else if (state.active_tab == 1) {
        GuiDrawSimilarityPanel(&state, panel_inner);
      } else if (state.active_tab == 2) {
        GuiDrawOrganizePanel(&state, panel_inner);
      } else if (state.active_tab == 3) {
        GuiDrawDuplicatesPanel(&state, panel_inner);
      } else {
        GuiDrawRenamePanel(&state, panel_inner);
      }
    } else {
      GuiLabel((Rectangle){panel_inner.x + 14.0f, panel_inner.y + 14.0f, 600.0f, 30.0f},
               "Action confirmation pending...");
    }

    DrawRectangleLinesEx(ToRayRect(layout.status_outer), 1.0f, LIGHTGRAY);
    GuiLabel(ToRayRect(layout.status_label), "Task status:");

    Color status_color = DARKGRAY;
    const char *status_text =
        state.banner_message[0] != '\0' ? state.banner_message : "Idle";

    if (state.worker_snapshot.busy) {
      char progress[256] = {0};
      snprintf(progress, sizeof(progress), "Running [%s] %d/%d",
               state.worker_snapshot.progress_stage,
               state.worker_snapshot.progress_current,
               state.worker_snapshot.progress_total);
      DrawTextEx(GuiGetFont(), progress,
                 (Vector2){layout.progress_text.x + 4.0f,
                           layout.progress_text.y + 6.0f},
                 g_gui_font_size, 1.0f, (Color){40, 80, 140, 255});

      if (GuiButton(ToRayRect(layout.cancel_button), "Cancel")) {
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
      } else if (state.banner_is_error) {
        status_color = (Color){160, 50, 40, 255};
      }

      DrawTextEx(GuiGetFont(), status_text,
                 (Vector2){layout.status_value.x + 4.0f,
                           layout.status_value.y + 6.0f},
                 g_gui_font_size, 1.0f, status_color);
    }

    DrawRectangleLinesEx(ToRayRect(layout.log_box), 1.0f, (Color){200, 200, 200, 255});
    Rectangle log_box = ToRayRect(layout.log_box);
    Rectangle log_text = {log_box.x + 8.0f, log_box.y + 8.0f, log_box.width - 16.0f,
                          log_box.height - 16.0f};
    bool truncated = DrawMultilineTextInRect(state.detail_text, log_text,
                                             (int)g_gui_font_size - 3,
                                             (Color){64, 64, 64, 255});
    if (truncated) {
      const char *truncated_text = "... (truncated)";
      Vector2 size =
          MeasureTextEx(GuiGetFont(), truncated_text, g_gui_font_size - 3, 1.0f);
      DrawTextEx(
          GuiGetFont(), truncated_text,
          (Vector2){log_text.x + log_text.width - size.x - 2.0f,
                    log_text.y + log_text.height - size.y - 2.0f},
          g_gui_font_size - 3, 1.0f, (Color){120, 120, 120, 255});
    }

    if (show_exit_dialog) {
      ExitDialogAction action = DrawUnsavedExitDialog();
      if (action == EXIT_DIALOG_SAVE_AND_EXIT) {
        if (GuiUiPersistState(&state)) {
          exit_requested = true;
        } else {
          GuiUiSetBannerError(&state, "Failed to save state before exit");
          show_exit_dialog = false;
        }
      } else if (action == EXIT_DIALOG_DISCARD_EXIT) {
        exit_requested = true;
      } else if (action == EXIT_DIALOG_CANCEL) {
        show_exit_dialog = false;
      }
    } else if (state.rebuild_confirm_pending) {
      RebuildDialogAction action =
          DrawRebuildConfirmDialog(state.rebuild_confirm_reason);
      if (action == REBUILD_DIALOG_CONTINUE) {
        if (!GuiUiStartPendingRebuildTask(&state)) {
          GuiUiSetBannerError(&state,
                              "Failed to start task after rebuild confirmation");
        }
      } else if (action == REBUILD_DIALOG_CANCEL) {
        GuiUiCancelPendingRebuildTask(&state);
        GuiUiSetBannerInfo(&state, "Task start cancelled");
      }
    }

    GuiHelpDrawTooltip();
    EndDrawing();
  }

  GuiWorkerShutdown();
  AppContextDestroy(app);
  ShutdownGuiFont();
  CloseWindow();

  return 0;
}
