// fsw/Drivers/Payload/gps.h
// GPS Navigasyon Sürücüsü (NEO-M9N vb. u-blox modülü)
// UART/Serial üzerinden NMEA protokolü ile konum okuma
#ifndef GPS_H
#define GPS_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

// GPS Durum Kodu
typedef enum {
    GPS_STATUS_OK = 0x00,
    GPS_STATUS_NOT_FIXED = 0x01,       // Konumlandırma yapılmadı
    GPS_STATUS_NOT_INITIALIZED = 0x02,
    GPS_STATUS_COMM_ERROR = 0x03,
    GPS_STATUS_INVALID_DATA = 0x04,
    GPS_STATUS_TIMEOUT = 0x05,
    GPS_STATUS_SEU_DETECTED = 0x06
} GPSStatus_t;

// GPS Fix Tipi
typedef enum {
    GPS_FIX_INVALID = 0x00,
    GPS_FIX_GNSS_ONLY = 0x01,
    GPS_FIX_2D = 0x02,
    GPS_FIX_3D = 0x03,
    GPS_FIX_DGPS = 0x04,
    GPS_FIX_RTK_FLOAT = 0x05,
    GPS_FIX_RTK_FIXED = 0x06
} GPSFixType_t;

// GPS Ölçüm Verisi
typedef struct __attribute__((packed)) {
    uint32_t timestamp;             // UTC zamanı (saniye)
    uint32_t utm_timestamp;         // Micro zamanı (ns) - hassas saat
    
    // Konum (WGS84 koordinat sistemi)
    double   latitude_deg;          // Derece (-90 ~ 90)
    double   longitude_deg;         // Derece (-180 ~ 180)
    float    altitude_m;            // Metre (MSL)
    float    ellipsoidal_height_m;  // Ellipsoid yüksekliği
    
    // Fix Kalitesi
    uint8_t  fix_type;              // GPSFixType_t
    uint8_t  num_satellites;        // Bağlı uydu sayısı
    float    hdop;                  // Yatay Dilution of Precision
    float    vdop;                  // Dikey Dilution of Precision
    
    // Hız ve Yön
    float    velocity_mps;          // Hız (m/s)
    float    heading_deg;           // Yön (0-360°)
    
    // Doğruluk
    float    horizontal_accuracy_m; // Yatay doğruluk (m)
    float    vertical_accuracy_m;   // Dikey doğruluk (m)
    
    uint8_t  status;                // GPSStatus_t
} GPSMeasurement_t;

// GPS Konfigürasyonu
typedef struct {
    uint32_t uart_baudrate;         // Varsayılan: 38400 bps
    uint16_t update_rate_ms;        // Güncelleme periyodu (varsayılan: 1000 ms = 1 Hz)
    uint8_t  cold_start;            // 1: Cold start yapılsın
    uint8_t  use_dgps;              // 1: DGPS / RTK kullan
    char     nmea_sentences[256];   // NMEA cümleleri (GGA, RMC, vb.)
} GPSConfig_t;

// ============================================================================
// Sensör Yönetimi
// ============================================================================

/**
 * @brief GPS'i başlat (UART + warm-up)
 * @param config: GPS konfigürasyonu
 * @return 0: Başarılı, -1: Hata
 */
int32_t gps_init(const GPSConfig_t *config);

/**
 * @brief GPS'ten konum oku
 * @param measurement: Konum ölçüm verisi output buffer
 * @return 0: Başarılı, -1: Hata (fix yok)
 */
int32_t gps_read(GPSMeasurement_t *measurement);

/**
 * @brief GPS durumunu kontrol et
 * @return GPSStatus_t enum değeri
 */
uint8_t gps_get_status(void);

/**
 * @brief Fix tipi nedir?
 * @return GPSFixType_t (0 ise no fix)
 */
uint8_t gps_get_fix_type(void);

/**
 * @brief Uydu sayısını oku
 * @return Bağlı uydu sayısı
 */
uint8_t gps_get_num_satellites(void);

/**
 * @brief HDOP (Yatay DOP) oku
 * @return HDOP değeri
 */
float gps_get_hdop(void);

/**
 * @brief Son geçerli konumu oku (cache'den)
 * @param measurement: Çıktı buffer
 * @return 0: Başarılı, -1: Konumlama yapılmadı
 */
int32_t gps_read_cached_position(GPSMeasurement_t *measurement);

/**
 * @brief Konum hassasiyetini kontrol et (DOP).
 * @return DOP değeri (< 5 iyi, < 10 kabul edilebilir)
 */
float gps_get_position_accuracy(void);

/**
 * @brief Saat senkronizasyonu (GPS UTC'si ile)
 * @param utc_timestamp: UTC zamanı (saniye)
 * @return 0: Başarılı, -1: Hata
 */
int32_t gps_sync_rtc(uint32_t utc_timestamp);

/**
 * @brief GPS'i uykuya al (güç tasarrufu)
 */
void gps_sleep(void);

/**
 * @brief GPS'i uyan
 */
void gps_wake(void);

// ============================================================================
// NMEA Protokolü (Low-level)
// ============================================================================

/**
 * @brief NMEA cümlesini parse et
 * @param nmea_sentence: $GPRMC,... gibi NMEA cümlesi
 * @param measurement: Parse edilen veriler
 * @return 0: Başarılı, -1: Parse hatası
 */
int32_t gps_parse_nmea_sentence(const char *nmea_sentence,
                               GPSMeasurement_t *measurement);

/**
 * @brief NMEA checksum'unu doğrula
 * @param sentence: NMEA cümlesi
 * @return 1: Geçerli, 0: Bozuk
 */
uint8_t gps_verify_nmea_checksum(const char *sentence);

// ============================================================================
// Test ve Debug
// ============================================================================

/**
 * @brief GPS modülünü kendini test et
 * @return 0: Test başarılı, -1: Hata
 */
int32_t gps_selftest(void);

/**
 * @brief Koordinatları basit formata çevir (dd.ddddd)
 * @param lat: Enlem raw (1E-7 derece)
 * @param lon: Boylam raw (1E-7 derece)
 * @param lat_str: Çıktı buffer (minimum 16 byte)
 * @param lon_str: Çıktı buffer (minimum 16 byte)
 */
void gps_format_coordinates(int32_t lat, int32_t lon, char *lat_str, char *lon_str);

/**
 * @brief SEU (Single Event Upset) deteksiyonu
 * @return 1: SEU tespit edildi, 0: Normal
 */
uint8_t gps_detect_seu(void);

#endif // GPS_H
