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

void GuiDrawScanPanel(GuiUiState *state, Rectangle panel_bounds) {
  if (!state) {
    return;
  }

  GuiScanPanelLayout layout = {0};
  GuiRect panel = {panel_bounds.x, panel_bounds.y, panel_bounds.width,
                   panel_bounds.height};
  GuiBuildScanPanelLayout(panel, &layout);

  GuiLabel(ToRayRect(layout.gallery_label), "Gallery Directory");
  GuiTextBox(ToRayRect(layout.gallery_input), state->gallery_dir,
             (int)sizeof(state->gallery_dir),
             true);

  GuiLabel(ToRayRect(layout.env_label), "Environment Dir");
  GuiTextBox(ToRayRect(layout.env_input), state->env_dir, (int)sizeof(state->env_dir),
             true);

  state->exhaustive = GuiCheckBox(ToRayRect(layout.exhaustive),
                                  "Exhaustive metadata", state->exhaustive);

  GuiLabel(ToRayRect(layout.jobs_label), "Jobs");
  if (GuiTextBox(ToRayRect(layout.jobs_input), state->jobs_input,
                 (int)sizeof(state->jobs_input), true)) {
    int parsed = atoi(state->jobs_input);
    if (parsed > 0 && parsed <= 256) {
      state->jobs = parsed;
      snprintf(state->jobs_input, sizeof(state->jobs_input), "%d", state->jobs);
    }
  }

  GuiLabel(ToRayRect(layout.cache_label), "Cache compression");
  if (GuiButtonStyled(ToRayRect(layout.cache_none), "none", true,
                      state->cache_mode == APP_CACHE_COMPRESSION_NONE)) {
    state->cache_mode = APP_CACHE_COMPRESSION_NONE;
  }
  if (GuiButtonStyled(ToRayRect(layout.cache_zstd), "zstd", true,
                      state->cache_mode == APP_CACHE_COMPRESSION_ZSTD)) {
    state->cache_mode = APP_CACHE_COMPRESSION_ZSTD;
  }
  if (GuiButtonStyled(ToRayRect(layout.cache_auto), "auto", true,
                      state->cache_mode == APP_CACHE_COMPRESSION_AUTO)) {
    state->cache_mode = APP_CACHE_COMPRESSION_AUTO;
  }

  GuiLabel(ToRayRect(layout.level_label), "Level");
  if (GuiTextBox(ToRayRect(layout.level_input), state->cache_level_input,
                 (int)sizeof(state->cache_level_input), true)) {
    int parsed = atoi(state->cache_level_input);
    if (parsed >= 1 && parsed <= 19) {
      state->cache_level = parsed;
      snprintf(state->cache_level_input, sizeof(state->cache_level_input), "%d",
               state->cache_level);
    }
  }

  DrawLineEx((Vector2){layout.actions_divider.x, layout.actions_divider.y},
             (Vector2){layout.actions_divider.x + layout.actions_divider.width,
                       layout.actions_divider.y},
             1.0f, (Color){192, 192, 192, 255});

  if (ActionButton(state, ToRayRect(layout.scan_button), "Scan/Cache",
                   GUI_ACTION_SCAN_CACHE)) {
    GuiUiStartTask(state, GUI_TASK_SCAN);
  }
  if (ActionButton(state, ToRayRect(layout.ml_button), "ML Enrich",
                   GUI_ACTION_ML_ENRICH)) {
    GuiUiStartTask(state, GUI_TASK_ML_ENRICH);
  }
  if (ActionButton(state, ToRayRect(layout.save_paths_button), "Save Paths",
                   GUI_ACTION_SAVE_PATHS)) {
    if (GuiUiPersistState(state)) {
      strncpy(state->banner_message, "Paths saved",
              sizeof(state->banner_message) - 1);
    } else {
      strncpy(state->banner_message, "Failed to save paths",
              sizeof(state->banner_message) - 1);
    }
  }
  if (ActionButton(state, ToRayRect(layout.reset_paths_button),
                   "Reset Saved Paths", GUI_ACTION_RESET_PATHS)) {
    GuiStateReset();
    state->gallery_dir[0] = '\0';
    state->env_dir[0] = '\0';
    state->persisted_state.gallery_dir[0] = '\0';
    state->persisted_state.env_dir[0] = '\0';
    strncpy(state->banner_message, "Saved GUI state reset",
            sizeof(state->banner_message) - 1);
  }
}
