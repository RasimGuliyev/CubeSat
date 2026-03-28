// fsw/Drivers/Payload/radiation_sensor.h
// Radyasyon Sensörü Sürücüsü (GQ-511A / GM Tube based)
// Serial/UART üzerinden veri okuma
#ifndef RADIATION_SENSOR_H
#define RADIATION_SENSOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdint.h>

// Radyasyon Sensörü Türleri
typedef enum {
    RAD_SENSOR_GQ511A = 0x01,  // GQ-511A (Geiger Müller tube)
    RAD_SENSOR_UNKNOWN = 0xFF
} RadiationSensorType_t;

// Sensör Durum Kodu
typedef enum {
    RAD_STATUS_OK = 0x00,
    RAD_STATUS_INIT_FAILED = 0x01,
    RAD_STATUS_COMM_ERROR = 0x02,
    RAD_STATUS_DATA_INVALID = 0x03,
    RAD_STATUS_TIMEOUT = 0x04,
    RAD_STATUS_SEU_DETECTED = 0x05  // Radyasyon kaynaklı single event upset
} RadiationStatus_t;

// Radyasyon Sensörü Ölçüm Verisi
typedef struct __attribute__((packed)) {
    uint32_t timestamp;           // UTC saniye
    uint16_t cpm;                 // Counts Per Minute (Raw)
    float    dose_rate_usv_h;     // µSv/h cinsinden doz hızı
    float    cumulative_dose_usv; // Toplam birikmişdoz
    uint8_t  status;              // RadiationStatus_t
    uint8_t  signal_strength;     // 0-100% ISM bandında
} RadiationMeasurement_t;

// Sensör Konfigürasyonu
typedef struct {
    uint8_t sensor_type;           // RadiationSensorType_t
    uint32_t uart_baudrate;        // Tipik olarak 115200 bps
    uint16_t sampling_period_ms;   // Ölçüm periyodu (önerilen: 1000 ms)
    float conversion_factor;       // CPM → µSv/h dönüştürme faktörü
} RadiationSensorConfig_t;

// ============================================================================
// Sensör Yönetimi
// ============================================================================

/**
 * @brief Radyasyon sensörünü başlat
 * @param config: Sensör konfigürasyonu
 * @return 0: Başarılı, -1: Hata
 */
int32_t rad_sensor_init(const RadiationSensorConfig_t *config);

/**
 * @brief Sensörden son ölçümü oku
 * @param measurement: Ölçüm verisi output buffer
 * @return 0: Başarılı, -1: Hata
 */
int32_t rad_sensor_read(RadiationMeasurement_t *measurement);

/**
 * @brief Sensör kalibrasyon faktörünü güncelle
 * @param factor: Yeni dönüştürme faktörü (µSv/h per CPM)
 */
void rad_sensor_set_calibration_factor(float factor);

/**
 * @brief Sensörü suspend et (enerji tasarrufu)
 */
void rad_sensor_sleep(void);

/**
 * @brief Sensörü uyan (aktif mod)
 */
void rad_sensor_wake(void);

/**
 * @brief Sensörün durumunu kontrol et
 * @return RadiationStatus_t enum değeri
 */
uint8_t rad_sensor_get_status(void);

/**
 * @brief SEU (Single Event Upset) tespiti - radyasyon nedeniyle memoryhasar kontrolü
 * @return 1: SEU tespit edildi, 0: Normal
 */
uint8_t rad_sensor_detect_seu(void);

// ============================================================================
// Kalibrasyonve Test
// ============================================================================

/**
 * @brief Sensör kendini test et (self-test)
 * @return 0: Test başarılı, -1: Test başarısız
 */
int32_t rad_sensor_selftest(void);

/**
 * @brief CPM'den µSv/h'ye dönüştür
 * @param cpm: Counts Per Minute
 * @return µSv/h cinsinden doz hızı
 */
float rad_sensor_cpm_to_dose_rate(uint16_t cpm);

#endif // RADIATION_SENSOR_H
