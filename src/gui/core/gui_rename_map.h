#ifndef GUI_RENAME_MAP_H
#define GUI_RENAME_MAP_H

#include <stdbool.h>
#include <stddef.h>

bool GuiRenameMapUpsertManualTags(const char *map_path, const char *source_path,
                                  const char *manual_tags_csv,
                                  char *out_error, size_t out_error_size);

#endif // GUI_RENAME_MAP_H
