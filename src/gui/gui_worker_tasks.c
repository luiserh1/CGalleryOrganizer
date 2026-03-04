#include <string.h>

#include "gui/gui_worker_internal.h"
#include "gui/gui_worker_tasks_internal.h"

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

  if (GuiWorkerRunCoreTask(&input)) {
    return NULL;
  }
  if (GuiWorkerRunRenameOpsTask(&input)) {
    return NULL;
  }
  if (GuiWorkerRunRenameHistoryTask(&input)) {
    return NULL;
  }

  GuiWorkerSetResult(APP_STATUS_INVALID_ARGUMENT, "Unknown task type", false);
  return NULL;
}
