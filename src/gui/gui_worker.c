#include <pthread.h>
#include <stdbool.h>
#include <string.h>

#include "app_api.h"

#include "gui/gui_worker_internal.h"

GuiWorkerState g_worker = {0};

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

AppOperationHooks GuiWorkerBuildHooks(void) {
  AppOperationHooks hooks = {
      .progress_cb = WorkerProgressCallback,
      .cancel_cb = WorkerCancelCallback,
      .user_data = NULL,
  };
  return hooks;
}

void GuiWorkerSetResult(AppStatus status, const char *fallback_message,
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

void GuiWorkerAppendDetail(const char *line) {
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

bool GuiWorkerInit(AppContext *app) {
  if (!app) {
    return false;
  }

  memset(&g_worker, 0, sizeof(g_worker));
  pthread_mutex_init(&g_worker.mutex, NULL);
  g_worker.app = app;
  g_worker.initialized = true;
  g_worker.snapshot.duplicate_report_ready = false;
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
  memset(&g_worker.snapshot.model_install_result, 0,
         sizeof(g_worker.snapshot.model_install_result));
  g_worker.snapshot.message[0] = '\0';
  g_worker.snapshot.detail_text[0] = '\0';
  g_worker.snapshot.duplicate_report_ready = g_worker.duplicate_report_ready;
  pthread_mutex_unlock(&g_worker.mutex);

  if (pthread_create(&g_worker.worker_thread, NULL, GuiWorkerThreadMain, NULL) !=
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
