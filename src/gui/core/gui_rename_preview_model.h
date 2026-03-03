#ifndef GUI_RENAME_PREVIEW_MODEL_H
#define GUI_RENAME_PREVIEW_MODEL_H

#include <stdbool.h>

#include "gui/gui_worker.h"

int GuiRenameBuildVisibleRowIndexList(const GuiRenamePreviewRow *rows,
                                      int row_count, bool collisions_only,
                                      bool warnings_only, int *out_indices,
                                      int max_indices);

int GuiRenameResolveSelectedRow(const int *visible_indices, int visible_count,
                                int selected_row);

#endif // GUI_RENAME_PREVIEW_MODEL_H
