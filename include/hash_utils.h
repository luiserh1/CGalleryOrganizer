#ifndef HASH_UTILS_H
#define HASH_UTILS_H

#include <stdbool.h>

#define MD5_HASH_LEN 32    // 16 bytes hex-encoded
#define SHA256_HASH_LEN 64 // 32 bytes hex-encoded

// Computes MD5 hash of a file. Returns true if successful, populates out_hex
// with null-terminated string.
bool ComputeFileMd5(const char *filepath, char out_hex[MD5_HASH_LEN + 1]);

// Computes SHA256 hash of a file. Returns true if successful, populates out_hex
// with null-terminated string.
bool ComputeFileSha256(const char *filepath, char out_hex[SHA256_HASH_LEN + 1]);

#endif /* HASH_UTILS_H */
