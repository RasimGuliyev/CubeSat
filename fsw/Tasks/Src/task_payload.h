// fsw/Tasks/Src/task_payload.h
// Payload Task Header - Radyasyon sensörü, barometre, GPS veri toplama
#ifndef TASK_PAYLOAD_H
#define TASK_PAYLOAD_H

#include <stdint.h>
#include "../../shared/telemetry.h"

// Payload Task Konfigürasyonu
typedef struct {
    uint16_t sampling_period_ms;    // Örneğim periyodu (1000 ms = 1 Hz önerilen)
    uint8_t  use_compression;       // 1: Veriyi sıkıştır (LZ4 vb.)
    uint8_t  write_to_sd;          // 1: SD karta yaz
    uint8_t  encrypt_data;         // 1: AES-256 ile şifrele
    uint32_t max_queue_depth;      // Queue'da maksimum paket sayısı
} PayloadTaskConfig_t;

// Payload Task Durum Kodu
typedef enum {
    PAYLOAD_OK = 0x00,
    PAYLOAD_INIT_FAILED = 0x01,
    PAYLOAD_SENSOR_ERROR = 0x02,
    PAYLOAD_SD_ERROR = 0x03,
    PAYLOAD_QUEUE_FULL = 0x04,
    PAYLOAD_ENCRYPTION_ERROR = 0x05
} PayloadTaskStatus_t;

/**
 * @brief Payload Task'ı başlat
 * @param config: Payload konfigürasyonu
 * @return 0: Başarılı, -1: Hata
 */
int32_t payload_task_init(const PayloadTaskConfig_t *config);

/**
 * @brief FreeRTOS Task fonksiyonu (Task'ı oluşturmak için kullan)
 * Sonsuz döngü - dispatcher tarafından yapılan vTaskCreate('payload_task', ...)
 */
void payload_task(void *pvParameters);

/**
 * @brief Payload Task durumunu kontrol et
 * @return PayloadTaskStatus_t enum değeri
 */
uint8_t payload_task_get_status(void);

/**
 * @brief Son toplanan Payload paketini oku
 * @param payload: Çıktı buffer
 * @return 0: Başarılı, -1: Hata/paket yok
 */
int32_t payload_task_read_last_packet(PayloadPacket_t *payload);

/**
 * @brief Queue'daki paket sayısını oku
 * @return Paket sayısı
 */
uint32_t payload_task_queue_depth(void);

#endif // TASK_PAYLOAD_H
