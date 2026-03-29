// fsw/Drivers/Payload/radiation_sensor.c
// Radyasyon Sensörü Sürücüsü (GQ-511A) Implementasyonu
// DMA + UART interrupt based, FreeRTOS queue ile haberleşme

#include "radiation_sensor.h"
#include <string.h>
#include <math.h>

// FreeRTOS headers (gerçek projeye eklenecek)
// #include "FreeRTOS.h"
// #include "queue.h"

// STM32H743IIK6 UART headers (gerçek projeye eklenecek)
// #include "stm32h7xx_hal.h"

// ============================================================================
// Sensör Durum Değişkenleri (Global State - SEU koruması ile)
// ============================================================================

static volatile struct {
    RadiationSensorConfig_t config;
    RadiationMeasurement_t last_measurement;
    uint32_t last_read_timestamp;
    uint8_t sensor_initialized;
    uint8_t seu_counter;  // SEU deteksiyonu için sayaç
    float calibration_factor;  // CPM → µSv/h dönüştürme
} rad_sensor_state = {0};

// UART Receive Buffer (DMA ile dolu olacak)
#define RAD_SENSOR_RX_BUFFER_SIZE 64
static volatile uint8_t rad_sensor_rx_buffer[RAD_SENSOR_RX_BUFFER_SIZE] = {0};
static volatile uint16_t rad_sensor_rx_index = 0;

// ============================================================================
// Yardımcı Fonksiyonlar
// ============================================================================

/**
 * @brief Sensör State'ini validate et (SEU deteksiyonu)
 * Radyasyon ortamında hafıza bozulması olabilir
 */
static uint8_t rad_sensor_validate_state(void) {
    // Temel kontrol: Config yapısı geçerli mi?
    if (rad_sensor_state.config.sensor_type == 0x00 ||
        rad_sensor_state.config.sensor_type > 0xFF) {
        rad_sensor_state.seu_counter++;
        if (rad_sensor_state.seu_counter > 3) {
            return 0;  // SEU tespit edildi
        }
    }
    
    rad_sensor_state.seu_counter = 0;
    return 1;  // State geçerli
}

/**
 * @brief UART verisi parse et ve CPM çıkart
 * GQ-511A protokolü: << CPM >> (ASCII format)
 */
static uint16_t rad_sensor_parse_uart_data(const uint8_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return 0;
    }
    
    uint16_t cpm = 0;
    
    // Basit parse: ilk '<< ' skip, sonra sayı oku
    for (size_t i = 0; i < len; i++) {
        if (buffer[i] >= '0' && buffer[i] <= '9') {
            cpm = cpm * 10 + (buffer[i] - '0');
        }
    }
    
    return cpm;
}

/**
 * @brief Checksum kontrol (data integrity)
 */
static uint8_t rad_sensor_verify_checksum(const uint8_t *data, size_t len) {
    if (data == NULL || len < 2) {
        return 0;
    }
    
    uint8_t calculated_sum = 0;
    for (size_t i = 0; i < len - 1; i++) {
        calculated_sum ^= data[i];
    }
    
    return (calculated_sum == data[len - 1]) ? 1 : 0;
}

// ============================================================================
// Kalibrasyonve Dönüştürme
// ============================================================================

float rad_sensor_cpm_to_dose_rate(uint16_t cpm) {
    // Varsayılan kalibrasyon: 151 CPM = 1 µSv/h (GQ-511A için)
    float factor = (rad_sensor_state.calibration_factor > 0.0f) ?
                   rad_sensor_state.calibration_factor : (1.0f / 151.0f);
    
    return (float)cpm * factor;
}

void rad_sensor_set_calibration_factor(float factor) {
    if (factor > 0.0f) {
        rad_sensor_state.calibration_factor = factor;
    }
}

// ============================================================================
// Sensör Yönetimi
// ============================================================================

int32_t rad_sensor_init(const RadiationSensorConfig_t *config) {
    // NULL pointer kontrolü (SEU koruması)
    if (config == NULL) {
        return -1;
    }
    
    // Konfigürasyonlı kopyala
    memcpy((void *)&rad_sensor_state.config, config, sizeof(RadiationSensorConfig_t));
    
    // Kalibrasyon faktörü seti
    if (rad_sensor_state.config.conversion_factor > 0.0f) {
        rad_sensor_state.calibration_factor = rad_sensor_state.config.conversion_factor;
    } else {
        rad_sensor_state.calibration_factor = 1.0f / 151.0f;  // Default GQ-511A
    }
    
    // TODO: HAL_UART_Init() çağrı (STM32 UART başlatma)
    // uint8_t uart_handle = 3;  // UART3 varsayımı
    // if (HAL_UART_Init(...) != HAL_OK) {
    //     return -1;
    // }
    
    // TODO: DMA başlatma (UART RX DMA)
    
    rad_sensor_state.sensor_initialized = 1;
    rad_sensor_state.last_read_timestamp = 0;
    rad_sensor_state.seu_counter = 0;
    
    return 0;
}

int32_t rad_sensor_read(RadiationMeasurement_t *measurement) {
    // NULL pointer kontrolü
    if (measurement == NULL) {
        return -1;
    }
    
    // Sensör başlatılmış mı?
    if (!rad_sensor_state.sensor_initialized) {
        return -1;
    }
    
    // State validate (SEU kontrolü)
    if (!rad_sensor_validate_state()) {
        return -1;
    }
    
    // TODO: UART'tan veri oku (blocking veya interrupt-based)
    // uint8_t rx_data[RAD_SENSOR_RX_BUFFER_SIZE];
    // if (HAL_UART_Receive(..., rx_data, ..., 1000) != HAL_OK) {
    //     return -1;  // UART timeout
    // }
    
    // Simülasyon: Son buffer'dan parse et
    uint16_t cpm = rad_sensor_parse_uart_data(
        (const uint8_t *)rad_sensor_rx_buffer,
        rad_sensor_rx_index
    );
    
    // TODO: CPM = 0 ise timeout olabileceğini kontrol et
    
    // Ölçüm yapı'sını doldur
    measurement->timestamp = 0;  // TODO: xTaskGetTickCount() veya system time
    measurement->cpm = cpm;
    measurement->dose_rate_usv_h = rad_sensor_cpm_to_dose_rate(cpm);
    measurement->cumulative_dose_usv = 0.0f;  // TODO: SD karttan veya EEPROM'dan oku
    measurement->status = RAD_STATUS_OK;
    measurement->signal_strength = 95;  // TODO: RSSI oku
    
    // Son ölçümü kaydet
    memcpy((void *)&rad_sensor_state.last_measurement, measurement,
           sizeof(RadiationMeasurement_t));
    
    return 0;
}

void rad_sensor_sleep(void) {
    // TODO: UART'ı disable et, sensörü suspend et
    // HAL_UART_DeInit(...);
}

void rad_sensor_wake(void) {
    // TODO: UART'ı re-enable et
    // HAL_UART_Init(...);
}

uint8_t rad_sensor_get_status(void) {
    if (!rad_sensor_state.sensor_initialized) {
        return RAD_STATUS_INIT_FAILED;
    }
    
    return rad_sensor_state.last_measurement.status;
}

uint8_t rad_sensor_detect_seu(void) {
    // SEU sayacı 3'ten fazlaysa SEU var demek
    return (rad_sensor_state.seu_counter > 3) ? 1 : 0;
}

int32_t rad_sensor_selftest(void) {
    // TODO: Sensörün kendini test etmesini sağla
    // Örn: Known radiation source ile test
    // GQ-511A: <START_TEST> komutu gönder
    
    return 0;  // Başarılı (simülasyon)
}

// ============================================================================
// UART Interrupt Handler (ISR - Interrupt Service Routine)
// ============================================================================

// Bu fonksiyon STM32 UART interrupt'ında çağrılacak
extern void rad_sensor_uart_interrupt_handler(uint8_t rx_byte) {
    // Basit circular buffer
    if (rad_sensor_rx_index >= RAD_SENSOR_RX_BUFFER_SIZE) {
        rad_sensor_rx_index = 0;  // Wrap around
    }
    
    rad_sensor_rx_buffer[rad_sensor_rx_index++] = rx_byte;
    
    // Satır sonu ('\n' veya '\r') tespit edilirse hazır
    if (rx_byte == '\n' || rx_byte == '\r') {
        // TODO: FreeRTOS queue'ye signal gönder
        // BaseType_t higher_priority_task_woken = pdFALSE;
        // xQueueSendFromISR(radiation_data_queue, &rad_sensor_rx_buffer, &higher_priority_task_woken);
        // portYIELD_FROM_ISR(higher_priority_task_woken);
        
        rad_sensor_rx_index = 0;  // Buffer'ı sıfırla
    }
}

// ============================================================================
// DEBUG / TEST
// ============================================================================

#ifdef DEBUG_RADIATION_SENSOR

void rad_sensor_debug_print_state(void) {
    // (DEBUG) State print
    // printf("[RAD] Init: %u, SEU Counter: %u\n",
    //        rad_sensor_state.sensor_initialized,
    //        rad_sensor_state.seu_counter);
    // printf("[RAD] Last CPM: %u, Dose: %.2f µSv/h\n",
    //        rad_sensor_state.last_measurement.cpm,
    //        rad_sensor_state.last_measurement.dose_rate_usv_h);
}

#endif // DEBUG_RADIATION_SENSOR
