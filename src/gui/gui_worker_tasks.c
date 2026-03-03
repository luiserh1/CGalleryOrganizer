#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "cli/cli_rename_utils.h"
#include "gui/gui_worker_internal.h"

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

static void WorkerRunScanLike(const GuiTaskInput *input, bool ml_enrich,
                              bool similarity) {
  AppScanRequest scan_request = {
      .target_dir = input->gallery_dir,
      .env_dir = input->env_dir,
      .exhaustive = input->exhaustive,
      .ml_enrich = ml_enrich,
      .similarity_report = similarity,
      .sim_incremental = input->sim_incremental,
      .jobs = input->jobs,
      .cache_compression_mode = input->cache_mode,
      .cache_compression_level = input->cache_level,
      .models_root_override = NULL,
      .hooks = GuiWorkerBuildHooks(),
  };

  AppScanResult scan_result = {0};
  AppStatus status = AppRunScan(g_worker.app, &scan_request, &scan_result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Scan task failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.scan_result = scan_result;
  pthread_mutex_unlock(&g_worker.mutex);

  if (similarity) {
    AppSimilarityRequest sim_request = {
        .env_dir = input->env_dir,
        .cache_compression_mode = input->cache_mode,
        .cache_compression_level = input->cache_level,
        .threshold = input->sim_threshold,
        .topk = input->sim_topk,
        .memory_mode = input->sim_memory_mode,
        .output_path_override = NULL,
        .hooks = GuiWorkerBuildHooks(),
    };
    AppSimilarityResult sim_result = {0};
    status = AppGenerateSimilarityReport(g_worker.app, &sim_request, &sim_result);
    if (status != APP_STATUS_OK) {
      GuiWorkerSetResult(status, "Similarity report task failed", false);
      return;
    }

    pthread_mutex_lock(&g_worker.mutex);
    g_worker.snapshot.similarity_result = sim_result;
    pthread_mutex_unlock(&g_worker.mutex);
  }

  GuiWorkerSetResult(APP_STATUS_OK, "Task completed", true);
}

static void WorkerRunOrganizePreview(const GuiTaskInput *input) {
  AppOrganizePlanRequest request = {
      .env_dir = input->env_dir,
      .cache_compression_mode = input->cache_mode,
      .cache_compression_level = input->cache_level,
      .group_by_arg = input->group_by[0] != '\0' ? input->group_by : NULL,
      .hooks = GuiWorkerBuildHooks(),
  };

  AppOrganizePlanResult result = {0};
  AppStatus status = AppPreviewOrganize(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Organize preview failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.detail_text[0] = '\0';
  if (result.plan_text) {
    strncpy(g_worker.snapshot.detail_text, result.plan_text,
            sizeof(g_worker.snapshot.detail_text) - 1);
    g_worker.snapshot.detail_text[sizeof(g_worker.snapshot.detail_text) - 1] =
        '\0';
  }
  pthread_mutex_unlock(&g_worker.mutex);

  AppFreeOrganizePlanResult(&result);
  GuiWorkerSetResult(APP_STATUS_OK, "Organize preview completed", true);
}

static void WorkerRunOrganizeExecute(const GuiTaskInput *input) {
  AppOrganizeExecuteRequest request = {
      .env_dir = input->env_dir,
      .cache_compression_mode = input->cache_mode,
      .cache_compression_level = input->cache_level,
      .group_by_arg = input->group_by[0] != '\0' ? input->group_by : NULL,
      .hooks = GuiWorkerBuildHooks(),
  };

  AppOrganizeExecuteResult result = {0};
  AppStatus status = AppExecuteOrganize(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Organize execution failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.organize_execute_result = result;
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Organize execution completed", true);
}

static void WorkerRunRollback(const GuiTaskInput *input) {
  AppRollbackRequest request = {.env_dir = input->env_dir};
  AppRollbackResult result = {0};
  AppStatus status = AppRollback(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rollback failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rollback_result = result;
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Rollback completed", true);
}

static void WorkerRunFindDuplicates(const GuiTaskInput *input) {
  if (g_worker.duplicate_report_ready) {
    AppFreeDuplicateReport(&g_worker.duplicate_report);
    memset(&g_worker.duplicate_report, 0, sizeof(g_worker.duplicate_report));
    g_worker.duplicate_report_ready = false;
    g_worker.snapshot.duplicate_report_ready = false;
  }

  AppDuplicateFindRequest request = {
      .env_dir = input->env_dir,
      .cache_compression_mode = input->cache_mode,
      .cache_compression_level = input->cache_level,
  };

  AppDuplicateReport report = {0};
  AppStatus status = AppFindDuplicates(g_worker.app, &request, &report);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Duplicate analysis failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.duplicate_report = report;
  g_worker.duplicate_report_ready = true;
  g_worker.snapshot.duplicate_group_count = report.group_count;
  g_worker.snapshot.duplicate_report_ready = true;
  g_worker.snapshot.detail_text[0] = '\0';
  pthread_mutex_unlock(&g_worker.mutex);

  char line[512];
  snprintf(line, sizeof(line), "Duplicate groups: %d\n", report.group_count);
  GuiWorkerAppendDetail(line);
  for (int i = 0; i < report.group_count; i++) {
    const char *original_path = report.groups[i].original_path;
    if (!original_path || original_path[0] == '\0') {
      original_path = "<unknown>";
    }
    snprintf(line, sizeof(line), "Group %d original: %s\n", i + 1,
             original_path);
    GuiWorkerAppendDetail(line);
  }

  GuiWorkerSetResult(APP_STATUS_OK, "Duplicate analysis completed", true);
}

static void WorkerRunMoveDuplicates(const GuiTaskInput *input) {
  if (!g_worker.duplicate_report_ready) {
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT,
                       "No duplicate report available. Run analyze first.",
                       false);
    return;
  }

  AppDuplicateMoveRequest request = {
      .target_dir = input->env_dir,
      .report = &g_worker.duplicate_report,
  };
  AppDuplicateMoveResult result = {0};
  AppStatus status = AppMoveDuplicates(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Duplicate move failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.duplicate_move_result = result;
  AppFreeDuplicateReport(&g_worker.duplicate_report);
  memset(&g_worker.duplicate_report, 0, sizeof(g_worker.duplicate_report));
  g_worker.duplicate_report_ready = false;
  g_worker.snapshot.duplicate_report_ready = false;
  g_worker.snapshot.duplicate_group_count = 0;
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Duplicate move completed", true);
}

static void WorkerRunDownloadModels(void) {
  AppModelInstallRequest request = {
      .manifest_path_override = NULL,
      .models_root_override = NULL,
      .force_redownload = false,
      .hooks = GuiWorkerBuildHooks(),
  };
  AppModelInstallResult result = {0};
  AppStatus status = AppInstallModels(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Model installation failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.model_install_result = result;
  snprintf(g_worker.snapshot.detail_text, sizeof(g_worker.snapshot.detail_text),
           "Model install completed.\nManifest entries: %d\nInstalled: %d\nSkipped: %d\n"
           "Models root: %s\nLockfile: %s\n",
           result.manifest_model_count, result.installed_count, result.skipped_count,
           result.models_root, result.lockfile_path);
  pthread_mutex_unlock(&g_worker.mutex);

  GuiWorkerSetResult(APP_STATUS_OK, "Model installation completed", true);
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

static void WorkerRunRenameHistory(const GuiTaskInput *input) {
  AppRenameHistoryEntry *entries = NULL;
  int count = 0;
  AppStatus status =
      AppListRenameHistory(g_worker.app, input->env_dir, &entries, &count);
  if (status != APP_STATUS_OK) {
    GuiWorkerSetResult(status, "Rename history query failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rename_history_count = count;
  g_worker.snapshot.detail_text[0] = '\0';
  char line[512] = {0};
  snprintf(line, sizeof(line), "Rename history entries: %d\n", count);
  strncat(g_worker.snapshot.detail_text, line,
          sizeof(g_worker.snapshot.detail_text) -
              strlen(g_worker.snapshot.detail_text) - 1);
  for (int i = 0; i < count; i++) {
    snprintf(line, sizeof(line),
             "%d. %s (%s) renamed=%d skipped=%d failed=%d rollback=%s\n",
             i + 1, entries[i].operation_id,
             entries[i].created_at_utc[0] != '\0' ? entries[i].created_at_utc
                                                  : "unknown-time",
             entries[i].renamed_count, entries[i].skipped_count,
             entries[i].failed_count,
             entries[i].rollback_performed ? "yes" : "no");
    strncat(g_worker.snapshot.detail_text, line,
            sizeof(g_worker.snapshot.detail_text) -
                strlen(g_worker.snapshot.detail_text) - 1);
  }
  pthread_mutex_unlock(&g_worker.mutex);

  AppFreeRenameHistoryEntries(entries);
  GuiWorkerSetResult(APP_STATUS_OK, "Rename history loaded", true);
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

void *GuiWorkerThreadMain(void *unused) {
  (void)unused;

  GuiTaskInput input = {0};
  pthread_mutex_lock(&g_worker.mutex);
  input = g_worker.active_input;
  g_worker.snapshot.progress_current = 0;
  g_worker.snapshot.progress_total = 0;
  g_worker.snapshot.progress_stage[0] = '\0';
  g_worker.snapshot.message[0] = '\0';
  g_worker.snapshot.detail_text[0] = '\0';
  pthread_mutex_unlock(&g_worker.mutex);

  switch (input.type) {
  case GUI_TASK_SCAN:
    WorkerRunScanLike(&input, false, false);
    break;
  case GUI_TASK_ML_ENRICH:
    WorkerRunScanLike(&input, true, false);
    break;
  case GUI_TASK_SIMILARITY:
    WorkerRunScanLike(&input, false, true);
    break;
  case GUI_TASK_PREVIEW_ORGANIZE:
    WorkerRunOrganizePreview(&input);
    break;
  case GUI_TASK_EXECUTE_ORGANIZE:
    WorkerRunOrganizeExecute(&input);
    break;
  case GUI_TASK_ROLLBACK:
    WorkerRunRollback(&input);
    break;
  case GUI_TASK_FIND_DUPLICATES:
    WorkerRunFindDuplicates(&input);
    break;
  case GUI_TASK_MOVE_DUPLICATES:
    WorkerRunMoveDuplicates(&input);
    break;
  case GUI_TASK_DOWNLOAD_MODELS:
    WorkerRunDownloadModels();
    break;
  case GUI_TASK_RENAME_PREVIEW:
    WorkerRunRenamePreview(&input);
    break;
  case GUI_TASK_RENAME_BOOTSTRAP_TAGS:
    WorkerRunRenameBootstrap(&input);
    break;
  case GUI_TASK_RENAME_APPLY:
    WorkerRunRenameApply(&input);
    break;
  case GUI_TASK_RENAME_HISTORY:
    WorkerRunRenameHistory(&input);
    break;
  case GUI_TASK_RENAME_ROLLBACK:
    WorkerRunRenameRollback(&input);
    break;
  default:
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT, "Unknown task type", false);
    break;
  }

  return NULL;
}
