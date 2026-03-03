#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "gui/core/gui_path_picker.h"
#include "gui/core/gui_path_picker_internal.h"

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

#if defined(_WIN32)
#define CGO_POPEN _popen
#define CGO_PCLOSE _pclose
#else
#define CGO_POPEN popen
#define CGO_PCLOSE pclose
#endif

typedef enum {
  GUI_PICKER_KIND_DIRECTORY = 0,
  GUI_PICKER_KIND_FILE = 1
} GuiPickerKind;

typedef struct {
  const char *backend;
  char command[2048];
} GuiPickerCommand;

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

#if defined(__linux__)
static bool EscapeSingleQuotedShell(const char *input, char *out,
                                    size_t out_size) {
  if (!out || out_size == 0) {
    return false;
  }
  out[0] = '\0';
  if (!input) {
    return true;
  }

  size_t used = 0;
  for (const char *cursor = input; *cursor != '\0'; cursor++) {
    if (*cursor == '\'') {
      static const char replacement[] = "'\"'\"'";
      size_t repl_len = sizeof(replacement) - 1;
      if (used + repl_len + 1 >= out_size) {
        return false;
      }
      memcpy(out + used, replacement, repl_len);
      used += repl_len;
    } else {
      if (used + 2 >= out_size) {
        return false;
      }
      out[used++] = *cursor;
    }
  }
  out[used] = '\0';
  return true;
}
#endif

static bool EscapeAppleScriptString(const char *input, char *out,
                                    size_t out_size) {
  if (!out || out_size == 0) {
    return false;
  }
  out[0] = '\0';
  if (!input) {
    return true;
  }

  size_t used = 0;
  for (const char *cursor = input; *cursor != '\0'; cursor++) {
    char ch = *cursor;
    if (ch == '\n' || ch == '\r' || ch == '\t') {
      ch = ' ';
    }
    if (ch == '"' || ch == '\\') {
      if (used + 3 >= out_size) {
        return false;
      }
      out[used++] = '\\';
      out[used++] = ch;
    } else {
      if (used + 2 >= out_size) {
        return false;
      }
      out[used++] = ch;
    }
  }
  out[used] = '\0';
  return true;
}

#if defined(_WIN32)
static bool EscapePowerShellSingleQuoted(const char *input, char *out,
                                         size_t out_size) {
  if (!out || out_size == 0) {
    return false;
  }
  out[0] = '\0';
  if (!input) {
    return true;
  }

  size_t used = 0;
  for (const char *cursor = input; *cursor != '\0'; cursor++) {
    char ch = *cursor;
    if (ch == '\n' || ch == '\r' || ch == '\t') {
      ch = ' ';
    }
    if (ch == '\'') {
      if (used + 3 >= out_size) {
        return false;
      }
      out[used++] = '\'';
      out[used++] = '\'';
    } else {
      if (used + 2 >= out_size) {
        return false;
      }
      out[used++] = ch;
    }
  }
  out[used] = '\0';
  return true;
}
#endif

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

static bool BuildCommand(GuiPickerCommand *candidate, const char *backend,
                         const char *template_text, const char *escaped_prompt,
                         char *out_error, size_t out_error_size) {
  if (!candidate || !backend || !template_text || !escaped_prompt) {
    SetError(out_error, out_error_size, "invalid picker candidate");
    return false;
  }

  candidate->backend = backend;
  int written = snprintf(candidate->command, sizeof(candidate->command),
                         template_text, escaped_prompt);
  if (written < 0 || (size_t)written >= sizeof(candidate->command)) {
    SetError(out_error, out_error_size,
             "picker command is too long for backend '%s'", backend);
    candidate->command[0] = '\0';
    return false;
  }
  return true;
}

static const char *DefaultPromptForKind(GuiPickerKind kind) {
  return kind == GUI_PICKER_KIND_DIRECTORY ? "Select Directory" : "Select File";
}

static GuiPathPickerStatus
RunPickerCandidates(const GuiPickerCommand *candidates, size_t candidate_count,
                    char *out_path, size_t out_size, char *out_error,
                    size_t out_error_size, const char *unavailable_message) {
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

static GuiPathPickerStatus GuiPickPathInternal(GuiPickerKind kind,
                                               const char *prompt,
                                               char *out_path, size_t out_size,
                                               char *out_error,
                                               size_t out_error_size) {
#if defined(__APPLE__)
  const char *effective_prompt =
      (prompt && prompt[0] != '\0') ? prompt : DefaultPromptForKind(kind);
  char escaped_prompt[512] = {0};
  GuiPickerCommand candidate = {0};
  if (!EscapeAppleScriptString(effective_prompt, escaped_prompt,
                               sizeof(escaped_prompt))) {
    SetError(out_error, out_error_size, "picker prompt is too long");
    if (out_path && out_size > 0) {
      out_path[0] = '\0';
    }
    return GUI_PATH_PICKER_STATUS_ERROR;
  }

  if (kind == GUI_PICKER_KIND_DIRECTORY) {
    if (!BuildCommand(&candidate, "osascript",
                      "osascript -e 'POSIX path of (choose folder with prompt "
                      "\"%s\")'",
                      escaped_prompt, out_error, out_error_size)) {
      if (out_path && out_size > 0) {
        out_path[0] = '\0';
      }
      return GUI_PATH_PICKER_STATUS_ERROR;
    }
  } else {
    if (!BuildCommand(&candidate, "osascript",
                      "osascript -e 'POSIX path of (choose file with prompt "
                      "\"%s\")'",
                      escaped_prompt, out_error, out_error_size)) {
      if (out_path && out_size > 0) {
        out_path[0] = '\0';
      }
      return GUI_PATH_PICKER_STATUS_ERROR;
    }
  }

  return RunPickerCandidates(&candidate, 1, out_path, out_size, out_error,
                             out_error_size,
                             "macOS picker backend unavailable; paste the path "
                             "manually");
#elif defined(_WIN32)
  const char *effective_prompt =
      (prompt && prompt[0] != '\0') ? prompt : DefaultPromptForKind(kind);
  char escaped_prompt[512] = {0};
  GuiPickerCommand candidates[2] = {0};
  if (!EscapePowerShellSingleQuoted(effective_prompt, escaped_prompt,
                                    sizeof(escaped_prompt))) {
    SetError(out_error, out_error_size, "picker prompt is too long");
    if (out_path && out_size > 0) {
      out_path[0] = '\0';
    }
    return GUI_PATH_PICKER_STATUS_ERROR;
  }

  if (kind == GUI_PICKER_KIND_DIRECTORY) {
    if (!BuildCommand(&candidates[0], "powershell",
                      "powershell -NoProfile -Command \"Add-Type -AssemblyName "
                      "System.Windows.Forms; $dialog = New-Object "
                      "System.Windows.Forms.FolderBrowserDialog; "
                      "$dialog.Description = '%s'; if ($dialog.ShowDialog() -eq "
                      "[System.Windows.Forms.DialogResult]::OK) { "
                      "[Console]::Write($dialog.SelectedPath) }\" 2>NUL",
                      escaped_prompt, out_error, out_error_size)) {
      if (out_path && out_size > 0) {
        out_path[0] = '\0';
      }
      return GUI_PATH_PICKER_STATUS_ERROR;
    }
    if (!BuildCommand(&candidates[1], "pwsh",
                      "pwsh -NoProfile -Command \"Add-Type -AssemblyName "
                      "System.Windows.Forms; $dialog = New-Object "
                      "System.Windows.Forms.FolderBrowserDialog; "
                      "$dialog.Description = '%s'; if ($dialog.ShowDialog() -eq "
                      "[System.Windows.Forms.DialogResult]::OK) { "
                      "[Console]::Write($dialog.SelectedPath) }\" 2>NUL",
                      escaped_prompt, out_error, out_error_size)) {
      if (out_path && out_size > 0) {
        out_path[0] = '\0';
      }
      return GUI_PATH_PICKER_STATUS_ERROR;
    }
  } else {
    if (!BuildCommand(&candidates[0], "powershell",
                      "powershell -NoProfile -Command \"Add-Type -AssemblyName "
                      "System.Windows.Forms; $dialog = New-Object "
                      "System.Windows.Forms.OpenFileDialog; $dialog.Title = "
                      "'%s'; if ($dialog.ShowDialog() -eq "
                      "[System.Windows.Forms.DialogResult]::OK) { "
                      "[Console]::Write($dialog.FileName) }\" 2>NUL",
                      escaped_prompt, out_error, out_error_size)) {
      if (out_path && out_size > 0) {
        out_path[0] = '\0';
      }
      return GUI_PATH_PICKER_STATUS_ERROR;
    }
    if (!BuildCommand(&candidates[1], "pwsh",
                      "pwsh -NoProfile -Command \"Add-Type -AssemblyName "
                      "System.Windows.Forms; $dialog = New-Object "
                      "System.Windows.Forms.OpenFileDialog; $dialog.Title = "
                      "'%s'; if ($dialog.ShowDialog() -eq "
                      "[System.Windows.Forms.DialogResult]::OK) { "
                      "[Console]::Write($dialog.FileName) }\" 2>NUL",
                      escaped_prompt, out_error, out_error_size)) {
      if (out_path && out_size > 0) {
        out_path[0] = '\0';
      }
      return GUI_PATH_PICKER_STATUS_ERROR;
    }
  }

  return RunPickerCandidates(
      candidates, sizeof(candidates) / sizeof(candidates[0]), out_path, out_size,
      out_error, out_error_size,
      "Windows picker backend unavailable; paste the path manually");
#elif defined(__linux__)
  const char *effective_prompt =
      (prompt && prompt[0] != '\0') ? prompt : DefaultPromptForKind(kind);
  char escaped_prompt[512] = {0};
  GuiPickerCommand candidates[3] = {0};
  if (!EscapeSingleQuotedShell(effective_prompt, escaped_prompt,
                               sizeof(escaped_prompt))) {
    SetError(out_error, out_error_size, "picker prompt is too long");
    if (out_path && out_size > 0) {
      out_path[0] = '\0';
    }
    return GUI_PATH_PICKER_STATUS_ERROR;
  }

  if (kind == GUI_PICKER_KIND_DIRECTORY) {
    if (!BuildCommand(&candidates[0], "zenity",
                      "zenity --file-selection --directory --title='%s' "
                      "2>/dev/null",
                      escaped_prompt, out_error, out_error_size) ||
        !BuildCommand(&candidates[1], "kdialog",
                      "kdialog --getexistingdirectory \"$HOME\" --title '%s' "
                      "2>/dev/null",
                      escaped_prompt, out_error, out_error_size) ||
        !BuildCommand(&candidates[2], "yad",
                      "yad --file-selection --directory --title='%s' 2>/dev/null",
                      escaped_prompt, out_error, out_error_size)) {
      if (out_path && out_size > 0) {
        out_path[0] = '\0';
      }
      return GUI_PATH_PICKER_STATUS_ERROR;
    }
  } else {
    if (!BuildCommand(&candidates[0], "zenity",
                      "zenity --file-selection --title='%s' 2>/dev/null",
                      escaped_prompt, out_error, out_error_size) ||
        !BuildCommand(&candidates[1], "kdialog",
                      "kdialog --getopenfilename \"$HOME\" --title '%s' "
                      "2>/dev/null",
                      escaped_prompt, out_error, out_error_size) ||
        !BuildCommand(&candidates[2], "yad",
                      "yad --file-selection --title='%s' 2>/dev/null",
                      escaped_prompt, out_error, out_error_size)) {
      if (out_path && out_size > 0) {
        out_path[0] = '\0';
      }
      return GUI_PATH_PICKER_STATUS_ERROR;
    }
  }

  return RunPickerCandidates(
      candidates, sizeof(candidates) / sizeof(candidates[0]), out_path, out_size,
      out_error, out_error_size,
      "No supported Linux picker backend found (tried: zenity, kdialog, yad); "
      "paste the path manually");
#else
  (void)prompt;
  (void)kind;
  if (out_path && out_size > 0) {
    out_path[0] = '\0';
  }
  SetError(out_error, out_error_size,
           "path picker is unavailable on this platform; paste the path "
           "manually");
  return GUI_PATH_PICKER_STATUS_UNAVAILABLE;
#endif
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

GuiPathPickerStatus GuiPickDirectoryPathEx(const char *prompt, char *out_path,
                                           size_t out_size, char *out_error,
                                           size_t out_error_size) {
  return GuiPickPathInternal(GUI_PICKER_KIND_DIRECTORY, prompt, out_path,
                             out_size, out_error, out_error_size);
}

GuiPathPickerStatus GuiPickFilePathEx(const char *prompt, char *out_path,
                                      size_t out_size, char *out_error,
                                      size_t out_error_size) {
  return GuiPickPathInternal(GUI_PICKER_KIND_FILE, prompt, out_path, out_size,
                             out_error, out_error_size);
}

bool GuiPickDirectoryPath(const char *prompt, char *out_path, size_t out_size,
                          char *out_error, size_t out_error_size) {
  return GuiPickDirectoryPathEx(prompt, out_path, out_size, out_error,
                                out_error_size) == GUI_PATH_PICKER_STATUS_OK;
}

bool GuiPickFilePath(const char *prompt, char *out_path, size_t out_size,
                     char *out_error, size_t out_error_size) {
  return GuiPickFilePathEx(prompt, out_path, out_size, out_error,
                           out_error_size) == GUI_PATH_PICKER_STATUS_OK;
}
