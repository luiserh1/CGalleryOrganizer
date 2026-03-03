#include <stdbool.h>
#include <string.h>

#include "gui/core/gui_path_picker.h"
#include "gui/core/gui_path_picker_internal.h"
#include "test_framework.h"

typedef struct {
  bool launch_ok;
  int exit_code;
  const char *output;
} MockPickerStep;

typedef struct {
  MockPickerStep steps[8];
  int step_count;
  int call_count;
  char last_command[2048];
} MockPickerRunnerState;

static bool MockPickerRunner(const char *command, char *out_path, size_t out_size,
                             int *out_exit_code, bool *out_has_output,
                             void *user_data) {
  MockPickerRunnerState *state = (MockPickerRunnerState *)user_data;
  if (!state || !command || !out_path || out_size == 0 || !out_exit_code ||
      !out_has_output) {
    return false;
  }

  out_path[0] = '\0';
  if (state->call_count < state->step_count) {
    MockPickerStep step = state->steps[state->call_count];
    state->call_count++;

    strncpy(state->last_command, command, sizeof(state->last_command) - 1);
    state->last_command[sizeof(state->last_command) - 1] = '\0';

    if (!step.launch_ok) {
      return false;
    }

    *out_exit_code = step.exit_code;
    if (step.output && step.output[0] != '\0') {
      strncpy(out_path, step.output, out_size - 1);
      out_path[out_size - 1] = '\0';
      *out_has_output = true;
    } else {
      *out_has_output = false;
    }
    return true;
  }

  return false;
}

void test_gui_path_picker_directory_success_status(void) {
  MockPickerRunnerState runner = {0};
  runner.steps[0].launch_ok = true;
  runner.steps[0].exit_code = 0;
  runner.steps[0].output = "/tmp/gallery";
  runner.step_count = 1;

  GuiPathPickerSetCommandRunnerForTest(MockPickerRunner, &runner);
  char picked[512] = {0};
  char error[256] = {0};
  GuiPathPickerStatus status = GuiPickDirectoryPathEx(
      "Select Gallery Directory", picked, sizeof(picked), error, sizeof(error));

  ASSERT_EQ(GUI_PATH_PICKER_STATUS_OK, status);
  ASSERT_STR_EQ("/tmp/gallery", picked);
  ASSERT_TRUE(runner.call_count == 1);
#if defined(__APPLE__)
  ASSERT_TRUE(strstr(runner.last_command, "osascript") != NULL);
#elif defined(__linux__)
  ASSERT_TRUE(strstr(runner.last_command, "zenity") != NULL);
#elif defined(_WIN32)
  ASSERT_TRUE(strstr(runner.last_command, "powershell") != NULL);
#endif

  GuiPathPickerResetCommandRunnerForTest();
}

void test_gui_path_picker_cancelled_status(void) {
  MockPickerRunnerState runner = {0};
  runner.steps[0].launch_ok = true;
  runner.steps[0].exit_code = 1;
  runner.steps[0].output = "";
  runner.step_count = 1;

  GuiPathPickerSetCommandRunnerForTest(MockPickerRunner, &runner);
  char picked[512] = {0};
  char error[256] = {0};
  GuiPathPickerStatus status = GuiPickFilePathEx("Pick file", picked,
                                                 sizeof(picked), error,
                                                 sizeof(error));

  ASSERT_EQ(GUI_PATH_PICKER_STATUS_CANCELLED, status);
  ASSERT_TRUE(picked[0] == '\0');
  ASSERT_STR_EQ("picker cancelled", error);

  GuiPathPickerResetCommandRunnerForTest();
}

void test_gui_path_picker_unavailable_status(void) {
  MockPickerRunnerState runner = {0};
  runner.steps[0].launch_ok = true;
  runner.steps[0].exit_code = 127;
  runner.steps[0].output = "";
#if defined(__linux__)
  runner.steps[1].launch_ok = true;
  runner.steps[1].exit_code = 127;
  runner.steps[1].output = "";
  runner.steps[2].launch_ok = true;
  runner.steps[2].exit_code = 127;
  runner.steps[2].output = "";
  runner.step_count = 3;
#elif defined(_WIN32)
  runner.steps[1].launch_ok = true;
  runner.steps[1].exit_code = 127;
  runner.steps[1].output = "";
  runner.step_count = 2;
#else
  runner.step_count = 1;
#endif

  GuiPathPickerSetCommandRunnerForTest(MockPickerRunner, &runner);
  char picked[512] = {0};
  char error[256] = {0};
  GuiPathPickerStatus status = GuiPickDirectoryPathEx(
      "Pick directory", picked, sizeof(picked), error, sizeof(error));

  ASSERT_EQ(GUI_PATH_PICKER_STATUS_UNAVAILABLE, status);
  ASSERT_TRUE(strstr(error, "manually") != NULL);

  GuiPathPickerResetCommandRunnerForTest();
}

void test_gui_path_picker_bool_wrapper_matches_status(void) {
  MockPickerRunnerState runner = {0};
  runner.steps[0].launch_ok = true;
  runner.steps[0].exit_code = 0;
  runner.steps[0].output = "";
  runner.step_count = 1;

  GuiPathPickerSetCommandRunnerForTest(MockPickerRunner, &runner);
  char picked[512] = {0};
  char error[256] = {0};
  bool ok = GuiPickFilePath("Pick file", picked, sizeof(picked), error,
                            sizeof(error));

  ASSERT_FALSE(ok);
  ASSERT_TRUE(strstr(error, "picker cancelled") != NULL);

  GuiPathPickerResetCommandRunnerForTest();
}

void test_gui_path_picker_fallback_to_next_backend_when_supported(void) {
#if defined(__linux__) || defined(_WIN32)
  MockPickerRunnerState runner = {0};
  runner.steps[0].launch_ok = true;
  runner.steps[0].exit_code = 127;
  runner.steps[0].output = "";
  runner.steps[1].launch_ok = true;
  runner.steps[1].exit_code = 0;
  runner.steps[1].output = "/tmp/tags_map.json";
  runner.step_count = 2;

  GuiPathPickerSetCommandRunnerForTest(MockPickerRunner, &runner);
  char picked[512] = {0};
  char error[256] = {0};
  GuiPathPickerStatus status =
      GuiPickFilePathEx("Pick file", picked, sizeof(picked), error,
                        sizeof(error));

  ASSERT_EQ(GUI_PATH_PICKER_STATUS_OK, status);
  ASSERT_STR_EQ("/tmp/tags_map.json", picked);
  ASSERT_TRUE(runner.call_count == 2);

  GuiPathPickerResetCommandRunnerForTest();
#endif
}

void register_gui_path_picker_tests(void) {
  register_test("test_gui_path_picker_directory_success_status",
                test_gui_path_picker_directory_success_status, "unit");
  register_test("test_gui_path_picker_cancelled_status",
                test_gui_path_picker_cancelled_status, "unit");
  register_test("test_gui_path_picker_unavailable_status",
                test_gui_path_picker_unavailable_status, "unit");
  register_test("test_gui_path_picker_bool_wrapper_matches_status",
                test_gui_path_picker_bool_wrapper_matches_status, "unit");
  register_test("test_gui_path_picker_fallback_to_next_backend_when_supported",
                test_gui_path_picker_fallback_to_next_backend_when_supported,
                "unit");
}
