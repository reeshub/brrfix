/* SHA-256 (sha256.c) */
#ifndef BRR_SHA256_H
#define BRR_SHA256_H
#include <stddef.h>
#include <stdint.h>
struct brr_sha256_state { uint32_t h[8], bytes; uint8_t block[64]; unsigned used; };
void brr_sha256_init(struct brr_sha256_state *s);
void brr_sha256_update(struct brr_sha256_state *s, const void *data, size_t size);
void brr_sha256_final(struct brr_sha256_state *s, uint8_t out[32]);
void brr_sha256(const void *data, size_t size, uint8_t out[32]);
void brr_sha256_hex(const uint8_t digest[32], char out[65]);
#endif
