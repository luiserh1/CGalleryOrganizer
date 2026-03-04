#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "gui/gui_state.h"

static bool GetConfigDirectory(char *out_dir, size_t out_size) {
  if (!out_dir || out_size == 0) {
    return false;
  }

  const char *override = getenv("CGO_GUI_STATE_PATH");
  if (override && override[0] != '\0') {
    strncpy(out_dir, override, out_size - 1);
    out_dir[out_size - 1] = '\0';

    char *last_slash = strrchr(out_dir, '/');
    if (last_slash) {
      *last_slash = '\0';
    } else {
      strncpy(out_dir, ".", out_size - 1);
      out_dir[out_size - 1] = '\0';
    }
    return true;
  }

#if defined(_WIN32)
  const char *base = getenv("APPDATA");
  if (!base || base[0] == '\0') {
    return false;
  }
  snprintf(out_dir, out_size, "%s/CGalleryOrganizer", base);
#elif defined(__APPLE__)
  const char *home = getenv("HOME");
  if (!home || home[0] == '\0') {
    return false;
  }
  snprintf(out_dir, out_size, "%s/Library/Application Support/CGalleryOrganizer",
           home);
#else
  const char *xdg = getenv("XDG_CONFIG_HOME");
  if (xdg && xdg[0] != '\0') {
    snprintf(out_dir, out_size, "%s/CGalleryOrganizer", xdg);
  } else {
    const char *home = getenv("HOME");
    if (!home || home[0] == '\0') {
      return false;
    }
    snprintf(out_dir, out_size, "%s/.config/CGalleryOrganizer", home);
  }
#endif

  return true;
}

bool GuiStateResolvePath(char *out_path, size_t out_size) {
  if (!out_path || out_size == 0) {
    return false;
  }

  const char *override = getenv("CGO_GUI_STATE_PATH");
  if (override && override[0] != '\0') {
    strncpy(out_path, override, out_size - 1);
    out_path[out_size - 1] = '\0';
    return true;
  }

  char config_dir[GUI_STATE_MAX_PATH] = {0};
  if (!GetConfigDirectory(config_dir, sizeof(config_dir))) {
    return false;
  }

  snprintf(out_path, out_size, "%s/gui_state.json", config_dir);
  return true;
}

bool GuiStateReset(void) {
  char path[GUI_STATE_MAX_PATH] = {0};
  if (!GuiStateResolvePath(path, sizeof(path))) {
    return false;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    return true;
  }

  return remove(path) == 0;
}
