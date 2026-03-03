#include <stdio.h>

#include "cli/cli_app_error.h"

void CliPrintAppError(AppContext *app, const char *prefix) {
  const char *details = AppGetLastError(app);
  if (details && details[0] != '\0') {
    printf("%s: %s\n", prefix, details);
  } else {
    printf("%s.\n", prefix);
  }
}
