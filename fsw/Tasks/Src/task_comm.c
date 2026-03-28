// fsw/Tasks/Src/task_comm.c
// COMM Task Implementasyonu - LoRa (RFM98W) + CSP Router
// TX: Payload queue'dan veri al → CSP format'la → LoRa TX
// RX: LoRa RX → CSP parse → Komut işle

#include "task_comm.h"
#include "../../Middleware/AES256/crypto.h"
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// CSP Kütüphanesi (libcsp)
#include <csp/csp.h>
#include <csp/interfaces/csp_if_kiss.h>

// ============================================================================
// COMM Task Global State
// ============================================================================

static volatile struct {
    CommTaskConfig_t config;
    CommTaskStatus_t status;
    uint32_t tx_count;
    uint32_t rx_count;
    uint32_t error_count;
    uint8_t initialized;
    int16_t last_rssi;
    float last_snr;
    TaskHandle_t task_handle;
} comm_state = {0};

// LoRa RX/TX Buffer
#define LORA_RX_BUFFER_SIZE 256
#define LORA_TX_BUFFER_SIZE 256
#define LORA_MAX_PACKET_SIZE 250

static uint8_t lora_rx_buffer[LORA_RX_BUFFER_SIZE] = {0};
static uint8_t lora_tx_buffer[LORA_TX_BUFFER_SIZE] = {0};

// FreeRTOS Queues
static QueueHandle_t tx_telemetry_queue = NULL;
static QueueHandle_t rx_command_queue = NULL;

// ============================================================================
// CSP Protokolü (libcsp kullanarak)
// ============================================================================

/**
 * @brief CSP paket header oluştur ve sarıkla
 */
static int32_t csp_wrap_packet(const uint8_t *payload, uint16_t payload_len,
                               uint8_t *buffer, uint16_t buffer_size,
                               uint8_t dst_port, uint8_t src_port) {
    if (payload == NULL || buffer == NULL) return -1;
    if (payload_len + 4 > buffer_size) return -1;
    
    CSPHeader_t csp_hdr = {
        .priority = 0,
        .src_port = src_port,
        .dst_port = dst_port,
        .dst_addr = comm_state.config.csp_gw_addr,
        .src_addr = comm_state.config.csp_local_addr,
        .reserved = 0
    };
    
    uint32_t hdr_val = *(uint32_t *)&csp_hdr;
    buffer[0] = (hdr_val >> 24) & 0xFF;
    buffer[1] = (hdr_val >> 16) & 0xFF;
    buffer[2] = (hdr_val >> 8) & 0xFF;
    buffer[3] = hdr_val & 0xFF;
    
    memcpy(&buffer[4], payload, payload_len);
    
    return payload_len + 4;
}

/**
 * @brief CSP paketini unwrap et
 */
static int32_t csp_unwrap_packet(const uint8_t *buffer, uint16_t buffer_len,
                                 uint8_t *payload, uint16_t *payload_len,
                                 uint8_t *dst_port, uint8_t *src_port) {
    if (buffer == NULL || payload == NULL) return -1;
    if (buffer_len < 4) return -1;
    
    uint32_t hdr_val = (buffer[0] << 24) | (buffer[1] << 16) |
                      (buffer[2] << 8) | buffer[3];
    
    CSPHeader_t *csp_hdr = (CSPHeader_t *)&hdr_val;
    
    *src_port = csp_hdr->src_port;
    *dst_port = csp_hdr->dst_port;
    
    uint16_t plen = buffer_len - 4;
    memcpy(payload, &buffer[4], plen);
    *payload_len = plen;
    
    return 0;
}

// ============================================================================
// LoRa TX/RX Operasyonları
// ============================================================================

/**
 * @brief LoRa üzerinden paket gönder
 */
static int32_t comm_lora_transmit(const uint8_t *data, uint16_t len) {
    if (data == NULL || len == 0) return -1;
    if (len > LORA_MAX_PACKET_SIZE) return -1;
    
    // TODO: RFM98W_Transmit(data, len);
    
    comm_state.tx_count++;
    return 0;
}

/**
 * @brief LoRa'dan paket oku
 */
static int32_t comm_lora_receive(uint8_t *data, uint16_t *len) {
    if (data == NULL || len == NULL) return -1;
    
    // TODO: RFM98W_Receive(data, LORA_MAX_PACKET_SIZE);
    
    comm_state.rx_count++;
    return 0;
}

/**
 * @brief RSSI/SNR oku
 */
static void comm_lora_read_signal_quality(void) {
    // TODO: comm_state.last_rssi = RFM98W_GetRSSI();
    // TODO: comm_state.last_snr = RFM98W_GetSNR();
    
    comm_state.last_rssi = -95;
    comm_state.last_snr = 8.5f;
}

// ============================================================================
// COMM Task Başlatma
// ============================================================================

int32_t comm_task_init(const CommTaskConfig_t *config) {
    if (config == NULL) return -1;
    
    memcpy((void *)&comm_state.config, config, sizeof(CommTaskConfig_t));
    
    tx_telemetry_queue = xQueueCreate(config->max_queue_depth, sizeof(PayloadPacket_t));
    if (tx_telemetry_queue == NULL) return -1;
    
    rx_command_queue = xQueueCreate(config->max_queue_depth, sizeof(CommandHeader_t));
    if (rx_command_queue == NULL) return -1;
    
    // TODO: LoRa başlat
    // RFM98W_Init(config->lora_frequency_hz, config->lora_tx_power_dbm);
    
    // CSP başlat
    csp_conf_t csp_conf;
    csp_conf_get_defaults(&csp_conf);
    csp_conf.address = config->csp_local_addr;
    
    if (csp_init(&csp_conf) != CSP_ERR_NONE) {
        return -1;
    }
    
    comm_state.initialized = 1;
    comm_state.status = COMM_OK;
    
    return 0;
}

// ============================================================================
// COMM Task FreeRTOS Loop
// ============================================================================

void comm_task(void *pvParameters) {
    (void)pvParameters;
    
    if (!comm_state.initialized) {
        vTaskDelete(NULL);
        return;
    }
    
    uint32_t last_rssi_update = 0;
    
    while (1) {
        // TX: Telemetri gönder
        PayloadPacket_t tx_payload = {0};
        
        if (xQueueReceive(tx_telemetry_queue, &tx_payload, 0) == pdTRUE) {
            uint16_t csp_len = csp_wrap_packet((const uint8_t *)&tx_payload,
                                               sizeof(PayloadPacket_t),
                                               lora_tx_buffer,
                                               sizeof(lora_tx_buffer),
                                               10, 20);
            
            if (csp_len > 0) {
                comm_lora_transmit(lora_tx_buffer, csp_len);
            }
        }
        
        // RX: Komut al
        uint16_t rx_len = 0;
        if (comm_lora_receive(lora_rx_buffer, &rx_len) == 0 && rx_len > 0) {
            uint8_t payload_buffer[LORA_RX_BUFFER_SIZE] = {0};
            uint16_t payload_len = 0;
            uint8_t src_port = 0, dst_port = 0;
            
            if (csp_unwrap_packet(lora_rx_buffer, rx_len, payload_buffer,
                                 &payload_len, &dst_port, &src_port) == 0) {
                
                if (src_port == 1 && payload_len >= sizeof(CommandHeader_t)) {
                    CommandHeader_t *cmd = (CommandHeader_t *)payload_buffer;
                    xQueueSend(rx_command_queue, cmd, pdMS_TO_TICKS(10));
                }
            }
        }
        
        // RSSI/SNR güncelle
        if ((xTaskGetTickCount() - last_rssi_update) > 1000) {
            comm_lora_read_signal_quality();
            last_rssi_update = xTaskGetTickCount();
        }
        
        vTaskDelay(comm_state.config.tx_poll_period_ms / portTICK_PERIOD_MS);
    }
}

// ============================================================================
// Interface Fonksiyonları
// ============================================================================

uint8_t comm_task_get_status(void) {
    return comm_state.status;
}

int32_t comm_task_send_packet(const uint8_t *data, uint16_t len) {
    if (data == NULL) return -1;
    return comm_lora_transmit(data, len);
}

int32_t comm_task_send_telemetry(const PayloadPacket_t *payload) {
    if (payload == NULL) return -1;
    if (xQueueSend(tx_telemetry_queue, (void *)payload, pdMS_TO_TICKS(10)) != pdTRUE) {
        comm_state.status = COMM_QUEUE_FULL;
        return -1;
    }
    return 0;
}

int32_t comm_task_send_beacon(const BeaconPacket_t *beacon) {
    if (beacon == NULL) return -1;
    uint16_t csp_len = csp_wrap_packet((const uint8_t *)beacon,
                                       sizeof(BeaconPacket_t),
                                       lora_tx_buffer,
                                       sizeof(lora_tx_buffer),
                                       11, 21);
    if (csp_len > 0) return comm_lora_transmit(lora_tx_buffer, csp_len);
    return -1;
}

int32_t comm_task_receive_command(CommandHeader_t *cmd) {
    if (cmd == NULL) return -1;
    if (xQueueReceive(rx_command_queue, cmd, 0) == pdTRUE) return 0;
    return -1;
}

int16_t comm_task_get_rssi(void) {
    return comm_state.last_rssi;
}

float comm_task_get_snr(void) {
    return comm_state.last_snr;
}

uint32_t comm_task_tx_queue_depth(void) {
    if (tx_telemetry_queue == NULL) return 0;
    return uxQueueMessagesWaiting(tx_telemetry_queue);
}

uint32_t comm_task_rx_queue_depth(void) {
    if (rx_command_queue == NULL) return 0;
    return uxQueueMessagesWaiting(rx_command_queue);
}

int32_t comm_task_get_stats(uint32_t *tx_count, uint32_t *rx_count, uint32_t *errors) {
    if (tx_count == NULL || rx_count == NULL || errors == NULL) return -1;
    *tx_count = comm_state.tx_count;
    *rx_count = comm_state.rx_count;
    *errors = comm_state.error_count;
    return 0;
}

    // 4. Ana Dinleme Döngüsü (Yer İstasyonundan Komut Bekleme)
    while (1) {
        // Sokete bir bağlantı gelene kadar görevi uyut (Timeout yok: CSP_MAX_TIMEOUT)
        csp_conn_t *conn = csp_accept(socket, CSP_MAX_TIMEOUT);
        if (conn) {
            // Bağlantı kuruldu, paketi oku
            csp_packet_t *packet = csp_read(conn, 100);
            if (packet != NULL) {
                
                // TODO: Yerden gelen paketi (Örn: Anteni aç komutu) AES-256 ile çöz ve işle
                
                // İşlem bitince paketi hafızadan sil (Memory leak olmaması için şart!)
                csp_buffer_free(packet);
            }
            // Bağlantıyı kapat
            csp_close(conn);
        }
    }
}