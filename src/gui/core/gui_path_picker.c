#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "gui/core/gui_path_picker.h"

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

static bool RunMacPickerScript(const char *command, char *out_path,
                               size_t out_size, char *out_error,
                               size_t out_error_size) {
  if (!command || !out_path || out_size == 0) {
    SetError(out_error, out_error_size, "invalid picker command");
    return false;
  }

  out_path[0] = '\0';
  FILE *pipe = popen(command, "r");
  if (!pipe) {
    SetError(out_error, out_error_size, "failed to invoke osascript");
    return false;
  }

  bool has_output = fgets(out_path, (int)out_size, pipe) != NULL;
  int status = pclose(pipe);
  TrimTrailingWhitespace(out_path);

  if (status != 0 || !has_output || out_path[0] == '\0') {
    SetError(out_error, out_error_size,
             "path picker cancelled or failed (status=%d)", status);
    out_path[0] = '\0';
    return false;
  }
  return true;
}

bool GuiPickDirectoryPath(const char *prompt, char *out_path, size_t out_size,
                          char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
#if defined(__APPLE__)
  char command[1024] = {0};
  if (prompt && prompt[0] != '\0') {
    snprintf(command, sizeof(command),
             "osascript -e 'POSIX path of (choose folder with prompt \"%s\")'",
             prompt);
  } else {
    snprintf(command, sizeof(command),
             "osascript -e 'POSIX path of (choose folder)'");
  }
  return RunMacPickerScript(command, out_path, out_size, out_error,
                            out_error_size);
#else
  (void)prompt;
  (void)out_path;
  (void)out_size;
  SetError(out_error, out_error_size,
           "path picker is unavailable on this platform; paste the path manually");
  return false;
#endif
}

bool GuiPickFilePath(const char *prompt, char *out_path, size_t out_size,
                     char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
#if defined(__APPLE__)
  char command[1024] = {0};
  if (prompt && prompt[0] != '\0') {
    snprintf(command, sizeof(command),
             "osascript -e 'POSIX path of (choose file with prompt \"%s\")'",
             prompt);
  } else {
    snprintf(command, sizeof(command),
             "osascript -e 'POSIX path of (choose file)'");
  }
  return RunMacPickerScript(command, out_path, out_size, out_error,
                            out_error_size);
#else
  (void)prompt;
  (void)out_path;
  (void)out_size;
  SetError(out_error, out_error_size,
           "file picker is unavailable on this platform; paste the path manually");
  return false;
#endif
}
