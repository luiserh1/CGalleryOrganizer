#ifndef GUI_WORKER_TASKS_INTERNAL_H
#define GUI_WORKER_TASKS_INTERNAL_H

#include <stdbool.h>

#include "gui/gui_worker.h"

bool GuiWorkerRunCoreTask(const GuiTaskInput *input);
bool GuiWorkerRunRenameOpsTask(const GuiTaskInput *input);
bool GuiWorkerRunRenameHistoryTask(const GuiTaskInput *input);

#endif // GUI_WORKER_TASKS_INTERNAL_H
