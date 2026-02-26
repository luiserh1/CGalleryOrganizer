#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cJSON.h"
#include "fs_utils.h"
#include "sha256.h"

#include "app/app_models_internal.h"

static void ResolveModelsRoot(const AppContext *ctx, const char *override_root,
                              char *out_root, size_t out_size) {
  if (!out_root || out_size == 0) {
    return;
  }

  out_root[0] = '\0';
  if (override_root && override_root[0] != '\0') {
    strncpy(out_root, override_root, out_size - 1);
    out_root[out_size - 1] = '\0';
    return;
  }

  if (ctx && ctx->models_root[0] != '\0') {
    strncpy(out_root, ctx->models_root, out_size - 1);
    out_root[out_size - 1] = '\0';
    return;
  }

  strncpy(out_root, "build/models", out_size - 1);
  out_root[out_size - 1] = '\0';
}

static int Base64Value(unsigned char c) {
  if (c >= 'A' && c <= 'Z') {
    return (int)(c - 'A');
  }
  if (c >= 'a' && c <= 'z') {
    return (int)(c - 'a') + 26;
  }
  if (c >= '0' && c <= '9') {
    return (int)(c - '0') + 52;
  }
  if (c == '+') {
    return 62;
  }
  if (c == '/') {
    return 63;
  }
  return -1;
}

static bool DecodeBase64(const char *encoded, unsigned char **out_bytes,
                         size_t *out_len) {
  if (!encoded || !out_bytes || !out_len) {
    return false;
  }

  size_t len = strlen(encoded);
  if (len == 0 || (len % 4) != 0) {
    return false;
  }

  size_t padding = 0;
  if (len >= 1 && encoded[len - 1] == '=') {
    padding++;
  }
  if (len >= 2 && encoded[len - 2] == '=') {
    padding++;
  }

  size_t decoded_len = (len / 4) * 3 - padding;
  unsigned char *decoded = malloc(decoded_len > 0 ? decoded_len : 1);
  if (!decoded) {
    return false;
  }

  size_t out_index = 0;
  for (size_t i = 0; i < len; i += 4) {
    int v0 = Base64Value((unsigned char)encoded[i]);
    int v1 = Base64Value((unsigned char)encoded[i + 1]);
    int v2 = encoded[i + 2] == '=' ? -2 : Base64Value((unsigned char)encoded[i + 2]);
    int v3 = encoded[i + 3] == '=' ? -2 : Base64Value((unsigned char)encoded[i + 3]);

    if (v0 < 0 || v1 < 0 || v2 == -1 || v3 == -1) {
      free(decoded);
      return false;
    }

    decoded[out_index++] = (unsigned char)((v0 << 2) | (v1 >> 4));
    if (v2 >= 0) {
      decoded[out_index++] = (unsigned char)(((v1 & 0x0F) << 4) | (v2 >> 2));
      if (v3 >= 0) {
        decoded[out_index++] = (unsigned char)(((v2 & 0x03) << 6) | v3);
      }
    }
  }

  *out_bytes = decoded;
  *out_len = decoded_len;
  return true;
}

static bool WriteBytesToFile(const char *path, const unsigned char *bytes,
                             size_t len) {
  if (!path || !bytes) {
    return false;
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }

  if (len > 0 && fwrite(bytes, 1, len, f) != len) {
    fclose(f);
    return false;
  }
  fclose(f);
  return true;
}

static bool DownloadModelFromDataUrl(AppContext *ctx, const char *url,
                                     const char *out_path) {
  const char *prefix = "data:application/octet-stream;base64,";
  size_t prefix_len = strlen(prefix);
  if (!url || strncmp(url, prefix, prefix_len) != 0) {
    return false;
  }

  const char *payload = url + prefix_len;
  unsigned char *decoded = NULL;
  size_t decoded_len = 0;
  if (!DecodeBase64(payload, &decoded, &decoded_len)) {
    AppSetError(ctx, "failed to decode base64 model payload");
    return false;
  }

  bool ok = WriteBytesToFile(out_path, decoded, decoded_len);
  free(decoded);
  if (!ok) {
    AppSetError(ctx, "failed to write model file '%s'", out_path);
  }
  return ok;
}

static bool ComputeFileSha256Hex(const char *path, char out_hex[65]) {
  if (!path || !out_hex) {
    return false;
  }

  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  SHA256_CTX ctx;
  SHA256_Init(&ctx);

  unsigned char buffer[4096];
  while (true) {
    size_t read_bytes = fread(buffer, 1, sizeof(buffer), f);
    if (read_bytes > 0) {
      SHA256_Update(&ctx, buffer, read_bytes);
    }
    if (read_bytes < sizeof(buffer)) {
      if (ferror(f)) {
        fclose(f);
        return false;
      }
      break;
    }
  }
  fclose(f);

  unsigned char digest[32];
  SHA256_Final(digest, &ctx);
  static const char *HEX = "0123456789abcdef";
  for (int i = 0; i < 32; i++) {
    out_hex[i * 2] = HEX[(digest[i] >> 4) & 0xF];
    out_hex[i * 2 + 1] = HEX[digest[i] & 0xF];
  }
  out_hex[64] = '\0';
  return true;
}

static bool ShaEqualIgnoreCase(const char *left, const char *right) {
  if (!left || !right) {
    return false;
  }

  for (int i = 0; i < 64; i++) {
    if (tolower((unsigned char)left[i]) != tolower((unsigned char)right[i])) {
      return false;
    }
    if (left[i] == '\0' || right[i] == '\0') {
      return false;
    }
  }
  return left[64] == '\0' && right[64] == '\0';
}

static bool WriteInstalledLockfile(AppContext *ctx, const AppModelManifest *manifest,
                                   const char *lockfile_path) {
  if (!ctx || !manifest || !lockfile_path) {
    return false;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON *models = cJSON_CreateArray();
  if (!root || !models) {
    cJSON_Delete(root);
    cJSON_Delete(models);
    AppSetError(ctx, "out of memory while creating model lockfile JSON");
    return false;
  }

  time_t now = time(NULL);
  struct tm utc_now = {0};
#if defined(_POSIX_VERSION)
  gmtime_r(&now, &utc_now);
#else
  struct tm *tmp = gmtime(&now);
  if (tmp) {
    utc_now = *tmp;
  }
#endif
  char ts[64] = {0};
  strftime(ts, sizeof(ts), "%Y-%m-%dT%H:%M:%SZ", &utc_now);
  cJSON_AddStringToObject(root, "installed_at", ts);
  cJSON_AddItemToObject(root, "models", models);

  for (int i = 0; i < manifest->count; i++) {
    const AppModelManifestEntry *entry = &manifest->entries[i];
    cJSON *obj = cJSON_CreateObject();
    if (!obj) {
      cJSON_Delete(root);
      AppSetError(ctx, "out of memory while creating model lockfile entries");
      return false;
    }
    cJSON_AddStringToObject(obj, "id", entry->id);
    cJSON_AddStringToObject(obj, "task", entry->task);
    cJSON_AddStringToObject(obj, "filename", entry->filename);
    cJSON_AddStringToObject(obj, "sha256", entry->sha256);
    cJSON_AddStringToObject(obj, "license_name", entry->license_name);
    cJSON_AddStringToObject(obj, "version", entry->version);
    cJSON_AddItemToArray(models, obj);
  }

  char *json = cJSON_Print(root);
  cJSON_Delete(root);
  if (!json) {
    AppSetError(ctx, "failed to serialize lockfile JSON");
    return false;
  }

  FILE *f = fopen(lockfile_path, "wb");
  if (!f) {
    free(json);
    AppSetError(ctx, "failed to open lockfile '%s'", lockfile_path);
    return false;
  }
  size_t json_len = strlen(json);
  bool ok = fwrite(json, 1, json_len, f) == json_len;
  fclose(f);
  free(json);
  if (!ok) {
    AppSetError(ctx, "failed to write lockfile '%s'", lockfile_path);
    return false;
  }
  return true;
}

static bool FileExistsRegular(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  fclose(f);
  return true;
}

static AppStatus InstallOneModel(AppContext *ctx, const AppModelManifestEntry *entry,
                                 const char *models_root, bool force_redownload,
                                 bool *out_installed) {
  if (!ctx || !entry || !models_root || !out_installed) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  *out_installed = false;

  char target_path[APP_MAX_PATH] = {0};
  snprintf(target_path, sizeof(target_path), "%s/%s", models_root,
           entry->filename);

  if (!force_redownload && FileExistsRegular(target_path)) {
    char existing_sha[65] = {0};
    if (ComputeFileSha256Hex(target_path, existing_sha) &&
        ShaEqualIgnoreCase(existing_sha, entry->sha256)) {
      return APP_STATUS_OK;
    }
  }

  if (!DownloadModelFromDataUrl(ctx, entry->url, target_path)) {
    if (AppGetLastError(ctx) == NULL && entry->url[0] != '\0') {
      AppSetError(ctx,
                  "unsupported model URL for '%s'. Only data URLs are supported "
                  "by this build",
                  entry->id);
    }
    return APP_STATUS_IO_ERROR;
  }

  char actual_sha[65] = {0};
  if (!ComputeFileSha256Hex(target_path, actual_sha)) {
    AppSetError(ctx, "failed to compute sha256 for '%s'", target_path);
    remove(target_path);
    return APP_STATUS_IO_ERROR;
  }

  if (!ShaEqualIgnoreCase(actual_sha, entry->sha256)) {
    AppSetError(ctx,
                "SHA256 mismatch for model '%s': expected=%s actual=%s",
                entry->id, entry->sha256, actual_sha);
    remove(target_path);
    return APP_STATUS_IO_ERROR;
  }

  *out_installed = true;
  return APP_STATUS_OK;
}

AppStatus AppInstallModels(AppContext *ctx,
                           const AppModelInstallRequest *request,
                           AppModelInstallResult *out_result) {
  if (!ctx || !request || !out_result) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  AppClearError(ctx);
  memset(out_result, 0, sizeof(*out_result));

  char models_root[APP_MAX_PATH] = {0};
  ResolveModelsRoot(ctx, request->models_root_override, models_root,
                    sizeof(models_root));
  strncpy(out_result->models_root, models_root, sizeof(out_result->models_root) - 1);

  const char *manifest_path = request->manifest_path_override;
  if (!manifest_path || manifest_path[0] == '\0') {
    manifest_path = APP_MODELS_DEFAULT_MANIFEST;
  }

  if (!FsMakeDirRecursive(models_root)) {
    AppSetError(ctx, "failed to create models directory '%s'", models_root);
    return APP_STATUS_IO_ERROR;
  }

  AppModelManifest manifest = {0};
  if (!AppLoadModelManifest(ctx, manifest_path, &manifest)) {
    return APP_STATUS_INVALID_ARGUMENT;
  }

  out_result->manifest_model_count = manifest.count;

  AppEmitProgress(&request->hooks, "models_install", 0, manifest.count,
                  "Installing model artifacts");

  for (int i = 0; i < manifest.count; i++) {
    if (AppShouldCancel(&request->hooks)) {
      AppFreeModelManifest(&manifest);
      AppSetError(ctx, "model installation cancelled");
      return APP_STATUS_CANCELLED;
    }

    bool installed = false;
    AppStatus status = InstallOneModel(ctx, &manifest.entries[i], models_root,
                                       request->force_redownload, &installed);
    if (status != APP_STATUS_OK) {
      AppFreeModelManifest(&manifest);
      return status;
    }

    if (installed) {
      out_result->installed_count++;
    } else {
      out_result->skipped_count++;
    }

    AppEmitProgress(&request->hooks, "models_install", i + 1, manifest.count,
                    manifest.entries[i].id);
  }

  snprintf(out_result->lockfile_path, sizeof(out_result->lockfile_path), "%s/%s",
           models_root, APP_MODELS_LOCKFILE);
  if (!WriteInstalledLockfile(ctx, &manifest,
                              out_result->lockfile_path)) {
    AppFreeModelManifest(&manifest);
    return APP_STATUS_IO_ERROR;
  }

  AppFreeModelManifest(&manifest);
  AppReleaseMl(ctx);
  return APP_STATUS_OK;
}
