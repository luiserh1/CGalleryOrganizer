#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "cJSON.h"
#include "fs_utils.h"

#include "gui/gui_layout.h"
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

bool GuiStateLoad(GuiState *out_state) {
  if (!out_state) {
    return false;
  }

  memset(out_state, 0, sizeof(*out_state));
  out_state->ui_scale_percent = GUI_STATE_DEFAULT_UI_SCALE_PERCENT;
  out_state->window_width = GUI_STATE_DEFAULT_WINDOW_WIDTH;
  out_state->window_height = GUI_STATE_DEFAULT_WINDOW_HEIGHT;

  char path[GUI_STATE_MAX_PATH] = {0};
  if (!GuiStateResolvePath(path, sizeof(path))) {
    return false;
  }

  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long len = ftell(f);
  if (len < 0) {
    fclose(f);
    return false;
  }
  rewind(f);

  char *data = calloc((size_t)len + 1, 1);
  if (!data) {
    fclose(f);
    return false;
  }

  size_t read = fread(data, 1, (size_t)len, f);
  fclose(f);
  data[read] = '\0';

  cJSON *json = cJSON_Parse(data);
  free(data);
  if (!json) {
    return false;
  }

  cJSON *gallery = cJSON_GetObjectItem(json, "galleryDir");
  cJSON *env = cJSON_GetObjectItem(json, "envDir");
  cJSON *ui_scale = cJSON_GetObjectItem(json, "uiScalePercent");
  cJSON *window_width = cJSON_GetObjectItem(json, "windowWidth");
  cJSON *window_height = cJSON_GetObjectItem(json, "windowHeight");
  cJSON *updated_at = cJSON_GetObjectItem(json, "updatedAt");

  if (cJSON_IsString(gallery)) {
    strncpy(out_state->gallery_dir, gallery->valuestring,
            sizeof(out_state->gallery_dir) - 1);
  }
  if (cJSON_IsString(env)) {
    strncpy(out_state->env_dir, env->valuestring, sizeof(out_state->env_dir) - 1);
  }
  if (cJSON_IsNumber(ui_scale)) {
    out_state->ui_scale_percent = GuiLayoutClampUiScalePercent(ui_scale->valueint);
  }
  if (cJSON_IsNumber(window_width) && window_width->valueint > 0) {
    out_state->window_width = window_width->valueint;
  }
  if (cJSON_IsNumber(window_height) && window_height->valueint > 0) {
    out_state->window_height = window_height->valueint;
  }
  if (cJSON_IsString(updated_at)) {
    strncpy(out_state->updated_at, updated_at->valuestring,
            sizeof(out_state->updated_at) - 1);
  }

  cJSON_Delete(json);
  return true;
}

bool GuiStateSave(const GuiState *state) {
  if (!state) {
    return false;
  }

  char path[GUI_STATE_MAX_PATH] = {0};
  if (!GuiStateResolvePath(path, sizeof(path))) {
    return false;
  }

  char dir[GUI_STATE_MAX_PATH] = {0};
  strncpy(dir, path, sizeof(dir) - 1);
  char *slash = strrchr(dir, '/');
  if (slash) {
    *slash = '\0';
  }

  if (dir[0] != '\0' && !FsMakeDirRecursive(dir)) {
    return false;
  }

  cJSON *json = cJSON_CreateObject();
  if (!json) {
    return false;
  }

  char updated_at[GUI_STATE_UPDATED_AT_MAX] = {0};
  time_t now = time(NULL);
  struct tm *utc = gmtime(&now);
  if (utc) {
    strftime(updated_at, sizeof(updated_at), "%Y-%m-%dT%H:%M:%SZ", utc);
  }

  cJSON_AddStringToObject(json, "galleryDir", state->gallery_dir);
  cJSON_AddStringToObject(json, "envDir", state->env_dir);
  cJSON_AddNumberToObject(
      json, "uiScalePercent",
      GuiLayoutClampUiScalePercent(state->ui_scale_percent));
  cJSON_AddNumberToObject(
      json, "windowWidth",
      state->window_width > 0 ? state->window_width : GUI_STATE_DEFAULT_WINDOW_WIDTH);
  cJSON_AddNumberToObject(
      json, "windowHeight",
      state->window_height > 0 ? state->window_height : GUI_STATE_DEFAULT_WINDOW_HEIGHT);
  if (updated_at[0] != '\0') {
    cJSON_AddStringToObject(json, "updatedAt", updated_at);
  }

  char *text = cJSON_Print(json);
  cJSON_Delete(json);
  if (!text) {
    return false;
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    cJSON_free(text);
    return false;
  }

  fputs(text, f);
  fclose(f);
  cJSON_free(text);

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
