#include "task_eps.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stdio.h>
#include <string.h>

// BQ24295 register addresses
#define BQ24295_REG_STATUS          0x08
#define BQ24295_REG_CONTROL         0x00
#define BQ24295_REG_VOLTAGE         0x02
#define BQ24295_REG_CURRENT         0x04
#define BQ24295_REG_INPUT_VOLTAGE   0x06

// BQ24295 status bits
#define BQ24295_STAT_CHARGING      (1 << 4)
#define BQ24295_STAT_FAULT          (1 << 7)

// LiPo 2S voltage ranges (6.0V - 8.4V)
#define BATT_VOLTAGE_MIN            6.0f    // 3.0V per cell
#define BATT_VOLTAGE_MAX            8.4f    // 4.2V per cell
#define BATT_VOLTAGE_CRITICAL       6.2f    // 3.1V per cell
#define BATT_VOLTAGE_LOW            6.8f    // 3.4V per cell
#define BATT_VOLTAGE_NORMAL         7.4f    // 3.7V per cell

// Global variables
static EPSTaskConfig_t eps_config;
static EPSTaskBatteryStatus_t eps_status;
static SemaphoreHandle_t eps_mutex = NULL;
static bool eps_initialized = false;

// Forward declarations
static int eps_i2c_write(uint8_t reg, uint8_t data);
static int eps_i2c_read(uint8_t reg, uint8_t* data);
static float eps_read_battery_voltage(void);
static float eps_read_battery_current(void);
static EPSTaskBatterySOC_t eps_calculate_soc(float voltage);
static int eps_set_charge_current(uint16_t ma);
static int eps_set_charge_voltage(uint16_t mv);
static void eps_update_power_mode(void);

// Mock I2C functions (replace with real HAL_I2C functions)
static int eps_i2c_write(uint8_t reg, uint8_t data) {
    // TODO: Replace with HAL_I2C_Master_Transmit
    printf("[EPS] I2C Write: Reg 0x%02X = 0x%02X\n", reg, data);
    return 0; // Success
}

static int eps_i2c_read(uint8_t reg, uint8_t* data) {
    // TODO: Replace with HAL_I2C_Master_Transmit + HAL_I2C_Master_Receive
    // Mock data based on register
    switch (reg) {
        case BQ24295_REG_STATUS:
            *data = BQ24295_STAT_CHARGING; // Mock: charging
            break;
        case BQ24295_REG_VOLTAGE:
            *data = 0x8A; // Mock: ~8.0V
            break;
        case BQ24295_REG_CURRENT:
            *data = 0x40; // Mock: 1A charging
            break;
        default:
            *data = 0x00;
            break;
    }
    printf("[EPS] I2C Read: Reg 0x%02X = 0x%02X\n", reg, *data);
    return 0; // Success
}

static float eps_read_battery_voltage(void) {
    uint8_t msb, lsb;
    if (eps_i2c_read(BQ24295_REG_VOLTAGE, &msb) != 0) return 0.0f;
    if (eps_i2c_read(BQ24295_REG_VOLTAGE + 1, &lsb) != 0) return 0.0f;

    uint16_t raw = (msb << 8) | lsb;
    // BQ24295 voltage register: 64mV per LSB, offset 3.5V
    return 3.5f + (raw * 0.064f);
}

static float eps_read_battery_current(void) {
    uint8_t msb, lsb;
    if (eps_i2c_read(BQ24295_REG_CURRENT, &msb) != 0) return 0.0f;
    if (eps_i2c_read(BQ24295_REG_CURRENT + 1, &lsb) != 0) return 0.0f;

    uint16_t raw = (msb << 8) | lsb;
    // BQ24295 current register: 50mA per LSB
    return raw * 0.05f;
}

static EPSTaskBatterySOC_t eps_calculate_soc(float voltage) {
    if (voltage < BATT_VOLTAGE_CRITICAL) return EPS_BATT_SOC_CRITICAL;
    if (voltage < BATT_VOLTAGE_LOW) return EPS_BATT_SOC_LOW;
    if (voltage < BATT_VOLTAGE_NORMAL) return EPS_BATT_SOC_MEDIUM;
    if (voltage < BATT_VOLTAGE_MAX * 0.95f) return EPS_BATT_SOC_HIGH;
    return EPS_BATT_SOC_FULL;
}

static int eps_set_charge_current(uint16_t ma) {
    // Convert mA to register value (50mA per LSB)
    uint8_t reg_val = ma / 50;
    if (reg_val > 0x3F) reg_val = 0x3F; // Max 3.15A

    return eps_i2c_write(BQ24295_REG_CURRENT, reg_val);
}

static int eps_set_charge_voltage(uint16_t mv) {
    // Convert mV to register value (16mV per LSB, offset 3.5V)
    uint16_t reg_val = (mv - 3500) / 16;
    if (reg_val > 0xFF) reg_val = 0xFF; // Max ~8.4V

    return eps_i2c_write(BQ24295_REG_VOLTAGE, reg_val >> 8); // MSB
    // TODO: Write LSB as well
}

static void eps_update_power_mode(void) {
    EPSTaskPowerMode_t new_mode;

    switch (eps_status.soc) {
        case EPS_BATT_SOC_CRITICAL:
            new_mode = EPS_POWER_MODE_SAFE;
            break;
        case EPS_BATT_SOC_LOW:
            new_mode = EPS_POWER_MODE_LOW_POWER;
            break;
        case EPS_BATT_SOC_MEDIUM:
            new_mode = EPS_POWER_MODE_BALANCED;
            break;
        case EPS_BATT_SOC_HIGH:
        case EPS_BATT_SOC_FULL:
        default:
            new_mode = EPS_POWER_MODE_NORMAL;
            break;
    }

    if (new_mode != eps_status.mode) {
        printf("[EPS] Power mode change: %d -> %d\n", eps_status.mode, new_mode);
        eps_status.mode = new_mode;
    }
}

int eps_task_init(const EPSTaskConfig_t* config) {
    if (config == NULL) return -1;

    memcpy(&eps_config, config, sizeof(EPSTaskConfig_t));

    // Initialize status
    eps_status.voltage = 0.0f;
    eps_status.current = 0.0f;
    eps_status.soc = EPS_BATT_SOC_MEDIUM;
    eps_status.mode = EPS_POWER_MODE_NORMAL;
    eps_status.is_charging = false;
    eps_status.timestamp = 0;

    // Create mutex for thread safety
    eps_mutex = xSemaphoreCreateMutex();
    if (eps_mutex == NULL) return -1;

    eps_initialized = true;
    printf("[EPS] Initialized with I2C bus %d, BQ24295 addr 0x%02X\n",
           eps_config.i2c_bus_id, eps_config.bq24295_address);

    return 0;
}

void eps_task(void* pvParameters) {
    (void)pvParameters;

    if (!eps_initialized) {
        printf("[EPS] Task started but not initialized!\n");
        vTaskDelete(NULL);
        return;
    }

    printf("[EPS] Task started, monitoring period: %d ms\n", eps_config.monitoring_period_ms);

    while (1) {
        // Take mutex
        if (xSemaphoreTake(eps_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            // Read battery voltage and current
            eps_status.voltage = eps_read_battery_voltage();
            eps_status.current = eps_read_battery_current();

            // Check charging status
            uint8_t status_reg;
            if (eps_i2c_read(BQ24295_REG_STATUS, &status_reg) == 0) {
                eps_status.is_charging = (status_reg & BQ24295_STAT_CHARGING) != 0;
            }

            // Calculate SOC and update timestamp
            eps_status.soc = eps_calculate_soc(eps_status.voltage);
            eps_status.timestamp = xTaskGetTickCount();

            // Update power mode based on battery status
            eps_update_power_mode();

            // Log status
            printf("[EPS] V=%.2fV, I=%.2fA, SOC=%d, Mode=%d, Charging=%d\n",
                   eps_status.voltage, eps_status.current, eps_status.soc,
                   eps_status.mode, eps_status.is_charging);

            // Release mutex
            xSemaphoreGive(eps_mutex);
        }

        // Delay until next measurement
        vTaskDelay(pdMS_TO_TICKS(eps_config.monitoring_period_ms));
    }
}

bool eps_task_should_enter_safe_mode(void) {
    if (!eps_initialized) return false;

    bool should_safe = false;

    if (xSemaphoreTake(eps_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        should_safe = (eps_status.voltage < eps_config.batt_voltage_threshold_safe) ||
                     (eps_status.soc == EPS_BATT_SOC_CRITICAL);
        xSemaphoreGive(eps_mutex);
    }

    return should_safe;
}

EPSTaskBatteryStatus_t eps_task_get_status(void) {
    EPSTaskBatteryStatus_t status = {0};

    if (eps_initialized && xSemaphoreTake(eps_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        memcpy(&status, &eps_status, sizeof(EPSTaskBatteryStatus_t));
        xSemaphoreGive(eps_mutex);
    }

    return status;
}

int eps_task_set_power_mode(EPSTaskPowerMode_t mode) {
    if (!eps_initialized) return -1;

    if (xSemaphoreTake(eps_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        eps_status.mode = mode;
        xSemaphoreGive(eps_mutex);
        printf("[EPS] Power mode manually set to %d\n", mode);
        return 0;
    }

    return -1;
}