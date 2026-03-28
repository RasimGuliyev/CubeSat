// fsw/Drivers/Storage/sd_card.h
// SD Kart Depolama Sürücüsü (FatFS + SQLite)
// Radyasyon ve telemetri verilerini zaman damgasıyla kaydetme
#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include <stddef.h>
#include <time.h>

// SD Kart Durum Kodu
typedef enum {
    SD_STATUS_OK = 0x00,
    SD_STATUS_NOT_INITIALIZED = 0x01,
    SD_STATUS_NOT_INSERTED = 0x02,
    SD_STATUS_WRITE_ERROR = 0x03,
    SD_STATUS_READ_ERROR = 0x04,
    SD_STATUS_FILESYSTEM_ERROR = 0x05,
    SD_STATUS_FULL = 0x06,
    SD_STATUS_CORRUPTED = 0x07,
    SD_STATUS_DATABASE_ERROR = 0x08
} SDCardStatus_t;

// SD Kart Bilgileri
typedef struct __attribute__((packed)) {
    uint32_t total_capacity_mb;    // Toplam kapasite (MB)
    uint32_t free_space_mb;        // Boş alan (MB)
    uint32_t total_records;        // Toplam kayıt sayısı
    uint32_t last_write_timestamp; // Son yazma zamanı
    uint8_t  filesystem_type;      // 0: FAT32, 1: exFAT
} SDCardInfo_t;

// SD Kart Veri Kaydı (Radyasyon Ölçümü)
typedef struct __attribute__((packed)) {
    uint32_t record_id;            // Sıra numarası
    uint32_t timestamp;            // UTC zamanı
    uint16_t radiation_cpm;        // Counts Per Minute
    float    radiation_dose_usv_h; // µSv/h
    float    temperature_c;        // °C
    float    pressure_pa;          // Pascal
    float    altitude_m;           // Metre
    int32_t  latitude_raw;         // 1E-7 derece
    int32_t  longitude_raw;        // 1E-7 derece
    uint8_t  gps_valid;            // GPS geçerliliği
    uint8_t  error_flags;          // Hata bitmap
    uint16_t crc16;                // Kayıt bütünlüğü CRC
} SDCardRecord_t;

// ============================================================================
// Temel SD Kart Yönetimi
// ============================================================================

/**
 * @brief SD kart'ı başlat (FatFS)
 * @return 0: Başarılı, -1: Hata
 */
int32_t sd_card_init(void);

/**
 * @brief SD kart'ı çıkart (güvenli shutdown)
 * @return 0: Başarılı, -1: Hata
 */
int32_t sd_card_deinit(void);

/**
 * @brief SD kart durumunu kontrol et
 * @return SDCardStatus_t enum değeri
 */
uint8_t sd_card_get_status(void);

/**
 * @brief SD kart bilgilerini oku
 * @param info: Çıktı buffer
 * @return 0: Başarılı, -1: Hata
 */
int32_t sd_card_get_info(SDCardInfo_t *info);

/**
 * @brief SD kart boş alanını kontrol et
 * @return Boş alan MB cinsinden
 */
uint32_t sd_card_get_free_space_mb(void);

// ============================================================================
// Veri Kayıt ve Okuma (CSV Format)
// ============================================================================

/**
 * @brief Radyasyon kaydını SD karta yaz
 * @param record: Yazılacak kayıt
 * @return 0: Başarılı, -1: Hata
 */
int32_t sd_card_write_record(const SDCardRecord_t *record);

/**
 * @brief Belirtilen timestamp aralığında kayıtları oku
 * @param start_timestamp: Başlangıç zamanı
 * @param end_timestamp: Bitiş zamanı
 * @param max_records: Maksimum döndürülecek kayıt sayısı
 * @param records_out: Çıktı buffer (dinamik bellek kullanıyor)
 * @param count_out: Döndürülen kayıt sayısı
 * @return 0: Başarılı, -1: Hata
 */
int32_t sd_card_read_records(uint32_t start_timestamp, uint32_t end_timestamp,
                             uint16_t max_records, SDCardRecord_t **records_out,
                             uint16_t *count_out);

/**
 * @brief Son N kaydı oku (en yeni veri)
 * @param n: Ne kadar kayıt istenir
 * @param records_out: Çıktı buffer
 * @param count_out: Döndürülen kayıt sayısı
 * @return 0: Başarılı, -1: Hata
 */
int32_t sd_card_read_last_n_records(uint16_t n, SDCardRecord_t **records_out,
                                    uint16_t *count_out);

// ============================================================================
// Dosya Yönetimi
// ============================================================================

/**
 * @brief Radyasyon verisi dosyasını aç/oluştur (append mode)
 * @return File handle, negatif: Hata
 */
int32_t sd_card_open_radiation_file(void);

/**
 * @brief Dosyayı kapat
 * @param file_handle: Dosya handle
 * @return 0: Başarılı, -1: Hata
 */
int32_t sd_card_close_file(int32_t file_handle);

/**
 * @brief Başlık satırını yaz (CSV header)
 * @return 0: Başarılı, -1: Hata
 */
int32_t sd_card_write_csv_header(void);

/**
 * @brief Kaydı CSV formatında yaz
 * @param file_handle: Dosya handle
 * @param record: Yazılacak kayıt
 * @return Yazılan bayt sayısı, negatif: Hata
 */
int32_t sd_card_write_csv_record(int32_t file_handle, const SDCardRecord_t *record);

/**
 * @brief Dosya sistemini kontrol et (fsck)
 * @return 0: Tamam, -1: Hata bulundu
 */
int32_t sd_card_check_filesystem(void);

/**
 * @brief SD kartı format et (tüm veri silinir!)
 * @return 0: Başarılı, -1: Hata
 */
int32_t sd_card_format(void);

// ============================================================================
// Veri Güvenliği ve Bütünlük Kontrolü
// ============================================================================

/**
 * @brief Kaydın CRC16'sını hesapla ve doğrula
 * @param record: Kontrol edilecek kayıt
 * @return 1: Geçerli, 0: Bozuk
 */
uint8_t sd_card_verify_record_crc(const SDCardRecord_t *record);

/**
 * @brief Bozuk kayıtları onar (recovery)
 * @return Onarılan kayıt sayısı
 */
int32_t sd_card_repair_corrupted_records(void);

// ============================================================================
// Yardımcı Fonksiyonlar
// ============================================================================

/**
 * @brief Timestamp'i okunabilir metin formatına çevir
 * @param timestamp: UTC zamanı
 * @param buffer: Çıktı buffer (en az 32 byte)
 * @return Buffer pointeri
 */
char* sd_card_timestamp_to_string(uint32_t timestamp, char *buffer);

/**
 * @brief Belleği temizle (SD karttan okunan kayıtlar)
 * @param records: Boşaltılacak kayıt array
 * @param count: Kayıt sayısı
 */
void sd_card_free_records(SDCardRecord_t *records, uint16_t count);

/**
 * @brief SD Kart'taki tüm dosyaları listele (debug)
 * @return Dosya sayısı
 */
int32_t sd_card_list_files(void);

#endif // SD_CARD_H
