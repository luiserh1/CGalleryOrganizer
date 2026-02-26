#include <stdlib.h>
#include <string.h>

#include "organizer.h"

#include "app/app_internal.h"

AppContext *AppContextCreate(const AppRuntimeOptions *options) {
  AppContext *ctx = calloc(1, sizeof(AppContext));
  if (!ctx) {
    return NULL;
  }

  const char *env_models = getenv("CGO_MODELS_ROOT");
  if (options && options->models_root && options->models_root[0] != '\0') {
    strncpy(ctx->models_root, options->models_root, sizeof(ctx->models_root) - 1);
  } else if (env_models && env_models[0] != '\0') {
    strncpy(ctx->models_root, env_models, sizeof(ctx->models_root) - 1);
  } else {
    strncpy(ctx->models_root, "build/models", sizeof(ctx->models_root) - 1);
  }

  AppClearError(ctx);
  return ctx;
}

void AppContextDestroy(AppContext *ctx) {
  if (!ctx) {
    return;
  }

  AppReleaseMl(ctx);
  AppReleaseCache(ctx);
  free(ctx);
}

const char *AppGetLastError(const AppContext *ctx) {
  if (!ctx || ctx->last_error[0] == '\0') {
    return NULL;
  }
  return ctx->last_error;
}

const char *AppStatusToString(AppStatus status) {
  switch (status) {
  case APP_STATUS_OK:
    return "ok";
  case APP_STATUS_INVALID_ARGUMENT:
    return "invalid_argument";
  case APP_STATUS_IO_ERROR:
    return "io_error";
  case APP_STATUS_CACHE_ERROR:
    return "cache_error";
  case APP_STATUS_ML_ERROR:
    return "ml_error";
  case APP_STATUS_CANCELLED:
    return "cancelled";
  case APP_STATUS_INTERNAL_ERROR:
    return "internal_error";
  default:
    return "unknown";
  }
}

AppStatus AppRollback(AppContext *ctx, const AppRollbackRequest *request,
                      AppRollbackResult *out_result) {
  if (!ctx || !request || !out_result || !request->env_dir ||
      request->env_dir[0] == '\0') {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  int restored = OrganizerRollback(request->env_dir);
  if (restored < 0) {
    AppSetError(ctx, "rollback failed for '%s'", request->env_dir);
    return APP_STATUS_IO_ERROR;
  }

  out_result->restored_count = restored;
  return APP_STATUS_OK;
}
