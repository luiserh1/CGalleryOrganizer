#include <stdio.h>
#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_action_rules.h"
#include "gui/core/gui_help.h"
#include "gui/core/gui_path_picker.h"
#include "gui/core/gui_rename_map.h"
#include "gui/frontends/functional/gui_fixed_layout.h"
#include "gui/gui_common.h"

#define RENAME_TABLE_HEADER_HEIGHT 24.0f
#define RENAME_TABLE_ROW_HEIGHT 24.0f

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

static bool RowMatchesFilter(const GuiUiState *state, const GuiRenamePreviewRow *row) {
  if (!state || !row) {
    return false;
  }

  if (state->rename_filter_collisions_only && !row->collision) {
    return false;
  }
  if (state->rename_filter_warnings_only && !row->warning) {
    return false;
  }
  return true;
}

static int BuildVisibleRowIndexList(const GuiUiState *state, int *out_indices,
                                    int max_indices) {
  if (!state || !out_indices || max_indices <= 0) {
    return 0;
  }

  int count = 0;
  int total = state->rename_preview_row_count;
  if (total < 0) {
    total = 0;
  }
  if (total > GUI_RENAME_PREVIEW_ROWS_MAX) {
    total = GUI_RENAME_PREVIEW_ROWS_MAX;
  }

  for (int i = 0; i < total && count < max_indices; i++) {
    if (RowMatchesFilter(state, &state->rename_preview_rows[i])) {
      out_indices[count++] = i;
    }
  }
  return count;
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

static void SyncSelectedTagsInput(GuiUiState *state) {
  if (!state) {
    return;
  }
  if (state->rename_selected_row < 0 ||
      state->rename_selected_row >= state->rename_preview_row_count) {
    state->rename_selected_tags_csv[0] = '\0';
    return;
  }

  strncpy(state->rename_selected_tags_csv,
          state->rename_preview_rows[state->rename_selected_row].tags_manual,
          sizeof(state->rename_selected_tags_csv) - 1);
  state->rename_selected_tags_csv[sizeof(state->rename_selected_tags_csv) - 1] =
      '\0';
}

static void DrawTableText(Rectangle bounds, const char *text, Color color) {
  if (bounds.width <= 4.0f || bounds.height <= 2.0f) {
    return;
  }

  int scissor_x = (int)bounds.x + 1;
  int scissor_y = (int)bounds.y + 1;
  int scissor_w = (int)bounds.width - 2;
  int scissor_h = (int)bounds.height - 2;
  if (scissor_w <= 0 || scissor_h <= 0) {
    return;
  }

  float font_size = GuiGetFontSize() - 3.0f;
  if (font_size < 12.0f) {
    font_size = 12.0f;
  }

  BeginScissorMode(scissor_x, scissor_y, scissor_w, scissor_h);
  DrawTextEx(GuiGetFont(), text ? text : "",
             (Vector2){bounds.x + 4.0f, bounds.y + 3.0f}, font_size, 1.0f, color);
  EndScissorMode();
}

static int DrawRenamePreviewTable(GuiUiState *state, Rectangle table_bounds) {
  if (!state) {
    return 0;
  }

  DrawRectangleRec(table_bounds, (Color){251, 251, 251, 255});
  DrawRectangleLinesEx(table_bounds, 1.0f, (Color){186, 186, 186, 255});

  int visible_indices[GUI_RENAME_PREVIEW_ROWS_MAX] = {0};
  int visible_count = BuildVisibleRowIndexList(
      state, visible_indices, (int)(sizeof(visible_indices) / sizeof(visible_indices[0])));

  Rectangle header = {table_bounds.x + 1.0f, table_bounds.y + 1.0f,
                      table_bounds.width - 2.0f, RENAME_TABLE_HEADER_HEIGHT};
  DrawRectangleRec(header, (Color){235, 239, 243, 255});
  DrawRectangleLinesEx(header, 1.0f, (Color){170, 170, 170, 255});

  float status_w = header.width * 0.12f;
  float source_w = header.width * 0.30f;
  float candidate_w = header.width * 0.37f;
  float tags_w = header.width - status_w - source_w - candidate_w;

  Rectangle status_col = {header.x, header.y, status_w, header.height};
  Rectangle source_col = {status_col.x + status_col.width, header.y, source_w,
                          header.height};
  Rectangle candidate_col = {source_col.x + source_col.width, header.y,
                             candidate_w, header.height};
  Rectangle tags_col = {candidate_col.x + candidate_col.width, header.y, tags_w,
                        header.height};

  DrawTableText(status_col, "Status", (Color){70, 70, 70, 255});
  DrawTableText(source_col, "Source", (Color){70, 70, 70, 255});
  DrawTableText(candidate_col, "Candidate", (Color){70, 70, 70, 255});
  DrawTableText(tags_col, "Manual Tags", (Color){70, 70, 70, 255});

  DrawLineEx((Vector2){source_col.x, header.y},
             (Vector2){source_col.x, header.y + header.height}, 1.0f,
             (Color){178, 178, 178, 255});
  DrawLineEx((Vector2){candidate_col.x, header.y},
             (Vector2){candidate_col.x, header.y + header.height}, 1.0f,
             (Color){178, 178, 178, 255});
  DrawLineEx((Vector2){tags_col.x, header.y},
             (Vector2){tags_col.x, header.y + header.height}, 1.0f,
             (Color){178, 178, 178, 255});

  Rectangle body = {table_bounds.x + 1.0f, header.y + header.height,
                    table_bounds.width - 2.0f,
                    table_bounds.height - header.height - 2.0f};
  if (body.height <= 4.0f) {
    return visible_count;
  }

  int rows_per_page = (int)(body.height / RENAME_TABLE_ROW_HEIGHT);
  if (rows_per_page < 1) {
    rows_per_page = 1;
  }
  int max_scroll = visible_count - rows_per_page;
  if (max_scroll < 0) {
    max_scroll = 0;
  }

  if (state->rename_table_scroll < 0) {
    state->rename_table_scroll = 0;
  }
  if (state->rename_table_scroll > max_scroll) {
    state->rename_table_scroll = max_scroll;
  }

  if (visible_count > 0) {
    bool selected_visible = false;
    for (int i = 0; i < visible_count; i++) {
      if (visible_indices[i] == state->rename_selected_row) {
        selected_visible = true;
        break;
      }
    }
    if (!selected_visible) {
      state->rename_selected_row = visible_indices[0];
      SyncSelectedTagsInput(state);
    }
  } else {
    state->rename_selected_row = -1;
    state->rename_selected_tags_csv[0] = '\0';
  }

  Vector2 mouse = GetMousePosition();
  if (CheckCollisionPointRec(mouse, body)) {
    float wheel = GetMouseWheelMove();
    if (wheel > 0.0f) {
      state->rename_table_scroll--;
    } else if (wheel < 0.0f) {
      state->rename_table_scroll++;
    }
    if (state->rename_table_scroll < 0) {
      state->rename_table_scroll = 0;
    }
    if (state->rename_table_scroll > max_scroll) {
      state->rename_table_scroll = max_scroll;
    }
  }

  int start = state->rename_table_scroll;
  int end = start + rows_per_page;
  if (end > visible_count) {
    end = visible_count;
  }

  for (int i = start; i < end; i++) {
    int row_index = visible_indices[i];
    const GuiRenamePreviewRow *row = &state->rename_preview_rows[row_index];

    Rectangle row_rect = {body.x,
                          body.y + (float)(i - start) * RENAME_TABLE_ROW_HEIGHT,
                          body.width, RENAME_TABLE_ROW_HEIGHT};
    bool selected = row_index == state->rename_selected_row;

    Color row_fill = (Color){255, 255, 255, 255};
    if (selected) {
      row_fill = (Color){210, 224, 238, 255};
    } else if (row->collision) {
      row_fill = (Color){255, 236, 219, 255};
    } else if (row->warning) {
      row_fill = (Color){255, 249, 215, 255};
    } else if (((i - start) % 2) == 1) {
      row_fill = (Color){248, 248, 248, 255};
    }

    DrawRectangleRec(row_rect, row_fill);
    DrawRectangleLinesEx(row_rect, 1.0f, (Color){214, 214, 214, 255});

    Rectangle status_cell = {row_rect.x, row_rect.y, status_w, row_rect.height};
    Rectangle source_cell = {status_cell.x + status_cell.width, row_rect.y,
                             source_w, row_rect.height};
    Rectangle candidate_cell = {source_cell.x + source_cell.width, row_rect.y,
                                candidate_w, row_rect.height};
    Rectangle tags_cell = {candidate_cell.x + candidate_cell.width, row_rect.y,
                           tags_w, row_rect.height};

    const char *status_text =
        row->collision ? "collision" : (row->warning ? "warning" : "ok");
    DrawTableText(status_cell, status_text, (Color){60, 60, 60, 255});
    DrawTableText(source_cell, row->source_name, (Color){46, 46, 46, 255});
    DrawTableText(candidate_cell, row->candidate_name, (Color){46, 46, 46, 255});
    DrawTableText(tags_cell, row->tags_manual, (Color){46, 46, 46, 255});

    DrawLineEx((Vector2){source_cell.x, row_rect.y},
               (Vector2){source_cell.x, row_rect.y + row_rect.height}, 1.0f,
               (Color){214, 214, 214, 255});
    DrawLineEx((Vector2){candidate_cell.x, row_rect.y},
               (Vector2){candidate_cell.x, row_rect.y + row_rect.height}, 1.0f,
               (Color){214, 214, 214, 255});
    DrawLineEx((Vector2){tags_cell.x, row_rect.y},
               (Vector2){tags_cell.x, row_rect.y + row_rect.height}, 1.0f,
               (Color){214, 214, 214, 255});

    if (CheckCollisionPointRec(mouse, row_rect) &&
        IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      state->rename_selected_row = row_index;
      SyncSelectedTagsInput(state);
    }
  }

  if (visible_count == 0) {
    DrawTableText(body, "No preview rows for current filter.",
                  (Color){120, 120, 120, 255});
  }

  return visible_count;
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
  if (GuiButtonStyled(ToRayRect(layout.tags_map_pick_button), "Pick...", true,
                      false)) {
    char picked[GUI_STATE_MAX_PATH] = {0};
    char error[APP_MAX_ERROR] = {0};
    if (GuiPickFilePath("Select Rename Tags Map JSON", picked, sizeof(picked), error,
                        sizeof(error))) {
      strncpy(state->rename_tags_map_path, picked,
              sizeof(state->rename_tags_map_path) - 1);
      state->rename_tags_map_path[sizeof(state->rename_tags_map_path) - 1] = '\0';
    } else {
      GuiUiSetBannerError(state, error[0] != '\0' ? error : "Tags map picker failed");
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

  GuiLabel(ToRayRect(layout.selected_tags_label), "Selected tags");
  GuiTextBox(ToRayRect(layout.selected_tags_input), state->rename_selected_tags_csv,
             (int)sizeof(state->rename_selected_tags_csv), true);
  GuiHelpRegister(ToRayRect(layout.selected_tags_input),
                  "CSV manual tags for selected preview row (ex: 001,frag-a)");
  if (GuiButtonStyled(ToRayRect(layout.selected_tags_apply_button),
                      "Apply to Selected File", true, false)) {
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

  int visible_rows = DrawRenamePreviewTable(state, ToRayRect(layout.preview_table));
  GuiHelpRegister(ToRayRect(layout.preview_table),
                  "Preview rows sorted by source path. Click row to edit manual tags");

  char hint[256] = {0};
  snprintf(hint, sizeof(hint),
           "Last preview: id=%s files=%d visible=%d warnings=%d history=%d",
           state->worker_snapshot.rename_preview_id[0] != '\0'
               ? state->worker_snapshot.rename_preview_id
               : "n/a",
           state->worker_snapshot.rename_preview_file_count,
           visible_rows, state->rename_preview_warning_count,
           state->worker_snapshot.rename_history_count);
  GuiHelpDrawHintLabel(ToRayRect(layout.info_label), hint);
}
