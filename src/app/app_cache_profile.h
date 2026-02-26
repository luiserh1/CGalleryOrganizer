#ifndef APP_CACHE_PROFILE_H
#define APP_CACHE_PROFILE_H

#include <stdbool.h>
#include <stddef.h>

#include "app_api.h"

typedef struct {
  char source_root_abs[APP_MAX_PATH];
  bool exhaustive;
  bool ml_enrich_requested;
  bool similarity_prep_requested;
  char models_fingerprint[65];
  bool has_cache_entry_count;
  int cache_entry_count;
} AppCacheProfile;

typedef enum {
  APP_CACHE_PROFILE_LOAD_OK = 0,
  APP_CACHE_PROFILE_LOAD_MISSING = 1,
  APP_CACHE_PROFILE_LOAD_MALFORMED = 2
} AppCacheProfileLoadStatus;

bool AppBuildRequestedCacheProfile(AppContext *ctx, const AppScanRequest *request,
                                   AppCacheProfile *out_profile,
                                   char *out_error, size_t out_error_size);

AppCacheProfileLoadStatus AppLoadCacheProfile(const char *profile_path,
                                              AppCacheProfile *out_profile);

bool AppSaveCacheProfile(const char *profile_path,
                         const AppCacheProfile *profile);

bool AppCompareCacheProfiles(const AppCacheProfile *expected,
                             const AppCacheProfile *actual, char *out_reason,
                             size_t out_reason_size);

bool AppLoadCacheProfileEntryCount(const char *profile_path,
                                   int *out_entry_count);

#endif // APP_CACHE_PROFILE_H
