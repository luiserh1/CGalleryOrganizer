#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache_codec.h"
#include "core/gallery_cache_internal.h"

cJSON *g_cache_root = NULL;
char g_cache_path[1024] = {0};
CacheCompressionMode g_cache_compression_mode = CACHE_COMPRESSION_NONE;
int g_cache_compression_level = 3;
const size_t DEFAULT_AUTO_THRESHOLD_BYTES = 8u * 1024u * 1024u;

bool CacheInit(const char *cache_path) {
  if (!cache_path)
    return false;

  strncpy(g_cache_path, cache_path, sizeof(g_cache_path) - 1);
  g_cache_path[sizeof(g_cache_path) - 1] = '\0';

  FILE *f = fopen(cache_path, "rb");
  bool file_exists = (f != NULL);
  if (f) {
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (length > 0) {
      unsigned char *raw = malloc((size_t)length);
      if (!raw) {
        fclose(f);
        return false;
      }

      size_t bytes_read = fread(raw, 1, (size_t)length, f);
      if (bytes_read != (size_t)length) {
        free(raw);
        fclose(f);
        return false;
      }

      unsigned char *decoded = NULL;
      size_t decoded_len = 0;
      CacheCompressionMode detected_mode = CACHE_COMPRESSION_NONE;
      char decode_error[256] = {0};
      if (!CacheCodecDecode(raw, (size_t)length, &decoded, &decoded_len,
                            &detected_mode, decode_error,
                            sizeof(decode_error))) {
        fprintf(stderr, "Error: Failed to decode cache '%s': %s\n", cache_path,
                decode_error[0] != '\0' ? decode_error : "unknown");
        free(raw);
        fclose(f);
        return false;
      }
      free(raw);

      char *content = malloc(decoded_len + 1);
      if (!content) {
        free(decoded);
        fclose(f);
        return false;
      }

      if (decoded_len > 0) {
        memcpy(content, decoded, decoded_len);
      }
      content[decoded_len] = '\0';
      g_cache_root = cJSON_Parse(content);

      free(content);
      free(decoded);
    }
    fclose(f);
  }

  if (!g_cache_root) {
    if (file_exists) {
      return false;
    }
    g_cache_root = cJSON_CreateObject();
  }
  return true;
}

void CacheShutdown(void) {
  if (g_cache_root) {
    cJSON_Delete(g_cache_root);
    g_cache_root = NULL;
  }
  g_cache_path[0] = '\0';
}

bool CacheSave(void) {
  if (!g_cache_root || g_cache_path[0] == '\0')
    return false;

  char *json_str = cJSON_PrintUnformatted(g_cache_root);
  if (!json_str)
    return false;

  CacheCompressionMode effective_mode = g_cache_compression_mode;
  if (effective_mode == CACHE_COMPRESSION_AUTO) {
    size_t threshold = DEFAULT_AUTO_THRESHOLD_BYTES;
    const char *threshold_override = getenv("CGO_CACHE_AUTO_THRESHOLD_BYTES");
    if (threshold_override && threshold_override[0] != '\0') {
      char *endptr = NULL;
      unsigned long parsed = strtoul(threshold_override, &endptr, 10);
      if (endptr && *endptr == '\0' && parsed > 0) {
        threshold = (size_t)parsed;
      }
    }

    if (strlen(json_str) < threshold) {
      effective_mode = CACHE_COMPRESSION_NONE;
    } else {
      effective_mode = CACHE_COMPRESSION_ZSTD;
      if (!CacheCodecIsAvailable(CACHE_COMPRESSION_ZSTD)) {
        fprintf(stderr,
                "Error: auto cache compression selected zstd but zstd is unavailable.\n");
        free(json_str);
        return false;
      }
    }
  }

  unsigned char *encoded = NULL;
  size_t encoded_len = 0;
  char encode_error[256] = {0};
  if (!CacheCodecEncode((const unsigned char *)json_str, strlen(json_str),
                        effective_mode, g_cache_compression_level, &encoded,
                        &encoded_len, encode_error, sizeof(encode_error))) {
    fprintf(stderr, "Error: Failed to encode cache '%s': %s\n", g_cache_path,
            encode_error[0] != '\0' ? encode_error : "unknown");
    free(json_str);
    return false;
  }
  free(json_str);

  FILE *f = fopen(g_cache_path, "wb");
  if (!f) {
    free(encoded);
    return false;
  }

  if (encoded_len > 0) {
    size_t written = fwrite(encoded, 1, encoded_len, f);
    if (written != encoded_len) {
      fclose(f);
      free(encoded);
      return false;
    }
  }
  fclose(f);
  free(encoded);

  return true;
}

bool CacheSetCompression(CacheCompressionMode mode, int level) {
  if (mode != CACHE_COMPRESSION_NONE && mode != CACHE_COMPRESSION_ZSTD &&
      mode != CACHE_COMPRESSION_AUTO) {
    return false;
  }

  if (mode == CACHE_COMPRESSION_ZSTD || mode == CACHE_COMPRESSION_AUTO) {
    if (level < 1 || level > 19) {
      return false;
    }
    if (mode == CACHE_COMPRESSION_ZSTD &&
        !CacheCodecIsAvailable(CACHE_COMPRESSION_ZSTD)) {
      return false;
    }
  }

  g_cache_compression_mode = mode;
  g_cache_compression_level = level;
  return true;
}
