/* SHA-256 (sha256.c) */
#ifndef BRR_SHA256_H
#define BRR_SHA256_H
#include <stddef.h>
#include <stdint.h>
void brr_sha256(const void *data, size_t size, uint8_t out[32]);
void brr_sha256_hex(const uint8_t digest[32], char out[65]);
#endif
