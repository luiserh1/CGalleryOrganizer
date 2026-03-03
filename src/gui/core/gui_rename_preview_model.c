#include "gui/core/gui_rename_preview_model.h"

static bool RowMatchesFilter(const GuiRenamePreviewRow *row, bool collisions_only,
                             bool warnings_only) {
  if (!row) {
    return false;
  }

  if (collisions_only && !row->collision) {
    return false;
  }
  if (warnings_only && !row->warning) {
    return false;
  }
  return true;
}

int GuiRenameBuildVisibleRowIndexList(const GuiRenamePreviewRow *rows,
                                      int row_count, bool collisions_only,
                                      bool warnings_only, int *out_indices,
                                      int max_indices) {
  if (!rows || !out_indices || row_count <= 0 || max_indices <= 0) {
    return 0;
  }

  int count = 0;
  int limit = row_count;
  if (limit > GUI_RENAME_PREVIEW_ROWS_MAX) {
    limit = GUI_RENAME_PREVIEW_ROWS_MAX;
  }

  for (int i = 0; i < limit && count < max_indices; i++) {
    if (RowMatchesFilter(&rows[i], collisions_only, warnings_only)) {
      out_indices[count++] = i;
    }
  }

  return count;
}

int GuiRenameResolveSelectedRow(const int *visible_indices, int visible_count,
                                int selected_row) {
  if (!visible_indices || visible_count <= 0) {
    return -1;
  }

  for (int i = 0; i < visible_count; i++) {
    if (visible_indices[i] == selected_row) {
      return selected_row;
    }
  }

  return visible_indices[0];
}
