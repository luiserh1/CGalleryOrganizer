#ifndef APP_MODELS_INTERNAL_H
#define APP_MODELS_INTERNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "app/app_internal.h"

#define APP_MODELS_DEFAULT_MANIFEST "models/manifest.json"
#define APP_MODELS_LOCKFILE ".installed.json"

typedef struct {
  char id[64];
  char task[32];
  char url[4096];
  char sha256[65];
  char license_name[128];
  char license_url[512];
  char author[128];
  char source_url[512];
  char credit_text[512];
  char version[64];
  char filename[256];
} AppModelManifestEntry;

typedef struct {
  AppModelManifestEntry *entries;
  int count;
} AppModelManifest;

bool AppLoadModelManifest(AppContext *ctx, const char *manifest_path,
                          AppModelManifest *out_manifest);
void AppFreeModelManifest(AppModelManifest *manifest);

#endif // APP_MODELS_INTERNAL_H
