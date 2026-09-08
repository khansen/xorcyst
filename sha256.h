#ifndef XASM_SHA256_H
#define XASM_SHA256_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t state[8];
    uint64_t bytes;
    unsigned char block[64];
    size_t used;
} sha256_context;

void sha256_init(sha256_context *ctx);
void sha256_update(sha256_context *ctx, const void *data, size_t size);
void sha256_finish(sha256_context *ctx, char hex[65]);

#endif
