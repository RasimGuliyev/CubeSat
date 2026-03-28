#ifndef RM3100_H
#define RM3100_H

#include <stdint.h>
#include <stdbool.h>

// RM3100 I2C addresses
#define RM3100_I2C_ADDR_LOW          0x20    // AD0 = GND
#define RM3100_I2C_ADDR_HIGH         0x21    // AD0 = VCC

// RM3100 register addresses
#define RM3100_REG_POLL              0x00
#define RM3100_REG_CMM               0x01
#define RM3100_REG_CCX               0x04
#define RM3100_REG_CCY               0x06
#define RM3100_REG_CCZ               0x08
#define RM3100_REG_TMRC              0x0B
#define RM3100_REG_MX                0x24
#define RM3100_REG_MY                0x27
#define RM3100_REG_MZ                0x2A
#define RM3100_REG_BIST              0x33
#define RM3100_REG_STATUS            0x34
#define RM3100_REG_HSHAKE            0x35
#define RM3100_REG_REVID             0x36

// RM3100 operating modes
typedef enum {
    RM3100_MODE_SINGLE,            // Single measurement
    RM3100_MODE_CONTINUOUS         // Continuous measurement
} RM3100Mode_t;

// RM3100 output data rates
typedef enum {
    RM3100_ODR_600HZ   = 0x92,     // 600 Hz
    RM3100_ODR_300HZ   = 0x93,     // 300 Hz
    RM3100_ODR_150HZ   = 0x94,     // 150 Hz
    RM3100_ODR_75HZ    = 0x95,     // 75 Hz
    RM3100_ODR_37HZ    = 0x96,     // 37 Hz
    RM3100_ODR_18HZ    = 0x97,     // 18 Hz
    RM3100_ODR_9HZ     = 0x98,     // 9 Hz
    RM3100_ODR_4_5HZ   = 0x99,     // 4.5 Hz
    RM3100_ODR_2_3HZ   = 0x9A,     // 2.3 Hz
    RM3100_ODR_1_2HZ   = 0x9B,     // 1.2 Hz
    RM3100_ODR_0_6HZ   = 0x9C,     // 0.6 Hz
    RM3100_ODR_0_3HZ   = 0x9D,     // 0.3 Hz
    RM3100_ODR_0_15HZ  = 0x9E,     // 0.15 Hz
    RM3100_ODR_0_075HZ = 0x9F      // 0.075 Hz
} RM3100ODR_t;

// 3-axis magnetic field measurement structure
typedef struct {
    float x;        // X-axis magnetic field (µT)
    float y;        // Y-axis magnetic field (µT)
    float z;        // Z-axis magnetic field (µT)
    uint32_t timestamp; // Measurement timestamp (ticks)
} RM3100Measurement_t;

// Calibration data structure
typedef struct {
    float hard_iron[3];        // Hard-iron offset (µT) [x, y, z]
    float soft_iron[3][3];     // Soft-iron matrix (3x3)
    bool calibrated;           // Calibration status
} RM3100Calibration_t;

// RM3100 configuration structure
typedef struct {
    uint8_t i2c_address;       // I2C address (0x20 or 0x21)
    RM3100Mode_t mode;         // Operating mode
    RM3100ODR_t odr;           // Output data rate
    uint16_t cycle_count;      // Measurement cycle count (200-400 recommended)
    bool use_calibration;      // Apply calibration corrections
} RM3100Config_t;

// Function prototypes
int rm3100_init(const RM3100Config_t* config);
int rm3100_read_measurement(RM3100Measurement_t* measurement);
int rm3100_start_continuous(void);
int rm3100_stop_continuous(void);
int rm3100_perform_self_test(void);
int rm3100_calibrate_hard_iron(uint8_t samples);
int rm3100_calibrate_soft_iron(uint8_t samples);
int rm3100_apply_calibration(RM3100Measurement_t* measurement);
int rm3100_detect_seu(void);
int rm3100_recover_from_seu(void);
RM3100Calibration_t rm3100_get_calibration(void);
void rm3100_set_calibration(const RM3100Calibration_t* cal);

#endif // RM3100_H