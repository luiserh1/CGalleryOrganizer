#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"

#include "gui/gui_common.h"

void GuiDrawSimilarityPanel(GuiUiState *state) {
  if (!state) {
    return;
  }

  GuiLabel((Rectangle){30, 120, 200, 24}, "Similarity Threshold (0..1)");
  {
    char threshold_buf[32] = {0};
    snprintf(threshold_buf, sizeof(threshold_buf), "%.3f", state->sim_threshold);
    if (GuiTextBox((Rectangle){250, 116, 100, 30}, threshold_buf,
                   sizeof(threshold_buf), true)) {
      float parsed = strtof(threshold_buf, NULL);
      if (parsed >= 0.0f && parsed <= 1.0f) {
        state->sim_threshold = parsed;
      }
    }
  }

  GuiLabel((Rectangle){370, 120, 120, 24}, "TopK");
  {
    char topk_buf[16] = {0};
    snprintf(topk_buf, sizeof(topk_buf), "%d", state->sim_topk);
    if (GuiTextBox((Rectangle){430, 116, 80, 30}, topk_buf, sizeof(topk_buf),
                   true)) {
      int parsed = atoi(topk_buf);
      if (parsed > 0 && parsed <= 1000) {
        state->sim_topk = parsed;
      }
    }
  }

  if (GuiCheckBox((Rectangle){30, 160, 220, 24}, "Incremental embeddings",
                  state->sim_incremental)) {
    state->sim_incremental = !state->sim_incremental;
  }

  GuiLabel((Rectangle){30, 196, 180, 24}, "Memory mode");
  if (GuiButton((Rectangle){170, 192, 90, 30}, "chunked")) {
    state->sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
  }
  if (GuiButton((Rectangle){270, 192, 90, 30}, "eager")) {
    state->sim_memory_mode = APP_SIM_MEMORY_EAGER;
  }

  if (GuiButton((Rectangle){30, 242, 240, 36}, "Run Similarity Report")) {
    GuiUiStartTask(state, GUI_TASK_SIMILARITY);
  }

  GuiLabel((Rectangle){30, 296, 900, 24},
           "This action runs scan/cache for embeddings and writes similarity_report.json");
}
