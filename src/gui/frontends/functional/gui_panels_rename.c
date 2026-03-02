#include <stdio.h>
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
    GuiUiSetBannerError(state, availability.reason);
  }
  return availability.enabled && clicked;
}

void GuiDrawRenamePanel(GuiUiState *state, Rectangle panel_bounds) {
  if (!state) {
    return;
  }

  GuiRenamePanelLayout layout = {0};
  GuiRect panel = {panel_bounds.x, panel_bounds.y, panel_bounds.width,
                   panel_bounds.height};
  GuiBuildRenamePanelLayout(panel, &layout);

  GuiLabel(ToRayRect(layout.pattern_label), "Pattern");
  GuiHelpRegister(ToRayRect(layout.pattern_label),
                  "Rename pattern tokens: [date],[time],[datetime],[camera],"
                  "[make],[model],[format],[index],[tags_manual],"
                  "[tags_meta],[tags]");
  GuiTextBox(ToRayRect(layout.pattern_input), state->rename_pattern_input,
             (int)sizeof(state->rename_pattern_input), true);
  GuiHelpRegister(ToRayRect(layout.pattern_input),
                  "Example: pic-[tags]-[camera].[format]");

  GuiLabel(ToRayRect(layout.tags_map_label), "Tags map JSON");
  GuiTextBox(ToRayRect(layout.tags_map_input), state->rename_tags_map_path,
             (int)sizeof(state->rename_tags_map_path), true);
  GuiHelpRegister(ToRayRect(layout.tags_map_input),
                  "Optional JSON path to apply per-file manual tag edits");

  GuiLabel(ToRayRect(layout.tag_add_label), "Bulk add tags");
  GuiTextBox(ToRayRect(layout.tag_add_input), state->rename_tag_add_csv,
             (int)sizeof(state->rename_tag_add_csv), true);
  GuiHelpRegister(ToRayRect(layout.tag_add_input),
                  "Optional CSV manual tags applied to every file in scope");

  GuiLabel(ToRayRect(layout.tag_remove_label), "Bulk remove");
  GuiTextBox(ToRayRect(layout.tag_remove_input), state->rename_tag_remove_csv,
             (int)sizeof(state->rename_tag_remove_csv), true);
  GuiHelpRegister(ToRayRect(layout.tag_remove_input),
                  "Optional CSV tags removed/suppressed from scope");

  GuiLabel(ToRayRect(layout.preview_id_label), "Preview ID");
  GuiTextBox(ToRayRect(layout.preview_id_input), state->rename_preview_id_input,
             (int)sizeof(state->rename_preview_id_input), true);
  GuiHelpRegister(ToRayRect(layout.preview_id_input),
                  "Required by Rename Apply; produced by Rename Preview");

  GuiLabel(ToRayRect(layout.operation_id_label), "Operation ID");
  GuiTextBox(ToRayRect(layout.operation_id_input),
             state->rename_operation_id_input,
             (int)sizeof(state->rename_operation_id_input), true);
  GuiHelpRegister(ToRayRect(layout.operation_id_input),
                  "Required by Rename Rollback; produced by Rename Apply");

  state->rename_accept_auto_suffix =
      GuiCheckBox(ToRayRect(layout.accept_suffix),
                  "Accept deterministic collision suffixes (_1,_2,...)",
                  state->rename_accept_auto_suffix);
  GuiHelpRegister(ToRayRect(layout.accept_suffix),
                  "Apply must enable this if preview reports collisions");

  if (ActionButton(state, ToRayRect(layout.preview_button), "Rename Preview",
                   GUI_ACTION_RENAME_PREVIEW,
                   "Build rename preview artifact without filesystem mutation")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_PREVIEW);
  }
  if (ActionButton(state, ToRayRect(layout.apply_button), "Rename Apply",
                   GUI_ACTION_RENAME_APPLY,
                   "Apply rename from preview id with handshake checks")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_APPLY);
  }
  if (ActionButton(state, ToRayRect(layout.history_button), "Rename History",
                   GUI_ACTION_RENAME_HISTORY,
                   "List dedicated rename operation history")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_HISTORY);
  }
  if (ActionButton(state, ToRayRect(layout.rollback_button), "Rename Rollback",
                   GUI_ACTION_RENAME_ROLLBACK,
                   "Rollback a dedicated rename operation id")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_ROLLBACK);
  }

  char hint[256] = {0};
  snprintf(hint, sizeof(hint),
           "Last preview: id=%s files=%d collisions=%d history=%d",
           state->worker_snapshot.rename_preview_id[0] != '\0'
               ? state->worker_snapshot.rename_preview_id
               : "n/a",
           state->worker_snapshot.rename_preview_file_count,
           state->worker_snapshot.rename_preview_collision_count,
           state->worker_snapshot.rename_history_count);
  GuiHelpDrawHintLabel(ToRayRect(layout.info_label), hint);
}
