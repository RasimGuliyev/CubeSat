// fsw/Middleware/AES256/crypto.h
// AES-256 Şifreleme/Deşifreleme Modülü
// TinyAES kullanarak CTR modu ile gerçekleştirilir
#ifndef CRYPTO_H
#define CRYPTO_H

#include <stdint.h>
#include <stddef.h>

// AES-256 anahtar boyutu (32 bayt = 256 bit)
#define AES_KEY_SIZE 32
// IV boyutu (16 bayt = 128 bit)
#define AES_IV_SIZE  16
// AES blok boyutu (128 bit)
#define AES_BLOCK_SIZE 16

// Tekrar saldırısı (Replay Attack) önleme: IV'nin ilk 4 baytı dinamik timestamp olacak
typedef struct __attribute__((packed)) {
    uint32_t timestamp;    // UTC saniye
    uint8_t  reserved[12]; // Kalan 12 bayt (deterministic)
} DynamicIV_t;

// Şifreleme bağlamı (context)
typedef struct {
    uint8_t key[AES_KEY_SIZE];     // 256-bit şifreleme anahtarı
    uint8_t iv[AES_IV_SIZE];       // 128-bit başlatma vektörü
    uint32_t counter;              // CTR modu için counter
} CryptoContext_t;

/**
 * @brief Şifreleme bağlamını başlat
 * @param ctx: Şifreleme bağlamı pointeri
 * @param key: 32 baytlık AES-256 anahtarı
 * @param timestamp: Dinamik IV oluşturmak için mevcut UTC zamanı
 */
void crypto_init(CryptoContext_t *ctx, const uint8_t *key, uint32_t timestamp);

/**
 * @brief Veriyi AES-256 CTR modunda şifrele
 * @param ctx: Şifreleme bağlamı
 * @param plaintext: Şifrelenecek veri
 * @param plaintext_len: Veri uzunluğu (herhangi bir boyut olabilir)
 * @param ciphertext: Çıktı buffer (en az plaintext_len kadar)
 * @return Şifrelenmiş veri uzunluğu, hata durumunda -1
 */
int32_t crypto_encrypt(CryptoContext_t *ctx, const uint8_t *plaintext,
                       size_t plaintext_len, uint8_t *ciphertext);

/**
 * @brief Veriyi AES-256 CTR modunda deşifrele
 * @param ctx: Şifreleme bağlamı
 * @param ciphertext: Şifrelenmiş veri
 * @param ciphertext_len: Şifrelenmiş veri uzunluğu
 * @param plaintext: Çıktı buffer (en az ciphertext_len kadar)
 * @return Deşifreli veri uzunluğu, hata durumunda -1
 */
int32_t crypto_decrypt(CryptoContext_t *ctx, const uint8_t *ciphertext,
                       size_t ciphertext_len, uint8_t *plaintext);

/**
 * @brief IV'yi döndür (header'da veya paket metadata'sında göndermek için)
 * @param ctx: Şifreleme bağlamı
 * @return IV (16 bayt array pointer)
 */
const uint8_t* crypto_get_iv(CryptoContext_t *ctx);

/**
 * @brief dinamik IV oluştur (timestamp + counter ile)
 * @param timestamp: Mevcut UTC zamanı
 * @param sequence_num: Tekrar saldırısı önleme sequence numarası
 * @param iv_out: Çıktı IV buffer (16 bayt)
 */
void crypto_generate_dynamic_iv(uint32_t timestamp, uint8_t sequence_num,
                                uint8_t *iv_out);

/**
 * @brief CRC8 checksum hesapla (veri bütünlüğü kontrolü)
 * @param data: Veri buffer
 * @param len: Veri uzunluğu
 * @return 8-bit CRC checksum
 */
uint8_t crypto_crc8(const uint8_t *data, size_t len);

/**
 * @brief CRC16 checksum hesapla (radyasyon verisi bütünlüğü için)
 * @param data: Veri buffer
 * @param len: Veri uzunluğu
 * @return 16-bit CRC checksum
 */
uint16_t crypto_crc16(const uint8_t *data, size_t len);

/**
 * @brief Radyasyon paketini şifrele ve checksumla
 * Telemetri paketleri için high-level wrapper
 * @param ctx: Şifreleme bağlamı
 * @param payload: Ödeme yükü veri buffer
 * @param payload_len: Ödeme yükü uzunluğu
 * @param output: Çıktı buffer (payload_len + CRC16 + IV bilgisi)
 * @param output_len: Çıktı buffer boyutu
 * @return Şifrelenmiş paket uzunluğu, hata durumunda -1
 */
int32_t crypto_encrypt_payload(CryptoContext_t *ctx, const uint8_t *payload,
                               size_t payload_len, uint8_t *output,
                               size_t output_len);

#endif // CRYPTO_H
