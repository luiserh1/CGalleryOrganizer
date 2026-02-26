#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "fs_utils.h"
#include "hash_utils.h"
#include "sha256.h"

#include "app/app_cache_profile.h"
#include "app/app_internal.h"

static void HexEncode32(const unsigned char digest[32], char out_hex[65]) {
  static const char HEX[] = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    out_hex[i * 2] = HEX[(digest[i] >> 4) & 0x0F];
    out_hex[i * 2 + 1] = HEX[digest[i] & 0x0F];
  }
  out_hex[64] = '\0';
}

static const char *ResolveModelsRoot(const AppContext *ctx,
                                     const AppScanRequest *request) {
  if (request && request->models_root_override &&
      request->models_root_override[0] != '\0') {
    return request->models_root_override;
  }
  if (ctx && ctx->models_root[0] != '\0') {
    return ctx->models_root;
  }
  return "build/models";
}

static bool FingerprintOneModel(SHA256_CTX *ctx, const char *models_root,
                                const char *model_id,
                                const char *model_filename,
                                char *out_error, size_t out_error_size) {
  if (!ctx || !models_root || !model_id || !model_filename) {
    return false;
  }

  char model_path[APP_MAX_PATH] = {0};
  snprintf(model_path, sizeof(model_path), "%s/%s", models_root, model_filename);

  char file_sha[SHA256_HASH_LEN + 1] = {0};
  if (!ComputeFileSha256(model_path, file_sha)) {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size,
               "failed to hash model '%s' at '%s'", model_id, model_path);
    }
    return false;
  }

  SHA256_Update(ctx, (const unsigned char *)model_id, strlen(model_id));
  SHA256_Update(ctx, (const unsigned char *)":", 1);
  SHA256_Update(ctx, (const unsigned char *)file_sha, strlen(file_sha));
  SHA256_Update(ctx, (const unsigned char *)"\n", 1);
  return true;
}

bool AppBuildRequestedCacheProfile(AppContext *ctx, const AppScanRequest *request,
                                   AppCacheProfile *out_profile,
                                   char *out_error, size_t out_error_size) {
  if (!ctx || !request || !out_profile) {
    return false;
  }

  if (out_error && out_error_size > 0) {
    out_error[0] = '\0';
  }

  memset(out_profile, 0, sizeof(*out_profile));
  out_profile->exhaustive = request->exhaustive;
  out_profile->ml_enrich_requested = request->ml_enrich;
  out_profile->similarity_prep_requested = request->similarity_report;

  if (!FsGetAbsolutePath(request->target_dir, out_profile->source_root_abs,
                         sizeof(out_profile->source_root_abs))) {
    if (out_error && out_error_size > 0) {
      snprintf(out_error, out_error_size, "failed to resolve absolute path for '%s'",
               request->target_dir ? request->target_dir : "<null>");
    }
    return false;
  }

  if (!out_profile->ml_enrich_requested &&
      !out_profile->similarity_prep_requested) {
    out_profile->models_fingerprint[0] = '\0';
    return true;
  }

  const char *models_root = ResolveModelsRoot(ctx, request);
  SHA256_CTX fingerprint_ctx;
  SHA256_Init(&fingerprint_ctx);

  if (out_profile->ml_enrich_requested) {
    if (!FingerprintOneModel(&fingerprint_ctx, models_root,
                             APP_MODEL_ID_CLASSIFICATION,
                             APP_MODEL_FILE_CLASSIFICATION, out_error,
                             out_error_size)) {
      return false;
    }
    if (!FingerprintOneModel(&fingerprint_ctx, models_root,
                             APP_MODEL_ID_TEXT_DETECTION,
                             APP_MODEL_FILE_TEXT_DETECTION, out_error,
                             out_error_size)) {
      return false;
    }
  }

  if (out_profile->similarity_prep_requested) {
    if (!FingerprintOneModel(&fingerprint_ctx, models_root,
                             APP_MODEL_ID_EMBEDDING, APP_MODEL_FILE_EMBEDDING,
                             out_error, out_error_size)) {
      return false;
    }
  }

  unsigned char digest[32] = {0};
  SHA256_Final(digest, &fingerprint_ctx);
  HexEncode32(digest, out_profile->models_fingerprint);
  return true;
}

static bool LoadJsonFile(const char *path, char **out_json) {
  if (!path || !out_json) {
    return false;
  }

  *out_json = NULL;
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long file_len = ftell(f);
  if (file_len < 0) {
    fclose(f);
    return false;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }

  char *buffer = malloc((size_t)file_len + 1);
  if (!buffer) {
    fclose(f);
    return false;
  }

  size_t bytes_read = fread(buffer, 1, (size_t)file_len, f);
  fclose(f);
  if (bytes_read != (size_t)file_len) {
    free(buffer);
    return false;
  }
  buffer[file_len] = '\0';
  *out_json = buffer;
  return true;
}

static bool ParseOptionalCacheEntryCount(cJSON *root, int *out_count) {
  if (!root || !out_count) {
    return false;
  }

  cJSON *count_node = cJSON_GetObjectItem(root, "cacheEntryCount");
  if (!count_node) {
    return false;
  }
  if (!cJSON_IsNumber(count_node)) {
    return false;
  }

  double raw = count_node->valuedouble;
  if (raw < 0.0) {
    return false;
  }

  int parsed = (int)raw;
  if ((double)parsed != raw) {
    return false;
  }

  *out_count = parsed;
  return true;
}

AppCacheProfileLoadStatus AppLoadCacheProfile(const char *profile_path,
                                              AppCacheProfile *out_profile) {
  if (!profile_path || !out_profile) {
    return APP_CACHE_PROFILE_LOAD_MALFORMED;
  }

  memset(out_profile, 0, sizeof(*out_profile));

  char *json_text = NULL;
  if (!LoadJsonFile(profile_path, &json_text)) {
    return APP_CACHE_PROFILE_LOAD_MISSING;
  }

  cJSON *root = cJSON_Parse(json_text);
  free(json_text);
  if (!root || !cJSON_IsObject(root)) {
    cJSON_Delete(root);
    return APP_CACHE_PROFILE_LOAD_MALFORMED;
  }

  cJSON *schema_version = cJSON_GetObjectItem(root, "schemaVersion");
  cJSON *source_root = cJSON_GetObjectItem(root, "sourceRootAbs");
  cJSON *exhaustive = cJSON_GetObjectItem(root, "exhaustive");
  cJSON *ml_enrich = cJSON_GetObjectItem(root, "mlEnrichRequested");
  cJSON *similarity = cJSON_GetObjectItem(root, "similarityPrepRequested");
  cJSON *fingerprint = cJSON_GetObjectItem(root, "modelsFingerprint");

  bool valid = cJSON_IsNumber(schema_version) &&
               (int)schema_version->valuedouble == 1 &&
               cJSON_IsString(source_root) && cJSON_IsBool(exhaustive) &&
               cJSON_IsBool(ml_enrich) && cJSON_IsBool(similarity) &&
               cJSON_IsString(fingerprint);

  if (!valid) {
    cJSON_Delete(root);
    return APP_CACHE_PROFILE_LOAD_MALFORMED;
  }

  strncpy(out_profile->source_root_abs, source_root->valuestring,
          sizeof(out_profile->source_root_abs) - 1);
  out_profile->exhaustive = cJSON_IsTrue(exhaustive);
  out_profile->ml_enrich_requested = cJSON_IsTrue(ml_enrich);
  out_profile->similarity_prep_requested = cJSON_IsTrue(similarity);
  strncpy(out_profile->models_fingerprint, fingerprint->valuestring,
          sizeof(out_profile->models_fingerprint) - 1);

  int cached_entry_count = 0;
  if (ParseOptionalCacheEntryCount(root, &cached_entry_count)) {
    out_profile->has_cache_entry_count = true;
    out_profile->cache_entry_count = cached_entry_count;
  }

  cJSON_Delete(root);
  return APP_CACHE_PROFILE_LOAD_OK;
}

static bool BuildIsoUtcNow(char *out_ts, size_t out_size) {
  if (!out_ts || out_size == 0) {
    return false;
  }

  time_t now = time(NULL);
  struct tm utc_now = {0};
#if defined(_POSIX_VERSION)
  gmtime_r(&now, &utc_now);
#else
  struct tm *tmp = gmtime(&now);
  if (!tmp) {
    return false;
  }
  utc_now = *tmp;
#endif
  return strftime(out_ts, out_size, "%Y-%m-%dT%H:%M:%SZ", &utc_now) > 0;
}

bool AppSaveCacheProfile(const char *profile_path,
                         const AppCacheProfile *profile) {
  if (!profile_path || !profile) {
    return false;
  }

  cJSON *root = cJSON_CreateObject();
  if (!root) {
    return false;
  }

  char timestamp[64] = {0};
  if (!BuildIsoUtcNow(timestamp, sizeof(timestamp))) {
    cJSON_Delete(root);
    return false;
  }

  cJSON_AddNumberToObject(root, "schemaVersion", 1);
  cJSON_AddStringToObject(root, "sourceRootAbs", profile->source_root_abs);
  cJSON_AddBoolToObject(root, "exhaustive", profile->exhaustive);
  cJSON_AddBoolToObject(root, "mlEnrichRequested", profile->ml_enrich_requested);
  cJSON_AddBoolToObject(root, "similarityPrepRequested",
                        profile->similarity_prep_requested);
  cJSON_AddStringToObject(root, "modelsFingerprint", profile->models_fingerprint);
  if (profile->has_cache_entry_count && profile->cache_entry_count >= 0) {
    cJSON_AddNumberToObject(root, "cacheEntryCount", profile->cache_entry_count);
  }
  cJSON_AddStringToObject(root, "updatedAtUtc", timestamp);

  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (!json) {
    return false;
  }

  FILE *f = fopen(profile_path, "wb");
  if (!f) {
    free(json);
    return false;
  }

  size_t len = strlen(json);
  bool ok = fwrite(json, 1, len, f) == len;
  fclose(f);
  free(json);
  return ok;
}

bool AppCompareCacheProfiles(const AppCacheProfile *expected,
                             const AppCacheProfile *actual, char *out_reason,
                             size_t out_reason_size) {
  if (!expected || !actual) {
    return false;
  }

  if (out_reason && out_reason_size > 0) {
    out_reason[0] = '\0';
  }

  if (strcmp(expected->source_root_abs, actual->source_root_abs) != 0) {
    if (out_reason && out_reason_size > 0) {
      snprintf(out_reason, out_reason_size,
               "source directory changed from '%s' to '%s'",
               actual->source_root_abs, expected->source_root_abs);
    }
    return false;
  }

  if (expected->exhaustive != actual->exhaustive) {
    if (out_reason && out_reason_size > 0) {
      snprintf(out_reason, out_reason_size,
               "exhaustive setting changed");
    }
    return false;
  }

  if (expected->ml_enrich_requested != actual->ml_enrich_requested) {
    if (out_reason && out_reason_size > 0) {
      snprintf(out_reason, out_reason_size,
               "ml enrich setting changed");
    }
    return false;
  }

  if (expected->similarity_prep_requested != actual->similarity_prep_requested) {
    if (out_reason && out_reason_size > 0) {
      snprintf(out_reason, out_reason_size,
               "similarity preparation setting changed");
    }
    return false;
  }

  if (strcmp(expected->models_fingerprint, actual->models_fingerprint) != 0) {
    if (out_reason && out_reason_size > 0) {
      snprintf(out_reason, out_reason_size,
               "model fingerprint changed");
    }
    return false;
  }

  if (out_reason && out_reason_size > 0) {
    snprintf(out_reason, out_reason_size, "cache profile matched");
  }
  return true;
}

bool AppLoadCacheProfileEntryCount(const char *profile_path,
                                   int *out_entry_count) {
  if (!profile_path || !out_entry_count) {
    return false;
  }

  AppCacheProfile profile = {0};
  AppCacheProfileLoadStatus load_status =
      AppLoadCacheProfile(profile_path, &profile);
  if (load_status != APP_CACHE_PROFILE_LOAD_OK ||
      !profile.has_cache_entry_count) {
    return false;
  }

  *out_entry_count = profile.cache_entry_count;
  return true;
}
