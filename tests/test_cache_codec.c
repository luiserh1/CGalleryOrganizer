#include <stdlib.h>
#include <string.h>

#include "core/cache_codec.h"
#include "test_framework.h"

void test_cache_codec_none_roundtrip(void) {
  const char *input = "{\"hello\":\"world\"}";

  unsigned char *encoded = NULL;
  size_t encoded_len = 0;
  char err[128] = {0};
  ASSERT_TRUE(CacheCodecEncode((const unsigned char *)input, strlen(input),
                               CACHE_COMPRESSION_NONE, 3, &encoded,
                               &encoded_len, err, sizeof(err)));

  unsigned char *decoded = NULL;
  size_t decoded_len = 0;
  CacheCompressionMode mode = CACHE_COMPRESSION_ZSTD;
  ASSERT_TRUE(CacheCodecDecode(encoded, encoded_len, &decoded, &decoded_len,
                               &mode, err, sizeof(err)));
  ASSERT_EQ(CACHE_COMPRESSION_NONE, mode);
  ASSERT_EQ((int)strlen(input), (int)decoded_len);
  ASSERT_TRUE(memcmp(decoded, input, decoded_len) == 0);

  free(encoded);
  free(decoded);
}

void test_cache_codec_header_validation(void) {
  unsigned char malformed[CACHE_CODEC_HEADER_SIZE] = {0};
  memcpy(malformed, CACHE_CODEC_MAGIC, 8);
  malformed[8] = 99;
  malformed[9] = (unsigned char)CACHE_COMPRESSION_ZSTD;

  unsigned char *decoded = NULL;
  size_t decoded_len = 0;
  CacheCompressionMode mode = CACHE_COMPRESSION_NONE;
  char err[128] = {0};
  ASSERT_FALSE(CacheCodecDecode(malformed, sizeof(malformed), &decoded,
                                &decoded_len, &mode, err, sizeof(err)));
}

void test_cache_codec_zstd_roundtrip_or_unavailable(void) {
  if (!CacheCodecIsAvailable(CACHE_COMPRESSION_ZSTD)) {
    return;
  }

  const char *input = "{\"very\":\"compressible compressible compressible\"}";
  unsigned char *encoded = NULL;
  size_t encoded_len = 0;
  char err[128] = {0};
  ASSERT_TRUE(CacheCodecEncode((const unsigned char *)input, strlen(input),
                               CACHE_COMPRESSION_ZSTD, 3, &encoded,
                               &encoded_len, err, sizeof(err)));

  unsigned char *decoded = NULL;
  size_t decoded_len = 0;
  CacheCompressionMode mode = CACHE_COMPRESSION_NONE;
  ASSERT_TRUE(CacheCodecDecode(encoded, encoded_len, &decoded, &decoded_len,
                               &mode, err, sizeof(err)));

  ASSERT_EQ(CACHE_COMPRESSION_ZSTD, mode);
  ASSERT_EQ((int)strlen(input), (int)decoded_len);
  ASSERT_TRUE(memcmp(decoded, input, decoded_len) == 0);

  free(encoded);
  free(decoded);
}

void register_cache_codec_tests(void) {
  register_test("test_cache_codec_none_roundtrip",
                test_cache_codec_none_roundtrip, "cache");
  register_test("test_cache_codec_header_validation",
                test_cache_codec_header_validation, "cache");
  register_test("test_cache_codec_zstd_roundtrip_or_unavailable",
                test_cache_codec_zstd_roundtrip_or_unavailable, "cache");
}
