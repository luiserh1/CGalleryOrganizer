#ifndef GUI_PATH_PICKER_INTERNAL_H
#define GUI_PATH_PICKER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "gui/core/gui_path_picker.h"

typedef bool (*GuiPathPickerCommandRunner)(const char *command, char *out_path,
                                           size_t out_size, int *out_exit_code,
                                           bool *out_has_output,
                                           void *user_data);

typedef struct {
  const char *backend;
  char command[2048];
} GuiPickerCommand;

GuiPathPickerStatus
GuiPathPickerRunCandidates(const GuiPickerCommand *candidates,
                           size_t candidate_count, char *out_path,
                           size_t out_size, char *out_error,
                           size_t out_error_size,
                           const char *unavailable_message);

void GuiPathPickerSetCommandRunnerForTest(GuiPathPickerCommandRunner runner,
                                          void *user_data);
void GuiPathPickerResetCommandRunnerForTest(void);

#endif // GUI_PATH_PICKER_INTERNAL_H
