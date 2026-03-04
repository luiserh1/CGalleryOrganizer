#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"
#include "cli/cli_rename_utils.h"
#include "gui/gui_worker_internal.h"
#include "gui/gui_worker_tasks_internal.h"

static void BuildBasename(const char *path, char *out_name, size_t out_name_size) {
  if (!out_name || out_name_size == 0) {
    return;
  }
  out_name[0] = '\0';
  if (!path || path[0] == '\0') {
    return;
  }

  const char *base = strrchr(path, '/');
  base = base ? base + 1 : path;
  strncpy(out_name, base, out_name_size - 1);
  out_name[out_name_size - 1] = '\0';
}

static void PopulateRenamePreviewRows(const char *details_json) {
  g_worker.snapshot.rename_preview_row_count = 0;
  g_worker.snapshot.rename_preview_warning_count = 0;
  memset(g_worker.snapshot.rename_preview_rows, 0,
         sizeof(g_worker.snapshot.rename_preview_rows));

  if (!details_json || details_json[0] == '\0') {
    return;
  }

  cJSON *root = cJSON_Parse(details_json);
  if (!root) {
    return;
  }

  cJSON *items = cJSON_GetObjectItem(root, "items");
  if (!cJSON_IsArray(items)) {
    cJSON_Delete(root);
    return;
  }

  int total = cJSON_GetArraySize(items);
  int limit = total;
  if (limit > GUI_RENAME_PREVIEW_ROWS_MAX) {
    limit = GUI_RENAME_PREVIEW_ROWS_MAX;
  }

  for (int i = 0; i < limit; i++) {
    cJSON *item = cJSON_GetArrayItem(items, i);
    if (!cJSON_IsObject(item)) {
      continue;
    }

    cJSON *source = cJSON_GetObjectItem(item, "source");
    cJSON *candidate_name = cJSON_GetObjectItem(item, "candidateName");
    cJSON *tags_manual = cJSON_GetObjectItem(item, "tagsManual");
    cJSON *tags_meta = cJSON_GetObjectItem(item, "tagsMeta");
    cJSON *collision_batch = cJSON_GetObjectItem(item, "collisionInBatch");
    cJSON *collision_disk = cJSON_GetObjectItem(item, "collisionOnDisk");
    cJSON *truncated = cJSON_GetObjectItem(item, "truncated");

    GuiRenamePreviewRow *row =
        &g_worker.snapshot.rename_preview_rows[g_worker.snapshot.rename_preview_row_count];

    if (cJSON_IsString(source) && source->valuestring) {
      strncpy(row->source_path, source->valuestring, sizeof(row->source_path) - 1);
      row->source_path[sizeof(row->source_path) - 1] = '\0';
      BuildBasename(row->source_path, row->source_name, sizeof(row->source_name));
    }

    if (cJSON_IsString(candidate_name) && candidate_name->valuestring) {
      strncpy(row->candidate_name, candidate_name->valuestring,
              sizeof(row->candidate_name) - 1);
      row->candidate_name[sizeof(row->candidate_name) - 1] = '\0';
    }
    if (cJSON_IsString(tags_manual) && tags_manual->valuestring) {
      strncpy(row->tags_manual, tags_manual->valuestring,
              sizeof(row->tags_manual) - 1);
      row->tags_manual[sizeof(row->tags_manual) - 1] = '\0';
    }
    if (cJSON_IsString(tags_meta) && tags_meta->valuestring) {
      strncpy(row->tags_meta, tags_meta->valuestring, sizeof(row->tags_meta) - 1);
      row->tags_meta[sizeof(row->tags_meta) - 1] = '\0';
    }

    row->collision = (cJSON_IsBool(collision_batch) && cJSON_IsTrue(collision_batch)) ||
                     (cJSON_IsBool(collision_disk) && cJSON_IsTrue(collision_disk));
    row->warning =
        row->collision || (cJSON_IsBool(truncated) && cJSON_IsTrue(truncated));
    if (row->warning) {
      g_worker.snapshot.rename_preview_warning_count++;
    }

    g_worker.snapshot.rename_preview_row_count++;
  }

  cJSON_Delete(root);
}

static void WorkerRunRenamePreview(const GuiTaskInput *input) {
  AppRenamePreviewRequest request = {
      .target_dir = input->gallery_dir,
      .env_dir = input->env_dir,
      .pattern = input->rename_pattern[0] != '\0' ? input->rename_pattern : NULL,
      .tags_map_json_path = input->rename_tags_map_path[0] != '\0'
                                ? input->rename_tags_map_path
                                : NULL,
      .tag_add_csv =
          input->rename_tag_add_csv[0] != '\0' ? input->rename_tag_add_csv : NULL,
      .tag_remove_csv = input->rename_tag_remove_csv[0] != '\0'
                            ? input->rename_tag_remove_csv
                            : NULL,
      .meta_tag_add_csv = input->rename_meta_tag_add_csv[0] != '\0'
                              ? input->rename_meta_tag_add_csv
                              : NULL,
      .meta_tag_remove_csv = input->rename_meta_tag_remove_csv[0] != '\0'
                                 ? input->rename_meta_tag_remove_csv
                                 : NULL,
      .hooks = GuiWorkerBuildHooks(),
  };

  AppRenamePreviewResult result = {0};
  AppStatus status = AppPreviewRename(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename preview failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  strncpy(g_worker.snapshot.rename_preview_id, result.preview_id,
          sizeof(g_worker.snapshot.rename_preview_id) - 1);
  g_worker.snapshot.rename_preview_id[sizeof(g_worker.snapshot.rename_preview_id) -
                                      1] = '\0';
  g_worker.snapshot.rename_preview_file_count = result.file_count;
  g_worker.snapshot.rename_preview_collision_count = result.collision_count;
  g_worker.snapshot.rename_preview_requires_auto_suffix =
      result.requires_auto_suffix_acceptance;
  g_worker.snapshot.detail_text[0] = '\0';
  if (result.details_json && result.details_json[0] != '\0') {
    PopulateRenamePreviewRows(result.details_json);
    strncpy(g_worker.snapshot.detail_text, result.details_json,
            sizeof(g_worker.snapshot.detail_text) - 1);
    g_worker.snapshot.detail_text[sizeof(g_worker.snapshot.detail_text) - 1] =
        '\0';
  } else {
    snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
             "Preview ID: %s\nFiles: %d\nCollisions: %d\nTruncated: %d\n",
             result.preview_id, result.file_count, result.collision_count,
             result.truncation_count);
  }
  pthread_mutex_unlock(&g_worker.mutex);

  AppFreeRenamePreviewResult(&result);
  GuiWorkerSetResult(APP_STATUS_OK, "Rename preview completed", true);
}

static void WorkerRunRenameBootstrap(const GuiTaskInput *input) {
  CliRenameBootstrapResult result = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!CliRenameBootstrapTagsFromFilename(input->gallery_dir, input->env_dir,
                                          &result, error, sizeof(error))) {
    if (error[0] == '\0') {
      strncpy(error, "Rename tag bootstrap failed", sizeof(error) - 1);
      error[sizeof(error) - 1] = '\0';
    }
    GuiWorkerSetResult(APP_STATUS_IO_ERROR, error, false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  strncpy(g_worker.snapshot.rename_bootstrap_map_path, result.map_path,
          sizeof(g_worker.snapshot.rename_bootstrap_map_path) - 1);
  g_worker.snapshot
      .rename_bootstrap_map_path[sizeof(g_worker.snapshot.rename_bootstrap_map_path) -
                                 1] = '\0';
  g_worker.snapshot.rename_bootstrap_files_scanned = result.files_scanned;
  g_worker.snapshot.rename_bootstrap_files_tagged = result.files_with_tags;
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Rename tag bootstrap completed.\nTags map: %s\nFiles scanned: %d\n"
           "Files with inferred tags: %d\n",
           result.map_path, result.files_scanned, result.files_with_tags);
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Rename tag bootstrap completed", true);
}

static void WorkerRunRenameApply(const GuiTaskInput *input) {
  AppRenameApplyRequest request = {
      .env_dir = input->env_dir,
      .preview_id = input->rename_preview_id[0] != '\0' ? input->rename_preview_id
                                                         : NULL,
      .accept_auto_suffix = input->rename_accept_auto_suffix,
      .hooks = GuiWorkerBuildHooks(),
  };

  AppRenameApplyResult result = {0};
  AppStatus status = AppApplyRename(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename apply failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rename_apply_result = result;
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Operation ID: %s\nRenamed: %d\nSkipped: %d\nFailed: %d\n"
           "Collision resolutions: %d\nTruncated in plan: %d\n",
           result.operation_id, result.renamed_count, result.skipped_count,
           result.failed_count, result.collision_resolved_count,
           result.truncation_count);
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Rename apply completed", true);
}

static void WorkerRunRenamePreviewLatestId(const GuiTaskInput *input) {
  char latest_preview_id[64] = {0};
  char error[APP_MAX_ERROR] = {0};
  if (!CliRenameResolveLatestPreviewId(input->env_dir, latest_preview_id,
                                       sizeof(latest_preview_id), error,
                                       sizeof(error))) {
    GuiWorkerSetResult(APP_STATUS_IO_ERROR,
                       error[0] != '\0' ? error
                                        : "Failed to resolve latest preview id",
                       false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  strncpy(g_worker.snapshot.rename_preview_id, latest_preview_id,
          sizeof(g_worker.snapshot.rename_preview_id) - 1);
  g_worker.snapshot.rename_preview_id[sizeof(g_worker.snapshot.rename_preview_id) -
                                      1] = '\0';
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Latest preview id: %s\n", latest_preview_id);
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Latest preview id resolved", true);
}

static void WorkerRunRenameRedo(const GuiTaskInput *input) {
  if (input->rename_operation_id[0] == '\0') {
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT,
                       "Rename redo requires an operation id", false);
    return;
  }

  char resolved_preview_id[64] = {0};
  char resolve_error[APP_MAX_ERROR] = {0};
  if (!CliRenameResolvePreviewIdFromOperation(
          input->env_dir, input->rename_operation_id, resolved_preview_id,
          sizeof(resolved_preview_id), resolve_error, sizeof(resolve_error))) {
    GuiWorkerSetResult(APP_STATUS_IO_ERROR,
                       resolve_error[0] != '\0'
                           ? resolve_error
                           : "Failed to resolve preview id from operation id",
                       false);
    return;
  }

  AppRenameApplyRequest request = {
      .env_dir = input->env_dir,
      .preview_id = resolved_preview_id,
      .accept_auto_suffix = input->rename_accept_auto_suffix,
      .hooks = GuiWorkerBuildHooks(),
  };

  AppRenameApplyResult result = {0};
  AppStatus status = AppApplyRename(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename redo failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  strncpy(g_worker.snapshot.rename_preview_id, resolved_preview_id,
          sizeof(g_worker.snapshot.rename_preview_id) - 1);
  g_worker.snapshot.rename_preview_id[sizeof(g_worker.snapshot.rename_preview_id) -
                                      1] = '\0';
  g_worker.snapshot.rename_apply_result = result;
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Rename redo completed.\n"
           "Source operation ID: %s\n"
           "Resolved preview ID: %s\n"
           "Operation ID: %s\n"
           "Renamed: %d\n"
           "Skipped: %d\n"
           "Failed: %d\n"
           "Collision resolutions: %d\n"
           "Truncated in plan: %d\n",
           input->rename_operation_id, resolved_preview_id, result.operation_id,
           result.renamed_count, result.skipped_count, result.failed_count,
           result.collision_resolved_count, result.truncation_count);
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Rename redo completed", true);
}

static void WorkerRunRenameRollbackPreflight(const GuiTaskInput *input) {
  AppRenameRollbackPreflightRequest request = {
      .env_dir = input->env_dir,
      .operation_id =
          input->rename_operation_id[0] != '\0' ? input->rename_operation_id : NULL,
  };

  AppRenameRollbackPreflightResult result = {0};
  AppStatus status = AppPreflightRenameRollback(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename rollback preflight failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rename_rollback_preflight_result = result;
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Rename rollback preflight completed.\n"
           "Operation ID: %s\n"
           "Total items: %d\n"
           "Restorable: %d\n"
           "Missing destination: %d\n"
           "Source conflicts: %d\n"
           "Invalid items: %d\n"
           "Fully restorable: %s\n",
           input->rename_operation_id, result.total_items, result.restorable_count,
           result.missing_destination_count,
           result.source_exists_conflict_count, result.invalid_item_count,
           result.fully_restorable ? "yes" : "no");
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Rename rollback preflight completed", true);
}

static void WorkerRunRenameRollback(const GuiTaskInput *input) {
  AppRenameRollbackRequest request = {
      .env_dir = input->env_dir,
      .operation_id =
          input->rename_operation_id[0] != '\0' ? input->rename_operation_id : NULL,
      .hooks = GuiWorkerBuildHooks(),
  };

  AppRenameRollbackResult result = {0};
  AppStatus status = AppRollbackRename(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename rollback failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rename_rollback_result = result;
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Rename rollback completed.\nRestored: %d\nSkipped: %d\nFailed: %d\n",
           result.restored_count, result.skipped_count, result.failed_count);
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Rename rollback completed", true);
}

bool GuiWorkerRunRenameOpsTask(const GuiTaskInput *input) {
  if (!input) {
    return false;
  }

  switch (input->type) {
  case GUI_TASK_RENAME_PREVIEW:
    WorkerRunRenamePreview(input);
    return true;
  case GUI_TASK_RENAME_BOOTSTRAP_TAGS:
    WorkerRunRenameBootstrap(input);
    return true;
  case GUI_TASK_RENAME_APPLY:
    WorkerRunRenameApply(input);
    return true;
  case GUI_TASK_RENAME_PREVIEW_LATEST_ID:
    WorkerRunRenamePreviewLatestId(input);
    return true;
  case GUI_TASK_RENAME_REDO:
    WorkerRunRenameRedo(input);
    return true;
  case GUI_TASK_RENAME_ROLLBACK_PREFLIGHT:
    WorkerRunRenameRollbackPreflight(input);
    return true;
  case GUI_TASK_RENAME_ROLLBACK:
    WorkerRunRenameRollback(input);
    return true;
  default:
    return false;
  }
}
