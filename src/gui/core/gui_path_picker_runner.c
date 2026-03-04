#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

#include "gui/core/gui_path_picker_internal.h"

#if defined(_WIN32)
#define CGO_POPEN _popen
#define CGO_PCLOSE _pclose
#else
#define CGO_POPEN popen
#define CGO_PCLOSE pclose
#endif

static bool RunCommandWithPopen(const char *command, char *out_path,
                                size_t out_size, int *out_exit_code,
                                bool *out_has_output, void *user_data);
static GuiPathPickerCommandRunner g_picker_runner = RunCommandWithPopen;
static void *g_picker_runner_user_data = NULL;

static void SetError(char *out_error, size_t out_error_size, const char *fmt,
                     ...) {
  if (!out_error || out_error_size == 0 || !fmt) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(out_error, out_error_size, fmt, args);
  va_end(args);
}

static void TrimTrailingWhitespace(char *text) {
  if (!text) {
    return;
  }

  size_t len = strlen(text);
  while (len > 0 &&
         (text[len - 1] == '\n' || text[len - 1] == '\r' || text[len - 1] == ' ' ||
          text[len - 1] == '\t')) {
    text[len - 1] = '\0';
    len--;
  }
}

static int NormalizeExitCode(int raw_status) {
  if (raw_status < 0) {
    return -1;
  }
#if defined(_WIN32)
  return raw_status;
#else
  if (WIFEXITED(raw_status)) {
    return WEXITSTATUS(raw_status);
  }
  return raw_status;
#endif
}

static bool RunCommandWithPopen(const char *command, char *out_path,
                                size_t out_size, int *out_exit_code,
                                bool *out_has_output, void *user_data) {
  (void)user_data;
  if (!command || !out_path || out_size == 0 || !out_exit_code ||
      !out_has_output) {
    return false;
  }

  out_path[0] = '\0';
  *out_exit_code = -1;
  *out_has_output = false;

  FILE *pipe = CGO_POPEN(command, "r");
  if (!pipe) {
    return false;
  }

  bool has_output = fgets(out_path, (int)out_size, pipe) != NULL;
  int raw_status = CGO_PCLOSE(pipe);
  TrimTrailingWhitespace(out_path);
  *out_has_output = has_output && out_path[0] != '\0';
  *out_exit_code = NormalizeExitCode(raw_status);
  return true;
}

static bool ExitCodeMeansUnavailable(int exit_code) {
  return exit_code == 126 || exit_code == 127 || exit_code == 9009;
}

static bool ExitCodeMeansCancelled(int exit_code) {
  return exit_code == 1;
}

static GuiPathPickerStatus ClassifyCommandResult(int exit_code,
                                                 bool has_output) {
  if (exit_code == 0 && has_output) {
    return GUI_PATH_PICKER_STATUS_OK;
  }
  if (exit_code == 0 && !has_output) {
    return GUI_PATH_PICKER_STATUS_CANCELLED;
  }
  if (ExitCodeMeansUnavailable(exit_code)) {
    return GUI_PATH_PICKER_STATUS_UNAVAILABLE;
  }
  if (!has_output && ExitCodeMeansCancelled(exit_code)) {
    return GUI_PATH_PICKER_STATUS_CANCELLED;
  }
  return GUI_PATH_PICKER_STATUS_ERROR;
}

GuiPathPickerStatus
GuiPathPickerRunCandidates(const GuiPickerCommand *candidates,
                           size_t candidate_count, char *out_path,
                           size_t out_size, char *out_error,
                           size_t out_error_size,
                           const char *unavailable_message) {
  if (!out_path || out_size == 0) {
    SetError(out_error, out_error_size, "invalid picker output buffer");
    return GUI_PATH_PICKER_STATUS_ERROR;
  }

  out_path[0] = '\0';
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }

  bool saw_runtime_error = false;
  char last_error[256] = {0};

  for (size_t i = 0; i < candidate_count; i++) {
    if (candidates[i].command[0] == '\0') {
      continue;
    }

    int exit_code = -1;
    bool has_output = false;
    bool launched =
        g_picker_runner(candidates[i].command, out_path, out_size, &exit_code,
                        &has_output, g_picker_runner_user_data);
    if (!launched) {
      saw_runtime_error = true;
      SetError(last_error, sizeof(last_error),
               "failed to launch picker backend '%s'", candidates[i].backend);
      continue;
    }

    GuiPathPickerStatus status = ClassifyCommandResult(exit_code, has_output);
    if (status == GUI_PATH_PICKER_STATUS_OK) {
      return status;
    }
    if (status == GUI_PATH_PICKER_STATUS_CANCELLED) {
      SetError(out_error, out_error_size, "picker cancelled");
      out_path[0] = '\0';
      return status;
    }
    if (status == GUI_PATH_PICKER_STATUS_UNAVAILABLE) {
      continue;
    }

    saw_runtime_error = true;
    SetError(last_error, sizeof(last_error),
             "picker backend '%s' failed (exit=%d)", candidates[i].backend,
             exit_code);
  }

  out_path[0] = '\0';
  if (saw_runtime_error) {
    SetError(out_error, out_error_size, "%s", last_error);
    return GUI_PATH_PICKER_STATUS_ERROR;
  }

  SetError(out_error, out_error_size, "%s", unavailable_message);
  return GUI_PATH_PICKER_STATUS_UNAVAILABLE;
}

void GuiPathPickerSetCommandRunnerForTest(GuiPathPickerCommandRunner runner,
                                          void *user_data) {
  g_picker_runner = runner ? runner : RunCommandWithPopen;
  g_picker_runner_user_data = user_data;
}

void GuiPathPickerResetCommandRunnerForTest(void) {
  g_picker_runner = RunCommandWithPopen;
  g_picker_runner_user_data = NULL;
}
