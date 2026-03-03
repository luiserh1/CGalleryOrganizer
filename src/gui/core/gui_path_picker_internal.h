#ifndef GUI_PATH_PICKER_INTERNAL_H
#define GUI_PATH_PICKER_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

typedef bool (*GuiPathPickerCommandRunner)(const char *command, char *out_path,
                                           size_t out_size, int *out_exit_code,
                                           bool *out_has_output,
                                           void *user_data);

void GuiPathPickerSetCommandRunnerForTest(GuiPathPickerCommandRunner runner,
                                          void *user_data);
void GuiPathPickerResetCommandRunnerForTest(void);

#endif // GUI_PATH_PICKER_INTERNAL_H
