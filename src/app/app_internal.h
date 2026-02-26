#ifndef APP_INTERNAL_H
#define APP_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "app_api.h"
#include "cli/cli_scan_pipeline.h"
#include "gallery_cache.h"
#include "similarity_engine.h"

#define APP_MODEL_ID_CLASSIFICATION "clf-default"
#define APP_MODEL_ID_TEXT_DETECTION "text-default"
#define APP_MODEL_ID_EMBEDDING "embed-default"

#define APP_MODEL_FILE_CLASSIFICATION "clf-default.onnx"
#define APP_MODEL_FILE_TEXT_DETECTION "text-default.onnx"
#define APP_MODEL_FILE_EMBEDDING "embed-default.onnx"

struct AppContext {
  char models_root[APP_MAX_PATH];
  bool cache_initialized;
  bool ml_initialized;
  char active_cache_path[APP_MAX_PATH];
  char last_error[APP_MAX_ERROR];
};

typedef struct {
  const char **keys;
  int count;
  char *owned_buffer;
} AppGroupKeyList;

void AppSetError(AppContext *ctx, const char *fmt, ...);
void AppClearError(AppContext *ctx);

void AppEmitProgress(const AppOperationHooks *hooks, const char *stage,
                     int current, int total, const char *message);
bool AppShouldCancel(const AppOperationHooks *hooks);

bool AppBuildCachePaths(const char *env_dir, char *out_cache_dir,
                        size_t cache_dir_size, char *out_cache_path,
                        size_t cache_path_size);
bool AppBuildCacheProfilePath(const char *env_dir, char *out_profile_path,
                              size_t profile_path_size);

AppStatus AppEnsureCacheReady(AppContext *ctx, const char *env_dir,
                              AppCacheCompressionMode mode, int level,
                              bool create_cache_dir);

AppStatus AppEnsureMlReady(AppContext *ctx, const char *models_root_override);

void AppReleaseCache(AppContext *ctx);
void AppReleaseMl(AppContext *ctx);

CacheCompressionMode AppMapCompressionMode(AppCacheCompressionMode mode);
SimilarityMemoryMode AppMapSimilarityMemoryMode(AppSimilarityMemoryMode mode);

bool AppParseGroupByKeys(const char *group_by_arg, const char **out_keys,
                         int max_keys, int *out_count,
                         char **out_owned_buffer, char *out_error,
                         size_t out_error_size);

void AppGroupKeyListFree(AppGroupKeyList *list);

#endif // APP_INTERNAL_H
