// fsw/Tasks/Src/task_comm.h
// COMM Task Header - LoRa Haberleşme (RFM98W) + CSP Router
#ifndef TASK_COMM_H
#define TASK_COMM_H

#include <stdint.h>
#include "../../shared/telemetry.h"
#include "../../shared/commands.h"

// COMM Task Konfigürasyonu
typedef struct {
    uint16_t tx_poll_period_ms;     // TX queue polling periyodu (100 ms)
    uint16_t rx_poll_period_ms;     // RX queue polling periyodu (50 ms)
    uint8_t  lora_tx_power_dbm;     // LoRa TX gücü (0-20 dBm)
    uint32_t lora_frequency_hz;     // LoRa frekansı (868 MHz standart)
    uint32_t csp_local_addr;        // CubeSat CSP adresi (1 = uydu)
    uint32_t csp_gw_addr;           // Ground station CSP adresi (0 = yer istasyonu)
    uint32_t max_queue_depth;       // TX/RX queue maksimum paket sayısı
} CommTaskConfig_t;

// COMM Task Durum Kodu
typedef enum {
    COMM_OK = 0x00,
    COMM_INIT_FAILED = 0x01,
    COMM_TX_ERROR = 0x02,
    COMM_RX_ERROR = 0x03,
    COMM_CSP_ERROR = 0x04,
    COMM_QUEUE_FULL = 0x05,
    COMM_ENCRYPTION_ERROR = 0x06,
    COMM_TIMEOUT = 0x07
} CommTaskStatus_t;

// CSP Paket Başlığı
typedef struct __attribute__((packed)) {
    uint32_t priority      : 2;     // CSP pri ocity (0-3)
    uint32_t src_port      : 6;     // Kaynak port
    uint32_t dst_port      : 6;     // Hedef port
    uint32_t dst_addr      : 5;     // Hedef CSP adresi
    uint32_t src_addr      : 5;     // Kaynak CSP adresi
    uint32_t reserved      : 8;     // Rezerv
} CSPHeader_t;

/**
 * @brief COMM Task'ı başlat (LoRa + CSP başlatması)
 * @param config: COMM konfigürasyonu
 * @return 0: Başarılı, -1: Hata
 */
int32_t comm_task_init(const CommTaskConfig_t *config);

/**
 * @brief FreeRTOS Task fonksiyonu
 */
void comm_task(void *pvParameters);

/**
 * @brief COMM Task durumunu kontrol et
 * @return CommTaskStatus_t enum değeri
 */
uint8_t comm_task_get_status(void);

/**
 * @brief LoRa paketini gönder (TX queue'ye koy)
 * @param data: Gönderilecek veri
 * @param len: Veri uzunluğu (max 255 byte)
 * @return 0: Başarılı, -1: Hata/Queue dolu
 */
int32_t comm_task_send_packet(const uint8_t *data, uint16_t len);

/**
 * @brief Telemetri paketini gönder (yüksek seviye)
 * @param payload: Payload paketi
 * @return 0: Başarılı, -1: Hata
 */
int32_t comm_task_send_telemetry(const PayloadPacket_t *payload);

/**
 * @brief Beacon paketini gönder (sistem sağlığı)
 * @param beacon: Beacon paketi
 * @return 0: Başarılı, -1: Hata
 */
int32_t comm_task_send_beacon(const BeaconPacket_t *beacon);

/**
 * @brief Komut kuyruğundan komut oku (yer istasyonundan)
 * @param cmd: Çıktı komut buffer
 * @return 0: Komut alındı, -1: Komut yok
 */
int32_t comm_task_receive_command(CommandHeader_t *cmd);

/**
 * @brief LoRa Received Signal Strength Indicator (RSSI) oku
 * @return RSSI değeri (dBm cinsinden, tipik olarak negatif)
 */
int16_t comm_task_get_rssi(void);

/**
 * @brief LoRa Signal-to-Noise Ratio (SNR) oku
 * @return SNR değeri (dB cinsinden)
 */
float comm_task_get_snr(void);

/**
 * @brief TX queue derinliği
 * @return Beklemede olan paket sayısı
 */
uint32_t comm_task_tx_queue_depth(void);

/**
 * @brief RX queue derinliği
 * @return Alınan ve işlenmemiş paket sayısı
 */
uint32_t comm_task_rx_queue_depth(void);

/**
 * @brief CSP router istatistiklerini oku
 * @param tx_count: Gönderilen paket sayısı (out)
 * @param rx_count: Alınan paket sayısı (out)
 * @param errors: Hata sayısı (out)
 * @return 0: Başarılı, -1: Hata
 */
int32_t comm_task_get_stats(uint32_t *tx_count, uint32_t *rx_count, uint32_t *errors);

#endif // TASK_COMM_H
