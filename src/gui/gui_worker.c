#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_api.h"

#include "gui/gui_worker.h"

typedef struct {
  pthread_mutex_t mutex;
  bool initialized;
  bool busy;
  bool cancel_requested;
  bool has_result;

  AppContext *app;
  GuiTaskInput active_input;

  AppDuplicateReport duplicate_report;
  bool duplicate_report_ready;

  GuiTaskSnapshot snapshot;
  pthread_t worker_thread;
  bool thread_started;
} GuiWorkerState;

static GuiWorkerState g_worker = {0};

static bool WorkerCancelCallback(void *user_data) {
  (void)user_data;
  bool cancel = false;
  pthread_mutex_lock(&g_worker.mutex);
  cancel = g_worker.cancel_requested;
  pthread_mutex_unlock(&g_worker.mutex);
  return cancel;
}

static void WorkerProgressCallback(const AppProgressEvent *event,
                                   void *user_data) {
  (void)user_data;
  if (!event) {
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.progress_current = event->current;
  g_worker.snapshot.progress_total = event->total;
  if (event->stage) {
    strncpy(g_worker.snapshot.progress_stage, event->stage,
            sizeof(g_worker.snapshot.progress_stage) - 1);
    g_worker.snapshot.progress_stage[sizeof(g_worker.snapshot.progress_stage) -
                                     1] = '\0';
  }
  pthread_mutex_unlock(&g_worker.mutex);
}

static AppOperationHooks BuildHooks(void) {
  AppOperationHooks hooks = {
      .progress_cb = WorkerProgressCallback,
      .cancel_cb = WorkerCancelCallback,
      .user_data = NULL,
  };
  return hooks;
}

static void WorkerSetResult(AppStatus status, const char *fallback_message,
                            bool success) {
  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.status = status;
  g_worker.snapshot.success = success;
  g_worker.snapshot.has_result = true;
  g_worker.has_result = true;

  const char *msg = AppGetLastError(g_worker.app);
  if (!msg || msg[0] == '\0') {
    msg = fallback_message;
  }
  if (msg) {
    strncpy(g_worker.snapshot.message, msg, sizeof(g_worker.snapshot.message) - 1);
    g_worker.snapshot.message[sizeof(g_worker.snapshot.message) - 1] = '\0';
  } else {
    g_worker.snapshot.message[0] = '\0';
  }

  g_worker.busy = false;
  g_worker.snapshot.busy = false;
  pthread_mutex_unlock(&g_worker.mutex);
}

static void WorkerAppendDetail(const char *line) {
  if (!line) {
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  size_t used = strlen(g_worker.snapshot.detail_text);
  size_t remaining = sizeof(g_worker.snapshot.detail_text) - used - 1;
  if (remaining > 0) {
    strncat(g_worker.snapshot.detail_text, line, remaining);
  }
  pthread_mutex_unlock(&g_worker.mutex);
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
      .hooks = BuildHooks(),
  };

  AppScanResult scan_result = {0};
  AppStatus status = AppRunScan(g_worker.app, &scan_request, &scan_result);
  if (status != APP_STATUS_OK) {
    WorkerSetResult(status, "Scan task failed", false);
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
        .hooks = BuildHooks(),
    };
    AppSimilarityResult sim_result = {0};
    status = AppGenerateSimilarityReport(g_worker.app, &sim_request, &sim_result);
    if (status != APP_STATUS_OK) {
      WorkerSetResult(status, "Similarity report task failed", false);
      return;
    }

    pthread_mutex_lock(&g_worker.mutex);
    g_worker.snapshot.similarity_result = sim_result;
    pthread_mutex_unlock(&g_worker.mutex);
  }

  WorkerSetResult(APP_STATUS_OK, "Task completed", true);
}

static void WorkerRunOrganizePreview(const GuiTaskInput *input) {
  AppOrganizePlanRequest request = {
      .env_dir = input->env_dir,
      .cache_compression_mode = input->cache_mode,
      .cache_compression_level = input->cache_level,
      .group_by_arg = input->group_by[0] != '\0' ? input->group_by : NULL,
      .hooks = BuildHooks(),
  };

  AppOrganizePlanResult result = {0};
  AppStatus status = AppPreviewOrganize(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    WorkerSetResult(status, "Organize preview failed", false);
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
  WorkerSetResult(APP_STATUS_OK, "Organize preview completed", true);
}

static void WorkerRunOrganizeExecute(const GuiTaskInput *input) {
  AppOrganizeExecuteRequest request = {
      .env_dir = input->env_dir,
      .cache_compression_mode = input->cache_mode,
      .cache_compression_level = input->cache_level,
      .group_by_arg = input->group_by[0] != '\0' ? input->group_by : NULL,
      .hooks = BuildHooks(),
  };

  AppOrganizeExecuteResult result = {0};
  AppStatus status = AppExecuteOrganize(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    WorkerSetResult(status, "Organize execution failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.organize_execute_result = result;
  pthread_mutex_unlock(&g_worker.mutex);

  WorkerSetResult(APP_STATUS_OK, "Organize execution completed", true);
}

static void WorkerRunRollback(const GuiTaskInput *input) {
  AppRollbackRequest request = {.env_dir = input->env_dir};
  AppRollbackResult result = {0};
  AppStatus status = AppRollback(g_worker.app, &request, &result);
  if (status != APP_STATUS_OK) {
    WorkerSetResult(status, "Rollback failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.rollback_result = result;
  pthread_mutex_unlock(&g_worker.mutex);

  WorkerSetResult(APP_STATUS_OK, "Rollback completed", true);
}

static void WorkerRunFindDuplicates(const GuiTaskInput *input) {
  if (g_worker.duplicate_report_ready) {
    AppFreeDuplicateReport(&g_worker.duplicate_report);
    memset(&g_worker.duplicate_report, 0, sizeof(g_worker.duplicate_report));
    g_worker.duplicate_report_ready = false;
  }

  AppDuplicateFindRequest request = {
      .env_dir = input->env_dir,
      .cache_compression_mode = input->cache_mode,
      .cache_compression_level = input->cache_level,
  };

  AppDuplicateReport report = {0};
  AppStatus status = AppFindDuplicates(g_worker.app, &request, &report);
  if (status != APP_STATUS_OK) {
    WorkerSetResult(status, "Duplicate analysis failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.duplicate_report = report;
  g_worker.duplicate_report_ready = true;
  g_worker.snapshot.duplicate_group_count = report.group_count;
  g_worker.snapshot.detail_text[0] = '\0';
  pthread_mutex_unlock(&g_worker.mutex);

  char line[512];
  snprintf(line, sizeof(line), "Duplicate groups: %d\n", report.group_count);
  WorkerAppendDetail(line);
  for (int i = 0; i < report.group_count; i++) {
    snprintf(line, sizeof(line), "Group %d original: %s\n", i + 1,
             report.groups[i].original_path);
    WorkerAppendDetail(line);
  }

  WorkerSetResult(APP_STATUS_OK, "Duplicate analysis completed", true);
}

static void WorkerRunMoveDuplicates(const GuiTaskInput *input) {
  if (!g_worker.duplicate_report_ready) {
    WorkerSetResult(APP_STATUS_INVALID_ARGUMENT,
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
    WorkerSetResult(status, "Duplicate move failed", false);
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.duplicate_move_result = result;
  pthread_mutex_unlock(&g_worker.mutex);

  WorkerSetResult(APP_STATUS_OK, "Duplicate move completed", true);
}

static void *WorkerThreadMain(void *unused) {
  (void)unused;

  GuiTaskInput input;
  memset(&input, 0, sizeof(input));

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
  default:
    WorkerSetResult(APP_STATUS_INVALID_ARGUMENT, "Unknown task type", false);
    break;
  }

  return NULL;
}

bool GuiWorkerInit(AppContext *app) {
  if (!app) {
    return false;
  }

  memset(&g_worker, 0, sizeof(g_worker));
  pthread_mutex_init(&g_worker.mutex, NULL);
  g_worker.app = app;
  g_worker.initialized = true;
  return true;
}

void GuiWorkerShutdown(void) {
  if (!g_worker.initialized) {
    return;
  }

  if (g_worker.busy) {
    g_worker.cancel_requested = true;
    pthread_join(g_worker.worker_thread, NULL);
    g_worker.thread_started = false;
  } else if (g_worker.thread_started) {
    pthread_join(g_worker.worker_thread, NULL);
    g_worker.thread_started = false;
  }

  if (g_worker.duplicate_report_ready) {
    AppFreeDuplicateReport(&g_worker.duplicate_report);
    g_worker.duplicate_report_ready = false;
  }

  pthread_mutex_destroy(&g_worker.mutex);
  memset(&g_worker, 0, sizeof(g_worker));
}

bool GuiWorkerStartTask(const GuiTaskInput *input) {
  if (!g_worker.initialized || !input) {
    return false;
  }

  pthread_mutex_lock(&g_worker.mutex);
  if (g_worker.busy) {
    pthread_mutex_unlock(&g_worker.mutex);
    return false;
  }

  g_worker.active_input = *input;
  g_worker.cancel_requested = false;
  g_worker.busy = true;
  g_worker.snapshot.busy = true;
  g_worker.snapshot.has_result = false;
  g_worker.snapshot.success = false;
  g_worker.snapshot.status = APP_STATUS_OK;
  g_worker.snapshot.message[0] = '\0';
  g_worker.snapshot.detail_text[0] = '\0';
  pthread_mutex_unlock(&g_worker.mutex);

  if (pthread_create(&g_worker.worker_thread, NULL, WorkerThreadMain, NULL) !=
      0) {
    pthread_mutex_lock(&g_worker.mutex);
    g_worker.busy = false;
    g_worker.snapshot.busy = false;
    pthread_mutex_unlock(&g_worker.mutex);
    return false;
  }
  g_worker.thread_started = true;

  return true;
}

void GuiWorkerRequestCancel(void) {
  if (!g_worker.initialized) {
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.cancel_requested = true;
  pthread_mutex_unlock(&g_worker.mutex);
}

void GuiWorkerGetSnapshot(GuiTaskSnapshot *out_snapshot) {
  if (!out_snapshot) {
    return;
  }

  memset(out_snapshot, 0, sizeof(*out_snapshot));
  if (!g_worker.initialized) {
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  *out_snapshot = g_worker.snapshot;
  pthread_mutex_unlock(&g_worker.mutex);
}

void GuiWorkerClearResult(void) {
  if (!g_worker.initialized) {
    return;
  }

  pthread_mutex_lock(&g_worker.mutex);
  g_worker.snapshot.has_result = false;
  g_worker.has_result = false;
  pthread_mutex_unlock(&g_worker.mutex);

  if (!g_worker.busy && g_worker.thread_started) {
    pthread_join(g_worker.worker_thread, NULL);
    g_worker.thread_started = false;
  }
}
