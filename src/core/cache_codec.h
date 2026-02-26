#ifndef CACHE_CODEC_H
#define CACHE_CODEC_H

#include <stdbool.h>
#include <stddef.h>

#include "gallery_cache.h"

#define CACHE_CODEC_MAGIC "CGOCACHE"
#define CACHE_CODEC_VERSION 1
#define CACHE_CODEC_HEADER_SIZE 20

bool CacheCodecIsAvailable(CacheCompressionMode mode);

bool CacheCodecEncode(const unsigned char *input, size_t input_len,
                      CacheCompressionMode mode, int level,
                      unsigned char **out_bytes, size_t *out_len,
                      char *out_error, size_t out_error_size);

bool CacheCodecDecode(const unsigned char *input, size_t input_len,
                      unsigned char **out_bytes, size_t *out_len,
                      CacheCompressionMode *out_mode, char *out_error,
                      size_t out_error_size);

#endif // CACHE_CODEC_H
