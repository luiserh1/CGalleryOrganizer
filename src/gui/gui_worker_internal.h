#ifndef GUI_WORKER_INTERNAL_H
#define GUI_WORKER_INTERNAL_H

#include <pthread.h>
#include <stdbool.h>

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

extern GuiWorkerState g_worker;

AppOperationHooks GuiWorkerBuildHooks(void);
void GuiWorkerSetResult(AppStatus status, const char *fallback_message,
                        bool success);
void GuiWorkerAppendDetail(const char *line);
void *GuiWorkerThreadMain(void *unused);

#endif // GUI_WORKER_INTERNAL_H
