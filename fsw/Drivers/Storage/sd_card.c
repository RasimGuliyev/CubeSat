// fsw/Drivers/Storage/sd_card.c
// SD Kart Depolama Sürücüsü Implementasyonu (FatFS)
// CSV formatında radyasyon ve telemetri verisi kayıt

#include "sd_card.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// FatFS headers (gerçek projeye eklenecek)
// #include "ff.h"
// #include "diskio.h"

// STM32H743IIK6 HAL headers
// #include "stm32h7xx_hal.h"

// ============================================================================
// SD Kart Durum Değişkenleri (Global State)
// ============================================================================

static volatile struct {
    uint8_t initialized;
    uint8_t sd_inserted;
    SDCardStatus_t last_status;
    uint32_t total_records;
    uint32_t last_write_timestamp;
    uint32_t total_capacity_mb;
    uint32_t free_space_mb;
} sd_state = {0};

// FatFS Nesneleri
// static FATFS fs;
// static FIL fil;
// static DIR dir;
// static FILINFO fno;

// SD Kart Dosya Yolları
#define SD_RADIATION_DATA_FILE "/ULAAS1/radiation.csv"
#define SD_BACKUP_DATA_FILE    "/ULAAS1/radiation_bak.csv"
#define SD_LOG_FILE            "/ULAAS1/system.log"

// ============================================================================
// Başlatma ve Shutdown
// ============================================================================

/**
 * @brief SD kart'ı başlat (FatFS)
 */
int32_t sd_card_init(void) {
    if (sd_state.initialized) {
        return 0;  // Zaten başlatılmış
    }
    
    // TODO: STM32H743 SDMMC/DMA başlatması
    // if (HAL_SD_Init(...) != HAL_OK) {
    //     sd_state.last_status = SD_STATUS_NOT_INITIALIZED;
    //     return -1;
    // }
    
    // TODO: FatFS başlatması
    // FRESULT res = f_mount(&fs, "0:", 1);
    // if (res != FR_OK) {
    //     sd_state.last_status = SD_STATUS_FILESYSTEM_ERROR;
    //     return -1;
    // }
    
    // TODO: Klasör oluştur (/ULAAS1)
    // f_mkdir("ULAAS1");
    
    sd_state.initialized = 1;
    sd_state.sd_inserted = 1;
    sd_state.last_status = SD_STATUS_OK;
    sd_state.total_records = 0;
    sd_state.total_capacity_mb = 0;
    sd_state.free_space_mb = 0;
    
    // CSV başlık satırını yaz
    if (sd_card_write_csv_header() != 0) {
        sd_state.last_status = SD_STATUS_WRITE_ERROR;
        return -1;
    }
    
    return 0;
}

/**
 * @brief SD kart'ı çıkart
 */
int32_t sd_card_deinit(void) {
    if (!sd_state.initialized) {
        return 0;
    }
    
    // TODO: FatFS unmount
    // f_unmount("0:");
    
    // TODO: SDMMC deinit
    // HAL_SD_DeInit(...);
    
    sd_state.initialized = 0;
    sd_state.last_status = SD_STATUS_OK;
    
    return 0;
}

/**
 * @brief SD kart durumunu kontrol et
 */
uint8_t sd_card_get_status(void) {
    if (!sd_state.initialized) {
        return SD_STATUS_NOT_INITIALIZED;
    }
    
    // TODO: SDMMC interrupt/polling ile sd_inserted kontrol et
    
    return sd_state.last_status;
}

/**
 * @brief SD kart bilgilerini oku
 */
int32_t sd_card_get_info(SDCardInfo_t *info) {
    if (info == NULL) {
        return -1;  // NULL pointer
    }
    
    if (!sd_state.initialized) {
        return -1;
    }
    
    // TODO: FatFS f_getfree() ile mekan al
    // DWORD fre_clust, tot_sect;
    // FRESULT res = f_getfree("0:", &fre_clust, &fs);
    // if (res == FR_OK) {
    //     sd_state.total_capacity_mb = (fs.n_fatent - 2) * fs.csize / 2048;
    //     sd_state.free_space_mb = fre_clust * fs.csize / 2048;
    // }
    
    info->total_capacity_mb = sd_state.total_capacity_mb;
    info->free_space_mb = sd_state.free_space_mb;
    info->total_records = sd_state.total_records;
    info->last_write_timestamp = sd_state.last_write_timestamp;
    info->filesystem_type = 0;  // FAT32
    
    return 0;
}

/**
 * @brief Boş alanı kontrol et
 */
uint32_t sd_card_get_free_space_mb(void) {
    return sd_state.free_space_mb;
}

// ============================================================================
// CSV Dosya İşlemleri
// ============================================================================

/**
 * @brief CSV başlık satırını yaz
 */
int32_t sd_card_write_csv_header(void) {
    // TODO: FatFS f_open() ve f_printf()
    // FIL fil;
    // FRESULT res = f_open(&fil, SD_RADIATION_DATA_FILE, FA_CREATE_ALWAYS | FA_WRITE);
    // if (res != FR_OK) return -1;
    
    // Header: record_id,timestamp,cpm,dose_usv_h,temp_c,pressure_pa,altitude_m,lat,lon,gps_valid,errors,crc16
    // const char *header = "ID,Timestamp,CPM,Dose(µSv/h),Temp(°C),Pressure(Pa),Altitude(m),Lat,Lon,GPS,Errors,CRC16\r\n";
    // UINT written = 0;
    // res = f_write(&fil, header, strlen(header), &written);
    // f_close(&fil);
    
    // if (written != strlen(header)) {
    //     sd_state.last_status = SD_STATUS_WRITE_ERROR;
    //     return -1;
    // }
    
    return 0;
}

/**
 * @brief Dosya aç (append mode)
 */
int32_t sd_card_open_radiation_file(void) {
    // TODO: FatFS f_open()
    // FIL *fil = (FIL *)malloc(sizeof(FIL));
    // if (fil == NULL) return -1;
    
    // FRESULT res = f_open(fil, SD_RADIATION_DATA_FILE, FA_OPEN_APPEND | FA_WRITE);
    // if (res != FR_OK) {
    //     free(fil);
    //     sd_state.last_status = SD_STATUS_WRITE_ERROR;
    //     return -1;
    // }
    
    // return (int32_t)fil;  // Handle olarak pointer döndür
    
    return 0;  // Simülasyon
}

/**
 * @brief Dosyayı kapat
 */
int32_t sd_card_close_file(int32_t file_handle) {
    if (file_handle <= 0) return -1;
    
    // TODO: FatFS f_close()
    // FIL *fil = (FIL *)file_handle;
    // FRESULT res = f_close(fil);
    // free(fil);
    
    // return (res == FR_OK) ? 0 : -1;
    
    return 0;
}

/**
 * @brief Kaydı CSV formatında yazVE
 */
int32_t sd_card_write_csv_record(int32_t file_handle, const SDCardRecord_t *record) {
    if (record == NULL) return -1;
    if (file_handle <= 0) return -1;
    
    // TODO: CSV satırını format et
    // char csv_line[256];
    // snprintf(csv_line, sizeof(csv_line),
    //         "%u,%u,%u,%.2f,%.2f,%.0f,%.1f,%d,%d,%u,%02X,%04X\r\n",
    //         record->record_id,
    //         record->timestamp,
    //         record->radiation_cpm,
    //         record->radiation_dose_usv_h,
    //         record->temperature_c,
    //         record->pressure_pa,
    //         record->altitude_m,
    //         record->latitude_raw,
    //         record->longitude_raw,
    //         record->gps_valid,
    //         record->error_flags,
    //         record->crc16);
    
    // FIL *fil = (FIL *)file_handle;
    // UINT written = 0;
    // FRESULT res = f_write(fil, csv_line, strlen(csv_line), &written);
    
    // if (res != FR_OK) {
    //     sd_state.last_status = SD_STATUS_WRITE_ERROR;
    //     return -1;
    // }
    
    // return written;
    
    return 0;  // Simülasyon
}

// ============================================================================
// Veri Yazma ve Okuma
// ============================================================================

/**
 * @brief Radyasyon kaydını SD karta yaz (binary + CSV)
 */
int32_t sd_card_write_record(const SDCardRecord_t *record) {
    if (record == NULL) return -1;
    if (!sd_state.initialized) return -1;
    
    // CRC kontrol
    if (!sd_card_verify_record_crc(record)) {
        sd_state.last_status = SD_STATUS_CORRUPTED;
        return -1;
    }
    
    // TODO: CSV dosyaya yaz
    int32_t file_handle = sd_card_open_radiation_file();
    if (file_handle < 0) return -1;
    
    int32_t written = sd_card_write_csv_record(file_handle, record);
    sd_card_close_file(file_handle);
    
    if (written < 0) return -1;
    
    sd_state.total_records++;
    sd_state.last_write_timestamp = record->timestamp;
    sd_state.last_status = SD_STATUS_OK;
    
    return 0;
}

/**
 * @brief Timestamp aralığında kayıtları oku
 */
int32_t sd_card_read_records(uint32_t start_timestamp, uint32_t end_timestamp,
                             uint16_t max_records, SDCardRecord_t **records_out,
                             uint16_t *count_out) {
    if (records_out == NULL || count_out == NULL) return -1;
    if (!sd_state.initialized) return -1;
    
    // TODO: CSV dosyasını oku ve parse et
    // FIL fil;
    // FRESULT res = f_open(&fil, SD_RADIATION_DATA_FILE, FA_READ);
    // if (res != FR_OK) return -1;
    
    // Dinamik bellek ayır
    SDCardRecord_t *records = (SDCardRecord_t *)malloc(
        max_records * sizeof(SDCardRecord_t)
    );
    if (records == NULL) return -1;
    
    uint16_t count = 0;
    // TODO: CSV parse ve filtrele
    
    *records_out = records;
    *count_out = count;
    
    // f_close(&fil);
    
    return 0;
}

/**
 * @brief Son N kaydı oku
 */
int32_t sd_card_read_last_n_records(uint16_t n, SDCardRecord_t **records_out,
                                    uint16_t *count_out) {
    if (records_out == NULL || count_out == NULL) return -1;
    
    // TODO: Dosya sonundan N satır oku (reverse seek)
    
    return 0;
}

// ============================================================================
// Dosya Sistem Kontrol
// ============================================================================

/**
 * @brief Dosya sistemi kontrol et
 */
int32_t sd_card_check_filesystem(void) {
    if (!sd_state.initialized) return -1;
    
    // TODO: FatFS f_chdir() ile basit test
    // FRESULT res = f_chdir("0:/ULAAS1");
    // return (res == FR_OK) ? 0 : -1;
    
    return 0;
}

/**
 * @brief SD kartı format et
 */
int32_t sd_card_format(void) {
    if (!sd_state.initialized) return -1;
    
    // TODO: FatFS f_mkfs()
    // FRESULT res = f_mkfs("0:", 0, 0);
    // if (res != FR_OK) {
    //     sd_state.last_status = SD_STATUS_FILESYSTEM_ERROR;
    //     return -1;
    // }
    
    sd_state.total_records = 0;
    
    return 0;
}

// ============================================================================
// Veri Güvenliği
// ============================================================================

/**
 * @brief Kaydın CRC16'sını hesapla ve doğrula
 */
uint8_t sd_card_verify_record_crc(const SDCardRecord_t *record) {
    if (record == NULL) return 0;
    
    // CRC16 hesapla (record_id'den error_flags'a kadar)
    const uint8_t *data = (const uint8_t *)record;
    size_t len = offsetof(SDCardRecord_t, crc16);
    
    uint16_t calculated_crc = 0xFFFF;
    const uint16_t POLY = 0x1021;
    
    for (size_t i = 0; i < len; i++) {
        calculated_crc ^= ((uint16_t)data[i] << 8);
        
        for (int j = 0; j < 8; j++) {
            if (calculated_crc & 0x8000) {
                calculated_crc = (calculated_crc << 1) ^ POLY;
            } else {
                calculated_crc = calculated_crc << 1;
            }
        }
    }
    
    return (calculated_crc == record->crc16) ? 1 : 0;
}

/**
 * @brief Bozuk kayıtları onar
 */
int32_t sd_card_repair_corrupted_records(void) {
    // TODO: Backup dosyasından oku ve onar
    return 0;
}

// ============================================================================
// Yardımcı Fonksiyonlar
// ============================================================================

/**
 * @brief Timestamp'i string'e çevir
 */
char* sd_card_timestamp_to_string(uint32_t timestamp, char *buffer) {
    if (buffer == NULL) return NULL;
    
    // TODO: time_t'den struct tm'e çevir
    time_t t = (time_t)timestamp;
    struct tm *tm_info = gmtime(&t);
    
    strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", tm_info);
    
    return buffer;
}

/**
 * @brief Okunan kayıtları serbest bırak
 */
void sd_card_free_records(SDCardRecord_t *records, uint16_t count) {
    if (records != NULL) {
        free(records);
    }
}

/**
 * @brief Dosyaları listele (debug)
 */
int32_t sd_card_list_files(void) {
    // TODO: FatFS f_opendir() ile dosyaları listele
    // DIR dir;
    // FILINFO fno;
    // FRESULT res = f_opendir(&dir, "0:/ULAAS1");
    // if (res != FR_OK) return -1;
    
    // int32_t count = 0;
    // while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0] != '\0') {
    //     // printf("%s\n", fno.fname);
    //     count++;
    // }
    // f_closedir(&dir);
    
    // return count;
    
    return 0;
}
