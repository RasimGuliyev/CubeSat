#include "rm3100.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// Global variables
static RM3100Config_t rm3100_config;
static RM3100Calibration_t rm3100_calibration;
static bool rm3100_initialized = false;

// Conversion constants
#define RM3100_LSB_TO_UT           0.075f      // µT per LSB at 200 cycle count
#define RM3100_REV_ID              0x22        // Expected revision ID

// Forward declarations
static int rm3100_i2c_write(uint8_t reg, uint8_t data);
static int rm3100_i2c_read(uint8_t reg, uint8_t* data);
static int rm3100_i2c_read_burst(uint8_t reg, uint8_t* buffer, uint8_t length);
static int rm3100_write_cycle_count(uint16_t ccx, uint16_t ccy, uint16_t ccz);
static int rm3100_write_tmr(uint8_t tmr);
static int32_t rm3100_convert_raw_to_int32(uint8_t* data);
static float rm3100_convert_lsb_to_ut(int32_t raw, uint16_t cycle_count);

// Mock I2C functions (replace with real HAL_I2C functions)
static int rm3100_i2c_write(uint8_t reg, uint8_t data) {
    printf("[RM3100] I2C Write: Reg 0x%02X = 0x%02X\n", reg, data);
    return 0;
}

static int rm3100_i2c_read(uint8_t reg, uint8_t* data) {
    // Mock responses based on register
    switch (reg) {
        case RM3100_REG_REVID:
            *data = RM3100_REV_ID;
            break;
        case RM3100_REG_STATUS:
            *data = 0x80; // DRDY bit set
            break;
        case RM3100_REG_BIST:
            *data = 0x00; // No BIST error
            break;
        default:
            *data = 0x00;
            break;
    }
    printf("[RM3100] I2C Read: Reg 0x%02X = 0x%02X\n", reg, *data);
    return 0;
}

static int rm3100_i2c_read_burst(uint8_t reg, uint8_t* buffer, uint8_t length) {
    // Mock measurement data
    static uint32_t mock_counter = 0;
    mock_counter++;

    // Generate mock magnetic field data (simulate Earth's field ~50µT)
    int32_t mock_x = (int32_t)(50000 + 1000 * sinf(mock_counter * 0.1f));
    int32_t mock_y = (int32_t)(20000 + 500 * cosf(mock_counter * 0.1f));
    int32_t mock_z = (int32_t)(30000 + 800 * sinf(mock_counter * 0.15f));

    // Convert to big-endian 24-bit format
    buffer[0] = (mock_x >> 16) & 0xFF;
    buffer[1] = (mock_x >> 8) & 0xFF;
    buffer[2] = mock_x & 0xFF;

    buffer[3] = (mock_y >> 16) & 0xFF;
    buffer[4] = (mock_y >> 8) & 0xFF;
    buffer[5] = mock_y & 0xFF;

    buffer[6] = (mock_z >> 16) & 0xFF;
    buffer[7] = (mock_z >> 8) & 0xFF;
    buffer[8] = mock_z & 0xFF;

    printf("[RM3100] I2C Burst Read: %d bytes from reg 0x%02X\n", length, reg);
    return 0;
}

static int rm3100_write_cycle_count(uint16_t ccx, uint16_t ccy, uint16_t ccz) {
    uint8_t data[6];

    // Convert to big-endian format
    data[0] = (ccx >> 8) & 0xFF;
    data[1] = ccx & 0xFF;
    data[2] = (ccy >> 8) & 0xFF;
    data[3] = ccy & 0xFF;
    data[4] = (ccz >> 8) & 0xFF;
    data[5] = ccz & 0xFF;

    // Write CCX, CCY, CCZ registers
    for (int i = 0; i < 6; i++) {
        if (rm3100_i2c_write(RM3100_REG_CCX + i, data[i]) != 0) {
            return -1;
        }
    }

    return 0;
}

static int rm3100_write_tmr(uint8_t tmr) {
    return rm3100_i2c_write(RM3100_REG_TMRC, tmr);
}

static int32_t rm3100_convert_raw_to_int32(uint8_t* data) {
    // Convert 24-bit big-endian to 32-bit signed integer
    int32_t value = (data[0] << 16) | (data[1] << 8) | data[2];

    // Sign extend if negative
    if (value & 0x800000) {
        value |= 0xFF000000;
    }

    return value;
}

static float rm3100_convert_lsb_to_ut(int32_t raw, uint16_t cycle_count) {
    // RM3100 sensitivity: 75 µT/LSB at 200 cycle count
    // Scales linearly with cycle count
    float sensitivity = RM3100_LSB_TO_UT * (200.0f / cycle_count);
    return raw * sensitivity;
}

int rm3100_init(const RM3100Config_t* config) {
    if (config == NULL) return -1;

    memcpy(&rm3100_config, config, sizeof(RM3100Config_t));

    // Initialize calibration data
    memset(&rm3100_calibration, 0, sizeof(RM3100Calibration_t));
    rm3100_calibration.soft_iron[0][0] = 1.0f;
    rm3100_calibration.soft_iron[1][1] = 1.0f;
    rm3100_calibration.soft_iron[2][2] = 1.0f;

    // Check revision ID for SEU detection
    uint8_t rev_id;
    if (rm3100_i2c_read(RM3100_REG_REVID, &rev_id) != 0) {
        printf("[RM3100] Failed to read revision ID\n");
        return -1;
    }

    if (rev_id != RM3100_REV_ID) {
        printf("[RM3100] Invalid revision ID: 0x%02X (expected 0x%02X)\n", rev_id, RM3100_REV_ID);
        return -1;
    }

    // Set cycle counts for all axes
    if (rm3100_write_cycle_count(config->cycle_count, config->cycle_count, config->cycle_count) != 0) {
        printf("[RM3100] Failed to set cycle counts\n");
        return -1;
    }

    // Set data rate
    if (rm3100_write_tmr(config->odr) != 0) {
        printf("[RM3100] Failed to set data rate\n");
        return -1;
    }

    rm3100_initialized = true;
    printf("[RM3100] Initialized at address 0x%02X, cycle count %d\n",
           config->i2c_address, config->cycle_count);

    return 0;
}

int rm3100_read_measurement(RM3100Measurement_t* measurement) {
    if (!rm3100_initialized || measurement == NULL) return -1;

    uint8_t buffer[9];

    // Trigger single measurement if in single mode
    if (rm3100_config.mode == RM3100_MODE_SINGLE) {
        if (rm3100_i2c_write(RM3100_REG_POLL, 0x70) != 0) { // Poll X, Y, Z
            return -1;
        }

        // Wait for measurement to complete (blocking)
        uint8_t status;
        do {
            if (rm3100_i2c_read(RM3100_REG_STATUS, &status) != 0) {
                return -1;
            }
        } while ((status & 0x80) == 0); // Wait for DRDY bit
    }

    // Read measurement data
    if (rm3100_i2c_read_burst(RM3100_REG_MX, buffer, 9) != 0) {
        return -1;
    }

    // Convert raw data to magnetic field
    int32_t raw_x = rm3100_convert_raw_to_int32(&buffer[0]);
    int32_t raw_y = rm3100_convert_raw_to_int32(&buffer[3]);
    int32_t raw_z = rm3100_convert_raw_to_int32(&buffer[6]);

    measurement->x = rm3100_convert_lsb_to_ut(raw_x, rm3100_config.cycle_count);
    measurement->y = rm3100_convert_lsb_to_ut(raw_y, rm3100_config.cycle_count);
    measurement->z = rm3100_convert_lsb_to_ut(raw_z, rm3100_config.cycle_count);
    measurement->timestamp = 0; // TODO: Add real timestamp

    // Apply calibration if enabled
    if (rm3100_config.use_calibration && rm3100_calibration.calibrated) {
        rm3100_apply_calibration(measurement);
    }

    return 0;
}

int rm3100_start_continuous(void) {
    if (!rm3100_initialized) return -1;

    // Set continuous measurement mode
    uint8_t cmm = 0x79; // Enable X, Y, Z continuous mode
    if (rm3100_i2c_write(RM3100_REG_CMM, cmm) != 0) {
        return -1;
    }

    rm3100_config.mode = RM3100_MODE_CONTINUOUS;
    printf("[RM3100] Started continuous measurement mode\n");

    return 0;
}

int rm3100_stop_continuous(void) {
    if (!rm3100_initialized) return -1;

    // Stop continuous measurement
    if (rm3100_i2c_write(RM3100_REG_CMM, 0x00) != 0) {
        return -1;
    }

    rm3100_config.mode = RM3100_MODE_SINGLE;
    printf("[RM3100] Stopped continuous measurement mode\n");

    return 0;
}

int rm3100_perform_self_test(void) {
    if (!rm3100_initialized) return -1;

    // Enable self-test
    if (rm3100_i2c_write(RM3100_REG_BIST, 0x8F) != 0) { // Enable all axes self-test
        return -1;
    }

    // Wait for self-test to complete
    uint8_t bist_status;
    do {
        if (rm3100_i2c_read(RM3100_REG_BIST, &bist_status) != 0) {
            return -1;
        }
    } while ((bist_status & 0x80) == 0); // Wait for completion

    // Check results
    if (bist_status & 0x01) { // X-axis failure
        printf("[RM3100] Self-test failed: X-axis\n");
        return -1;
    }
    if (bist_status & 0x02) { // Y-axis failure
        printf("[RM3100] Self-test failed: Y-axis\n");
        return -1;
    }
    if (bist_status & 0x04) { // Z-axis failure
        printf("[RM3100] Self-test failed: Z-axis\n");
        return -1;
    }

    printf("[RM3100] Self-test passed\n");
    return 0;
}

int rm3100_calibrate_hard_iron(uint8_t samples) {
    if (!rm3100_initialized || samples < 6) return -1;

    printf("[RM3100] Starting hard-iron calibration with %d samples\n", samples);

    float min_x = INFINITY, max_x = -INFINITY;
    float min_y = INFINITY, max_y = -INFINITY;
    float min_z = INFINITY, max_z = -INFINITY;

    // Collect samples (user should rotate sensor in all directions)
    for (uint8_t i = 0; i < samples; i++) {
        RM3100Measurement_t meas;
        if (rm3100_read_measurement(&meas) != 0) {
            return -1;
        }

        min_x = fminf(min_x, meas.x);
        max_x = fmaxf(max_x, meas.x);
        min_y = fminf(min_y, meas.y);
        max_y = fmaxf(max_y, meas.y);
        min_z = fminf(min_z, meas.z);
        max_z = fmaxf(max_z, meas.z);

        printf("[RM3100] Sample %d: %.2f, %.2f, %.2f µT\n", i+1, meas.x, meas.y, meas.z);
        vTaskDelay(pdMS_TO_TICKS(1000)); // Wait 1 second between samples
    }

    // Calculate hard-iron offsets (center of the measurement sphere)
    rm3100_calibration.hard_iron[0] = (min_x + max_x) / 2.0f;
    rm3100_calibration.hard_iron[1] = (min_y + max_y) / 2.0f;
    rm3100_calibration.hard_iron[2] = (min_z + max_z) / 2.0f;

    rm3100_calibration.calibrated = true;

    printf("[RM3100] Hard-iron calibration complete: [%.2f, %.2f, %.2f] µT\n",
           rm3100_calibration.hard_iron[0],
           rm3100_calibration.hard_iron[1],
           rm3100_calibration.hard_iron[2]);

    return 0;
}

int rm3100_calibrate_soft_iron(uint8_t samples) {
    // Soft-iron calibration requires more complex algorithms
    // For now, just set identity matrix
    printf("[RM3100] Soft-iron calibration not implemented (requires ellipsoid fitting)\n");
    return 0;
}

int rm3100_apply_calibration(RM3100Measurement_t* measurement) {
    if (!rm3100_calibration.calibrated) return -1;

    // Apply hard-iron correction
    measurement->x -= rm3100_calibration.hard_iron[0];
    measurement->y -= rm3100_calibration.hard_iron[1];
    measurement->z -= rm3100_calibration.hard_iron[2];

    // Apply soft-iron correction (matrix multiplication)
    float corrected[3];
    corrected[0] = rm3100_calibration.soft_iron[0][0] * measurement->x +
                   rm3100_calibration.soft_iron[0][1] * measurement->y +
                   rm3100_calibration.soft_iron[0][2] * measurement->z;

    corrected[1] = rm3100_calibration.soft_iron[1][0] * measurement->x +
                   rm3100_calibration.soft_iron[1][1] * measurement->y +
                   rm3100_calibration.soft_iron[1][2] * measurement->z;

    corrected[2] = rm3100_calibration.soft_iron[2][0] * measurement->x +
                   rm3100_calibration.soft_iron[2][1] * measurement->y +
                   rm3100_calibration.soft_iron[2][2] * measurement->z;

    measurement->x = corrected[0];
    measurement->y = corrected[1];
    measurement->z = corrected[2];

    return 0;
}

int rm3100_detect_seu(void) {
    uint8_t rev_id;
    if (rm3100_i2c_read(RM3100_REG_REVID, &rev_id) != 0) {
        return -1; // Communication error
    }

    if (rev_id != RM3100_REV_ID) {
        printf("[RM3100] SEU detected: Revision ID changed to 0x%02X\n", rev_id);
        return 1; // SEU detected
    }

    return 0; // No SEU
}

int rm3100_recover_from_seu(void) {
    printf("[RM3100] Attempting SEU recovery...\n");

    // Reset the device (power cycle would be ideal, but try software reset)
    // RM3100 doesn't have a software reset, so reinitialize
    RM3100Config_t temp_config = rm3100_config;
    rm3100_initialized = false;

    if (rm3100_init(&temp_config) != 0) {
        printf("[RM3100] SEU recovery failed\n");
        return -1;
    }

    printf("[RM3100] SEU recovery successful\n");
    return 0;
}

RM3100Calibration_t rm3100_get_calibration(void) {
    return rm3100_calibration;
}

void rm3100_set_calibration(const RM3100Calibration_t* cal) {
    if (cal != NULL) {
        memcpy(&rm3100_calibration, cal, sizeof(RM3100Calibration_t));
    }
}