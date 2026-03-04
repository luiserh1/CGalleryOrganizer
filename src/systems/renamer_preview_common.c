#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "systems/renamer_preview_internal.h"

void RenamerPreviewSetError(char *out_error, size_t out_error_size, const char *fmt,
                     ...) {
  if (!out_error || out_error_size == 0 || !fmt) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(out_error, out_error_size, fmt, args);
  va_end(args);
}
void RenamerPreviewNowUtc(char *out_text, size_t out_text_size) {
  if (!out_text || out_text_size == 0) {
    return;
  }

  out_text[0] = '\0';
  time_t now = time(NULL);
  struct tm tm_utc;
#if defined(_WIN32)
  gmtime_s(&tm_utc, &now);
#else
  gmtime_r(&now, &tm_utc);
#endif
  strftime(out_text, out_text_size, "%Y-%m-%dT%H:%M:%SZ", &tm_utc);
}

bool RenamerPreviewEnsureRenameCachePaths(const char *env_dir, char *out_config_path,
                                   size_t out_config_path_size,
                                   char *out_preview_dir,
                                   size_t out_preview_dir_size) {
  if (!env_dir || env_dir[0] == '\0') {
    return false;
  }

  char cache_dir[MAX_PATH_LENGTH] = {0};
  snprintf(cache_dir, sizeof(cache_dir), "%s/.cache", env_dir);
  if (!FsMakeDirRecursive(cache_dir)) {
    return false;
  }

  if (out_config_path && out_config_path_size > 0) {
    snprintf(out_config_path, out_config_path_size, "%s/rename_config.json",
             cache_dir);
  }

  if (out_preview_dir && out_preview_dir_size > 0) {
    snprintf(out_preview_dir, out_preview_dir_size, "%s/rename_previews",
             cache_dir);
    if (!FsMakeDirRecursive(out_preview_dir)) {
      return false;
    }
  }

  return true;
}
bool RenamerPreviewLoadFileText(const char *path, char **out_text) {
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

  long size = ftell(f);
  if (size < 0) {
    fclose(f);
    return false;
  }

  rewind(f);
  char *buffer = calloc((size_t)size + 1, 1);
  if (!buffer) {
    fclose(f);
    return false;
  }

  size_t read_bytes = fread(buffer, 1, (size_t)size, f);
  fclose(f);
  buffer[read_bytes] = '\0';

  *out_text = buffer;
  return true;
}

bool RenamerPreviewSaveFileText(const char *path, const char *text) {
  if (!path || !text) {
    return false;
  }

  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }

  bool ok = fputs(text, f) >= 0;
  fclose(f);
  return ok;
}

static void FingerprintInit(uint64_t *h1, uint64_t *h2) {
  if (!h1 || !h2) {
    return;
  }
  *h1 = 1469598103934665603ULL;
  *h2 = 1099511628211ULL;
}

static void FingerprintUpdate(uint64_t *h1, uint64_t *h2, const char *text) {
  if (!h1 || !h2 || !text) {
    return;
  }

  const unsigned char *p = (const unsigned char *)text;
  while (*p) {
    *h1 ^= (uint64_t)(*p);
    *h1 *= 1099511628211ULL;

    *h2 += (uint64_t)(*p);
    *h2 *= 14029467366897019727ULL;
    *h2 ^= (*h2 >> 33);
    p++;
  }
}

static void FingerprintFinalize(uint64_t h1, uint64_t h2, char *out_text,
                                size_t out_text_size) {
  if (!out_text || out_text_size == 0) {
    return;
  }
  snprintf(out_text, out_text_size, "%016llx%016llx",
           (unsigned long long)h1, (unsigned long long)h2);
}

bool RenamerPreviewBuildFingerprint(const char *target_dir, const char *pattern,
                             const RenamerPreviewItem *items, int item_count,
                             char *out_fingerprint,
                             size_t out_fingerprint_size) {
  if (!target_dir || !pattern || !out_fingerprint || out_fingerprint_size == 0 ||
      item_count < 0) {
    return false;
  }

  uint64_t h1 = 0;
  uint64_t h2 = 0;
  FingerprintInit(&h1, &h2);
  FingerprintUpdate(&h1, &h2, target_dir);
  FingerprintUpdate(&h1, &h2, "|");
  FingerprintUpdate(&h1, &h2, pattern);

  for (int i = 0; i < item_count; i++) {
    char line[1280] = {0};
    snprintf(line, sizeof(line), "|%s|%.0f|%ld|%s",
             items[i].source_path, items[i].source_mod_time,
             items[i].source_size, items[i].candidate_path);
    FingerprintUpdate(&h1, &h2, line);
  }

  FingerprintFinalize(h1, h2, out_fingerprint, out_fingerprint_size);
  return true;
}

