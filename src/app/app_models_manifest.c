#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cJSON.h"

#include "app/app_models_internal.h"

static bool ReadTextFile(const char *path, char **out_text) {
  if (!path || !out_text) {
    return false;
  }

  *out_text = NULL;

  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }

  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return false;
  }
  long len = ftell(f);
  if (len < 0) {
    fclose(f);
    return false;
  }
  if (fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return false;
  }

  char *buffer = calloc((size_t)len + 1, 1);
  if (!buffer) {
    fclose(f);
    return false;
  }

  if (len > 0) {
    size_t read_bytes = fread(buffer, 1, (size_t)len, f);
    if (read_bytes != (size_t)len) {
      free(buffer);
      fclose(f);
      return false;
    }
  }
  buffer[len] = '\0';
  fclose(f);

  *out_text = buffer;
  return true;
}

static bool IsSupportedTask(const char *task) {
  if (!task) {
    return false;
  }
  return strcmp(task, "classification") == 0 ||
         strcmp(task, "text_detection") == 0 ||
         strcmp(task, "embedding") == 0;
}

static bool CopyJsonString(cJSON *obj, const char *field, char *dst, size_t dst_size,
                           AppContext *ctx, int index) {
  if (!obj || !field || !dst || dst_size == 0 || !ctx) {
    return false;
  }

  cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, field);
  if (!cJSON_IsString(item) || !item->valuestring || item->valuestring[0] == '\0') {
    AppSetError(ctx, "manifest entry %d missing required field '%s'", index,
                field);
    return false;
  }

  strncpy(dst, item->valuestring, dst_size - 1);
  dst[dst_size - 1] = '\0';
  return true;
}

static bool ValidateSha256(const char *sha) {
  if (!sha || strlen(sha) != 64) {
    return false;
  }

  for (size_t i = 0; i < 64; i++) {
    char c = sha[i];
    bool is_hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F');
    if (!is_hex) {
      return false;
    }
  }
  return true;
}

bool AppLoadModelManifest(AppContext *ctx, const char *manifest_path,
                          AppModelManifest *out_manifest) {
  if (!ctx || !manifest_path || !out_manifest) {
    return false;
  }

  memset(out_manifest, 0, sizeof(*out_manifest));

  char *text = NULL;
  if (!ReadTextFile(manifest_path, &text)) {
    AppSetError(ctx, "failed to read manifest '%s'", manifest_path);
    return false;
  }

  cJSON *root = cJSON_Parse(text);
  free(text);
  if (!root) {
    AppSetError(ctx, "failed to parse manifest JSON '%s'", manifest_path);
    return false;
  }

  cJSON *models = cJSON_GetObjectItemCaseSensitive(root, "models");
  if (!cJSON_IsArray(models)) {
    cJSON_Delete(root);
    AppSetError(ctx, "manifest '%s' must contain a 'models' array",
                manifest_path);
    return false;
  }

  int count = cJSON_GetArraySize(models);
  if (count < 0) {
    cJSON_Delete(root);
    AppSetError(ctx, "invalid models array in '%s'", manifest_path);
    return false;
  }

  if (count == 0) {
    cJSON_Delete(root);
    return true;
  }

  out_manifest->entries =
      calloc((size_t)count, sizeof(AppModelManifestEntry));
  if (!out_manifest->entries) {
    cJSON_Delete(root);
    AppSetError(ctx, "out of memory while reading manifest '%s'", manifest_path);
    return false;
  }
  out_manifest->count = count;

  for (int i = 0; i < count; i++) {
    cJSON *entry = cJSON_GetArrayItem(models, i);
    if (!cJSON_IsObject(entry)) {
      AppSetError(ctx, "manifest entry %d is not an object", i);
      AppFreeModelManifest(out_manifest);
      cJSON_Delete(root);
      return false;
    }

    AppModelManifestEntry *out = &out_manifest->entries[i];
    if (!CopyJsonString(entry, "id", out->id, sizeof(out->id), ctx, i) ||
        !CopyJsonString(entry, "task", out->task, sizeof(out->task), ctx, i) ||
        !CopyJsonString(entry, "url", out->url, sizeof(out->url), ctx, i) ||
        !CopyJsonString(entry, "sha256", out->sha256, sizeof(out->sha256), ctx,
                        i) ||
        !CopyJsonString(entry, "license_name", out->license_name,
                        sizeof(out->license_name), ctx, i) ||
        !CopyJsonString(entry, "license_url", out->license_url,
                        sizeof(out->license_url), ctx, i) ||
        !CopyJsonString(entry, "author", out->author, sizeof(out->author), ctx,
                        i) ||
        !CopyJsonString(entry, "source_url", out->source_url,
                        sizeof(out->source_url), ctx, i) ||
        !CopyJsonString(entry, "credit_text", out->credit_text,
                        sizeof(out->credit_text), ctx, i) ||
        !CopyJsonString(entry, "version", out->version, sizeof(out->version), ctx,
                        i) ||
        !CopyJsonString(entry, "filename", out->filename,
                        sizeof(out->filename), ctx, i)) {
      AppFreeModelManifest(out_manifest);
      cJSON_Delete(root);
      return false;
    }

    if (!IsSupportedTask(out->task)) {
      AppSetError(ctx, "manifest entry %d has unsupported task '%s'", i,
                  out->task);
      AppFreeModelManifest(out_manifest);
      cJSON_Delete(root);
      return false;
    }

    if (!ValidateSha256(out->sha256)) {
      AppSetError(ctx, "manifest entry %d has invalid sha256 '%s'", i,
                  out->sha256);
      AppFreeModelManifest(out_manifest);
      cJSON_Delete(root);
      return false;
    }
  }

  cJSON_Delete(root);
  return true;
}

void AppFreeModelManifest(AppModelManifest *manifest) {
  if (!manifest) {
    return;
  }

  free(manifest->entries);
  manifest->entries = NULL;
  manifest->count = 0;
}
