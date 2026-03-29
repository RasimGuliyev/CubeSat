// fsw/Drivers/Payload/gps.c
// GPS Navigasyon Sürücüsü (u-blox NEO-M9N) Implementasyonu
// NMEA protokolü ile UART üzerinden veri okuma

#include "gps.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

// STM32H743IIK6 HAL headers
// #include "stm32h7xx_hal.h"

// UART NMEA Buffer
#define GPS_NMEA_BUFFER_SIZE 256
static volatile struct {
    uint8_t rx_buffer[GPS_NMEA_BUFFER_SIZE];
    uint16_t rx_index;
    uint8_t line_ready;  // Satır tamamlandı mı?
} nmea_rx = {0};

// GPS Global State
static volatile struct {
    GPSConfig_t config;
    GPSMeasurement_t last_measurement;
    uint8_t initialized;
    uint8_t has_fix;
    uint8_t seu_counter;
    GPSStatus_t last_status;
    uint32_t last_fix_timestamp;
} gps_state = {0};

// ============================================================================
// NMEA Protokolü (Parsing)
// ============================================================================

/**
 * @brief NMEA checksum'unu hesapla
 * Checksum = XOR of all characters between '$' and '*'
 */
static uint8_t gps_calculate_nmea_checksum(const char *sentence) {
    if (sentence == NULL) return 0xFF;
    
    uint8_t checksum = 0;
    const char *ptr = sentence;
    
    // '$' karakterini atla
    if (*ptr == '$') ptr++;
    
    // '*' bulana kadar XOR'la
    while (*ptr && *ptr != '*') {
        checksum ^= (uint8_t)*ptr;
        ptr++;
    }
    
    return checksum;
}

/**
 * @brief NMEA checksum'unu doğrula
 */
uint8_t gps_verify_nmea_checksum(const char *sentence) {
    if (sentence == NULL) return 0;
    
    // Format: $...data*XX
    const char *checksum_ptr = strchr(sentence, '*');
    if (checksum_ptr == NULL) return 0;
    
    uint8_t calculated = gps_calculate_nmea_checksum(sentence);
    uint8_t received = (uint8_t)strtol(checksum_ptr + 1, NULL, 16);
    
    return (calculated == received) ? 1 : 0;
}

/**
 * @brief NMEA cümlesi parse et ($GPRMC, $GPGGA, vb.)
 * Basit parser - tam implementasyon tüm NMEA cümlelerini desteklemeli
 */
int32_t gps_parse_nmea_sentence(const char *nmea_sentence,
                               GPSMeasurement_t *measurement) {
    if (nmea_sentence == NULL || measurement == NULL) {
        return -1;
    }
    
    // Checksum doğrulama
    if (!gps_verify_nmea_checksum(nmea_sentence)) {
        gps_state.last_status = GPS_STATUS_INVALID_DATA;
        return -1;
    }
    
    // GPRMC (Recommended Minimum Navigation Information)
    if (strstr(nmea_sentence, "$GPRMC") != NULL) {
        // Format: $GPRMC,hhmmss.ss,status,latitude,lat_dir,longitude,lon_dir,speed,heading,ddmmyy,magvar*XX
        
        // Example: $GPRMC,081350.00,A,4717.113210,N,00833.915187,E,0.295,,011223,,,A*7C
        
        char *token = NULL;
        char sentence_copy[256];
        strncpy(sentence_copy, nmea_sentence, sizeof(sentence_copy) - 1);
        
        const char delim[] = ",*";
        token = strtok(sentence_copy, delim);
        
        // Token numarası tuple'ları: [1]=UTC time, [2]=Status, [3]=Lat, [4]=N/S, [5]=Lon, [6]=E/W, [7]=Speed
        int token_idx = 0;
        double lat_deg = 0.0, lon_deg = 0.0;
        char lat_dir = 'N', lon_dir = 'E';
        uint8_t status_char = '0';
        
        while (token != NULL && token_idx < 8) {
            if (token_idx == 2) {
                status_char = token[0];  // 'A' = aktif, 'V' = geçersiz
            } else if (token_idx == 3) {
                // Latitude: DDMM.MMMM
                double lat_raw = strtod(token, NULL);
                int lat_deg_int = (int)(lat_raw / 100.0);
                double lat_min = lat_raw - (lat_deg_int * 100.0);
                lat_deg = lat_deg_int + (lat_min / 60.0);
            } else if (token_idx == 4) {
                lat_dir = token[0];
            } else if (token_idx == 5) {
                // Longitude: DDDMM.MMMM
                double lon_raw = strtod(token, NULL);
                int lon_deg_int = (int)(lon_raw / 100.0);
                double lon_min = lon_raw - (lon_deg_int * 100.0);
                lon_deg = lon_deg_int + (lon_min / 60.0);
            } else if (token_idx == 6) {
                lon_dir = token[0];
            } else if (token_idx == 7) {
                measurement->velocity_mps = strtof(token, NULL) * 0.51444f;  // Knots to m/s
            }
            
            token = strtok(NULL, delim);
            token_idx++;
        }
        
        // Yön düzeltmesi
        if (lat_dir == 'S') lat_deg = -lat_deg;
        if (lon_dir == 'W') lon_deg = -lon_deg;
        
        measurement->latitude_deg = lat_deg;
        measurement->longitude_deg = lon_deg;
        measurement->fix_type = (status_char == 'A') ? GPS_FIX_GNSS_ONLY : GPS_FIX_INVALID;
        
        if (measurement->fix_type == GPS_FIX_INVALID) {
            gps_state.has_fix = 0;
            return -1;
        }
    }
    // GPGGA (Fix Data) - uydu sayısı, DOP, vb.
    else if (strstr(nmea_sentence, "$GPGGA") != NULL) {
        // Format: $GPGGA,hhmmss.ss,latitude,lat_dir,longitude,lon_dir,fix_quality,num_sats,hdop,altitude,...
        
        char *token = NULL;
        char sentence_copy[256];
        strncpy(sentence_copy, nmea_sentence, sizeof(sentence_copy) - 1);
        
        int token_idx = 0;
        
        token = strtok(sentence_copy, ",*");
        while (token != NULL && token_idx < 9) {
            if (token_idx == 6) {
                uint8_t fix_quality = (uint8_t)strtol(token, NULL, 10);
                // 0=No fix, 1=GPS, 2=DGPS, 3 ve üzeri = RTK/DGPS
                if (fix_quality == 0) {
                    measurement->fix_type = GPS_FIX_INVALID;
                } else if (fix_quality == 1) {
                    measurement->fix_type = GPS_FIX_GNSS_ONLY;
                } else if (fix_quality == 2) {
                    measurement->fix_type = GPS_FIX_DGPS;
                } else {
                    measurement->fix_type = GPS_FIX_RTK_FLOAT;
                }
            } else if (token_idx == 7) {
                measurement->num_satellites = (uint8_t)strtol(token, NULL, 10);
            } else if (token_idx == 8) {
                measurement->hdop = strtof(token, NULL);
            }
            
            token = strtok(NULL, ",*");
            token_idx++;
        }
    }
    
    measurement->status = GPS_STATUS_OK;
    return 0;
}

// ============================================================================
// GPS Başlatma ve Durum Yönetimi
// ============================================================================

/**
 * @brief GPS'i başlat
 */
int32_t gps_init(const GPSConfig_t *config) {
    if (config == NULL) return -1;
    
    memcpy((void *)&gps_state.config, config, sizeof(GPSConfig_t));
    
    // TODO: UART başlat (GPS modeline göre)
    // HAL_UART_Init(...);
    
    // TODO: UART interrupt'ını enable et
    // HAL_UART_Receive_IT(...);
    
    // TODO: Warm-up süresi (20-30 saniye)
    // vTaskDelay(30000 / portTICK_PERIOD_MS);
    
    gps_state.initialized = 1;
    gps_state.last_status = GPS_STATUS_OK;
    gps_state.has_fix = 0;
    
    return 0;
}

/**
 * @brief GPS durumunu kontrol et
 */
uint8_t gps_get_status(void) {
    if (!gps_state.initialized) {
        return GPS_STATUS_NOT_INITIALIZED;
    }
    
    return gps_state.last_status;
}

/**
 * @brief Uydu sayısını oku
 */
uint8_t gps_get_num_satellites(void) {
    return gps_state.last_measurement.num_satellites;
}

/**
 * @brief HDOP değerini oku
 */
float gps_get_hdop(void) {
    return gps_state.last_measurement.hdop;
}

/**
 * @brief Doğruluk değeri
 */
float gps_get_position_accuracy(void) {
    float dop = gps_state.last_measurement.hdop;
    if (dop <= 0.0f) return 999.0f;
    
    // Kaba tahmin: accuracy ≈ DOP × sigma (meter)
    return dop * 5.0f;  // ~5 metre baseline
}

/**
 * @brief Fix tipi nedir?
 */
uint8_t gps_get_fix_type(void) {
    return gps_state.last_measurement.fix_type;
}

// ============================================================================
// Konum Okuma
// ============================================================================

/**
 * @brief GPS'ten konum oku (UART buffer'dan)
 */
int32_t gps_read(GPSMeasurement_t *measurement) {
    if (measurement == NULL) return -1;
    if (!gps_state.initialized) return -1;
    
    // Son geçerli ölçümü döndür
    if (gps_state.has_fix) {
        memcpy(measurement, (void *)&gps_state.last_measurement,
               sizeof(GPSMeasurement_t));
        return 0;
    }
    
    // Fix yok - timeout
    gps_state.last_status = GPS_STATUS_NOT_FIXED;
    return -1;
}

/**
 * @brief Cache'den konum oku
 */
int32_t gps_read_cached_position(GPSMeasurement_t *measurement) {
    if (measurement == NULL) return -1;
    
    if (!gps_state.has_fix) {
        return -1;
    }
    
    memcpy(measurement, (void *)&gps_state.last_measurement,
           sizeof(GPSMeasurement_t));
    
    return 0;
}

/**
 * @brief RTC'yi GPS UTC'si ile senkronize et
 */
int32_t gps_sync_rtc(uint32_t utc_timestamp) {
    if (utc_timestamp == 0) return -1;
    
    // TODO: STM32 RTC'yi güncelle
    // time_t t = (time_t)utc_timestamp;
    // RTC_Set(t);
    
    return 0;
}

/**
 * @brief GPS'i uykuya al
 */
void gps_sleep(void) {
    // TODO: UART disable, GPS modülüne UBX komut gönder (power save mode)
}

/**
 * @brief GPS'i uyan
 */
void gps_wake(void) {
    // TODO: UART enable, GPS modülüne UBX komut gönder (normal mode)
}

// ============================================================================
// Test ve Debug
// ============================================================================

/**
 * @brief Self-test
 */
int32_t gps_selftest(void) {
    if (!gps_state.initialized) return -1;
    
    // TODO: Test NMEA cümlesi parse et
    const char *test_sentence = "$GPRMC,081350.00,A,4717.113210,N,00833.915187,E,0.295,,011223,,,A*7C";
    GPSMeasurement_t test_meas = {0};
    
    return gps_parse_nmea_sentence(test_sentence, &test_meas);
}

/**
 * @brief Koordinatları format et
 */
void gps_format_coordinates(int32_t lat, int32_t lon, char *lat_str, char *lon_str) {
    if (lat_str == NULL || lon_str == NULL) return;
    
    // 1E-7 derece -> decimal derece
    double lat_deg = lat / 10000000.0;
    double lon_deg = lon / 10000000.0;
    
    snprintf(lat_str, 16, "%.7f", lat_deg);
    snprintf(lon_str, 16, "%.7f", lon_deg);
}

/**
 * @brief SEU deteksiyonu
 */
uint8_t gps_detect_seu(void) {
    return (gps_state.seu_counter > 3) ? 1 : 0;
}

// ============================================================================
// UART Interrupt Handler
// ============================================================================

extern void gps_uart_interrupt_handler(uint8_t rx_byte) {
    // Circular buffer ile NMEA cümlesi topla
    if (nmea_rx.rx_index >= GPS_NMEA_BUFFER_SIZE) {
        nmea_rx.rx_index = 0;
    }
    
    nmea_rx.rx_buffer[nmea_rx.rx_index++] = rx_byte;
    
    // Satır sonu tespit edilmesi durumunda
    if (rx_byte == '\n' || rx_byte == '\r') {
        nmea_rx.line_ready = 1;
        
        // NMEA cümlesini parse et
        if (nmea_rx.rx_index > 10) {  // Minimum cümle uzunluğu
            gps_parse_nmea_sentence((const char *)nmea_rx.rx_buffer,
                                   (GPSMeasurement_t *)&gps_state.last_measurement);
            
            if (gps_state.last_measurement.fix_type != GPS_FIX_INVALID) {
                gps_state.has_fix = 1;
            }
        }
        
        nmea_rx.rx_index = 0;
        nmea_rx.line_ready = 0;
    }
}
