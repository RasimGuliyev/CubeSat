#ifndef TASK_EPS_H
#define TASK_EPS_H

#include <stdint.h>
#include <stdbool.h>

// Power management modes
typedef enum {
    EPS_POWER_MODE_NORMAL,      // Full power for downlink operations
    EPS_POWER_MODE_BALANCED,    // Balanced power consumption
    EPS_POWER_MODE_LOW_POWER,   // Reduced power for idle periods
    EPS_POWER_MODE_SAFE         // Minimum power, essential systems only
} EPSTaskPowerMode_t;

// Battery state of charge levels
typedef enum {
    EPS_BATT_SOC_CRITICAL = 0,  // < 10% - immediate safe mode
    EPS_BATT_SOC_LOW = 1,       // 10-20% - low power mode
    EPS_BATT_SOC_MEDIUM = 2,    // 20-70% - balanced mode
    EPS_BATT_SOC_HIGH = 3,      // 70-90% - normal mode
    EPS_BATT_SOC_FULL = 4       // > 90% - full charge
} EPSTaskBatterySOC_t;

// EPS task configuration structure
typedef struct {
    uint32_t monitoring_period_ms;      // How often to check battery (ms)
    float batt_voltage_threshold_safe;  // Voltage to trigger safe mode (V)
    float batt_voltage_threshold_low;   // Voltage for low power mode (V)
    float batt_voltage_threshold_normal;// Voltage for normal mode (V)
    float batt_voltage_threshold_charge;// Voltage when charging starts (V)
    uint8_t i2c_bus_id;                 // I2C bus number (1 or 2)
    uint8_t bq24295_address;            // BQ24295 I2C address
} EPSTaskConfig_t;

// Battery status structure
typedef struct {
    float voltage;              // Battery voltage (V)
    float current;              // Battery current (A, positive = charging)
    EPSTaskBatterySOC_t soc;    // State of charge
    EPSTaskPowerMode_t mode;    // Current power mode
    bool is_charging;           // Charging status
    uint32_t timestamp;         // Last measurement timestamp
} EPSTaskBatteryStatus_t;

// Function prototypes
int eps_task_init(const EPSTaskConfig_t* config);
void eps_task(void* pvParameters);
bool eps_task_should_enter_safe_mode(void);
EPSTaskBatteryStatus_t eps_task_get_status(void);
int eps_task_set_power_mode(EPSTaskPowerMode_t mode);

#endif // TASK_EPS_H