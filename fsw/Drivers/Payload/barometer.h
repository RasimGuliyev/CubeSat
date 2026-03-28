// fsw/Drivers/Payload/barometer.h
// Barometre/Altimetre Sürücüsü (BMP388)
// I2C üzerinden basınç, sıcaklık ve yükseklik ölçümü
#ifndef BAROMETER_H
#define BAROMETER_H

#include <stdint.h>
#include <stddef.h>

// Barometre Durum Kodu
typedef enum {
    BARO_STATUS_OK = 0x00,
    BARO_STATUS_NOT_INITIALIZED = 0x01,
    BARO_STATUS_COMM_ERROR = 0x02,
    BARO_STATUS_INVALID_DATA = 0x03,
    BARO_STATUS_TIMEOUT = 0x04,
    BARO_STATUS_SEU_DETECTED = 0x05
} BarometerStatus_t;

// Barometre Ayarları (Çözünürlük/Güç Modları)
typedef enum {
    BARO_MODE_SLEEP = 0x00,      // Uyku modu
    BARO_MODE_FORCED = 0x01,     // Single measurement
    BARO_MODE_NORMAL = 0x03      // Continuous measurement
} BarometerMode_t;

// Barometre Kalibrasyon Verileri (NVM'den okunacak)
typedef struct {
    int16_t T1, T2, T3;           // Sıcaklık kalibrasyon katsayıları
    int64_t P1, P2, P3;           // Basınç kalibrasyon katsayıları
    int8_t P4, P5, P6, P7;        // Basınç ince tuning
    int8_t T4, T5;                // Sıcaklık ADC ince ayar
} BarometerCalibration_t;

// Barometre Ölçüm Verisi
typedef struct __attribute__((packed)) {
    uint32_t timestamp;           // UTC zamanı
    float    pressure_pa;         // Pascal cinsinden basınç
    float    temperature_c;       // Derece Celsius
    float    altitude_m;          // Metre (deniz seviyesinden)
    float    altitude_raw;        // Ham yükseklik (filtrelenmemiş)
    uint8_t  status;              // BarometerStatus_t
    uint8_t  measurement_valid;   // 1: Geçerli ölçüm
} BarometerMeasurement_t;

// Barometre Konfigürasyonu
typedef struct {
    uint8_t  i2c_address;         // Varsayılan: 0x76 veya 0x77
    uint32_t i2c_baudrate;        // Varsayılan: 400 kHz
    uint16_t sampling_period_ms;  // Ölçüm periyodu
    uint8_t  mode;                // BarometerMode_t
    uint8_t  oversample_press;    // Basınç oversampling (0-5)
    uint8_t  oversample_temp;     // Sıcaklık oversampling (0-5)
    float    sea_level_pressure_pa; // Referans basınç (yükseklik hesabı için)
} BarometerConfig_t;

// ============================================================================
// Sensör Yönetimi
// ============================================================================

/**
 * @brief Barometreyi başlat (I2C handshake + kalibrasyon)
 * @param config: Barometre konfigürasyonu
 * @return 0: Başarılı, -1: Hata
 */
int32_t baro_init(const BarometerConfig_t *config);

/**
 * @brief Sensörden ölçüm oku
 * @param measurement: Ölçüm verisi output buffer
 * @return 0: Başarılı, -1: Hata
 */
int32_t baro_read(BarometerMeasurement_t *measurement);

/**
 * @brief Sensörü moda göre ayarla (sleep/forced/normal)
 * @param mode: BarometerMode_t
 * @return 0: Başarılı, -1: Hata
 */
int32_t baro_set_mode(uint8_t mode);

/**
 * @brief Sensörün durumunu kontrol et
 * @return BarometerStatus_t enum değeri
 */
uint8_t baro_get_status(void);

/**
 * @brief Deniz seviyesi referans basıncını güncelle (yükseklik hesabı için)
 * @param sea_level_pa: Pascal cinsinden basınç
 */
void baro_set_sea_level_pressure(float sea_level_pa);

/**
 * @brief SEU (Single Event Upset) tespiti
 * @return 1: SEU tespit edildi, 0: Normal
 */
uint8_t baro_detect_seu(void);

// ============================================================================
// Kalibrasyon ve Kalibrasyondan Sonra İyileştirme
// ============================================================================

/**
 * @brief Kalibrasyon Verilerini NVM'den oku
 * @param calib: Çıktı kalibrasyon yapısı
 * @return 0: Başarılı, -1: Hata
 */
int32_t baro_read_calibration(BarometerCalibration_t *calib);

/**
 * @brief Kalibrasyona başlat (önce başlatmadan 1 saniye bekle)
 * @return 0: Başarılı, -1: Hata
 */
int32_t baro_calibrate(void);

/**
 * @brief Basınçtan yüksekliği hesapla (hipsometrik formül)
 * @param pressure_pa: Ölçülen basınç
 * @param sea_level_pressure_pa: Referans deniz seviyesi basıncı
 * @return Metre cinsinden yükseklik
 */
float baro_calculate_altitude(float pressure_pa, float sea_level_pressure_pa);

// ============================================================================
// Test ve Debug
// ============================================================================

/**
 * @brief Barometre kendini test et (self-test)
 * @return 0: Test başarılı, -1: Test başarısız
 */
int32_t baro_selftest(void);

/**
 * @brief BMP388 CHIP ID'sini oku (doğrulama)
 * @return CHIP ID (0x50 beklenir)
 */
uint8_t baro_read_chip_id(void);

#endif // BAROMETER_H
