// fsw/Tasks/Src/task_adcs.c
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "../../shared/telemetry.h"

// Dışarıdan gelen haberleşme kuyruğu (main.c içinde oluşturulacak)
extern QueueHandle_t telemetryQueue;

void vTaskADCS(void *pvParameters) {
    BeaconPacket_t my_beacon;
    
    // TODO: RM3100 I2C Başlatma fonksiyonunu çağır
    // RM3100_Init();

    while(1) {
        // 1. RM3100 Sensöründen manyetik alan verilerini oku
        // (Bu fonksiyonlar Drivers/RM3100 içinden gelecek)
        my_beacon.mag_x = RM3100_ReadX();
        my_beacon.mag_y = RM3100_ReadY();
        my_beacon.mag_z = RM3100_ReadZ();
        
        // 2. Zaman damgasını FreeRTOS tick sayacından al
        my_beacon.timestamp = xTaskGetTickCount() / 1000;
        
        // 3. Veriyi paketleyip Haberleşme (COMM) görevine gönder
        // Eğer kuyruk doluysa 10 milisaniye bekle
        xQueueSend(telemetryQueue, &my_beacon, pdMS_TO_TICKS(10));
        
        // Bu görev saniyede 10 kez (10 Hz) çalışsın
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}