#include "hash_utils.h"
#include "md5.h"
#include "sha256.h"
#include <stdio.h>

static void hex_encode(const unsigned char *data, size_t len, char *out) {
  static const char hex_chars[] = "0123456789abcdef";
  for (size_t i = 0; i < len; i++) {
    out[i * 2] = hex_chars[(data[i] >> 4) & 0x0F];
    out[i * 2 + 1] = hex_chars[data[i] & 0x0F];
  }
  out[len * 2] = '\0';
}

bool ComputeFileMd5(const char *filepath, char out_hex[MD5_HASH_LEN + 1]) {
  FILE *f = fopen(filepath, "rb");
  if (!f)
    return false;

  MD5_CTX ctx;
  MD5_Init(&ctx);

  unsigned char buf[8192];
  size_t bytes;
  while ((bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
    MD5_Update(&ctx, buf, bytes);
  }

  fclose(f);

  unsigned char digest[16];
  MD5_Final(digest, &ctx);

  hex_encode(digest, 16, out_hex);
  return true;
}

bool ComputeFileSha256(const char *filepath,
                       char out_hex[SHA256_HASH_LEN + 1]) {
  FILE *f = fopen(filepath, "rb");
  if (!f)
    return false;

  SHA256_CTX ctx;
  SHA256_Init(&ctx);

  unsigned char buf[8192];
  size_t bytes;
  while ((bytes = fread(buf, 1, sizeof(buf), f)) > 0) {
    SHA256_Update(&ctx, buf, bytes);
  }

  fclose(f);

  unsigned char hash[32];
  SHA256_Final(hash, &ctx);

  hex_encode(hash, 32, out_hex);
  return true;
}
