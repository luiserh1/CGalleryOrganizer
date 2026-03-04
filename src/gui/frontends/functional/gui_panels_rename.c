#include <stdio.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_action_rules.h"
#include "gui/core/gui_help.h"
#include "gui/core/gui_path_picker.h"
#include "gui/core/gui_rename_map.h"
#include "gui/frontends/functional/gui_fixed_layout.h"
#include "gui/frontends/functional/gui_panels_rename_internal.h"
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

static void HandleFilePickerFailure(GuiUiState *state,
                                    GuiPathPickerStatus status,
                                    const char *error_message,
                                    const char *fallback_error) {
  const char *message = (error_message && error_message[0] != '\0')
                            ? error_message
                            : fallback_error;
  if (status == GUI_PATH_PICKER_STATUS_CANCELLED ||
      status == GUI_PATH_PICKER_STATUS_UNAVAILABLE) {
    GuiUiSetBannerInfo(state, message);
    return;
  }
  GuiUiSetBannerError(state, message);
}

static bool ResolveManualTagsMapPath(GuiUiState *state, char *out_map_path,
                                     size_t out_map_path_size) {
  if (!state || !out_map_path || out_map_path_size == 0) {
    return false;
  }

  out_map_path[0] = '\0';
  if (state->rename_tags_map_path[0] != '\0') {
    strncpy(out_map_path, state->rename_tags_map_path, out_map_path_size - 1);
    out_map_path[out_map_path_size - 1] = '\0';
    return true;
  }

  if (state->env_dir[0] == '\0') {
    return false;
  }

  snprintf(out_map_path, out_map_path_size, "%s/.cache/rename_tags.json",
           state->env_dir);
  strncpy(state->rename_tags_map_path, out_map_path,
          sizeof(state->rename_tags_map_path) - 1);
  state->rename_tags_map_path[sizeof(state->rename_tags_map_path) - 1] = '\0';
  return true;
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
                  "[make],[model],[format],[gps_lat],[gps_lon],[location],"
                  "[index],[tags_manual],"
                  "[tags_meta],[tags]");
  GuiTextBox(ToRayRect(layout.pattern_input), state->rename_pattern_input,
             (int)sizeof(state->rename_pattern_input), true);
  GuiHelpRegister(ToRayRect(layout.pattern_input),
                  "Example: pic-[tags]-[camera].[format]");

  GuiLabel(ToRayRect(layout.tags_map_label), "Tags map JSON");
  GuiTextBox(ToRayRect(layout.tags_map_input), state->rename_tags_map_path,
             (int)sizeof(state->rename_tags_map_path), true);
  GuiHelpRegister(ToRayRect(layout.tags_map_input),
                  "Optional JSON path to apply per-file manual/metadata tag edits");
  if (GuiButtonStyled(ToRayRect(layout.tags_map_pick_button), "Pick...", true,
                      false)) {
    char picked[GUI_STATE_MAX_PATH] = {0};
    char error[APP_MAX_ERROR] = {0};
    GuiPathPickerStatus status = GuiPickFilePathEx(
        "Select Rename Tags Map JSON", picked, sizeof(picked), error,
        sizeof(error));
    if (status == GUI_PATH_PICKER_STATUS_OK) {
      strncpy(state->rename_tags_map_path, picked,
              sizeof(state->rename_tags_map_path) - 1);
      state->rename_tags_map_path[sizeof(state->rename_tags_map_path) - 1] = '\0';
    } else {
      HandleFilePickerFailure(state, status, error, "Tags map picker failed");
    }
  }
  GuiHelpRegister(ToRayRect(layout.tags_map_pick_button),
                  "Pick an existing tags map JSON file");
  if (ActionButton(state, ToRayRect(layout.tags_map_bootstrap_button),
                   "Bootstrap Tags", GUI_ACTION_RENAME_BOOTSTRAP_TAGS,
                   "Generate tags map from filename numeric tokens")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_BOOTSTRAP_TAGS);
  }

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

  GuiLabel(ToRayRect(layout.meta_tag_add_label), "Bulk add meta");
  GuiTextBox(ToRayRect(layout.meta_tag_add_input), state->rename_meta_tag_add_csv,
             (int)sizeof(state->rename_meta_tag_add_csv), true);
  GuiHelpRegister(ToRayRect(layout.meta_tag_add_input),
                  "Optional CSV metadata tags added to every file in scope");

  GuiLabel(ToRayRect(layout.meta_tag_remove_label), "Bulk remove meta");
  GuiTextBox(ToRayRect(layout.meta_tag_remove_input),
             state->rename_meta_tag_remove_csv,
             (int)sizeof(state->rename_meta_tag_remove_csv), true);
  GuiHelpRegister(ToRayRect(layout.meta_tag_remove_input),
                  "Optional CSV metadata tags suppressed for preview scope");

  GuiLabel(ToRayRect(layout.preview_id_label), "Preview ID");
  GuiTextBox(ToRayRect(layout.preview_id_input), state->rename_preview_id_input,
             (int)sizeof(state->rename_preview_id_input), true);
  GuiHelpRegister(ToRayRect(layout.preview_id_input),
                  "Required by Rename Apply; produced by Rename Preview");
  if (ActionButton(state, ToRayRect(layout.preview_latest_button), "Latest",
                   GUI_ACTION_RENAME_PREVIEW_LATEST_ID,
                   "Resolve latest preview id from rename preview artifacts")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_PREVIEW_LATEST_ID);
  }

  GuiLabel(ToRayRect(layout.operation_id_label), "Operation ID");
  GuiTextBox(ToRayRect(layout.operation_id_input),
             state->rename_operation_id_input,
             (int)sizeof(state->rename_operation_id_input), true);
  GuiHelpRegister(ToRayRect(layout.operation_id_input),
                  "Required by Rename Rollback; produced by Rename Apply");
  if (ActionButton(state, ToRayRect(layout.history_latest_button), "Latest",
                   GUI_ACTION_RENAME_HISTORY_LATEST_ID,
                   "Resolve latest operation id from rename history")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_HISTORY_LATEST_ID);
  }

  state->rename_accept_auto_suffix =
      GuiCheckBox(ToRayRect(layout.accept_suffix),
                  "Accept deterministic collision suffixes (_1,_2,...)",
                  state->rename_accept_auto_suffix);
  GuiHelpRegister(ToRayRect(layout.accept_suffix),
                  "Apply must enable this if preview reports collisions");
  state->rename_filter_collisions_only =
      GuiCheckBox(ToRayRect(layout.filter_collisions), "Collisions only",
                  state->rename_filter_collisions_only);
  GuiHelpRegister(ToRayRect(layout.filter_collisions),
                  "Show only preview rows with collision flags");
  state->rename_filter_warnings_only =
      GuiCheckBox(ToRayRect(layout.filter_warnings), "Warnings only",
                  state->rename_filter_warnings_only);
  GuiHelpRegister(ToRayRect(layout.filter_warnings),
                  "Show only preview rows with warning or truncation flags");
  GuiLabel(ToRayRect(layout.history_rollback_mode_label), "Rollback");
  bool rollback_any = state->rename_history_rollback_filter_mode == 0;
  bool rollback_yes = state->rename_history_rollback_filter_mode == 1;
  bool rollback_no = state->rename_history_rollback_filter_mode == 2;
  if (GuiButtonStyled(ToRayRect(layout.history_rollback_any), "Any", true,
                      rollback_any)) {
    state->rename_history_rollback_filter_mode = 0;
  }
  if (GuiButtonStyled(ToRayRect(layout.history_rollback_yes), "Yes", true,
                      rollback_yes)) {
    state->rename_history_rollback_filter_mode = 1;
  }
  if (GuiButtonStyled(ToRayRect(layout.history_rollback_no), "No", true,
                      rollback_no)) {
    state->rename_history_rollback_filter_mode = 2;
  }
  GuiHelpRegister(ToRayRect(layout.history_rollback_any),
                  "History/export rollback filter: any");
  GuiHelpRegister(ToRayRect(layout.history_rollback_yes),
                  "History/export rollback filter: rollback completed only");
  GuiHelpRegister(ToRayRect(layout.history_rollback_no),
                  "History/export rollback filter: rollback not performed");

  GuiLabel(ToRayRect(layout.history_from_label), "From");
  GuiTextBox(ToRayRect(layout.history_from_input), state->rename_history_from_input,
             (int)sizeof(state->rename_history_from_input), true);
  GuiHelpRegister(ToRayRect(layout.history_from_input),
                  "History/export lower time bound: YYYY-MM-DD or UTC timestamp");
  GuiLabel(ToRayRect(layout.history_to_label), "To");
  GuiTextBox(ToRayRect(layout.history_to_input), state->rename_history_to_input,
             (int)sizeof(state->rename_history_to_input), true);
  GuiHelpRegister(ToRayRect(layout.history_to_input),
                  "History/export upper time bound: YYYY-MM-DD or UTC timestamp");

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
                   "List dedicated rename operation history (filters use "
                   "Operation ID as prefix + rollback/date controls)")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_HISTORY);
  }
  if (ActionButton(state, ToRayRect(layout.detail_button), "History Detail",
                   GUI_ACTION_RENAME_HISTORY_DETAIL,
                   "Show detailed summary and manifest JSON for operation id")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_HISTORY_DETAIL);
  }
  if (ActionButton(state, ToRayRect(layout.redo_button), "Rename Redo",
                   GUI_ACTION_RENAME_REDO,
                   "Resolve operation->preview and re-apply with standard safeguards")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_REDO);
  }
  if (ActionButton(state, ToRayRect(layout.preflight_button), "Rollback Check",
                   GUI_ACTION_RENAME_ROLLBACK_PREFLIGHT,
                   "Non-mutating rollback preflight for operation id")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_ROLLBACK_PREFLIGHT);
  }
  if (ActionButton(state, ToRayRect(layout.rollback_button), "Rename Rollback",
                   GUI_ACTION_RENAME_ROLLBACK,
                   "Rollback a dedicated rename operation id")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_ROLLBACK);
  }

  GuiLabel(ToRayRect(layout.selected_tags_label), "Selected manual");
  GuiTextBox(ToRayRect(layout.selected_tags_input), state->rename_selected_tags_csv,
             (int)sizeof(state->rename_selected_tags_csv), true);
  GuiHelpRegister(ToRayRect(layout.selected_tags_input),
                  "CSV manual tags for selected preview row (ex: 001,frag-a)");
  if (GuiButtonStyled(ToRayRect(layout.selected_tags_apply_button),
                      "Apply Manual", true, false)) {
    if (state->rename_selected_row < 0 ||
        state->rename_selected_row >= state->rename_preview_row_count) {
      GuiUiSetBannerError(state, "Select a preview row before applying tags");
    } else {
      char map_path[GUI_STATE_MAX_PATH] = {0};
      if (!ResolveManualTagsMapPath(state, map_path, sizeof(map_path))) {
        GuiUiSetBannerError(
            state,
            "Set Environment Dir or Tags map JSON before applying per-file tags");
      } else {
        char error[APP_MAX_ERROR] = {0};
        GuiRenamePreviewRow *row =
            &state->rename_preview_rows[state->rename_selected_row];
        if (!GuiRenameMapUpsertManualTags(
                map_path, row->source_path, state->rename_selected_tags_csv, error,
                sizeof(error))) {
          GuiUiSetBannerError(state, error[0] != '\0' ? error
                                                       : "Failed to apply manual tags");
        } else {
          if (state->rename_selected_tags_csv[0] != '\0') {
            strncpy(row->tags_manual, state->rename_selected_tags_csv,
                    sizeof(row->tags_manual) - 1);
            row->tags_manual[sizeof(row->tags_manual) - 1] = '\0';
          } else {
            strncpy(row->tags_manual, "untagged", sizeof(row->tags_manual) - 1);
            row->tags_manual[sizeof(row->tags_manual) - 1] = '\0';
          }
          GuiUiSetBannerInfo(
              state,
              "Manual tags updated. Run Rename Preview to refresh candidates.");
        }
      }
    }
  }
  GuiHelpRegister(ToRayRect(layout.selected_tags_apply_button),
                  "Persist selected-row manual tags into tags map");

  GuiLabel(ToRayRect(layout.selected_meta_tags_label), "Selected meta");
  GuiTextBox(ToRayRect(layout.selected_meta_tags_input),
             state->rename_selected_meta_tags_csv,
             (int)sizeof(state->rename_selected_meta_tags_csv), true);
  GuiHelpRegister(ToRayRect(layout.selected_meta_tags_input),
                  "CSV metadata tags for selected preview row");
  if (GuiButtonStyled(ToRayRect(layout.selected_meta_tags_apply_button),
                      "Apply Meta", true, false)) {
    if (state->rename_selected_row < 0 ||
        state->rename_selected_row >= state->rename_preview_row_count) {
      GuiUiSetBannerError(state, "Select a preview row before applying tags");
    } else {
      char map_path[GUI_STATE_MAX_PATH] = {0};
      if (!ResolveManualTagsMapPath(state, map_path, sizeof(map_path))) {
        GuiUiSetBannerError(
            state,
            "Set Environment Dir or Tags map JSON before applying per-file tags");
      } else {
        char error[APP_MAX_ERROR] = {0};
        GuiRenamePreviewRow *row =
            &state->rename_preview_rows[state->rename_selected_row];
        if (!GuiRenameMapUpsertMetadataTags(
                map_path, row->source_path, state->rename_selected_meta_tags_csv,
                error, sizeof(error))) {
          GuiUiSetBannerError(state, error[0] != '\0' ? error
                                                       : "Failed to apply metadata tags");
        } else {
          if (state->rename_selected_meta_tags_csv[0] != '\0') {
            strncpy(row->tags_meta, state->rename_selected_meta_tags_csv,
                    sizeof(row->tags_meta) - 1);
            row->tags_meta[sizeof(row->tags_meta) - 1] = '\0';
          } else {
            strncpy(row->tags_meta, "untagged", sizeof(row->tags_meta) - 1);
            row->tags_meta[sizeof(row->tags_meta) - 1] = '\0';
          }
          GuiUiSetBannerInfo(
              state,
              "Metadata tags updated. Run Rename Preview to refresh candidates.");
        }
      }
    }
  }
  GuiHelpRegister(ToRayRect(layout.selected_meta_tags_apply_button),
                  "Persist selected-row metadata tags into tags map");

  GuiLabel(ToRayRect(layout.history_export_label), "Export");
  GuiTextBox(ToRayRect(layout.history_export_input), state->rename_history_export_path,
             (int)sizeof(state->rename_history_export_path), true);
  GuiHelpRegister(ToRayRect(layout.history_export_input),
                  "Output JSON path for filtered history export");
  if (ActionButton(state, ToRayRect(layout.history_export_button), "Export JSON",
                   GUI_ACTION_RENAME_HISTORY_EXPORT,
                   "Export filtered rename history audit JSON")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_HISTORY_EXPORT);
  }

  GuiLabel(ToRayRect(layout.history_prune_label), "Prune");
  GuiTextBox(ToRayRect(layout.history_prune_input),
             state->rename_history_prune_keep_input,
             (int)sizeof(state->rename_history_prune_keep_input), true);
  GuiHelpRegister(ToRayRect(layout.history_prune_input),
                  "History keep count (non-negative integer)");
  if (ActionButton(state, ToRayRect(layout.history_prune_button), "Prune History",
                   GUI_ACTION_RENAME_HISTORY_PRUNE,
                   "Delete oldest history entries and keep latest N")) {
    GuiUiStartTask(state, GUI_TASK_RENAME_HISTORY_PRUNE);
  }

  int visible_rows =
      GuiRenameDrawPreviewTable(state, ToRayRect(layout.preview_table));
  GuiHelpRegister(ToRayRect(layout.preview_table),
                  "Preview rows sorted by source path. Click row to edit tags");
  (void)visible_rows;
}
