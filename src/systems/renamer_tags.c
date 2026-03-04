#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "fs_utils.h"
#include "systems/renamer_tags.h"
#include "systems/renamer_tags_internal.h"
static bool EnsureCacheDir(const char *env_dir, char *out_path,
                           size_t out_path_size) {
  if (!env_dir || env_dir[0] == '\0' || !out_path || out_path_size == 0) {
    return false;
  }

  char cache_dir[MAX_PATH_LENGTH] = {0};
  snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", env_dir);
  if (!FsMakeDirRecursive(cache_dir)) {
    return false;
  }

  snprintf(out_path, out_path_size, "%s/rename_tags.json", cache_dir);
  return true;
}
static cJSON *CreateDefaultSidecar(void) {
  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return NULL;
  }
  cJSON_AddNumberToObject(root, "version", 1);
  cJSON_AddItemToObject(root, "files", cJSON_CreateObject());
  return root;
}

static cJSON *GetFilesObject(cJSON *root) {
  if (!root) {
    return NULL;
  }

  cJSON *files = cJSON_GetObjectItem(root, "files");
  if (!cJSON_IsObject(files)) {
    cJSON *replacement = cJSON_CreateObject();
    if (!replacement) {
      return NULL;
    }
    if (files) {
      cJSON_ReplaceItemInObject(root, "files", replacement);
    } else {
      cJSON_AddItemToObject(root, "files", replacement);
    }
    files = replacement;
  }
  return files;
}
void RenamerTagsNowUtc(char *out_text, size_t out_text_size) {
  if (!out_text || out_text_size == 0) {
    return;
  }

  out_text[0] = '\0';
  time_t now = time(NULL);
  struct tm tm_utc;
#if defined(_WIN32)
  gmtime_s(&tm_utc, &now);
#else
  gmtime_r(&now, &tm_utc);
#endif
  strftime(out_text, out_text_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

cJSON *RenamerTagsEnsurePathEntry(cJSON *root, const char *absolute_path) {
  if (!root || !absolute_path || absolute_path[0] == '\0') {
    return NULL;
  }

  cJSON *files = GetFilesObject(root);
  if (!files) {
    return NULL;
  }

  cJSON *entry = cJSON_GetObjectItem(files, absolute_path);
  if (!cJSON_IsObject(entry)) {
    cJSON *replacement = cJSON_CreateObject();
    if (!replacement) {
      return NULL;
    }

    cJSON_AddItemToObject(replacement, "manualTags", cJSON_CreateArray());
    cJSON_AddItemToObject(replacement, "metaTagAdds", cJSON_CreateArray());
    cJSON_AddItemToObject(replacement, "suppressedMetaTags", cJSON_CreateArray());
    char updated_at[64] = {0};
    RenamerTagsNowUtc(updated_at, sizeof(updated_at));
    cJSON_AddStringToObject(replacement, "updatedAtUtc", updated_at);

    if (entry) {
      cJSON_ReplaceItemInObject(files, absolute_path, replacement);
    } else {
      cJSON_AddItemToObject(files, absolute_path, replacement);
    }
    entry = replacement;
  }

  cJSON *manual = cJSON_GetObjectItem(entry, "manualTags");
  if (!cJSON_IsArray(manual)) {
    cJSON *replacement = cJSON_CreateArray();
    if (!replacement) {
      return NULL;
    }
    if (manual) {
      cJSON_ReplaceItemInObject(entry, "manualTags", replacement);
    } else {
      cJSON_AddItemToObject(entry, "manualTags", replacement);
    }
  }

  cJSON *meta_adds = cJSON_GetObjectItem(entry, "metaTagAdds");
  if (!cJSON_IsArray(meta_adds)) {
    cJSON *replacement = cJSON_CreateArray();
    if (!replacement) {
      return NULL;
    }
    if (meta_adds) {
      cJSON_ReplaceItemInObject(entry, "metaTagAdds", replacement);
    } else {
      cJSON_AddItemToObject(entry, "metaTagAdds", replacement);
    }
  }

  cJSON *suppressed = cJSON_GetObjectItem(entry, "suppressedMetaTags");
  if (!cJSON_IsArray(suppressed)) {
    cJSON *replacement = cJSON_CreateArray();
    if (!replacement) {
      return NULL;
    }
    if (suppressed) {
      cJSON_ReplaceItemInObject(entry, "suppressedMetaTags", replacement);
    } else {
      cJSON_AddItemToObject(entry, "suppressedMetaTags", replacement);
    }
  }

  return entry;
}
bool RenamerTagsLoadSidecar(const char *env_dir, cJSON **out_root,
                            char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_root) {
    RenamerTagsSetError(out_error, out_error_size, "env_dir is required for rename tags");
    return false;
  }

  *out_root = NULL;
  char sidecar_path[MAX_PATH_LENGTH] = {0};
  if (!EnsureCacheDir(env_dir, sidecar_path, sizeof(sidecar_path))) {
    RenamerTagsSetError(out_error, out_error_size,
             "failed to prepare .cache directory for rename tags");
    return false;
  }

  char *text = NULL;
  if (!RenamerTagsLoadFileText(sidecar_path, &text)) {
    *out_root = CreateDefaultSidecar();
    if (!*out_root) {
      RenamerTagsSetError(out_error, out_error_size, "failed to initialize tag sidecar");
      return false;
    }
    return true;
  }

  cJSON *root = cJSON_Parse(text);
  free(text);
  if (!root) {
    *out_root = CreateDefaultSidecar();
    if (!*out_root) {
      RenamerTagsSetError(out_error, out_error_size,
               "failed to recover malformed rename tag sidecar");
      return false;
    }
    return true;
  }

  cJSON *files = cJSON_GetObjectItem(root, "files");
  if (!cJSON_IsObject(files)) {
    cJSON_Delete(root);
    root = CreateDefaultSidecar();
    if (!root) {
      RenamerTagsSetError(out_error, out_error_size, "failed to initialize tag sidecar");
      return false;
    }
  }

  *out_root = root;
  return true;
}

bool RenamerTagsSaveSidecar(const char *env_dir, const cJSON *root,
                            char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !root) {
    RenamerTagsSetError(out_error, out_error_size,
             "env_dir and root are required for rename tag save");
    return false;
  }

  char sidecar_path[MAX_PATH_LENGTH] = {0};
  if (!EnsureCacheDir(env_dir, sidecar_path, sizeof(sidecar_path))) {
    RenamerTagsSetError(out_error, out_error_size,
             "failed to prepare .cache directory for rename tags");
    return false;
  }

  char *json_text = cJSON_Print(root);
  if (!json_text) {
    RenamerTagsSetError(out_error, out_error_size,
             "failed to serialize rename tag sidecar");
    return false;
  }

  bool ok = RenamerTagsSaveFileText(sidecar_path, json_text);
  cJSON_free(json_text);
  if (!ok) {
    RenamerTagsSetError(out_error, out_error_size,
             "failed to write rename tag sidecar '%s'", sidecar_path);
    return false;
  }

  return true;
}
bool RenamerTagsMovePathKey(cJSON *root, const char *old_path,
                            const char *new_path) {
  if (!root || !old_path || old_path[0] == '\0' || !new_path ||
      new_path[0] == '\0') {
    return false;
  }

  cJSON *files = GetFilesObject(root);
  if (!files) {
    return false;
  }

  cJSON *entry = cJSON_DetachItemFromObject(files, old_path);
  if (!entry) {
    return true;
  }

  cJSON_DeleteItemFromObject(files, new_path);
  cJSON_AddItemToObject(files, new_path, entry);
  return true;
}
