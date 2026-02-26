#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "gui/gui_worker_internal.h"

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
  default:
    GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT, "Unknown task type", false);
    break;
  }

  return NULL;
}
