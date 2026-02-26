#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "organizer.h"

#include "app/app_internal.h"

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} TextBuffer;

static bool TextBufferEnsure(TextBuffer *buf, size_t need) {
  if (!buf) {
    return false;
  }
  if (buf->len + need + 1 <= buf->cap) {
    return true;
  }

  size_t next = buf->cap == 0 ? 256 : buf->cap;
  while (next < buf->len + need + 1) {
    next *= 2;
  }

  char *resized = realloc(buf->data, next);
  if (!resized) {
    return false;
  }
  buf->data = resized;
  buf->cap = next;
  return true;
}

static bool TextBufferAppendf(TextBuffer *buf, const char *fmt, ...) {
  if (!buf || !fmt) {
    return false;
  }

  va_list args;
  va_start(args, fmt);
  va_list copy;
  va_copy(copy, args);
  int needed = vsnprintf(NULL, 0, fmt, copy);
  va_end(copy);
  if (needed < 0) {
    va_end(args);
    return false;
  }

  if (!TextBufferEnsure(buf, (size_t)needed)) {
    va_end(args);
    return false;
  }

  vsnprintf(buf->data + buf->len, buf->cap - buf->len, fmt, args);
  buf->len += (size_t)needed;
  va_end(args);
  return true;
}

static int CompareStrings(const void *a, const void *b) {
  const char *const *left = (const char *const *)a;
  const char *const *right = (const char *const *)b;
  return strcmp(*left, *right);
}

static char *BuildPlanText(const OrganizerPlan *plan) {
  if (!plan || plan->count == 0) {
    return strdup("Plan is empty.\n");
  }

  char **dirs = calloc((size_t)plan->count, sizeof(char *));
  if (!dirs) {
    return NULL;
  }

  for (int i = 0; i < plan->count; i++) {
    const char *new_path = plan->moves[i].new_path;
    const char *slash = strrchr(new_path, '/');
    size_t dir_len = slash ? (size_t)(slash - new_path) : strlen(new_path);
    dirs[i] = calloc(dir_len + 1, 1);
    if (!dirs[i]) {
      for (int j = 0; j < i; j++) {
        free(dirs[j]);
      }
      free(dirs);
      return NULL;
    }
    memcpy(dirs[i], new_path, dir_len);
    dirs[i][dir_len] = '\0';
  }

  qsort(dirs, (size_t)plan->count, sizeof(char *), CompareStrings);

  TextBuffer out = {0};
  if (!TextBufferAppendf(&out, "Target Environment Tree Preview:\n")) {
    goto fail;
  }

  const char *current_dir = dirs[0];
  int current_count = 1;

  for (int i = 1; i < plan->count; i++) {
    if (strcmp(dirs[i], current_dir) == 0) {
      current_count++;
      continue;
    }

    const char *rel_start = strstr(current_dir, "/_");
    const char *display = rel_start ? rel_start + 1 : current_dir;
    if (!TextBufferAppendf(&out, "  - %-40s [%d files]\n", display,
                           current_count)) {
      goto fail;
    }

    current_dir = dirs[i];
    current_count = 1;
  }

  {
    const char *rel_start = strstr(current_dir, "/_");
    const char *display = rel_start ? rel_start + 1 : current_dir;
    if (!TextBufferAppendf(&out, "  - %-40s [%d files]\n", display,
                           current_count)) {
      goto fail;
    }
  }

  for (int i = 0; i < plan->count; i++) {
    free(dirs[i]);
  }
  free(dirs);

  if (!out.data) {
    return strdup("Plan is empty.\n");
  }

  return out.data;

fail:
  for (int i = 0; i < plan->count; i++) {
    free(dirs[i]);
  }
  free(dirs);
  free(out.data);
  return NULL;
}

static AppStatus BuildPlanInternal(AppContext *ctx, const char *env_dir,
                                   const char *group_by_arg,
                                   AppOrganizePlanResult *out_result,
                                   OrganizerPlan **out_plan) {
  const char *group_keys[16] = {0};
  int group_key_count = 0;
  char *group_keys_owned = NULL;
  char error[APP_MAX_ERROR] = {0};

  if (!AppParseGroupByKeys(group_by_arg, group_keys, 16, &group_key_count,
                           &group_keys_owned, error, sizeof(error))) {
    AppSetError(ctx, "%s", error[0] != '\0' ? error : "invalid --group-by");
    free(group_keys_owned);
    return APP_STATUS_INVALID_ARGUMENT;
  }

  OrganizerPlan *plan = OrganizerComputePlan(env_dir, group_keys, group_key_count);
  free(group_keys_owned);
  if (!plan) {
    AppSetError(ctx, "failed to compute organization plan");
    return APP_STATUS_INTERNAL_ERROR;
  }

  char *plan_text = BuildPlanText(plan);
  if (!plan_text) {
    OrganizerFreePlan(plan);
    AppSetError(ctx, "failed to render organization plan");
    return APP_STATUS_INTERNAL_ERROR;
  }

  if (out_result) {
    out_result->plan_text = plan_text;
    out_result->planned_moves = plan->count;
  } else {
    free(plan_text);
  }

  if (out_plan) {
    *out_plan = plan;
  } else {
    OrganizerFreePlan(plan);
  }

  return APP_STATUS_OK;
}

AppStatus AppPreviewOrganize(AppContext *ctx,
                             const AppOrganizePlanRequest *request,
                             AppOrganizePlanResult *out_result) {
  if (!ctx || !request || !out_result || !request->env_dir ||
      request->env_dir[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  AppStatus status =
      AppEnsureCacheReady(ctx, request->env_dir, request->cache_compression_mode,
                          request->cache_compression_level, true);
  if (status != APP_STATUS_OK) {
    return status;
  }

  if (AppShouldCancel(&request->hooks)) {
    AppSetError(ctx, "organize preview cancelled");
    return APP_STATUS_CANCELLED;
  }

  AppEmitProgress(&request->hooks, "organize_preview", 0, 1,
                  "Computing organization plan");

  status = BuildPlanInternal(ctx, request->env_dir, request->group_by_arg,
                             out_result, NULL);
  if (status != APP_STATUS_OK) {
    return status;
  }

  AppEmitProgress(&request->hooks, "organize_preview", 1, 1,
                  "Organization preview ready");
  return APP_STATUS_OK;
}

AppStatus AppExecuteOrganize(AppContext *ctx,
                             const AppOrganizeExecuteRequest *request,
                             AppOrganizeExecuteResult *out_result) {
  if (!ctx || !request || !out_result || !request->env_dir ||
      request->env_dir[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  AppStatus status =
      AppEnsureCacheReady(ctx, request->env_dir, request->cache_compression_mode,
                          request->cache_compression_level, true);
  if (status != APP_STATUS_OK) {
    return status;
  }

  if (AppShouldCancel(&request->hooks)) {
    AppSetError(ctx, "organize execution cancelled");
    return APP_STATUS_CANCELLED;
  }

  if (!OrganizerInit(request->env_dir)) {
    AppSetError(ctx, "failed to initialize organizer");
    return APP_STATUS_INTERNAL_ERROR;
  }

  AppOrganizePlanResult preview = {0};
  OrganizerPlan *plan = NULL;
  status = BuildPlanInternal(ctx, request->env_dir, request->group_by_arg,
                             &preview, &plan);
  if (status != APP_STATUS_OK) {
    OrganizerShutdown();
    return status;
  }

  out_result->planned_moves = preview.planned_moves;
  free(preview.plan_text);

  AppEmitProgress(&request->hooks, "organize_execute", 0, 1,
                  "Executing organization plan");

  bool ok = OrganizerExecutePlan(plan);
  OrganizerFreePlan(plan);
  OrganizerShutdown();

  if (!ok) {
    AppSetError(ctx, "organize execution failed");
    return APP_STATUS_INTERNAL_ERROR;
  }

  out_result->executed_moves = out_result->planned_moves;
  AppEmitProgress(&request->hooks, "organize_execute", 1, 1,
                  "Organization execution completed");

  return APP_STATUS_OK;
}

void AppFreeOrganizePlanResult(AppOrganizePlanResult *result) {
  if (!result) {
    return;
  }
  free(result->plan_text);
  result->plan_text = NULL;
  result->planned_moves = 0;
}
