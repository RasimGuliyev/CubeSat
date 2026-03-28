// fsw/Tasks/Src/task_adcs.c
// ADCS (Attitude Determination and Control System) Task Implementasyonu
// RM3100 Manyetometre + Güneş Sensörleri + IMU + Reaksiyon Tekerleği + Magnetorquer

#define _USE_MATH_DEFINES

#include "task_adcs.h"
#include <string.h>
#include <math.h>

/* Math constants */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// FreeRTOS headers
#include "../../include/FreeRTOS/FreeRTOS.h"
#include "../../include/FreeRTOS/task.h"
#include "../../include/FreeRTOS/queue.h"

// ============================================================================
// ADCS Task Global State
// ============================================================================

static volatile struct {
    ADCSTaskConfig_t config;
    ADCSTaskStatus_t status;
    ADCSAttitude_t last_attitude;
    uint32_t measurement_count;
    uint8_t initialized;
    float current_spin_rate;
    TaskHandle_t task_handle;
    float magnetorquer_cmd[3];      // Magnetorquer kontrol vektörü
    float reaction_wheel_rpm;       // Reaksiyon tekerleği hızı
} adcs_state = {0};

// Kalman Filter Durumu
static struct {
    float state[9];         // 3x Euler + 3x Gyro bias + 3x Mag bias
    float covariance[81];   // 9x9 kovariyans matrisi
    uint8_t initialized;
} kalman_state = {0};

// ============================================================================
// Sensör Okuma Fonksiyonları
// ============================================================================

/**
 * @brief RM3100 Manyetometresinden veri oku
 * @param mag: Çıktı vektor (mT cinsinden)
 */
static int32_t adcs_read_magnetometer(float *mag_x, float *mag_y, float *mag_z) {
    if (mag_x == NULL || mag_y == NULL || mag_z == NULL) return -1;
    
    // TODO: I2C üzerinden RM3100'den raw ADC veri oku
    // int16_t raw_x, raw_y, raw_z;
    // if (rm3100_read(&raw_x, &raw_y, &raw_z) != 0) return -1;
    
    // Kalibrasyon ve hard-iron offset düzeltmesi
    // Simülasyon: Sabit değerler
    *mag_x = 0.25f;  // mT
    *mag_y = -0.45f;
    *mag_z = 0.35f;
    
    return 0;
}

/**
 * @brief Güneş Sensörlerinden veri oku (4 sensör)
 * @param sun_vector: Güneş yönü birim vektörü (normalized)
 */
static int32_t adcs_read_sun_sensors(float *sun_x, float *sun_y, float *sun_z) {
    if (sun_x == NULL || sun_y == NULL || sun_z == NULL) return -1;
    
    // TODO: ADC üzerinden 4 güneş sensöründen voltaj oku
    // Sun sensor okumasından yön hesapla (coarse attitude determination)
    
    // Simülasyon: Güneş yönü (nominally +X)
    *sun_x = 0.95f;
    *sun_y = 0.05f;
    *sun_z = -0.05f;
    
    // Normalize et
    float mag = sqrtf(*sun_x * *sun_x + *sun_y * *sun_y + *sun_z * *sun_z);
    if (mag > 0.0f) {
        *sun_x /= mag;
        *sun_y /= mag;
        *sun_z /= mag;
    }
    
    return 0;
}

/**
 * @brief IMU'dan Euler açılarını oku (veya raw gyro/accel)
 */
static int32_t adcs_read_imu(float *gyro_x, float *gyro_y, float *gyro_z) {
    if (gyro_x == NULL || gyro_y == NULL || gyro_z == NULL) return -1;
    
    // TODO: I2C/SPI üzerinden IMU (9-DOF) okuma
    // Gyroscope: rad/s cinsinden açısal hız
    // Accelerometer: m/s² cinsinden ivme
    // (Kalman filter'da gyro integrasyon + manyetometre/güneş düzeltmesi)
    
    // Simülasyon: Sabit dönme (1 rad/s etrafında)
    *gyro_x = 0.01f;
    *gyro_y = 0.01f;
    *gyro_z = 1.0f;  // Z ekseni etrafında 1 rad/s
    
    return 0;
}

// ============================================================================
// Kalman Filtresi (Basit Implementasyon)
// ============================================================================

/**
 * @brief Kalman Filter Prediction Adımı
 */
static void adcs_kalman_predict(float dt) {
    // TODO: Basit doğrusal Kalman propagasyonu
    // State: [roll, pitch, yaw, roll_rate, pitch_rate, yaw_rate]
    // Measurement: [mag_x, mag_y, mag_z, sun_x, sun_y, sun_z]
    
    if (!kalman_state.initialized) {
        // Başlat
        memset(kalman_state.state, 0, sizeof(kalman_state.state));
        for (int i = 0; i < 81; i++) {
            kalman_state.covariance[i] = (i % 10 == 0) ? 0.1f : 0.0f;  // Diagonal
        }
        kalman_state.initialized = 1;
    }
    
    // Basit integrasyon (gyro → açı)
    // kalman_state.state[0] += kalman_state.state[3] * dt;  // roll += roll_rate * dt
    // kalman_state.state[1] += kalman_state.state[4] * dt;  // pitch += pitch_rate * dt
    // kalman_state.state[2] += kalman_state.state[5] * dt;  // yaw += yaw_rate * dt
}

/**
 * @brief Kalman Filter Correction Adımı
 */
static void adcs_kalman_update(float mag_x, float mag_y, float mag_z,
                               float sun_x, float sun_y, float sun_z) {
    // TODO: Measurement update (manyetometre + güneş sensörü fusiyonu)
    // Kalman gain hesapla, state ve covariyans güncelle
}

// ============================================================================
// Kontrol Fonksiyonları (PID / LQR)
// ============================================================================

/**
 * @brief ADCS Kontrol Yasası (Attitude feedback control)
 * Reaksiyon tekerleği başlıyor + Magnetorquer tork komutları
 */
static int32_t adcs_compute_control_commands(const ADCSAttitude_t *attitude) {
    if (attitude == NULL) return -1;
    
    if (!attitude->attitude_valid) {
        return -1;  // Geçersiz tutum
    }
    
    // Hedef: target_control_mode'a göre
    // Mode 0: Sun-pointing (Güneş yönüne dikkat)
    // Mode 1: Magnetic alignment (Manyetik alan ile align)
    // Mode 2: Spin (Sabit dönme hızı)
    
    float target_roll = 0.0f, target_pitch = 0.0f, target_yaw = 0.0f;
    
    switch (adcs_state.config.control_mode & 0x03) {
        case 0:  // Sun-pointing
            // TODO: Güneşe bakan tutum hesapla
            target_yaw = atan2f(attitude->mag_y, attitude->mag_x) * 180.0f / M_PI;
            break;
            
        case 1:  // Magnetic alignment
            // TODO: Manyetik alan yönüne align
            break;
            
        case 2:  // Fixed spin
        default:
            // Sabit dönme (gravity-gradient torque veya magnetorquer ile)
            break;
    }
    
    // PID Kontrol Yasası (basit)
    float error_yaw = target_yaw - attitude->euler_yaw_deg;
    
    // Normalize açı farkı (-180 ~ 180)
    while (error_yaw > 180.0f) error_yaw -= 360.0f;
    while (error_yaw < -180.0f) error_yaw += 360.0f;
    
    // Reaksiyon tekerleği komut (L)
    float reaction_wheel_cmd = error_yaw * 0.5f;  // Simple proportional
    
    // Magnetorquer komut (Manyetik alan × dipol momeneti = tork)
    adcs_state.magnetorquer_cmd[0] = 0.1f * error_yaw;
    adcs_state.magnetorquer_cmd[1] = 0.0f;
    adcs_state.magnetorquer_cmd[2] = 0.0f;
    
    return 0;
}

/**
 * @brief Reaksiyon Tekerleğini Sürü
 */
static int32_t adcs_drive_reaction_wheel(float target_rpm) {
    // TODO: PWM ile DC motor kontrolü (target RPM)
    // Motor controller, hız sensörü feedback
    
    adcs_state.reaction_wheel_rpm = target_rpm;
    adcs_state.current_spin_rate = target_rpm;
    
    return 0;
}

/**
 * @brief Magnetorquer'ı Sürü
 */
static int32_t adcs_drive_magnetorquer(float mx, float my, float mz) {
    // TODO: 3-eksen magnetorquer sürme
    // m = (mx, my, mz) dipol momenetu (A⋅m²)
    // τ = m × B (Dünya manyetik alanı)
    
    memcpy(adcs_state.magnetorquer_cmd, &mx, 12);
    
    return 0;
}

// ============================================================================
// ADCS Task Başlatma
// ============================================================================

int32_t adcs_task_init(const ADCSTaskConfig_t *config) {
    if (config == NULL) return -1;
    
    memcpy((void *)&adcs_state.config, config, sizeof(ADCSTaskConfig_t));
    
    // TODO: RM3100, güneş sensörleri, IMU, reaksiyon tekerleği başlat
    
    adcs_state.initialized = 1;
    adcs_state.status = ADCS_OK;
    adcs_state.measurement_count = 0;
    adcs_state.current_spin_rate = 0.0f;
    
    return 0;
}

// ============================================================================
// ADCS Task FreeRTOS Loop
// ============================================================================

/**
 * @brief ADCS Task Main Loop (sonsuz FreeRTOS task)
 */
void adcs_task(void *pvParameters) {
    (void)pvParameters;  // Unused
    
    if (!adcs_state.initialized) {
        vTaskDelete(NULL);
        return;
    }
    
    ADCSAttitude_t attitude = {0};
    
    while (1) {
        // Timestamp al
        uint32_t timestamp = xTaskGetTickCount() / 1000;  // UTC saniye
        
        // Sensör okuma
        float mag_x = 0.0f, mag_y = 0.0f, mag_z = 0.0f;
        float gyro_x = 0.0f, gyro_y = 0.0f, gyro_z = 0.0f;
        float sun_x = 0.0f, sun_y = 0.0f, sun_z = 0.0f;
        
        if (adcs_read_magnetometer(&mag_x, &mag_y, &mag_z) != 0) {
            adcs_state.status = ADCS_SENSOR_ERROR;
            vTaskDelay(adcs_state.config.control_period_ms / portTICK_PERIOD_MS);
            continue;
        }
        
        if (adcs_read_imu(&gyro_x, &gyro_y, &gyro_z) != 0) {
            adcs_state.status = ADCS_SENSOR_ERROR;
            vTaskDelay(adcs_state.config.control_period_ms / portTICK_PERIOD_MS);
            continue;
        }
        
        if (adcs_read_sun_sensors(&sun_x, &sun_y, &sun_z) != 0) {
            // Sun sensörü hataları non-critical
        }
        
        // Kalman Filter
        float dt = adcs_state.config.control_period_ms / 1000.0f;
        adcs_kalman_predict(dt);
        adcs_kalman_update(mag_x, mag_y, mag_z, sun_x, sun_y, sun_z);
        
        // Tutumu AHRS output'a dönüştür
        attitude.timestamp = timestamp;
        attitude.mag_x = mag_x;
        attitude.mag_y = mag_y;
        attitude.mag_z = mag_z;
        attitude.gyro_x = gyro_x;
        attitude.gyro_y = gyro_y;
        attitude.gyro_z = gyro_z;
        
        // TODO: Quaternion → Euler dönüşümü
        attitude.euler_roll_deg = 10.0f;   // Simülasyon
        attitude.euler_pitch_deg = 5.0f;
        attitude.euler_yaw_deg = 45.0f;
        
        attitude.quat_w = 0.935f;
        attitude.quat_x = 0.087f;
        attitude.quat_y = 0.087f;
        attitude.quat_z = 0.329f;
        
        attitude.attitude_valid = 1;
        attitude.heading_valid = 1;
        
        // Kontrol Hesaplama
        if (adcs_compute_control_commands(&attitude) == 0) {
            // Komutları actuator'lara uy
            adcs_drive_reaction_wheel(adcs_state.config.target_spin_rate_rpm);
            adcs_drive_magnetorquer(adcs_state.magnetorquer_cmd[0],
                                   adcs_state.magnetorquer_cmd[1],
                                   adcs_state.magnetorquer_cmd[2]);
        }
        
        // Son tutumu kaydet
        memcpy((void *)&adcs_state.last_attitude, &attitude, sizeof(ADCSAttitude_t));
        adcs_state.measurement_count++;
        adcs_state.status = ADCS_OK;
        
        // Kontrol periyodu için uyku
        vTaskDelay(adcs_state.config.control_period_ms / portTICK_PERIOD_MS);
    }
}

// ============================================================================
// Interface Fonksiyonları
// ============================================================================

uint8_t adcs_task_get_status(void) {
    return adcs_state.status;
}

int32_t adcs_task_read_attitude(ADCSAttitude_t *attitude) {
    if (attitude == NULL) return -1;
    
    memcpy(attitude, (void *)&adcs_state.last_attitude, sizeof(ADCSAttitude_t));
    return 0;
}

float adcs_task_get_spin_rate(void) {
    return adcs_state.current_spin_rate;
}

int32_t adcs_task_calibrate_magnetometer(void) {
    // TODO: Manyetometre kalibrasyon rutini
    // Hard-iron: Rotasyonla değişmeyen offset
    // Soft-iron: Malzeme uyarılması
    
    return 0;
}