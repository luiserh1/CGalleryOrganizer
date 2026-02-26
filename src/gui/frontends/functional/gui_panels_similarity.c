#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_action_rules.h"
#include "gui/core/gui_help.h"
#include "gui/frontends/functional/gui_fixed_layout.h"
#include "gui/gui_common.h"

static Rectangle ToRayRect(GuiRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
}

static bool ActionButton(GuiUiState *state, Rectangle bounds, const char *text,
                         GuiActionId action_id, const char *help_message) {
  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(state, action_id, &availability);
  if (availability.enabled) {
    GuiHelpRegister(bounds, help_message);
  } else {
    char blocked_help[APP_MAX_ERROR + 96] = {0};
    snprintf(blocked_help, sizeof(blocked_help), "%s (blocked: %s)",
             help_message, availability.reason);
    GuiHelpRegister(bounds, blocked_help);
  }
  bool clicked = GuiButtonStyled(bounds, text, availability.enabled, false);
  if (!availability.enabled &&
      CheckCollisionPointRec(GetMousePosition(), bounds) &&
      IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
      availability.reason[0] != '\0') {
    strncpy(state->banner_message, availability.reason,
            sizeof(state->banner_message) - 1);
  }
  return availability.enabled && clicked;
}

void GuiDrawSimilarityPanel(GuiUiState *state, Rectangle panel_bounds) {
  if (!state) {
    return;
  }

  GuiSimilarityPanelLayout layout = {0};
  GuiRect panel = {panel_bounds.x, panel_bounds.y, panel_bounds.width,
                   panel_bounds.height};
  GuiBuildSimilarityPanelLayout(panel, &layout);

  GuiLabel(ToRayRect(layout.threshold_label), "Similarity Threshold (0..1)");
  GuiHelpRegister(ToRayRect(layout.threshold_label),
                  "Minimum cosine similarity accepted into report groups");
  if (GuiTextBox(ToRayRect(layout.threshold_input), state->sim_threshold_input,
                 (int)sizeof(state->sim_threshold_input), true)) {
    float parsed = strtof(state->sim_threshold_input, NULL);
    if (parsed >= 0.0f && parsed <= 1.0f) {
      state->sim_threshold = parsed;
      snprintf(state->sim_threshold_input, sizeof(state->sim_threshold_input),
               "%.3f", state->sim_threshold);
    }
  }
  GuiHelpRegister(ToRayRect(layout.threshold_input),
                  "Valid range: 0.000..1.000");

  GuiLabel(ToRayRect(layout.topk_label), "TopK");
  GuiHelpRegister(ToRayRect(layout.topk_label),
                  "Maximum neighbors per anchor in report");
  if (GuiTextBox(ToRayRect(layout.topk_input), state->sim_topk_input,
                 (int)sizeof(state->sim_topk_input), true)) {
    int parsed = atoi(state->sim_topk_input);
    if (parsed > 0 && parsed <= 1000) {
      state->sim_topk = parsed;
      snprintf(state->sim_topk_input, sizeof(state->sim_topk_input), "%d",
               state->sim_topk);
    }
  }
  GuiHelpRegister(ToRayRect(layout.topk_input), "Valid range: 1..1000");

  state->sim_incremental = GuiCheckBox(ToRayRect(layout.incremental),
                                       "Incremental embeddings",
                                       state->sim_incremental);
  GuiHelpRegister(ToRayRect(layout.incremental),
                  "Reuse valid embeddings and infer only stale/missing entries");

  GuiLabel(ToRayRect(layout.memory_label), "Memory mode");
  GuiHelpRegister(ToRayRect(layout.memory_label),
                  "chunked lowers memory; eager decodes all embeddings");
  if (GuiButtonStyled(ToRayRect(layout.mode_chunked), "chunked", true,
                      state->sim_memory_mode == APP_SIM_MEMORY_CHUNKED)) {
    state->sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
  }
  GuiHelpRegister(ToRayRect(layout.mode_chunked),
                  "Default mode with reduced peak memory usage");
  if (GuiButtonStyled(ToRayRect(layout.mode_eager), "eager", true,
                      state->sim_memory_mode == APP_SIM_MEMORY_EAGER)) {
    state->sim_memory_mode = APP_SIM_MEMORY_EAGER;
  }
  GuiHelpRegister(ToRayRect(layout.mode_eager),
                  "Debug parity mode; higher memory usage");

  if (ActionButton(state, ToRayRect(layout.run_button), "Run Similarity Report",
                   GUI_ACTION_SIMILARITY_REPORT,
                   "Generate similarity_report.json using cache embeddings")) {
    GuiUiStartTask(state, GUI_TASK_SIMILARITY);
  }

  GuiHelpDrawHintLabel(ToRayRect(layout.info_label),
                       "Similarity uses cache embeddings and writes similarity_report.json");
}
