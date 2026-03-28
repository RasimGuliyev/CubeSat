// shared/commands.h
// Yer istasyonundan alınan komut paketleri
#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

// Komut türleri
typedef enum {
    CMD_INVALID = 0x00,
    CMD_MODE_CHANGE = 0x01,          // İşletim modu değiştir (Normal/Safe)
    CMD_RESET_SYSTEM = 0x02,         // Sistem resetle
    CMD_DATA_REQUEST = 0x03,         // Son telemetri paketi talep et
    CMD_CONFIG_ADCS = 0x04,          // ADCS parametreleri config
    CMD_CONFIG_PAYLOAD = 0x05,       // Payload config (sampling rate vb.)
    CMD_SD_CARD_DUMP = 0x06,         // SD karttan veri (timestamp aralığı)
    CMD_CLEAR_BUFFER = 0x07,         // İç buffer'ları temizle
    CMD_ENABLE_LOG = 0x08,           // Log kayıtlarını aç/kapat
    CMD_GET_SYSTEM_STATUS = 0x09,    // Sistem sağlığı sorgusu
    CMD_ACK = 0xFF
} CommandType_t;

// Temel komut paketi yapısı
typedef struct __attribute__((packed)) {
    uint32_t timestamp;              // Zaman damgası (UTC saniye)
    uint8_t  command_id;             // CommandType_t
    uint8_t  sequence_num;           // Tekrar saldırısı önleme (IV'ye gömülecek)
    uint16_t payload_len;            // Komut payload'ı uzunluğu
    uint8_t  checksum;               // CRC8 tek bayt checksum
} CommandHeader_t;

// MODE_CHANGE komutu payload'ı
typedef struct __attribute__((packed)) {
    uint8_t target_mode;             // 0: Normal, 1: Safe Mode
} CMD_ModeChange_t;

// ADCS config komutu
typedef struct __attribute__((packed)) {
    float spin_rate;                 // Dönme hızı (rpm)
    float mag_field_strength;        // Manyetik alan şiddeti (mT)
    uint16_t kalman_q;               // Kalman filter Q parametresi
} CMD_ConfigADCS_t;

// PAYLOAD config komutu
typedef struct __attribute__((packed)) {
    uint16_t sampling_rate_ms;       // Örnekleme periyodu (ms)
    uint8_t  do_compression;         // 1: Sıkıştırma etkinleştir
    uint8_t  overwrite_full_sd;      // 1: SD dolu olunca üzerine yaz
} CMD_ConfigPayload_t;

// SD Kart dump isteği
typedef struct __attribute__((packed)) {
    uint32_t start_timestamp;        // Başlanması gereken zaman
    uint32_t end_timestamp;          // Sonlanması gereken zaman
    uint16_t max_packets;            // Maksimum paket sayısı
} CMD_SDCardDump_t;

// Acknowledgment paketi (CMD_ACK)
typedef struct __attribute__((packed)) {
    uint8_t  original_cmd_id;        // Yanıt verilen komut
    uint8_t  status;                 // 0: Başarılı, 1: Hata, 2: Devam ediyor
    uint16_t error_code;             // Hata kodu (varsa)
} CMD_Acknowledgment_t;

#endif // COMMANDS_H
