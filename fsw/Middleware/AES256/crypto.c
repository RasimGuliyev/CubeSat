// fsw/Middleware/AES256/crypto.c
// AES-256 CTR Modu Şifreleme Implementasyonu
// TinyAES kütüphanesi kullanılır, SEU/Tekrar Saldırısı işleme dahil

#include "crypto.h"
#include "aes.h"
#include <string.h>

// Üretim ortamında bu anahtar secure key storage (HSM/KMS) üzerinden yüklenmelidir
// Bu değer sadece test/demo amaçlıdır
static const uint8_t SECRET_KEY[32] = {
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C,
    0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
    0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

// ============================================================================
// CRC Hesaplama Fonksiyonları
// ============================================================================

/**
 * CRC-8 Checksum (Paket başlığı kontrolü)
 * Polinom: x^8 + x^7 + x^6 + x^4 + x^2 + 1 (0x59)
 */
uint8_t crypto_crc8(const uint8_t *data, size_t len) {
    // SEU koruması: NULL pointer kontrolü
    if (data == NULL) {
        return 0xFF;
    }
    
    if (len == 0) {
        return 0x00;
    }
    
    uint8_t crc = 0x00;
    const uint8_t POLY = 0x59;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ POLY;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

/**
 * CRC-16 Checksum (Radyasyon sensörü verisi bütünlüğü)
 * CCITT-FALSE polinom: 0x1021
 */
uint16_t crypto_crc16(const uint8_t *data, size_t len) {
    if (data == NULL) {
        return 0xFFFF;
    }
    
    if (len == 0) {
        return 0x0000;
    }
    
    uint16_t crc = 0xFFFF;
    const uint16_t POLY = 0x1021;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= ((uint16_t)data[i] << 8);
        
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ POLY;
            } else {
                crc = crc << 1;
            }
        }
    }
    
    return crc;
}

// ============================================================================
// IV ve Dinamik Şifreleme Vektörü Yönetimi
// ============================================================================

/**
 * Dinamik IV oluştur: İlk 4 bayt timestamp, son 12 bayt deterministic
 * Tekrar saldırısı (Replay Attack) önleme için timestamp her paket'te değişir
 */
void crypto_generate_dynamic_iv(uint32_t timestamp, uint8_t sequence_num,
                                uint8_t *iv_out) {
    if (iv_out == NULL) {
        return;  // SEU koruması
    }
    
    // IV'nin ilk 4 baytına timestamp gömü (big-endian)
    iv_out[0] = (timestamp >> 24) & 0xFF;
    iv_out[1] = (timestamp >> 16) & 0xFF;
    iv_out[2] = (timestamp >> 8) & 0xFF;
    iv_out[3] = timestamp & 0xFF;
    
    // Bayt 4: Sequence numarası
    iv_out[4] = sequence_num;
    
    // Baytlar 5-15: Belirlenmiş sabit değerler
    const uint8_t STATIC_IV[11] = {
        0x00, 0x01, 0x02, 0x03,
        0xAA, 0xBB, 0xCC, 0xDD,
        0xEE, 0xFF, 0x42
    };
    
    memcpy(&iv_out[5], STATIC_IV, 11);
}

// ============================================================================
// Şifreleme Bağlamı Yönetimi (Context Management)
// ============================================================================

/**
 * @brief Şifreleme bağlamını başlat
 */
void crypto_init(CryptoContext_t *ctx, const uint8_t *key, uint32_t timestamp) {
    if (ctx == NULL || key == NULL) {
        return;  // SEU koruması
    }
    
    // Anahtarı kopyala
    memcpy(ctx->key, key, AES_KEY_SIZE);
    
    // Dinamik IV oluştur
    crypto_generate_dynamic_iv(timestamp, 0x00, ctx->iv);
    
    // Counter'ı başlat
    ctx->counter = 0;
}

/**
 * @brief Şifreleme bağlamındaki IV'yi döndür
 */
const uint8_t* crypto_get_iv(CryptoContext_t *ctx) {
    if (ctx == NULL) {
        return NULL;
    }
    return ctx->iv;
}

// ============================================================================
// ESKI API FONKSİYONLARI (Geriye Uyumluluk)
// ============================================================================

/**
 * Eski API: Payload'ı şifrele
 * (@deprecated: Yeni kod crypto_init + crypto_encrypt kullanmalı)
 */
void Crypto_EncryptPayload(uint8_t* plaintext_payload, uint8_t* encrypted_payload, 
                          uint16_t length, uint32_t current_timestamp) {
    if (plaintext_payload == NULL || encrypted_payload == NULL) {
        return;  // SEU koruması
    }
    
    struct AES_ctx ctx;
    uint8_t iv[16] = {0};
    
    // IV'nin ilk 4 baytına timestamp yerleştir
    memcpy(iv, &current_timestamp, sizeof(uint32_t));
    // Kalan 12 bayt doğası gereği 0 kalır
    
    AES_init_ctx_iv(&ctx, SECRET_KEY, iv);
    memcpy(encrypted_payload, plaintext_payload, length);
    AES_CTR_xcrypt_buffer(&ctx, encrypted_payload, length);
}

/**
 * Eski API: Payload'ı deşifrele
 * (@deprecated: Yeni kod crypto_init + crypto_decrypt kullanmalı)
 */
void Crypto_DecryptPayload(uint8_t* encrypted_payload, uint8_t* decrypted_payload, 
                          uint16_t length, uint32_t packet_timestamp) {
    if (encrypted_payload == NULL || decrypted_payload == NULL) {
        return;  // SEU koruması
    }
    
    struct AES_ctx ctx;
    uint8_t iv[16] = {0};
    
    // Deşifreleme sırasında aynı timestamp ile IV'yi oluştur
    memcpy(iv, &packet_timestamp, sizeof(uint32_t));
    
    AES_init_ctx_iv(&ctx, SECRET_KEY, iv);
    memcpy(decrypted_payload, encrypted_payload, length);
    AES_CTR_xcrypt_buffer(&ctx, decrypted_payload, length);
}

// ============================================================================
// YÜKSEKSeviye Payload Şifreleme (High-Level API)
// ============================================================================

/**
 * Radyasyon paketini şifrele ve CRC ekle
 * Çıktı formatı: [Ciphertext (payload_len)] [CRC16 (2-byte)] [IV suffix (1-byte)]
 */
int32_t crypto_encrypt_payload(CryptoContext_t *ctx, const uint8_t *payload,
                               size_t payload_len, uint8_t *output,
                               size_t output_len) {
    // SEU koruması: NULL pointer kontrolü
    if (ctx == NULL || payload == NULL || output == NULL) {
        return -1;
    }
    
    // Gerekli buffer boyutu: payload + CRC16 (2) + IV_suffix (1)
    size_t required_len = payload_len + 2 + 1;
    if (output_len < required_len) {
        return -1;  // Buffer yetersiz
    }
    
    // Adım 1: Payload'ı şifrele
    struct AES_ctx aes_ctx;
    AES_init_ctx_iv(&aes_ctx, ctx->key, ctx->iv);
    memcpy(output, payload, payload_len);
    AES_CTR_xcrypt_buffer(&aes_ctx, output, payload_len);
    
    // Adım 2: Şifrelenmiş veri üzerine CRC16 ekle
    uint16_t crc16 = crypto_crc16(output, payload_len);
    output[payload_len] = (crc16 >> 8) & 0xFF;
    output[payload_len + 1] = crc16 & 0xFF;
    
    // Adım 3: IV'nin 4. baytını metadata olarak ekle
    output[payload_len + 2] = ctx->iv[3];
    
    return (int32_t)(payload_len + 3);
}

// ============================================================================
// Yardımcı Fonksiyonlar
// ============================================================================

/**
 * @brief Şifreleme bağlamını güvenli şekilde temizle
 * (Radyasyon nedeniyle hafıza bozulması riskine karşı)
 */
void crypto_clear_context(CryptoContext_t *ctx) {
    if (ctx == NULL) {
        return;
    }
    
    // Volatile yazım optimizer'ın hafıza temizliğini atlamasını engeller
    volatile uint8_t *v_ctx = (volatile uint8_t *)ctx;
    for (size_t i = 0; i < sizeof(CryptoContext_t); i++) {
        v_ctx[i] = 0x00;
    }
}

#ifdef DEBUG_CRYPTO
/**
 * DEBUG ONLY: Hex dump yazdır
 */
void crypto_print_hex(const uint8_t *data, size_t len) {
    if (data == NULL) return;
    
    for (size_t i = 0; i < len; i++) {
        // printf("%02X ", data[i]);
        // if ((i + 1) % 16 == 0) printf("\n");
    }
}

/**
 * DEBUG ONLY: IV debugging
 */
void crypto_debug_iv(CryptoContext_t *ctx) {
    if (ctx == NULL) return;
    // printf("[DEBUG] IV: ");
    // crypto_print_hex(ctx->iv, AES_IV_SIZE);
    // printf("\nCounter: %u\n", ctx->counter);
}
#endif // DEBUG_CRYPTO