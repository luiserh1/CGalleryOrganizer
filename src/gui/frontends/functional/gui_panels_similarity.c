#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_action_rules.h"
#include "gui/frontends/functional/gui_fixed_layout.h"
#include "gui/gui_common.h"

static Rectangle ToRayRect(GuiRect rect) {
  Rectangle out = {rect.x, rect.y, rect.width, rect.height};
  return out;
}

static bool ActionButton(GuiUiState *state, Rectangle bounds, const char *text,
                         GuiActionId action_id) {
  GuiActionAvailability availability = {0};
  GuiResolveActionAvailability(state, action_id, &availability);
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
  if (GuiTextBox(ToRayRect(layout.threshold_input), state->sim_threshold_input,
                 (int)sizeof(state->sim_threshold_input), true)) {
    float parsed = strtof(state->sim_threshold_input, NULL);
    if (parsed >= 0.0f && parsed <= 1.0f) {
      state->sim_threshold = parsed;
      snprintf(state->sim_threshold_input, sizeof(state->sim_threshold_input),
               "%.3f", state->sim_threshold);
    }
  }

  GuiLabel(ToRayRect(layout.topk_label), "TopK");
  if (GuiTextBox(ToRayRect(layout.topk_input), state->sim_topk_input,
                 (int)sizeof(state->sim_topk_input), true)) {
    int parsed = atoi(state->sim_topk_input);
    if (parsed > 0 && parsed <= 1000) {
      state->sim_topk = parsed;
      snprintf(state->sim_topk_input, sizeof(state->sim_topk_input), "%d",
               state->sim_topk);
    }
  }

  state->sim_incremental = GuiCheckBox(ToRayRect(layout.incremental),
                                       "Incremental embeddings",
                                       state->sim_incremental);

  GuiLabel(ToRayRect(layout.memory_label), "Memory mode");
  if (GuiButtonStyled(ToRayRect(layout.mode_chunked), "chunked", true,
                      state->sim_memory_mode == APP_SIM_MEMORY_CHUNKED)) {
    state->sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
  }
  if (GuiButtonStyled(ToRayRect(layout.mode_eager), "eager", true,
                      state->sim_memory_mode == APP_SIM_MEMORY_EAGER)) {
    state->sim_memory_mode = APP_SIM_MEMORY_EAGER;
  }

  if (ActionButton(state, ToRayRect(layout.run_button), "Run Similarity Report",
                   GUI_ACTION_SIMILARITY_REPORT)) {
    GuiUiStartTask(state, GUI_TASK_SIMILARITY);
  }

  GuiLabel(ToRayRect(layout.info_label),
           "This action runs scan/cache for embeddings and writes similarity_report.json");
}
