#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cache_codec.h"

#if defined(__has_include)
#if __has_include(<zstd.h>)
#define CACHE_CODEC_HAVE_ZSTD 1
#include <zstd.h>
#endif
#endif

static void SetError(char *out_error, size_t out_error_size,
                     const char *message) {
  if (!out_error || out_error_size == 0) {
    return;
  }
  snprintf(out_error, out_error_size, "%s", message ? message : "unknown error");
}

static void WriteLe64(unsigned char *dst, uint64_t value) {
  for (int i = 0; i < 8; i++) {
    dst[i] = (unsigned char)((value >> (i * 8)) & 0xFF);
  }
}

static uint64_t ReadLe64(const unsigned char *src) {
  uint64_t value = 0;
  for (int i = 0; i < 8; i++) {
    value |= ((uint64_t)src[i]) << (i * 8);
  }
  return value;
}

bool CacheCodecIsAvailable(CacheCompressionMode mode) {
  if (mode == CACHE_COMPRESSION_NONE) {
    return true;
  }

  if (mode == CACHE_COMPRESSION_ZSTD) {
#if CACHE_CODEC_HAVE_ZSTD
    return true;
#else
    return false;
#endif
  }

  return false;
}

bool CacheCodecEncode(const unsigned char *input, size_t input_len,
                      CacheCompressionMode mode, int level,
                      unsigned char **out_bytes, size_t *out_len,
                      char *out_error, size_t out_error_size) {
  if (!input || !out_bytes || !out_len) {
    SetError(out_error, out_error_size, "invalid cache encode arguments");
    return false;
  }

  *out_bytes = NULL;
  *out_len = 0;

  if (mode == CACHE_COMPRESSION_NONE) {
    unsigned char *copy = malloc(input_len > 0 ? input_len : 1);
    if (!copy) {
      SetError(out_error, out_error_size, "allocation failed for plain cache");
      return false;
    }
    if (input_len > 0) {
      memcpy(copy, input, input_len);
    }
    *out_bytes = copy;
    *out_len = input_len;
    return true;
  }

  if (mode != CACHE_COMPRESSION_ZSTD) {
    SetError(out_error, out_error_size, "unsupported cache compression mode");
    return false;
  }

#if CACHE_CODEC_HAVE_ZSTD
  if (level < 1 || level > 19) {
    SetError(out_error, out_error_size, "invalid zstd compression level");
    return false;
  }

  size_t max_payload = ZSTD_compressBound(input_len);
  size_t total_len = CACHE_CODEC_HEADER_SIZE + max_payload;
  unsigned char *buffer = malloc(total_len);
  if (!buffer) {
    SetError(out_error, out_error_size, "allocation failed for compressed cache");
    return false;
  }

  memcpy(buffer, CACHE_CODEC_MAGIC, 8);
  buffer[8] = (unsigned char)CACHE_CODEC_VERSION;
  buffer[9] = (unsigned char)CACHE_COMPRESSION_ZSTD;
  buffer[10] = 0;
  buffer[11] = 0;
  WriteLe64(buffer + 12, (uint64_t)input_len);

  size_t compressed_size =
      ZSTD_compress(buffer + CACHE_CODEC_HEADER_SIZE, max_payload, input,
                    input_len, level);
  if (ZSTD_isError(compressed_size)) {
    SetError(out_error, out_error_size, "zstd compression failed");
    free(buffer);
    return false;
  }

  *out_bytes = buffer;
  *out_len = CACHE_CODEC_HEADER_SIZE + compressed_size;
  return true;
#else
  (void)level;
  SetError(out_error, out_error_size,
           "zstd compression requested but zstd is unavailable");
  return false;
#endif
}

bool CacheCodecDecode(const unsigned char *input, size_t input_len,
                      unsigned char **out_bytes, size_t *out_len,
                      CacheCompressionMode *out_mode, char *out_error,
                      size_t out_error_size) {
  if (!input || !out_bytes || !out_len || !out_mode) {
    SetError(out_error, out_error_size, "invalid cache decode arguments");
    return false;
  }

  *out_bytes = NULL;
  *out_len = 0;
  *out_mode = CACHE_COMPRESSION_NONE;

  if (input_len >= 8 && memcmp(input, CACHE_CODEC_MAGIC, 8) == 0) {
    if (input_len < CACHE_CODEC_HEADER_SIZE) {
      SetError(out_error, out_error_size, "truncated cache codec header");
      return false;
    }

    unsigned char version = input[8];
    unsigned char codec = input[9];
    if (version != CACHE_CODEC_VERSION) {
      SetError(out_error, out_error_size, "unsupported cache codec version");
      return false;
    }

    uint64_t raw_len64 = ReadLe64(input + 12);
    if (raw_len64 > (uint64_t)SIZE_MAX) {
      SetError(out_error, out_error_size, "decoded cache size overflow");
      return false;
    }
    size_t raw_len = (size_t)raw_len64;

    const unsigned char *payload = input + CACHE_CODEC_HEADER_SIZE;
    size_t payload_len = input_len - CACHE_CODEC_HEADER_SIZE;

    if (codec == CACHE_COMPRESSION_ZSTD) {
#if CACHE_CODEC_HAVE_ZSTD
      unsigned char *decoded = malloc(raw_len > 0 ? raw_len : 1);
      if (!decoded) {
        SetError(out_error, out_error_size,
                 "allocation failed for decoded zstd cache");
        return false;
      }

      size_t decompressed = ZSTD_decompress(decoded, raw_len, payload, payload_len);
      if (ZSTD_isError(decompressed) || decompressed != raw_len) {
        SetError(out_error, out_error_size, "zstd decompression failed");
        free(decoded);
        return false;
      }

      *out_bytes = decoded;
      *out_len = raw_len;
      *out_mode = CACHE_COMPRESSION_ZSTD;
      return true;
#else
      SetError(out_error, out_error_size,
               "zstd cache detected but zstd is unavailable");
      return false;
#endif
    }

    SetError(out_error, out_error_size, "unknown cache codec value");
    return false;
  }

  unsigned char *copy = malloc(input_len > 0 ? input_len : 1);
  if (!copy) {
    SetError(out_error, out_error_size, "allocation failed for plain cache");
    return false;
  }
  if (input_len > 0) {
    memcpy(copy, input, input_len);
  }

  *out_bytes = copy;
  *out_len = input_len;
  *out_mode = CACHE_COMPRESSION_NONE;
  return true;
}
