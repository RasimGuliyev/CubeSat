// shared/telemetry.h
#ifndef TELEMETRY_H
#define TELEMETRY_H

#include <stdint.h>

// Uydunun temel sağlık ve durum paketi (Beacon) - Düşük bant genişliği
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
    uint8_t  mcu_temp;       // STM32H743IIK6 çekirdek sıcaklığı
    uint16_t free_ram;       // FreeRTOS boş RAM miktarı (Hata ayıklama için kritik)
} BeaconPacket_t;

// Radyasyon + Çevre Sensörü Paketi (Yüksek çözünürlülü bilimsel veri)
typedef struct __attribute__((packed)) {
    uint32_t timestamp;           // Zaman damgası (UTC saniye)
    uint32_t sample_counter;      // Sıra numarası (SD karta yazma için)
    
    // Radyasyon Verileri (GQ-511A veya benzer)
    uint16_t radiation_cpm;       // Counts Per Minute
    float    radiation_dose_rate; // µSv/h cinsinden doz hızı
    
    // İklim/Basınç Sensörleri
    float    temperature_c;       // Derece Celsius
    float    barometric_pressure_pa; // Pascal cinsinden basınç
    float    altitude_m;          // Metrede hesaplanan yükseklik
    
    // GPS Konumu
    int32_t  latitude_raw;        // 1E-7 derece (microdegrees)
    int32_t  longitude_raw;       // 1E-7 derece (microdegrees)
    uint16_t gps_satellites;      // Bağlı uydu sayısı
    uint8_t  gps_fix_valid;       // GPS fix geçerliliği (1: Geçerli)
    
    // AHRS/ADCS Bilgisi
    float    euler_roll_deg;      // Roll açısı (derece)
    float    euler_pitch_deg;     // Pitch açısı (derece)
    float    euler_yaw_deg;       // Yaw açısı (derece)
    
    // Yazılım Sağlığı
    uint8_t  error_flags;         // Bit maskesi: SEU/Hata flagleri
    uint8_t  task_status;         // Task health bitmap
    uint16_t sd_card_free_mb;     // SD kart kalan alan
} PayloadPacket_t;

// SD Karta yazılan veri formu (telemetri + checksum + şifreleme bilgisi)
typedef struct __attribute__((packed)) {
    PayloadPacket_t payload_data;
    uint16_t crc16;               // Payload CRC16 checksum
    uint8_t  encryption_iv_suffix; // IV'nin son 4 baytı (timestamp gömülü)
} StoredPayloadRecord_t;

// Sistem durumu paketlemesi (tekrar saldırısı+tasdiğu için)
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    uint8_t  sequence_num;        // Counter (IV'ye gömülecek)
    uint8_t  reserved;
} PacketHeader_t;

#endif // TELEMETRY_H