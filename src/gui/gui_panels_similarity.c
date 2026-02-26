#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/gui_common.h"

static Rectangle ToRayRect(GuiLayoutRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
}

static float LabelWidth(const char *text, const GuiLayoutMetrics *metrics) {
  return GuiLayoutMeasureTextWidth(text, metrics) + (float)(metrics->gap * 2);
}

void GuiDrawSimilarityPanel(GuiUiState *state, GuiLayoutRect panel_bounds,
                            const GuiLayoutMetrics *metrics) {
  if (!state || !metrics) {
    return;
  }

  GuiLayoutContext ctx = {0};
  GuiLayoutInit(&ctx, panel_bounds, metrics);

  GuiLayoutRect threshold_label = GuiLayoutPlaceFixed(
      &ctx, LabelWidth("Similarity Threshold (0..1)", metrics),
      (float)metrics->label_h);
  GuiLabel(ToRayRect(threshold_label), "Similarity Threshold (0..1)");
  {
    char threshold_buf[32] = {0};
    snprintf(threshold_buf, sizeof(threshold_buf), "%.3f", state->sim_threshold);
    GuiLayoutRect threshold_input = GuiLayoutPlaceFixed(
        &ctx, (float)metrics->min_numeric_input_w + (float)metrics->gap,
        (float)metrics->input_h);
    if (GuiTextBox(ToRayRect(threshold_input), threshold_buf,
                   sizeof(threshold_buf), true)) {
      float parsed = strtof(threshold_buf, NULL);
      if (parsed >= 0.0f && parsed <= 1.0f) {
        state->sim_threshold = parsed;
      }
    }
  }

  GuiLayoutRect topk_label =
      GuiLayoutPlaceFixed(&ctx, LabelWidth("TopK", metrics), (float)metrics->label_h);
  GuiLabel(ToRayRect(topk_label), "TopK");
  {
    char topk_buf[16] = {0};
    snprintf(topk_buf, sizeof(topk_buf), "%d", state->sim_topk);
    GuiLayoutRect topk_input =
        GuiLayoutPlaceFixed(&ctx, (float)metrics->min_numeric_input_w,
                            (float)metrics->input_h);
    if (GuiTextBox(ToRayRect(topk_input), topk_buf, sizeof(topk_buf), true)) {
      int parsed = atoi(topk_buf);
      if (parsed > 0 && parsed <= 1000) {
        state->sim_topk = parsed;
      }
    }
  }

  GuiLayoutNextLine(&ctx);

  GuiLayoutRect incremental_box = GuiLayoutPlaceFixed(
      &ctx, GuiLayoutButtonWidth("Incremental embeddings", metrics,
                                 210.0f * metrics->effective_scale),
      (float)metrics->label_h);
  if (GuiCheckBox(ToRayRect(incremental_box), "Incremental embeddings",
                  state->sim_incremental)) {
    state->sim_incremental = !state->sim_incremental;
  }

  GuiLayoutNextLine(&ctx);

  GuiLayoutRect memory_label = GuiLayoutPlaceFixed(
      &ctx, LabelWidth("Memory mode", metrics), (float)metrics->label_h);
  GuiLabel(ToRayRect(memory_label), "Memory mode");
  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, GuiLayoutButtonWidth("chunked", metrics,
                                               96.0f * metrics->effective_scale),
                    (float)metrics->button_h)),
                "chunked")) {
    state->sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
  }
  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, GuiLayoutButtonWidth("eager", metrics,
                                               96.0f * metrics->effective_scale),
                    (float)metrics->button_h)),
                "eager")) {
    state->sim_memory_mode = APP_SIM_MEMORY_EAGER;
  }

  GuiLayoutNextLine(&ctx);

  if (GuiButton(ToRayRect(GuiLayoutPlaceFixed(
                    &ctx, GuiLayoutButtonWidth("Run Similarity Report", metrics,
                                               220.0f * metrics->effective_scale),
                    (float)metrics->button_h)),
                "Run Similarity Report")) {
    GuiUiStartTask(state, GUI_TASK_SIMILARITY);
  }

  GuiLayoutNextLine(&ctx);
  GuiLayoutRect info = GuiLayoutPlaceFlex(&ctx, 300.0f * metrics->effective_scale,
                                          (float)metrics->label_h);
  GuiLabel(ToRayRect(info),
           "This action runs scan/cache for embeddings and writes similarity_report.json");

#if defined(CGO_GUI_LAYOUT_DEBUG)
  if (!GuiLayoutContextIsValid(&ctx)) {
    strncpy(state->banner_message,
            "Layout warning: similarity panel overlap/bounds",
            sizeof(state->banner_message) - 1);
  }
#endif
}
