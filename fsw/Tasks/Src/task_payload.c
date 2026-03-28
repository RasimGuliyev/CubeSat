// fsw/Tasks/Src/task_payload.c
// Payload Task Implementasyonu - Radyasyon, Barometre, GPS veri toplama
// FreeRTOS Task olarak çalışır, periyodik olarak veri okur ve queue'ya koyar

#include "task_payload.h"
#include "../../../Drivers/Payload/radiation_sensor.h"
#include "../../../Drivers/Payload/barometer.h"
#include "../../../Drivers/Payload/gps.h"
#include "../../../Drivers/Storage/sd_card.h"
#include "../../../Middleware/AES256/crypto.h"
#include <string.h>

// FreeRTOS headers
// #include "FreeRTOS.h"
// #include "task.h"
// #include "queue.h"
// #include "semphr.h"

// ============================================================================
// Payload Task Global State
// ============================================================================

static volatile struct {
    PayloadTaskConfig_t config;
    PayloadTaskStatus_t status;
    PayloadPacket_t last_packet;
    uint32_t packet_count;
    uint8_t initialized;
    uint8_t  pau_error_flags;   // Payload Application Unit hata bayrakları
    TaskHandle_t task_handle;   // FreeRTOS task handle
    QueueHandle_t data_queue;   // Output queue
} payload_state = {0};

// Kriptografi bağlamı
static CryptoContext_t crypto_ctx = {0};

// Sensör Handle'ları
static int32_t rad_sensor_handle = -1;
static int32_t baro_handle = -1;
static int32_t gps_handle = -1;

// ============================================================================
// Yardımcı Fonksiyonlar
// ============================================================================

/**
 * @brief Tüm sensörleri başlat
 */
static int32_t payload_sensors_init(void) {
    // Radyasyon Sensörü (GQ-511A)
    RadiationSensorConfig_t rad_config = {
        .sensor_type = 0x01,           // GQ-511A
        .uart_baudrate = 115200,
        .sampling_period_ms = payload_state.config.sampling_period_ms,
        .conversion_factor = 1.0f / 151.0f  // 151 CPM = 1 µSv/h
    };
    
    if (rad_sensor_init(&rad_config) != 0) {
        payload_state.status = PAYLOAD_SENSOR_ERROR;
        payload_state.pau_error_flags |= 0x01;  // Radyasyon sensörü hatası
        return -1;
    }
    
    // Barometre (BMP388)
    BarometerConfig_t baro_config = {
        .i2c_address = 0x76,
        .i2c_baudrate = 400000,
        .sampling_period_ms = payload_state.config.sampling_period_ms,
        .mode = 0x03,           // BARO_MODE_NORMAL
        .oversample_press = 3,  // x4 oversampling
        .oversample_temp = 1,   // x2 oversampling
        .sea_level_pressure_pa = 101325.0f
    };
    
    if (baro_init(&baro_config) != 0) {
        payload_state.status = PAYLOAD_SENSOR_ERROR;
        payload_state.pau_error_flags |= 0x02;  // Barometre hatası
        return -1;
    }
    
    // GPS (u-blox NEO-M9N)
    GPSConfig_t gps_config = {
        .uart_baudrate = 38400,
        .update_rate_ms = payload_state.config.sampling_period_ms,
        .cold_start = 0,        // Warm-up (cache bulunması durumunda)
        .use_dgps = 0
    };
    
    if (gps_init(&gps_config) != 0) {
        payload_state.status = PAYLOAD_SENSOR_ERROR;
        payload_state.pau_error_flags |= 0x04;  // GPS hatası
        // GPS başarısız olsa bile devam et (opsiyonel sensör)
    }
    
    return 0;
}

/**
 * @brief Payload paketini doldur
 */
static int32_t payload_collect_data(PayloadPacket_t *packet) {
    if (packet == NULL) return -1;
    
    // Timestamp al
    uint32_t timestamp = 0;  // TODO: sys_time_ms() / 1000 gibi
    
    packet->timestamp = timestamp;
    packet->sample_counter = payload_state.packet_count++;
    
    // Radyasyon Sensörü oku
    RadiationMeasurement_t rad_meas = {0};
    if (rad_sensor_read(&rad_meas) == 0) {
        packet->radiation_cpm = rad_meas.cpm;
        packet->radiation_dose_rate = rad_meas.dose_rate_usv_h;
    } else {
        payload_state.pau_error_flags |= 0x01;
        packet->radiation_cpm = 0;
        packet->radiation_dose_rate = 0.0f;
    }
    
    // Barometre oku
    BarometerMeasurement_t baro_meas = {0};
    if (baro_read(&baro_meas) == 0) {
        packet->temperature_c = baro_meas.temperature_c;
        packet->barometric_pressure_pa = baro_meas.pressure_pa;
        packet->altitude_m = baro_meas.altitude_m;
    } else {
        payload_state.pau_error_flags |= 0x02;
        packet->temperature_c = 0.0f;
        packet->barometric_pressure_pa = 0.0f;
        packet->altitude_m = 0.0f;
    }
    
    // GPS oku
    GPSMeasurement_t gps_meas = {0};
    if (gps_read(&gps_meas) == 0) {
        packet->latitude_raw = (int32_t)(gps_meas.latitude_deg * 1e7);
        packet->longitude_raw = (int32_t)(gps_meas.longitude_deg * 1e7);
        packet->gps_satellites = gps_meas.num_satellites;
        packet->gps_fix_valid = 1;
    } else {
        payload_state.pau_error_flags |= 0x04;
        packet->latitude_raw = 0;
        packet->longitude_raw = 0;
        packet->gps_satellites = 0;
        packet->gps_fix_valid = 0;
    }
    
    // AHRS/Yönelim (ADCS Task'tan alınır, şimdilik sıfır)
    packet->euler_roll_deg = 0.0f;
    packet->euler_pitch_deg = 0.0f;
    packet->euler_yaw_deg = 0.0f;
    
    // Task Sağlık
    packet->error_flags = payload_state.pau_error_flags;
    packet->task_status = 0x00;  // TODO: İç task status bitmap
    packet->sd_card_free_mb = sd_card_get_free_space_mb();
    
    // Son paketi kaydet
    memcpy((void *)&payload_state.last_packet, packet, sizeof(PayloadPacket_t));
    
    return 0;
}

/**
 * @brief Paketi şifrele ve SD karta yaz
 */
static int32_t payload_process_packet(PayloadPacket_t *packet) {
    if (packet == NULL) return -1;
    
    // SD Karta yazma
    if (payload_state.config.write_to_sd) {
        SDCardRecord_t record = {
            .record_id = payload_state.packet_count,
            .timestamp = packet->timestamp,
            .radiation_cpm = packet->radiation_cpm,
            .radiation_dose_usv_h = packet->radiation_dose_rate,
            .temperature_c = packet->temperature_c,
            .pressure_pa = packet->barometric_pressure_pa,
            .altitude_m = packet->altitude_m,
            .latitude_raw = packet->latitude_raw,
            .longitude_raw = packet->longitude_raw,
            .gps_valid = packet->gps_fix_valid,
            .error_flags = packet->error_flags,
        };
        
        // CRC16 hesapla
        uint16_t crc = 0xFFFF;
        const uint8_t *data = (const uint8_t *)&record;
        size_t len = offsetof(SDCardRecord_t, crc16);
        
        for (size_t i = 0; i < len; i++) {
            crc ^= ((uint16_t)data[i] << 8);
            for (int j = 0; j < 8; j++) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ 0x1021;
                } else {
                    crc = crc << 1;
                }
            }
        }
        record.crc16 = crc;
        
        if (sd_card_write_record(&record) != 0) {
            payload_state.status = PAYLOAD_SD_ERROR;
            payload_state.pau_error_flags |= 0x08;
            return -1;
        }
    }
    
    // Şifreleme (AES-256 CTR)
    if (payload_state.config.encrypt_data) {
        // TODO: Packet'ı şifrele ve queue'ya koy
    }
    
    return 0;
}

// ============================================================================
// Task Implementation
// ============================================================================

int32_t payload_task_init(const PayloadTaskConfig_t *config) {
    if (config == NULL) return -1;
    
    // Config'i kaydet
    memcpy((void *)&payload_state.config, config, sizeof(PayloadTaskConfig_t));
    
    // SD Kart başlat
    if (payload_state.config.write_to_sd) {
        if (sd_card_init() != 0) {
            payload_state.status = PAYLOAD_SD_ERROR;
            return -1;
        }
    }
    
    // Sensörleri başlat
    if (payload_sensors_init() != 0) {
        return -1;
    }
    
    // Kriptografi başlat (gerekirse)
    if (payload_state.config.encrypt_data) {
        // TODO: Kriptografi bağlamını başlat
    }
    
    // TODO: FreeRTOS Queue oluştur
    // payload_state.data_queue = xQueueCreate(config->max_queue_depth, sizeof(PayloadPacket_t));
    // if (payload_state.data_queue == NULL) return -1;
    
    payload_state.initialized = 1;
    payload_state.status = PAYLOAD_OK;
    payload_state.packet_count = 0;
    
    return 0;
}

/**
 * @brief FreeRTOS Task Fonksiyonu (Sonsuz döngü)
 * void vTaskCreate(payload_task, "PayloadTask", configMINIMAL_STACK_SIZE * 4, NULL, 2, &handle);
 */
void payload_task(void *pvParameters) {
    (void)pvParameters;  // Unused
    
    if (!payload_state.initialized) {
        // Task başlatılmadan çalıştırılmış
        vTaskDelete(NULL);
        return;
    }
    
    // Task sonsuz döngüsü
    while (1) {
        PayloadPacket_t packet = {0};
        
        // Veri topla
        if (payload_collect_data(&packet) != 0) {
            payload_state.status = PAYLOAD_SENSOR_ERROR;
            vTaskDelay(payload_state.config.sampling_period_ms / portTICK_PERIOD_MS);
            continue;
        }
        
        // Paketi işle (şifrele, SD'ye yaz)
        if (payload_process_packet(&packet) != 0) {
            // Hata durumunda devam et (non-critical)
        }
        
        // Output queue'ya koy (LoRa TX için)
        // TODO: xQueueSend(payload_state.data_queue, &packet, portMAX_DELAY);
        
        payload_state.status = PAYLOAD_OK;
        
        // Periyot için uyku
        vTaskDelay(payload_state.config.sampling_period_ms / portTICK_PERIOD_MS);
    }
}

// ============================================================================
// Status/Debug Interface
// ============================================================================

uint8_t payload_task_get_status(void) {
    return payload_state.status;
}

int32_t payload_task_read_last_packet(PayloadPacket_t *payload) {
    if (payload == NULL) return -1;
    
    memcpy(payload, (void *)&payload_state.last_packet, sizeof(PayloadPacket_t));
    return 0;
}

uint32_t payload_task_queue_depth(void) {
    // TODO: return uxQueueMessagesWaiting(payload_state.data_queue);
    return 0;
}
