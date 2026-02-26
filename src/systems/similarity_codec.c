#include <stdlib.h>
#include <string.h>

#include "similarity_engine.h"

static const char B64_TABLE[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static int B64Value(char c) {
  if (c >= 'A' && c <= 'Z') {
    return c - 'A';
  }
  if (c >= 'a' && c <= 'z') {
    return c - 'a' + 26;
  }
  if (c >= '0' && c <= '9') {
    return c - '0' + 52;
  }
  if (c == '+') {
    return 62;
  }
  if (c == '/') {
    return 63;
  }
  return -1;
}

bool SimilarityEncodeEmbeddingBase64(const float *embedding, int dim,
                                     char **out_base64) {
  if (!embedding || dim <= 0 || !out_base64) {
    return false;
  }

  const unsigned char *bytes = (const unsigned char *)embedding;
  size_t byte_len = (size_t)dim * sizeof(float);
  size_t out_len = 4 * ((byte_len + 2) / 3);

  char *out = malloc(out_len + 1);
  if (!out) {
    return false;
  }

  size_t i = 0;
  size_t j = 0;
  while (i < byte_len) {
    unsigned int octet_a = i < byte_len ? bytes[i++] : 0;
    unsigned int octet_b = i < byte_len ? bytes[i++] : 0;
    unsigned int octet_c = i < byte_len ? bytes[i++] : 0;

    unsigned int triple = (octet_a << 16) | (octet_b << 8) | octet_c;
    out[j++] = B64_TABLE[(triple >> 18) & 0x3F];
    out[j++] = B64_TABLE[(triple >> 12) & 0x3F];
    out[j++] = B64_TABLE[(triple >> 6) & 0x3F];
    out[j++] = B64_TABLE[triple & 0x3F];
  }

  size_t mod = byte_len % 3;
  if (mod > 0) {
    out[out_len - 1] = '=';
    if (mod == 1) {
      out[out_len - 2] = '=';
    }
  }
  out[out_len] = '\0';

  *out_base64 = out;
  return true;
}

bool SimilarityDecodeEmbeddingBase64(const char *base64, float **out_embedding,
                                     int *out_dim) {
  if (!base64 || !out_embedding || !out_dim) {
    return false;
  }

  size_t len = strlen(base64);
  if (len == 0 || (len % 4) != 0) {
    return false;
  }

  size_t padding = 0;
  if (len >= 1 && base64[len - 1] == '=') {
    padding++;
  }
  if (len >= 2 && base64[len - 2] == '=') {
    padding++;
  }

  size_t decoded_len = (len / 4) * 3 - padding;
  if (decoded_len == 0 || (decoded_len % sizeof(float)) != 0) {
    return false;
  }

  unsigned char *decoded = malloc(decoded_len);
  if (!decoded) {
    return false;
  }

  size_t out_idx = 0;
  for (size_t i = 0; i < len; i += 4) {
    int v0 = B64Value(base64[i]);
    int v1 = B64Value(base64[i + 1]);
    int v2 = (base64[i + 2] == '=') ? -2 : B64Value(base64[i + 2]);
    int v3 = (base64[i + 3] == '=') ? -2 : B64Value(base64[i + 3]);

    if (v0 < 0 || v1 < 0 || v2 == -1 || v3 == -1) {
      free(decoded);
      return false;
    }

    unsigned int triple = ((unsigned int)v0 << 18) | ((unsigned int)v1 << 12) |
                          ((unsigned int)((v2 < 0) ? 0 : v2) << 6) |
                          (unsigned int)((v3 < 0) ? 0 : v3);

    if (out_idx < decoded_len) {
      decoded[out_idx++] = (unsigned char)((triple >> 16) & 0xFF);
    }
    if (v2 >= 0 && out_idx < decoded_len) {
      decoded[out_idx++] = (unsigned char)((triple >> 8) & 0xFF);
    }
    if (v3 >= 0 && out_idx < decoded_len) {
      decoded[out_idx++] = (unsigned char)(triple & 0xFF);
    }
  }

  *out_dim = (int)(decoded_len / sizeof(float));
  *out_embedding = (float *)decoded;
  return true;
}
