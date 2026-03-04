#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "gui/core/gui_ui_state_internal.h"
#include "gui/gui_common.h"

static void GuiUiSetBannerMessage(GuiUiState *state, const char *message,
                                  bool is_error) {
  if (!state) {
    return;
  }

  state->banner_message[0] = '\0';
  state->banner_is_error = is_error;

  if (!message) {
    return;
  }

  strncpy(state->banner_message, message, sizeof(state->banner_message) - 1);
  state->banner_message[sizeof(state->banner_message) - 1] = '\0';
}

void GuiUiSetBannerInfo(GuiUiState *state, const char *message) {
  GuiUiSetBannerMessage(state, message, false);
}

void GuiUiSetBannerError(GuiUiState *state, const char *message) {
  GuiUiSetBannerMessage(state, message, true);
}

bool GuiUiTryParseIntStrict(const char *text, int *out_value) {
  if (!text || !out_value) {
    return false;
  }

  char *endptr = NULL;
  long parsed = strtol(text, &endptr, 10);
  if (!endptr || *endptr != '\0') {
    return false;
  }
  if (parsed < INT_MIN || parsed > INT_MAX) {
    return false;
  }
  *out_value = (int)parsed;
  return true;
}

bool GuiUiTryParseFloatStrict(const char *text, float *out_value) {
  if (!text || !out_value) {
    return false;
  }

  char *endptr = NULL;
  float parsed = strtof(text, &endptr);
  if (!endptr || *endptr != '\0') {
    return false;
  }
  *out_value = parsed;
  return true;
}
