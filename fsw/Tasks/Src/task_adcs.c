// fsw/Tasks/Src/task_adcs.c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

// 1. Kendi yazdığımız başlık dosyalarını dahil ediyoruz (Include)
#include "../../shared/telemetry.h"         // Veri paketi yapımız (BeaconPacket_t)
#include "../../Middleware/AES256/crypto.h" // ŞİFRELEME KÜTÜPHANEMİZ BURADA EKLENDİ

// Haberleşme kuyruğu (main.c içinde oluşturulacak)
extern QueueHandle_t telemetryQueue;

void vTaskADCS(void *pvParameters) {
    BeaconPacket_t my_beacon;
    
    // Şifrelenmiş veriyi tutacak 24 byte'lık (BeaconPacket_t boyutu kadar) dizi
    uint8_t encrypted_buffer[sizeof(BeaconPacket_t)];

    // TODO: RM3100 I2C Başlatma fonksiyonunu çağır
    // RM3100_Init();

    while(1) {
        // 1. RM3100 Sensöründen manyetik alan verilerini oku
        my_beacon.mag_x = 0.5; // Şimdilik örnek veri (RM3100_ReadX() gelecek)
        my_beacon.mag_y = 0.2; // Örnek veri
        my_beacon.mag_z = -0.8; // Örnek veri
        my_beacon.safe_mode = 0;
        
        // Zaman damgasını al
        my_beacon.timestamp = xTaskGetTickCount() / 1000;
        
        // 2. ŞİFRELEME ADIMI: Açık veriyi (my_beacon) al, şifrele ve encrypted_buffer'a yaz
        Crypto_EncryptPayload((uint8_t*)&my_beacon, encrypted_buffer, sizeof(BeaconPacket_t));
        
        // 3. KUYRUĞA GÖNDERME: Artık açık veriyi değil, şifreli diziyi (encrypted_buffer) gönderiyoruz!
        xQueueSend(telemetryQueue, &encrypted_buffer, pdMS_TO_TICKS(10));
        
        // Görevi 100ms (10 Hz) uyut
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}