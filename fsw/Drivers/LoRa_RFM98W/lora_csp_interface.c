#include <csp/csp.h>
#include "FreeRTOS.h"
#include "task.h"
#include "stm32h7xx_hal.h" // STM32H743IIK6 donanım kütüphanesi

// İleri bildirimler (Arayüz yapısını aşağıda tanımlayacağız)
extern csp_iface_t csp_if_lora;

// ---------------------------------------------------------
// 1. TX (GÖNDERİM) FONKSİYONU
// CSP bir paket göndermek istediğinde otomatik olarak buraya düşer.
// ---------------------------------------------------------
int csp_lora_tx(csp_iface_t * iface, uint16_t via, csp_packet_t * packet, int from_me) {
    // 1. CSP'nin hazırladığı paketin toplam uzunluğunu al
    uint16_t length = packet->length;
    
    // 2. CSP Başlığını (Header) ve Görev Verisini (Payload) LoRa çipine SPI üzerinden yaz
    // TODO: RFM98W_SPI_WriteBuffer(packet->frame_begin, length + CSP_HEADER_SIZE);
    
    // 3. LoRa çipine "Antenden Çıkış Yap" (TX Mode) komutunu ver
    // TODO: RFM98W_SetMode(MODE_TX);
    
    // 4. Çok Kritik: Gönderim bittikten sonra paketi STM32'nin RAM'inden sil (Memory Leak olmaması için)
    csp_buffer_free(packet);
    
    return CSP_ERR_NONE;
}

// ---------------------------------------------------------
// 2. RX (DİNLEME) GÖREVİ - FreeRTOS Task
// Uzayda sürekli çalışarak antenden gelen sinyalleri yakalar ve CSP'ye besler.
// ---------------------------------------------------------
void vTaskLoRaRX(void * pvParameters) {
    while (1) {
        // 1. LoRa'dan bir kesme (Interrupt - DIO0 pini) gelmesini bekle
        // Bu pin, anten geçerli bir LoRa paketi yakaladığında donanımsal olarak "1" (HIGH) olur.
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_0) == GPIO_PIN_SET) { // Örnek Pin: PB0
            
            // 2. CSP'nin hafıza havuzundan boş bir paket (buffer) iste
            csp_packet_t * packet = csp_buffer_get(256);
            if (packet != NULL) {
                // 3. SPI üzerinden LoRa'nın içindeki veriyi çekip paketin içine doldur
                // TODO: RFM98W_SPI_ReadBuffer(packet->frame_begin, &packet->length);
                
                // 4. Gelen paketi CSP Yönlendiricisine (Router) teslim et!
                // Artık paket uydunun içinde doğru göreve (task_comm, task_adcs) otomatik gidecek.
                csp_qfifo_write(packet, &csp_if_lora, NULL);
            }
            
            // Kesme bayrağını temizle ve yeni paket dinlemeye dön
            // TODO: RFM98W_ClearIrq();
        }
        
        // Eğer donanımsal Interrupt kullanmıyorsak, polling (sürekli sorma) döngüsünü 10ms uyut
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------
// 3. ARAYÜZÜN (INTERFACE) TANIMLANMASI
// CSP'ye LoRa'nın özelliklerini tanıtıyoruz.
// ---------------------------------------------------------
csp_iface_t csp_if_lora = {
    .name = "LoRa_RFM98W",
    .nexthop = csp_lora_tx, // TX fonksiyonumuzu bağladık. CSP artık nereye veri atacağını biliyor.
    .mtu = 256,             // Maximum Transmission Unit: LoRa tek seferde maks 256 byte taşıyabilir.
};