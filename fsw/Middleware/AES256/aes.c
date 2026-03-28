// fsw/Middleware/AES256/aes.c
// Mock TinyAES implementations for compilation testing
// In production, use actual TinyAES library

#include "aes.h"
#include <string.h>

// Mock AES initialization (no-op for compilation testing)
void AES_init_ctx(struct AES_ctx* ctx, const uint8_t* key) {
    memcpy(ctx->key, key, AES_KEY_SIZE);
    memset(ctx->iv, 0, AES_IV_SIZE);
}

// Mock AES initialization with IV
void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, const uint8_t* iv) {
    memcpy(ctx->key, key, AES_KEY_SIZE);
    memcpy(ctx->iv, iv, AES_IV_SIZE);
}

// Mock CTR encryption/decryption (no-op for compilation testing)
void AES_CTR_xcrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length) {
    // Mock implementation - just copy data unchanged
    // In production, this would perform actual AES-CTR encryption/decryption
    (void)ctx;  // Suppress unused parameter warning
    (void)buf;  // Suppress unused parameter warning
    (void)length; // Suppress unused parameter warning
}