#ifndef SHA256_H
#define SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
  unsigned char data[64];
  uint32_t datalen;
  unsigned long long bitlen;
  uint32_t state[8];
} SHA256_CTX;

void SHA256_Init(SHA256_CTX *ctx);
void SHA256_Update(SHA256_CTX *ctx, const unsigned char data[], size_t len);
void SHA256_Final(unsigned char hash[32], SHA256_CTX *ctx);

#endif /* SHA256_H */
