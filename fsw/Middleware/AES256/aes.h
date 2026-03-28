// fsw/Middleware/AES256/aes.h
// Mock TinyAES header for compilation testing
// In production, use actual TinyAES library

#ifndef AES_H
#define AES_H

#include <stdint.h>
#include <stddef.h>

#define AES_KEY_SIZE 32
#define AES_IV_SIZE 16
#define AES_BLOCK_SIZE 16

// AES context structure
struct AES_ctx {
    uint8_t key[AES_KEY_SIZE];
    uint8_t iv[AES_IV_SIZE];
};

// AES functions (mock implementations for compilation)
void AES_init_ctx(struct AES_ctx* ctx, const uint8_t* key);
void AES_init_ctx_iv(struct AES_ctx* ctx, const uint8_t* key, const uint8_t* iv);
void AES_CTR_xcrypt_buffer(struct AES_ctx* ctx, uint8_t* buf, size_t length);

#endif // AES_H