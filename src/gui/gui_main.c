#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/gui_common.h"

static void DrawMultilineText(const char *text, int x, int y, int max_lines,
                              int font_size, Color color) {
  if (!text) {
    return;
  }

  int line = 0;
  const char *cursor = text;
  const char *line_start = text;

  while (*cursor != '\0' && line < max_lines) {
    if (*cursor == '\n') {
      int len = (int)(cursor - line_start);
      if (len > 0) {
        char buffer[1024] = {0};
        if (len >= (int)sizeof(buffer)) {
          len = (int)sizeof(buffer) - 1;
        }
        memcpy(buffer, line_start, (size_t)len);
        buffer[len] = '\0';
        DrawText(buffer, x, y + line * (font_size + 4), font_size, color);
      }
      line++;
      line_start = cursor + 1;
    }
    cursor++;
  }

  if (line < max_lines && line_start && *line_start != '\0') {
    DrawText(line_start, x, y + line * (font_size + 4), font_size, color);
  }
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

  InitWindow(1000, 760, "CGalleryOrganizer v0.5.0 GUI");
  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    GuiWorkerGetSnapshot(&state.worker_snapshot);

    if (state.worker_snapshot.has_result) {
      if (state.worker_snapshot.message[0] != '\0') {
        strncpy(state.banner_message, state.worker_snapshot.message,
                sizeof(state.banner_message) - 1);
      } else {
        strncpy(state.banner_message,
                state.worker_snapshot.success ? "Task completed"
                                             : "Task failed",
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

    GuiLabel((Rectangle){20, 20, 420, 30}, "CGalleryOrganizer v0.5.0 - GUI");

    if (GuiButton((Rectangle){20, 56, 120, 34}, "Scan")) {
      state.active_tab = 0;
    }
    if (GuiButton((Rectangle){150, 56, 120, 34}, "Similarity")) {
      state.active_tab = 1;
    }
    if (GuiButton((Rectangle){280, 56, 120, 34}, "Organize")) {
      state.active_tab = 2;
    }
    if (GuiButton((Rectangle){410, 56, 120, 34}, "Duplicates")) {
      state.active_tab = 3;
    }

    DrawRectangleLines(20, 100, 960, 620, GRAY);

    if (state.active_tab == 0) {
      GuiDrawScanPanel(&state);
    } else if (state.active_tab == 1) {
      GuiDrawSimilarityPanel(&state);
    } else if (state.active_tab == 2) {
      GuiDrawOrganizePanel(&state);
    } else {
      GuiDrawDuplicatesPanel(&state);
    }

    DrawRectangleLines(20, 330, 960, 360, LIGHTGRAY);
    GuiLabel((Rectangle){30, 338, 120, 24}, "Task status:");

    if (state.worker_snapshot.busy) {
      char progress[256] = {0};
      snprintf(progress, sizeof(progress), "Running [%s] %d/%d",
               state.worker_snapshot.progress_stage,
               state.worker_snapshot.progress_current,
               state.worker_snapshot.progress_total);
      GuiLabel((Rectangle){130, 338, 560, 24}, progress);

      if (GuiButton((Rectangle){710, 334, 120, 30}, "Cancel")) {
        GuiWorkerRequestCancel();
      }
    } else {
      GuiLabel((Rectangle){130, 338, 760, 24},
               state.banner_message[0] != '\0' ? state.banner_message
                                                : "Idle");
    }

    DrawRectangleLines(30, 370, 940, 310, (Color){200, 200, 200, 255});
    DrawMultilineText(state.detail_text, 40, 380, 16, 16, DARKGRAY);

    EndDrawing();
  }

  GuiWorkerShutdown();
  AppContextDestroy(app);
  CloseWindow();

  return 0;
}
