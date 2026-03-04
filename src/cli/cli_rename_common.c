#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "cli/cli_rename_common.h"

void CliRenameCommonSetError(char *out_error, size_t out_error_size,
                             const char *fmt, ...) {
  if (!out_error || out_error_size == 0 || !fmt) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(out_error, out_error_size, fmt, args);
  va_end(args);
}

bool CliRenameCommonIsExistingDirectory(const char *path) {
  if (!path || path[0] == '\0') {
    return false;
  }

  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

bool CliRenameCommonSaveTextFile(const char *path, const char *text) {
  if (!path || path[0] == '\0' || !text) {
    return false;
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }

  bool ok = fputs(text, f) >= 0;
  fclose(f);
  return ok;
}

bool CliRenameCommonLoadTextFile(const char *path, char **out_text) {
  if (!path || path[0] == '\0' || !out_text) {
    return false;
  }

  *out_text = NULL;
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }

  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return false;
  }

  rewind(f);
  char *text = calloc((size_t)size + 1, 1);
  if (!text) {
    fclose(f);
    return false;
  }

  size_t read_bytes = fread(text, 1, (size_t)size, f);
  fclose(f);
  text[read_bytes] = '\0';
  *out_text = text;
  return true;
}

bool CliRenameCommonEndsWith(const char *text, const char *suffix) {
  if (!text || !suffix) {
    return false;
  }

  size_t text_len = strlen(text);
  size_t suffix_len = strlen(suffix);
  if (suffix_len > text_len) {
    return false;
  }

  return strcmp(text + (text_len - suffix_len), suffix) == 0;
}
