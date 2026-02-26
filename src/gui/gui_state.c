#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "cJSON.h"
#include "fs_utils.h"

#include "gui/gui_state.h"

static void GuiStateSetDefaults(GuiState *state) {
  if (!state) {
    return;
  }

  memset(state, 0, sizeof(*state));
  state->scan_exhaustive = false;
  state->scan_jobs = 1;
  state->scan_cache_mode = APP_CACHE_COMPRESSION_NONE;
  state->scan_cache_level = 3;
  state->sim_incremental = true;
  state->sim_threshold = 0.92f;
  state->sim_topk = 5;
  state->sim_memory_mode = APP_SIM_MEMORY_CHUNKED;
  strncpy(state->organize_group_by, "date", sizeof(state->organize_group_by) - 1);
  state->organize_group_by[sizeof(state->organize_group_by) - 1] = '\0';
}

static int ClampInt(int value, int min_value, int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static float ClampFloat(float value, float min_value, float max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

static bool IsBlankString(const char *text) {
  if (!text) {
    return true;
  }
  while (*text != '\0') {
    if (!isspace((unsigned char)*text)) {
      return false;
    }
    text++;
  }
  return true;
}

static const char *CompressionModeToString(AppCacheCompressionMode mode) {
  if (mode == APP_CACHE_COMPRESSION_ZSTD) {
    return "zstd";
  }
  return "none";
}

static AppCacheCompressionMode ParseCompressionMode(const cJSON *node) {
  if (!cJSON_IsString(node) || !node->valuestring) {
    return APP_CACHE_COMPRESSION_NONE;
  }
  if (strcmp(node->valuestring, "zstd") == 0) {
    return APP_CACHE_COMPRESSION_ZSTD;
  }
  return APP_CACHE_COMPRESSION_NONE;
}

static const char *MemoryModeToString(AppSimilarityMemoryMode mode) {
  if (mode == APP_SIM_MEMORY_EAGER) {
    return "eager";
  }
  return "chunked";
}

static AppSimilarityMemoryMode ParseMemoryMode(const cJSON *node) {
  if (!cJSON_IsString(node) || !node->valuestring) {
    return APP_SIM_MEMORY_CHUNKED;
  }
  if (strcmp(node->valuestring, "eager") == 0) {
    return APP_SIM_MEMORY_EAGER;
  }
  return APP_SIM_MEMORY_CHUNKED;
}

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

  GuiStateSetDefaults(out_state);

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
  cJSON *scan_exhaustive = cJSON_GetObjectItem(json, "scanExhaustive");
  cJSON *scan_jobs = cJSON_GetObjectItem(json, "scanJobs");
  cJSON *scan_cache_mode = cJSON_GetObjectItem(json, "scanCacheCompressionMode");
  cJSON *scan_cache_level = cJSON_GetObjectItem(json, "scanCacheCompressionLevel");
  cJSON *sim_incremental = cJSON_GetObjectItem(json, "simIncremental");
  cJSON *sim_threshold = cJSON_GetObjectItem(json, "simThreshold");
  cJSON *sim_topk = cJSON_GetObjectItem(json, "simTopK");
  cJSON *sim_memory_mode = cJSON_GetObjectItem(json, "simMemoryMode");
  cJSON *organize_group_by = cJSON_GetObjectItem(json, "organizeGroupBy");
  cJSON *updated_at = cJSON_GetObjectItem(json, "updatedAt");

  if (cJSON_IsString(gallery) && gallery->valuestring) {
    strncpy(out_state->gallery_dir, gallery->valuestring,
            sizeof(out_state->gallery_dir) - 1);
    out_state->gallery_dir[sizeof(out_state->gallery_dir) - 1] = '\0';
  }
  if (cJSON_IsString(env) && env->valuestring) {
    strncpy(out_state->env_dir, env->valuestring, sizeof(out_state->env_dir) - 1);
    out_state->env_dir[sizeof(out_state->env_dir) - 1] = '\0';
  }

  if (cJSON_IsBool(scan_exhaustive)) {
    out_state->scan_exhaustive = cJSON_IsTrue(scan_exhaustive);
  }
  if (cJSON_IsNumber(scan_jobs)) {
    out_state->scan_jobs = ClampInt((int)scan_jobs->valuedouble, 1, 256);
  }
  out_state->scan_cache_mode = ParseCompressionMode(scan_cache_mode);
  if (cJSON_IsNumber(scan_cache_level)) {
    out_state->scan_cache_level =
        ClampInt((int)scan_cache_level->valuedouble, 1, 19);
  }

  if (cJSON_IsBool(sim_incremental)) {
    out_state->sim_incremental = cJSON_IsTrue(sim_incremental);
  }
  if (cJSON_IsNumber(sim_threshold)) {
    out_state->sim_threshold =
        ClampFloat((float)sim_threshold->valuedouble, 0.0f, 1.0f);
  }
  if (cJSON_IsNumber(sim_topk)) {
    out_state->sim_topk = ClampInt((int)sim_topk->valuedouble, 1, 1000);
  }
  out_state->sim_memory_mode = ParseMemoryMode(sim_memory_mode);

  if (cJSON_IsString(organize_group_by) && organize_group_by->valuestring &&
      !IsBlankString(organize_group_by->valuestring)) {
    strncpy(out_state->organize_group_by, organize_group_by->valuestring,
            sizeof(out_state->organize_group_by) - 1);
    out_state->organize_group_by[sizeof(out_state->organize_group_by) - 1] =
        '\0';
  }

  if (cJSON_IsString(updated_at) && updated_at->valuestring) {
    strncpy(out_state->updated_at, updated_at->valuestring,
            sizeof(out_state->updated_at) - 1);
    out_state->updated_at[sizeof(out_state->updated_at) - 1] = '\0';
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

  cJSON_AddNumberToObject(json, "schemaVersion", 2);
  cJSON_AddStringToObject(json, "galleryDir", state->gallery_dir);
  cJSON_AddStringToObject(json, "envDir", state->env_dir);
  cJSON_AddBoolToObject(json, "scanExhaustive", state->scan_exhaustive);
  cJSON_AddNumberToObject(json, "scanJobs", ClampInt(state->scan_jobs, 1, 256));
  cJSON_AddStringToObject(json, "scanCacheCompressionMode",
                          CompressionModeToString(state->scan_cache_mode));
  cJSON_AddNumberToObject(json, "scanCacheCompressionLevel",
                          ClampInt(state->scan_cache_level, 1, 19));
  cJSON_AddBoolToObject(json, "simIncremental", state->sim_incremental);
  cJSON_AddNumberToObject(json, "simThreshold",
                          ClampFloat(state->sim_threshold, 0.0f, 1.0f));
  cJSON_AddNumberToObject(json, "simTopK", ClampInt(state->sim_topk, 1, 1000));
  cJSON_AddStringToObject(json, "simMemoryMode",
                          MemoryModeToString(state->sim_memory_mode));
  cJSON_AddStringToObject(json, "organizeGroupBy", state->organize_group_by);
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
