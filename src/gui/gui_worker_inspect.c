#include <stdio.h>
#include <string.h>

#include "gui/gui_worker_internal.h"

bool GuiWorkerInspectRuntimeState(const AppRuntimeStateRequest *request,
                                  AppRuntimeState *out_state, char *out_error,
                                  size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!request || !out_state) {
    return false;
  }

  pthread_mutex_lock(&g_worker.mutex);
  bool initialized = g_worker.initialized;
  bool busy = g_worker.busy;
  AppContext *app = g_worker.app;
  pthread_mutex_unlock(&g_worker.mutex);

  if (!initialized || !app) {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size, "Worker is not initialized");
    }
    return false;
  }

  if (busy) {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size,
               "Runtime state refresh is blocked while a task is running");
    }
    return false;
  }

  AppStatus status = AppInspectRuntimeState(app, request, out_state);
  if (status != APP_STATUS_OK) {
    if (out_error && out_error_size > 0) {
      const char *last_error = AppGetLastError(app);
      if (last_error && last_error[0] != '\0') {
        strncpy(out_error, last_error, out_error_size - 1);
        out_error[out_error_size - 1] = '\0';
      } else {
        snprintf(out_error, out_error_size, "Runtime inspection failed: %s",
                 AppStatusToString(status));
      }
    }
    return false;
  }

  return true;
}

bool GuiWorkerInspectScanProfile(const AppScanRequest *request,
                                 AppScanProfileDecision *out_decision,
                                 char *out_error, size_t out_error_size) {
  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }
  if (!request || !out_decision) {
    return false;
  }

  pthread_mutex_lock(&g_worker.mutex);
  bool initialized = g_worker.initialized;
  bool busy = g_worker.busy;
  AppContext *app = g_worker.app;
  pthread_mutex_unlock(&g_worker.mutex);

  if (!initialized || !app) {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size, "Worker is not initialized");
    }
    return false;
  }

  if (busy) {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size,
               "Scan profile preflight is blocked while a task is running");
    }
    return false;
  }

  AppStatus status = AppInspectScanProfile(app, request, out_decision);
  if (status != APP_STATUS_OK) {
    if (out_error && out_error_size > 0) {
      const char *last_error = AppGetLastError(app);
      if (last_error && last_error[0] != '\0') {
        strncpy(out_error, last_error, out_error_size - 1);
        out_error[out_error_size - 1] = '\0';
      } else {
        snprintf(out_error, out_error_size,
                 "Scan profile preflight failed: %s", AppStatusToString(status));
      }
    }
    return false;
  }

  return true;
}
