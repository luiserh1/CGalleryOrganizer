#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cJSON.h"
#include "cli/cli_rename_common.h"
#include "cli/cli_rename_utils.h"

#if !defined(NAME_MAX)
#define NAME_MAX 255
#endif

bool CliRenameResolveLatestPreviewId(const char *env_dir, char *out_preview_id,
                                     size_t out_preview_id_size,
                                     char *out_error,
                                     size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_preview_id ||
      out_preview_id_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "environment directory and output buffer are required");
    return false;
  }
  out_preview_id[0] = '\0';

  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  char preview_dir[MAX_PATH_LENGTH] = {0};
  snprintf(preview_dir, sizeof(preview_dir), "%s/.cache/rename_previews",
           env_abs);
  DIR *dir = opendir(preview_dir);
  if (!dir) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename preview directory not found: '%s'", preview_dir);
    return false;
  }

  char latest_name[NAME_MAX + 1] = {0};
  struct dirent *entry = NULL;
  while ((entry = readdir(dir)) != NULL) {
    if (!CliRenameCommonEndsWith(entry->d_name, ".json")) {
      continue;
    }
    if (strncmp(entry->d_name, "rnp-", 4) != 0) {
      continue;
    }
    if (latest_name[0] == '\0' || strcmp(entry->d_name, latest_name) > 0) {
      strncpy(latest_name, entry->d_name, sizeof(latest_name) - 1);
      latest_name[sizeof(latest_name) - 1] = '\0';
    }
  }
  closedir(dir);

  if (latest_name[0] == '\0') {
    CliRenameCommonSetError(out_error, out_error_size,
             "no rename preview artifacts found under '%s'", preview_dir);
    return false;
  }

  size_t len = strlen(latest_name);
  if (len <= 5) {
    CliRenameCommonSetError(out_error, out_error_size, "invalid rename preview artifact name");
    return false;
  }
  latest_name[len - 5] = '\0';
  strncpy(out_preview_id, latest_name, out_preview_id_size - 1);
  out_preview_id[out_preview_id_size - 1] = '\0';
  return true;
}

static bool LoadRenameHistoryEntries(const char *env_dir, cJSON **out_entries,
                                     char *out_error, size_t out_error_size) {
  if (!env_dir || env_dir[0] == '\0' || !out_entries) {
    CliRenameCommonSetError(out_error, out_error_size,
             "environment directory and output payload are required");
    return false;
  }

  *out_entries = NULL;
  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  char index_path[MAX_PATH_LENGTH] = {0};
  snprintf(index_path, sizeof(index_path), "%s/.cache/rename_history/index.json",
           env_abs);

  char *index_text = NULL;
  if (!CliRenameCommonLoadTextFile(index_path, &index_text)) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename history index not found: '%s'", index_path);
    return false;
  }

  cJSON *index_json = cJSON_Parse(index_text);
  free(index_text);
  if (!index_json) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename history index is malformed: '%s'", index_path);
    return false;
  }

  cJSON *entries = cJSON_DetachItemFromObject(index_json, "entries");
  cJSON_Delete(index_json);
  if (!cJSON_IsArray(entries)) {
    cJSON_Delete(entries);
    CliRenameCommonSetError(out_error, out_error_size,
             "rename history index entries payload is invalid");
    return false;
  }

  *out_entries = entries;
  return true;
}

bool CliRenameResolveLatestOperationId(const char *env_dir, char *out_operation_id,
                                       size_t out_operation_id_size,
                                       char *out_error,
                                       size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !out_operation_id ||
      out_operation_id_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "environment directory and output buffer are required");
    return false;
  }

  out_operation_id[0] = '\0';
  cJSON *entries = NULL;
  if (!LoadRenameHistoryEntries(env_dir, &entries, out_error, out_error_size)) {
    return false;
  }

  bool found = false;
  int count = cJSON_GetArraySize(entries);
  for (int i = count - 1; i >= 0; i--) {
    cJSON *entry = cJSON_GetArrayItem(entries, i);
    cJSON *operation_id = cJSON_GetObjectItem(entry, "operationId");
    if (!cJSON_IsString(operation_id) || !operation_id->valuestring ||
        operation_id->valuestring[0] == '\0') {
      continue;
    }
    strncpy(out_operation_id, operation_id->valuestring,
            out_operation_id_size - 1);
    out_operation_id[out_operation_id_size - 1] = '\0';
    found = true;
    break;
  }

  cJSON_Delete(entries);
  if (!found) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename history contains no operation ids");
    return false;
  }
  return true;
}

bool CliRenameResolvePreviewIdFromOperation(const char *env_dir,
                                            const char *operation_id,
                                            char *out_preview_id,
                                            size_t out_preview_id_size,
                                            char *out_error,
                                            size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !operation_id ||
      operation_id[0] == '\0' || !out_preview_id ||
      out_preview_id_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "env directory, operation id, and output buffer are required");
    return false;
  }
  out_preview_id[0] = '\0';

  cJSON *entries = NULL;
  if (!LoadRenameHistoryEntries(env_dir, &entries, out_error, out_error_size)) {
    return false;
  }

  bool found = false;
  int count = cJSON_GetArraySize(entries);
  for (int i = count - 1; i >= 0; i--) {
    cJSON *entry = cJSON_GetArrayItem(entries, i);
    cJSON *entry_operation_id = cJSON_GetObjectItem(entry, "operationId");
    if (!cJSON_IsString(entry_operation_id) ||
        !entry_operation_id->valuestring ||
        strcmp(entry_operation_id->valuestring, operation_id) != 0) {
      continue;
    }

    cJSON *entry_preview_id = cJSON_GetObjectItem(entry, "previewId");
    if (!cJSON_IsString(entry_preview_id) || !entry_preview_id->valuestring ||
        entry_preview_id->valuestring[0] == '\0') {
      cJSON_Delete(entries);
      CliRenameCommonSetError(out_error, out_error_size,
               "operation '%s' does not reference a preview id", operation_id);
      return false;
    }

    strncpy(out_preview_id, entry_preview_id->valuestring,
            out_preview_id_size - 1);
    out_preview_id[out_preview_id_size - 1] = '\0';
    found = true;
    break;
  }

  cJSON_Delete(entries);
  if (!found) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename operation '%s' not found in history", operation_id);
    return false;
  }
  return true;
}

bool CliRenameResolveOperationManifestPath(const char *env_dir,
                                           const char *operation_id,
                                           char *out_manifest_path,
                                           size_t out_manifest_path_size,
                                           char *out_error,
                                           size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!env_dir || env_dir[0] == '\0' || !operation_id ||
      operation_id[0] == '\0' || !out_manifest_path ||
      out_manifest_path_size == 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "env directory, operation id, and output buffer are required");
    return false;
  }

  out_manifest_path[0] = '\0';
  char env_abs[MAX_PATH_LENGTH] = {0};
  if (!FsGetAbsolutePath(env_dir, env_abs, sizeof(env_abs))) {
    CliRenameCommonSetError(out_error, out_error_size,
             "failed to resolve absolute environment path '%s'", env_dir);
    return false;
  }

  char manifest_path[MAX_PATH_LENGTH] = {0};
  snprintf(manifest_path, sizeof(manifest_path),
           "%s/.cache/rename_history/%s.json", env_abs, operation_id);
  if (access(manifest_path, F_OK) != 0) {
    CliRenameCommonSetError(out_error, out_error_size,
             "rename operation manifest not found: '%s'", manifest_path);
    return false;
  }

  strncpy(out_manifest_path, manifest_path, out_manifest_path_size - 1);
  out_manifest_path[out_manifest_path_size - 1] = '\0';
  return true;
}
