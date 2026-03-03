#ifndef GUI_PATH_PICKER_H
#define GUI_PATH_PICKER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  GUI_PATH_PICKER_STATUS_OK = 0,
  GUI_PATH_PICKER_STATUS_CANCELLED = 1,
  GUI_PATH_PICKER_STATUS_UNAVAILABLE = 2,
  GUI_PATH_PICKER_STATUS_ERROR = 3
} GuiPathPickerStatus;

GuiPathPickerStatus GuiPickDirectoryPathEx(const char *prompt, char *out_path,
                                           size_t out_size, char *out_error,
                                           size_t out_error_size);

GuiPathPickerStatus GuiPickFilePathEx(const char *prompt, char *out_path,
                                      size_t out_size, char *out_error,
                                      size_t out_error_size);

bool GuiPickDirectoryPath(const char *prompt, char *out_path, size_t out_size,
                          char *out_error, size_t out_error_size);

bool GuiPickFilePath(const char *prompt, char *out_path, size_t out_size,
                     char *out_error, size_t out_error_size);

#endif // GUI_PATH_PICKER_H
