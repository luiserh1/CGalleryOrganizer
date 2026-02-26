#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

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

static bool TryParseIntStrict(const char *text, int *out_value) {
  if (!text || !out_value) {
    return false;
  }

  char *endptr = NULL;
  long parsed = strtol(text, &endptr, 10);
  if (!endptr || *endptr != '\0') {
    return false;
  }
  if (parsed < INT_MIN || parsed > INT_MAX) {
    return false;
  }
  *out_value = (int)parsed;
  return true;
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

void GuiDrawScanPanel(GuiUiState *state, Rectangle panel_bounds) {
  if (!state) {
    return;
  }

  GuiScanPanelLayout layout = {0};
  GuiRect panel = {panel_bounds.x, panel_bounds.y, panel_bounds.width,
                   panel_bounds.height};
  GuiBuildScanPanelLayout(panel, &layout);

  GuiLabel(ToRayRect(layout.gallery_label), "Gallery Directory");
  GuiHelpRegister(ToRayRect(layout.gallery_label),
                  "Directory to scan for media files");
  GuiTextBox(ToRayRect(layout.gallery_input), state->gallery_dir,
             (int)sizeof(state->gallery_dir),
             true);
  GuiHelpRegister(ToRayRect(layout.gallery_input),
                  "Enter source gallery path (copy/paste supported)");

  GuiLabel(ToRayRect(layout.env_label), "Environment Dir");
  GuiHelpRegister(ToRayRect(layout.env_label),
                  "Directory where cache/reports/manifest are stored");
  GuiTextBox(ToRayRect(layout.env_input), state->env_dir, (int)sizeof(state->env_dir),
             true);
  GuiHelpRegister(ToRayRect(layout.env_input),
                  "Enter environment path (copy/paste supported)");

  state->exhaustive = GuiCheckBox(ToRayRect(layout.exhaustive),
                                  "Exhaustive metadata", state->exhaustive);
  GuiHelpRegister(ToRayRect(layout.exhaustive),
                  "Enable deeper metadata extraction (slower, larger cache)");

  GuiLabel(ToRayRect(layout.jobs_label), "Jobs");
  char jobs_help[APP_MAX_ERROR] = {0};
  int recommended_jobs = state->runtime_state_valid
                             ? state->runtime_state.recommended_jobs
                             : 1;
  int logical_cores = state->runtime_state_valid ? state->runtime_state.logical_cores
                                                 : 1;
  snprintf(jobs_help, sizeof(jobs_help),
           "Parallel workers (1..256, recommended 1..%d for %d logical cores)",
           recommended_jobs, logical_cores);
  GuiHelpRegister(ToRayRect(layout.jobs_label),
                  jobs_help);
  if (GuiTextBox(ToRayRect(layout.jobs_input), state->jobs_input,
                 (int)sizeof(state->jobs_input), true)) {
    int parsed = 0;
    if (!TryParseIntStrict(state->jobs_input, &parsed)) {
      strncpy(state->banner_message, "Jobs must be an integer between 1 and 256",
              sizeof(state->banner_message) - 1);
      snprintf(state->jobs_input, sizeof(state->jobs_input), "%d", state->jobs);
    } else {
      int clamped = parsed;
      if (clamped < 1) {
        clamped = 1;
      } else if (clamped > 256) {
        clamped = 256;
      }
      if (clamped != parsed) {
        snprintf(state->banner_message, sizeof(state->banner_message),
                 "Jobs clamped to %d (allowed range: 1..256)", clamped);
      }
      state->jobs = clamped;
      snprintf(state->jobs_input, sizeof(state->jobs_input), "%d", state->jobs);
    }
  }
  GuiHelpRegister(ToRayRect(layout.jobs_input), jobs_help);

  GuiLabel(ToRayRect(layout.cache_label), "Cache compression");
  GuiHelpRegister(ToRayRect(layout.cache_label),
                  "none: fastest writes, zstd: smaller files, auto: threshold-based");
  if (GuiButtonStyled(ToRayRect(layout.cache_none), "none", true,
                      state->cache_mode == APP_CACHE_COMPRESSION_NONE)) {
    state->cache_mode = APP_CACHE_COMPRESSION_NONE;
  }
  GuiHelpRegister(ToRayRect(layout.cache_none),
                  "Write plain JSON cache without compression");
  if (GuiButtonStyled(ToRayRect(layout.cache_zstd), "zstd", true,
                      state->cache_mode == APP_CACHE_COMPRESSION_ZSTD)) {
    state->cache_mode = APP_CACHE_COMPRESSION_ZSTD;
  }
  GuiHelpRegister(ToRayRect(layout.cache_zstd),
                  "Force zstd-compressed cache");
  if (GuiButtonStyled(ToRayRect(layout.cache_auto), "auto", true,
                      state->cache_mode == APP_CACHE_COMPRESSION_AUTO)) {
    state->cache_mode = APP_CACHE_COMPRESSION_AUTO;
  }
  GuiHelpRegister(ToRayRect(layout.cache_auto),
                  "Auto-select compression based on cache size threshold");

  GuiLabel(ToRayRect(layout.level_label), "Level");
  GuiHelpRegister(ToRayRect(layout.level_label), "zstd compression level (1..19)");
  if (GuiTextBox(ToRayRect(layout.level_input), state->cache_level_input,
                 (int)sizeof(state->cache_level_input), true)) {
    int parsed = 0;
    if (!TryParseIntStrict(state->cache_level_input, &parsed)) {
      strncpy(state->banner_message,
              "Compression level must be an integer between 1 and 19",
              sizeof(state->banner_message) - 1);
      snprintf(state->cache_level_input, sizeof(state->cache_level_input), "%d",
               state->cache_level);
    } else {
      int clamped = parsed;
      if (clamped < 1) {
        clamped = 1;
      } else if (clamped > 19) {
        clamped = 19;
      }
      if (clamped != parsed) {
        snprintf(state->banner_message, sizeof(state->banner_message),
                 "Compression level clamped to %d (allowed range: 1..19)",
                 clamped);
      }
      state->cache_level = clamped;
      snprintf(state->cache_level_input, sizeof(state->cache_level_input), "%d",
               state->cache_level);
    }
  }
  GuiHelpRegister(ToRayRect(layout.level_input), "Valid compression level: 1..19");

  DrawLineEx((Vector2){layout.actions_divider.x, layout.actions_divider.y},
             (Vector2){layout.actions_divider.x + layout.actions_divider.width,
                       layout.actions_divider.y},
             1.0f, (Color){192, 192, 192, 255});

  if (ActionButton(state, ToRayRect(layout.scan_button), "Scan/Cache",
                   GUI_ACTION_SCAN_CACHE,
                   "Scan gallery and update cache in environment directory")) {
    GuiUiStartTask(state, GUI_TASK_SCAN);
  }
  if (ActionButton(state, ToRayRect(layout.ml_button), "ML Enrich",
                   GUI_ACTION_ML_ENRICH,
                   "Run classification/text detection/embeddings into cache")) {
    GuiUiStartTask(state, GUI_TASK_ML_ENRICH);
  }
  if (ActionButton(state, ToRayRect(layout.download_models_button),
                   "Download Models", GUI_ACTION_DOWNLOAD_MODELS,
                   "Install models from manifest into build/models")) {
    GuiUiStartTask(state, GUI_TASK_DOWNLOAD_MODELS);
  }
  if (ActionButton(state, ToRayRect(layout.save_paths_button), "Save Paths",
                   GUI_ACTION_SAVE_PATHS,
                   "Persist Gallery/Environment paths for next launch")) {
    if (GuiUiPersistState(state)) {
      strncpy(state->banner_message, "Paths saved",
              sizeof(state->banner_message) - 1);
    } else {
      strncpy(state->banner_message, "Failed to save paths",
              sizeof(state->banner_message) - 1);
    }
  }
  if (ActionButton(state, ToRayRect(layout.reset_paths_button),
                   "Reset Saved Paths", GUI_ACTION_RESET_PATHS,
                   "Clear persisted Gallery/Environment paths")) {
    GuiStateReset();
    state->gallery_dir[0] = '\0';
    state->env_dir[0] = '\0';
    state->persisted_state.gallery_dir[0] = '\0';
    state->persisted_state.env_dir[0] = '\0';
    strncpy(state->banner_message, "Saved GUI state reset",
            sizeof(state->banner_message) - 1);
  }

  GuiHelpDrawHintLabel(
      ToRayRect(layout.info_label),
      "Hints: configure paths and workers, install models if needed, then run tasks.");
}
