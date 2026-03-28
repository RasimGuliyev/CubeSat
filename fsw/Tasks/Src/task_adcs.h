// fsw/Tasks/Src/task_adcs.h
// ADCS Task Header - Yönelim Kontrol Sistemi (RM3100, Güneş Sensörleri, Reaksiyon Tekerleği)
#ifndef TASK_ADCS_H
#define TASK_ADCS_H

#include <stdint.h>

// ADCS Task Konfigürasyonu
typedef struct {
    uint16_t control_period_ms;     // Kontrol döngüsü periyodu (100-200 ms)
    uint8_t  control_mode;          // 0: Güneş-takibi, 1: Manyetik, 2: Sabit
    float    target_spin_rate_rpm;  // Hedef dönme hızı
    float    kalman_q;              // Kalman Filter Q (durum gürültü)
    float    kalman_r;              // Kalman Filter R (ölçüm gürültüsü)
    uint8_t  enable_magnetorquer;   // 1: Magnetorquer aktif
    uint8_t  enable_reaction_wheel; // 1: Reaksiyon tekerleği aktif
} ADCSTaskConfig_t;

// ADCS Task Durum Kodu
typedef enum {
    ADCS_OK = 0x00,
    ADCS_INIT_FAILED = 0x01,
    ADCS_SENSOR_ERROR = 0x02,
    ADCS_ACTUATOR_ERROR = 0x03,
    ADCS_COMPUTATION_ERROR = 0x04,
    ADCS_SATURATION = 0x05  // Actuator doyum (max. limit)
} ADCSTaskStatus_t;

// AHRS (Attitude and Heading Reference System) Çıkışı
typedef struct __attribute__((packed)) {
    uint32_t timestamp;
    float    euler_roll_deg;        // X ekseni rotasyonu (-180 ~ 180°)
    float    euler_pitch_deg;       // Y ekseni rotasyonu (-90 ~ 90°)
    float    euler_yaw_deg;         // Z ekseni rotasyonu (-180 ~ 180°)
    
    // Quaternion (Gimbal lock olmadan)
    float    quat_w, quat_x, quat_y, quat_z;
    
    // Açısal Hız (rad/s)
    float    gyro_x, gyro_y, gyro_z;
    
    // Manyetik Alan (mT)
    float    mag_x, mag_y, mag_z;
    
    uint8_t  attitude_valid;        // 1: Geçerli tutum
    uint8_t  heading_valid;         // 1: Geçerli başlık
} ADCSAttitude_t;

/**
 * @brief ADCS Task'ı başlat
 * @param config: ADCS konfigürasyonu
 * @return 0: Başarılı, -1: Hata
 */
int32_t adcs_task_init(const ADCSTaskConfig_t *config);

/**
 * @brief FreeRTOS Task fonksiyonu
 */
void adcs_task(void *pvParameters);

/**
 * @brief ADCS Task durumunu kontrol et
 * @return ADCSTaskStatus_t enum değeri
 */
uint8_t adcs_task_get_status(void);

/**
 * @brief Son AHRS ölçümünü oku
 * @param attitude: Çıktı buffer
 * @return 0: Başarılı, -1: Hata
 */
int32_t adcs_task_read_attitude(ADCSAttitude_t *attitude);

/**
 * @brief Spin hızını oku (rpm)
 * @return Mevcut dönme hızı (rpm)
 */
float adcs_task_get_spin_rate(void);

/**
 * @brief Manyetrometre kalibrasyonu
 * @return 0: Başarılı, -1: Hata
 */
int32_t adcs_task_calibrate_magnetometer(void);

#endif // TASK_ADCS_H
