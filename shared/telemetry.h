// shared/telemetry.h
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

// Uydunun temel sağlık ve durum paketi (Beacon)
typedef struct __attribute__((packed)) {
    uint32_t timestamp;      // Zaman damgası (Saniye cinsinden)
    uint8_t  safe_mode;      // 1 ise güvenli modda, 0 ise normal
    
    // EPS (Güç Sistemi) Verileri
    float    battery_voltage; // BQ24295'ten okunan voltaj (Örn: 7.4V)
    
    // ADCS (Yönelim) Verileri (RM3100 Manyetometre vb.)
    float    mag_x;
    float    mag_y;
    float    mag_z;
    
    // OBC (Sistem) Verileri
    uint8_t  mcu_temp;       // STM32H743 çekirdek sıcaklığı
    uint16_t free_ram;       // FreeRTOS boş RAM miktarı (Hata ayıklama için kritik)
} BeaconPacket_t;

#endif // TELEMETRY_H