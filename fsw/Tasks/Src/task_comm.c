// fsw/Tasks/Src/task_comm.c
#include "FreeRTOS.h"
#include "task.h"
#include <csp/csp.h>               // CSP Ana Kütüphanesi
#include <csp/interfaces/csp_if_kiss.h> // Seri haberleşme arayüzü (LoRa için modifiye edilecek)

// Uydumuzun uzaydaki ağ adresi (Örneğin 1 numaralı düğüm/node biziz)
#define MY_ADDRESS 1

void vTaskCOMM(void *pvParameters) {
    // 1. CSP Ayarlarını Başlat
    csp_conf_t csp_conf;
    csp_conf_get_defaults(&csp_conf);
    csp_conf.address = MY_ADDRESS; // Uydumuzun ID'si
    
    // Protokolü başlat (Hafızayı ayarlar, kuyrukları oluşturur)
    if (csp_init(&csp_conf) != CSP_ERR_NONE) {
        // Hata durumu: Sistemi yeniden başlat
    }

    // LoRa arayüzünü CSP'ye kaydet ve varsayılan (default) ağ rotası yap
    csp_iflist_add(&csp_if_lora);
    csp_rtable_set(CSP_DEFAULT_ROUTE, 0, &csp_if_lora, CSP_NODE_MAC);

    // 2. Yönlendiriciyi (Router) FreeRTOS Görevi Olarak Başlat
    // Gelen paketleri doğru yerlere (Soketlere) dağıtan arka plan görevidir
    // 500 byte stack size, 1 öncelik seviyesi (priority)
    csp_route_start_task(500, 1);

    // 3. Yer İstasyonunu Dinlemek İçin Soket (Socket) Oluştur
    // Tıpkı web sunucusu gibi, 10 numaralı portu komut dinlemek için açıyoruz
    csp_socket_t *socket = csp_socket(CSP_SO_NONE);
    csp_bind(socket, 10);
    csp_listen(socket, 5); // Aynı anda maksimum 5 bağlantı bekle

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