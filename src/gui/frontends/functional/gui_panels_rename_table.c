#include <string.h>

#include "raygui.h"
#include "raylib.h"

#include "gui/core/gui_rename_preview_model.h"
#include "gui/frontends/functional/gui_panels_rename_internal.h"

#define RENAME_TABLE_HEADER_HEIGHT 24.0f
#define RENAME_TABLE_ROW_HEIGHT 24.0f

static void GuiRenameSyncSelectedTagsInput(GuiUiState *state) {
  if (!state) {
    return;
  }
  if (state->rename_selected_row < 0 ||
      state->rename_selected_row >= state->rename_preview_row_count) {
    state->rename_selected_tags_csv[0] = '\0';
    state->rename_selected_meta_tags_csv[0] = '\0';
    return;
  }

  strncpy(state->rename_selected_tags_csv,
          state->rename_preview_rows[state->rename_selected_row].tags_manual,
          sizeof(state->rename_selected_tags_csv) - 1);
  state->rename_selected_tags_csv[sizeof(state->rename_selected_tags_csv) - 1] =
      '\0';
  strncpy(state->rename_selected_meta_tags_csv,
          state->rename_preview_rows[state->rename_selected_row].tags_meta,
          sizeof(state->rename_selected_meta_tags_csv) - 1);
  state->rename_selected_meta_tags_csv
      [sizeof(state->rename_selected_meta_tags_csv) - 1] = '\0';
}

static void GuiRenameDrawTableText(Rectangle bounds, const char *text, Color color) {
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

int GuiRenameDrawPreviewTable(GuiUiState *state, Rectangle table_bounds) {
  if (!state) {
    return 0;
  }

  DrawRectangleRec(table_bounds, (Color){251, 251, 251, 255});
  DrawRectangleLinesEx(table_bounds, 1.0f, (Color){186, 186, 186, 255});

  int visible_indices[GUI_RENAME_PREVIEW_ROWS_MAX] = {0};
  int visible_count = GuiRenameBuildVisibleRowIndexList(
      state->rename_preview_rows, state->rename_preview_row_count,
      state->rename_filter_collisions_only, state->rename_filter_warnings_only,
      visible_indices,
      (int)(sizeof(visible_indices) / sizeof(visible_indices[0])));

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

  GuiRenameDrawTableText(status_col, "Status", (Color){70, 70, 70, 255});
  GuiRenameDrawTableText(source_col, "Source", (Color){70, 70, 70, 255});
  GuiRenameDrawTableText(candidate_col, "Candidate", (Color){70, 70, 70, 255});
  GuiRenameDrawTableText(tags_col, "Manual Tags", (Color){70, 70, 70, 255});

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
    int resolved_selected = GuiRenameResolveSelectedRow(
        visible_indices, visible_count, state->rename_selected_row);
    if (resolved_selected != state->rename_selected_row) {
      state->rename_selected_row = resolved_selected;
      GuiRenameSyncSelectedTagsInput(state);
    }
  } else {
    state->rename_selected_row = -1;
    state->rename_selected_tags_csv[0] = '\0';
    state->rename_selected_meta_tags_csv[0] = '\0';
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
    GuiRenameDrawTableText(status_cell, status_text, (Color){60, 60, 60, 255});
    GuiRenameDrawTableText(source_cell, row->source_name, (Color){46, 46, 46, 255});
    GuiRenameDrawTableText(candidate_cell, row->candidate_name,
                           (Color){46, 46, 46, 255});
    GuiRenameDrawTableText(tags_cell, row->tags_manual, (Color){46, 46, 46, 255});

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
      GuiRenameSyncSelectedTagsInput(state);
    }
  }

  if (visible_count == 0) {
    GuiRenameDrawTableText(body, "No preview rows for current filter.",
                           (Color){120, 120, 120, 255});
  }

  return visible_count;
}
