#ifndef GUI_PATH_PICKER_H
#define GUI_PATH_PICKER_H

#include <stdbool.h>
#include <stddef.h>

bool GuiPickDirectoryPath(const char *prompt, char *out_path, size_t out_size,
                          char *out_error, size_t out_error_size);

bool GuiPickFilePath(const char *prompt, char *out_path, size_t out_size,
                     char *out_error, size_t out_error_size);

#endif // GUI_PATH_PICKER_H
