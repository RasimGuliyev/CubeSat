/**
 * @file main.c
 * @brief Ulaş-S1 CubeSat Flight Software - System Dispatcher & Main Entry Point
 * 
 * Sorumluluğu:
 * 1. Hardware initialization (STM32H743 HAL, UART, I2C, SPI, RTC, Watchdog)
 * 2. FreeRTOS kernel ve scheduler konfigürasyonu
 * 3. Global queue/mutex/semaphore oluşturma
 * 4. Task'ları spawn etme (Priority, Stack size)
 * 5. System state machine (BOOT → NORMAL → SAFE MODE → SHUTDOWN)
 * 6. FreeRTOS hooks (Tick, Malloc Failure, Stack Overflow)
 * 7. İstisnai durumlarda recovery
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* FreeRTOS */
#include "include/FreeRTOS/FreeRTOS.h"
#include "include/FreeRTOS/task.h"
#include "include/FreeRTOS/queue.h"

/* Tasks */
#include "Tasks/Src/task_adcs.h"
#include "Tasks/Src/task_payload.h"
#include "Tasks/Src/task_comm.h"
#include "Tasks/Src/task_eps.h"

/* Drivers */
#include "Drivers/Payload/radiation_sensor.h"
#include "Drivers/Payload/barometer.h"
#include "Drivers/Payload/gps.h"
#include "Drivers/Storage/sd_card.h"

/* Crypto */
#include "Middleware/AES256/crypto.h"

/* Shared */
#include "../shared/telemetry.h"
#include "../shared/commands.h"

/* ============================================================================
 * GLOBAL SYSTEM STATE & QUEUES
 * ============================================================================ */

/**
 * @brief Sistem durumu enum'u
 */
typedef enum {
    SYS_STATE_BOOT = 0,           /* Başlangıç, sensör init */
    SYS_STATE_NORMAL,             /* Normal operasyon */
    SYS_STATE_SAFE_MODE,          /* Batarya düşük / Hata oluştu */
    SYS_STATE_SHUTDOWN,           /* Güç kapatılıyor */
    SYS_STATE_FAULT_RADIATION,    /* Yüksek radyasyon SEU hasar tespit */
} SystemState_t;

/**
 * @brief Global sistem konteksti
 */
typedef struct {
    SystemState_t state;
    SystemState_t prev_state;
    uint32_t state_duration_ticks;
    uint32_t boot_timestamp;       /* UTC saniye */
    uint32_t uptime_seconds;
    uint16_t safe_mode_counter;    /* Kaç defa safe mode entered */
    uint8_t watchdog_fed;
    uint8_t system_initialized;
} SystemContext_t;

static volatile SystemContext_t sys_ctx = {
    .state = SYS_STATE_BOOT,
    .prev_state = SYS_STATE_BOOT,
    .boot_timestamp = 0,
    .uptime_seconds = 0,
    .safe_mode_counter = 0,
    .watchdog_fed = 0,
    .system_initialized = 0
};

/* Global Communication Queues */
static QueueHandle_t telemetry_tx_queue = NULL;      /* Payload/ADCS → COMM TX */
static QueueHandle_t command_rx_queue = NULL;        /* COMM RX → Dispatcher */
static QueueHandle_t gps_data_queue = NULL;          /* GPS → Payload */
static QueueHandle_t imu_data_queue = NULL;          /* IMU → ADCS */

/* Task Handles */
static TaskHandle_t task_payload_handle = NULL;
static TaskHandle_t task_adcs_handle = NULL;
static TaskHandle_t task_comm_handle = NULL;
static TaskHandle_t task_eps_handle = NULL;
static TaskHandle_t task_watchdog_handle = NULL;
static TaskHandle_t task_dispatcher_handle = NULL;

/* ============================================================================
 * FORWARD DECLARATIONS
 * ============================================================================ */

static void vTaskDispatcher(void *pvParameters);
static void vTaskWatchdog(void *pvParameters);
static int32_t system_init_hardware(void);
static int32_t system_init_freertos(void);
static void system_state_transition(SystemState_t new_state);
static void system_handle_state(void);

/* ============================================================================
 * HARDWARE INITIALIZATION (STM32H743 HAL)
 * ============================================================================ */

/**
 * @brief MCU ve peripherals'ı initialize et
 * - Clock configuration (480 MHz)
 * - UART (3x: GPS, Radiation, Debug)
 * - I2C (2x: Barometer, Magnetometer)
 * - SPI (SD Card SDMMC)
 * - RTC (Real-Time Clock)
 * - GPIO (LED, Relay, Fan)
 * - Watchdog (IWDG)
 * - ADC (Sun Sensors)
 */
static int32_t system_init_hardware(void) {
    printf("[BOOT] Initializing STM32H743 Hardware...\n");

    /* TODO: Replace with actual STM32CubeMX generated HAL code when available
     *
     * For now, using mock initialization for compilation testing.
     * Real implementation should include:
     * - HAL_Init()
     * - SystemClock_Config() (480 MHz PLL)
     * - UART/I2C/SPI peripheral initialization
     * - GPIO, ADC, TIM, RTC, IWDG setup
     */

    printf("[BOOT] ✓ STM32H743 mock hardware initialization complete\n");
    printf("[BOOT] ✓ System clock: 480 MHz\n");
    printf("[BOOT] ✓ Peripherals: UART2/3/4, I2C1/2, SPI3, ADC1, TIM1, RTC, IWDG\n");

    return 0;  /* Success */
}

/* ============================================================================
 * FREERTOS INITIALIZATION
 * ============================================================================ */

/**
 * @brief Global queue'ları ve semaphore'ları oluştur
 */
static int32_t system_init_freertos(void) {
    printf("[BOOT] Initializing FreeRTOS Queues...\n");
    
    /* Telemetry TX Queue (Payload/ADCS → COMM)
     * Boyut: 32 paket + metadata
     * Her item: BeaconPacket_t veya PayloadPacket_t (max 108 byte)
     */
    telemetry_tx_queue = xQueueCreate(32, sizeof(PayloadPacket_t));
    if (telemetry_tx_queue == NULL) {
        printf("[BOOT] ✗ Failed to create telemetry_tx_queue\n");
        return -1;
    }
    printf("[BOOT] ✓ telemetry_tx_queue created\n");
    
    /* Command RX Queue (COMM RX → Dispatcher)
     * Boyut: 16 komut + metadata
     * Her item: CommandHeader_t + payload (variable)
     */
    command_rx_queue = xQueueCreate(16, sizeof(CommandHeader_t));
    if (command_rx_queue == NULL) {
        printf("[BOOT] ✗ Failed to create command_rx_queue\n");
        return -1;
    }
    printf("[BOOT] ✓ command_rx_queue created\n");
    
    /* GPS Data Queue (GPS ISR → Payload Task)
     * Boyut: 4 fix (redundancy)
     * Her item: GPSMeasurement_t (80 byte)
     */
    gps_data_queue = xQueueCreate(4, sizeof(GPSMeasurement_t));
    if (gps_data_queue == NULL) {
        printf("[BOOT] ✗ Failed to create gps_data_queue\n");
        return -1;
    }
    printf("[BOOT] ✓ gps_data_queue created\n");
    
    /* IMU Data Queue (IMU ISR → ADCS Task)
     * Boyut: 8 ölçüm
     * Her item: 3×float (12 byte), 3×float = 24 byte
     */
    imu_data_queue = xQueueCreate(8, 24);
    if (imu_data_queue == NULL) {
        printf("[BOOT] ✗ Failed to create imu_data_queue\n");
        return -1;
    }
    printf("[BOOT] ✓ imu_data_queue created\n");
    
    return 0;  /* Success */
}

/* ============================================================================
 * TASK SPAWNING
 * ============================================================================ */

/**
 * @brief Tüm FreeRTOS task'larını oluştur
 * 
 * Priority levels:
 * - Dispatcher (Task): 4 (High - kontrolü ele alması lazım)
 * - ADCS (Real-time): 3 (High - tutum kontrolü kritik)
 * - Payload (Real-time): 3 (High - sensör sampling periyodik)
 * - COMM (Medium): 2 (Medium - LoRa polling)
 * - Watchdog (Low): 1 (Low  - background)
 */
static int32_t system_spawn_tasks(void) {
    printf("[BOOT] Spawning FreeRTOS Tasks...\n");
    
    /* Task 1: Payload Collection (100 ms sampling) */
    BaseType_t result1 = xTaskCreate(
        (TaskFunction_t)payload_task_init,  /* TODO: Actual task function */
        "PAYLOAD_TASK",                      /* Task name (DEBUG) */
        2048,                                /* Stack depth (bytes) */
        NULL,                                /* Parameter */
        3,                                   /* Priority (High) */
        &task_payload_handle                 /* Task handle output */
    );
    if (result1 != pdPASS) {
        printf("[BOOT] ✗ Failed to create PAYLOAD task\n");
        return -1;
    }
    printf("[BOOT] ✓ PAYLOAD_TASK spawned (Priority 3)\n");
    
    /* Task 2: ADCS Control (50 ms Kalman update) */
    BaseType_t result2 = xTaskCreate(
        (TaskFunction_t)adcs_task_init,      /* TODO: Actual task function */
        "ADCS_TASK",
        2560,                                /* Stack depth (bytes) */
        NULL,
        3,                                   /* Priority (High) */
        &task_adcs_handle
    );
    if (result2 != pdPASS) {
        printf("[BOOT] ✗ Failed to create ADCS task\n");
        return -1;
    }
    printf("[BOOT] ✓ ADCS_TASK spawned (Priority 3)\n");
    
    /* Task 3: Communication (LoRa TX/RX polling) */
    BaseType_t result3 = xTaskCreate(
        (TaskFunction_t)comm_task_init,      /* TODO: Actual task function */
        "COMM_TASK",
        2048,                                /* Stack depth (bytes) */
        NULL,
        2,                                   /* Priority (Medium) */
        &task_comm_handle
    );
    if (result3 != pdPASS) {
        printf("[BOOT] ✗ Failed to create COMM task\n");
        return -1;
    }
    printf("[BOOT] ✓ COMM_TASK spawned (Priority 2)\n");
    
    /* Task 3.5: EPS Power Management (Battery monitoring) */
    BaseType_t result3_5 = xTaskCreate(
        (TaskFunction_t)eps_task,            /* EPS task function */
        "EPS_TASK",
        1536,                                /* Stack depth (bytes) */
        NULL,
        2,                                   /* Priority (Medium) */
        &task_eps_handle
    );
    if (result3_5 != pdPASS) {
        printf("[BOOT] ✗ Failed to create EPS task\n");
        return -1;
    }
    printf("[BOOT] ✓ EPS_TASK spawned (Priority 2)\n");
    
    /* Task 4: Watchdog Feeder (Low priority, feeds every 5 seconds) */
    BaseType_t result4 = xTaskCreate(
        vTaskWatchdog,
        "WATCHDOG_TASK",
        512,                                 /* Minimal stack */
        NULL,
        1,                                   /* Priority (Low) */
        &task_watchdog_handle
    );
    if (result4 != pdPASS) {
        printf("[BOOT] ✗ Failed to create WATCHDOG task\n");
        return -1;
    }
    printf("[BOOT] ✓ WATCHDOG_TASK spawned (Priority 1)\n");
    
    /* Task 5: System Dispatcher (Master controller, monitors state) */
    BaseType_t result5 = xTaskCreate(
        vTaskDispatcher,
        "DISPATCHER_TASK",
        1024,
        NULL,
        4,                                   /* Priority (Highest) */
        &task_dispatcher_handle
    );
    if (result5 != pdPASS) {
        printf("[BOOT] ✗ Failed to create DISPATCHER task\n");
        return -1;
    }
    printf("[BOOT] ✓ DISPATCHER_TASK spawned (Priority 4 - Highest)\n");
    
    return 0;  /* Success */
}

/* ============================================================================
 * WATCHDOG TASK
 * ============================================================================ */

/**
 * @brief Watchdog timer'ını her 5 saniyede feed et
 * 
 * Görev: STM32 IWDG (Independent Watchdog) süresini sıfırla
 * Eğer task çalışmayıp feed edilmezse → Reset
 * SEU detection: Garbled task control → system restart
 */
static void vTaskWatchdog(void *pvParameters) {
    (void)pvParameters;
    
    printf("[WATCHDOG] Task started\n");
    
    const TickType_t feed_period = pdMS_TO_TICKS(5000);  /* 5 sec */
    
    while (1) {
        /* TODO: HAL_IWDG_Refresh();  <- Actual watchdog feed */
        
        sys_ctx.watchdog_fed = 1;  /* Status flag */
        
        printf("[WATCHDOG] ✓ Fed (uptime: %u sec)\n", sys_ctx.uptime_seconds);
        
        vTaskDelay(feed_period);
    }
}

/* ============================================================================
 * SYSTEM STATE MACHINE
 * ============================================================================ */

/**
 * @brief State değişkeni set et (transition guard'ı ile)
 */
static void system_state_transition(SystemState_t new_state) {
    if (new_state != sys_ctx.state) {
        printf("[STATE] Transition: %d → %d\n", sys_ctx.state, new_state);
        sys_ctx.prev_state = sys_ctx.state;
        sys_ctx.state = new_state;
        sys_ctx.state_duration_ticks = 0;
        
        if (new_state == SYS_STATE_SAFE_MODE) {
            sys_ctx.safe_mode_counter++;
            printf("[STATE] Safe Mode Count: %u\n", sys_ctx.safe_mode_counter);
        }
    }
}

/**
 * @brief Mevcut sistem state'ine göre aksiyon al
 */
static void system_handle_state(void) {
    switch (sys_ctx.state) {
        case SYS_STATE_BOOT: {
            /* Sensor init ve self-test */
            printf("[STATE] BOOT: Initializing sensors...\n");
            
            /* TODO: Check rad sensor, barometer, GPS, SD card
             * if (all_sensors_ok) {
             *     system_state_transition(SYS_STATE_NORMAL);
             * } else {
             *     system_state_transition(SYS_STATE_SAFE_MODE);
             * }
             */
            
            /* Simülasyon: Normal'e geç */
            system_state_transition(SYS_STATE_NORMAL);
            break;
        }
        
        case SYS_STATE_NORMAL: {
            /* Tüm task'lar çalışıyor, telemetri normal
             * 
             * Monitoring:
             * - Battery voltage > 5.0V
             * - Radiation dose < threshold
             * - SD card available
             * - GPS lock status (optional)
             */
            
            /* Check battery */
            // TODO: Check BQ24295 via I2C
            
            /* Check radiation */
            // TODO: If rad_dose > THRESHOLD → SAFE_MODE
            
            break;
        }
        
        case SYS_STATE_SAFE_MODE: {
            /* Reduced power mode
             * - Payload task: Sampling period 10 sec (vs 1 sec)
             * - ADCS task: Passive, no control (spin control off)
             * - COMM task: TX beacon only, no payload TX
             * - Wait for battery recovery or manual command
             */
            printf("[STATE] SAFE_MODE: Reduced power operation\n");
            
            /* TODO: Signal tasks to reduce their rates
             * adcs_task_set_mode(ADCS_MODE_PASSIVE);
             * payload_task_set_sampling_period(10000);  // 10 sec
             */
            
            break;
        }
        
        case SYS_STATE_FAULT_RADIATION: {
            /* Critical radiation condition
             * - All payload collection STOPPED
             * - ADCS control disabled
             * - Send distress beacon every 30 sec
             * - Wait for command or recovery
             */
            printf("[STATE] FAULT_RADIATION: Critical condition, beacon only\n");
            
            // TODO: Send emergency beacon
            
            break;
        }
        
        case SYS_STATE_SHUTDOWN: {
            /* Power-down sequence
             * - Save state to EEPROM/SD
             * - Disable all peripherals
             * - Sleep / Ultra-low power
             */
            printf("[STATE] SHUTDOWN: Powering down\n");
            while (1) {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            break;
        }
        
        default:
            break;
    }
}

/* ============================================================================
 * DISPATCHER TASK (Master Controller)
 * ============================================================================ */

/**
 * @brief Sistem dispatcher - Master control task
 * 
 * Görevler:
 * 1. System state machine yönet
 * 2. Command queue'dan komut al ve işle
 * 3. Task health monitoring
 * 4. Telemetry routing (Payload/ADCS → COMM)
 * 5. Error recovery
 */
static void vTaskDispatcher(void *pvParameters) {
    (void)pvParameters;
    
    printf("[DISPATCHER] Task started\n");
    
    const TickType_t poll_period = pdMS_TO_TICKS(1000);  /* 1 sec */
    
    while (1) {
        /* 1. System state handling */
        system_handle_state();
        
        /* 1.5. Check EPS for safe mode trigger */
        if (eps_task_should_enter_safe_mode()) {
            if (sys_ctx.state != SYS_STATE_SAFE_MODE) {
                printf("[DISPATCHER] Battery critical - entering SAFE MODE\n");
                system_state_transition(SYS_STATE_SAFE_MODE);
            }
        }
        
        /* 2. Check for incoming commands */
        CommandHeader_t cmd = {0};
        if (xQueueReceive(command_rx_queue, &cmd, 0) == pdTRUE) {
            printf("[DISPATCHER] Command received: ID=%d\n", cmd.command_id);
            
            /* TODO: Process command
             * switch (cmd.command_id) {
             *     case CMD_MODE_CHANGE:
             *         system_state_transition(SYS_STATE_NORMAL);
             *         break;
             *     case CMD_RESET_SYSTEM:
             *         NVIC_SystemReset();
             *         break;
             *     case CMD_DATA_REQUEST:
             *         // Send stored SD data to ground
             *         break;
             * }
             */
        }
        
        /* 3. Monitor tasks */
        if (task_payload_handle != NULL) {
            UBaseType_t stack_free = uxTaskGetStackHighWaterMark(task_payload_handle);
            if (stack_free < 256) {
                printf("[DISPATCHER] WARNING: Payload task stack low (%u bytes)\n", stack_free);
            }
        }
        
        /* 4. Update uptime */
        sys_ctx.uptime_seconds++;
        sys_ctx.state_duration_ticks++;
        
        vTaskDelay(poll_period);
    }
}

/* ============================================================================
 * FREERTOS HOOKS (sistem-level monitoring)
 * ============================================================================ */

/**
 * @brief FreeRTOS Tick Hook - Her tick'te çalışır (ISR context)
 */
void vApplicationTickHook(void) {
    /* Lightweight monitoring - feed watchdog every 1000 ticks (1 sec) */
    static uint32_t tick_count = 0;
    tick_count++;
    
    if (tick_count >= 1000) {
        tick_count = 0;
        sys_ctx.watchdog_fed = 1;  /* Signal watchdog task */
    }
}

/**
 * @brief Stack overflow hook - Task'ın stack'i overflow olursa
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("[ERROR] Stack Overflow in task: %s\n", pcTaskName);
    printf("[ERROR] Entering Safe Mode...\n");
    
    /* SEU protection: volatilize system state */
    sys_ctx.state = SYS_STATE_SAFE_MODE;
    
    /* Infinite loop - wait for watchdog reset */
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Malloc failure hook - Memory allocation failed
 */
void vApplicationMallocFailedHook(void) {
    printf("[ERROR] Memory allocation failed!\n");
    printf("[ERROR] Entering Safe Mode...\n");
    
    sys_ctx.state = SYS_STATE_SAFE_MODE;
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ============================================================================
 * MAIN ENTRY POINT
 * ============================================================================ */

/**
 * @brief Main function - STM32H743's entry point
 * 
 * Execution flow:
 * 1. Hardware init
 * 2. FreeRTOS queue/mutex init
 * 3. Task spawning
 * 4. Scheduler start (never returns)
 */
int main(void) {
    printf("\n╔════════════════════════════════════════════════════════════════╗\n");
    printf("║       Ulaş-S1 CubeSat Flight Software - System Boot          ║\n");
    printf("║                  (Stratosphere Balloon Mission)               ║\n");
    printf("╚════════════════════════════════════════════════════════════════╝\n\n");
    
    /* Step 1: Hardware Initialization */
    printf("[BOOT] === STEP 1: Hardware Initialization ===\n");
    if (system_init_hardware() != 0) {
        printf("[ERROR] Hardware initialization failed!\n");
        return -1;
    }
    
    /* Step 2: FreeRTOS Initialization */
    printf("\n[BOOT] === STEP 2: FreeRTOS Initialization ===\n");
    if (system_init_freertos() != 0) {
        printf("[ERROR] FreeRTOS initialization failed!\n");
        return -1;
    }
    
    /* Step 2.5: EPS Task Initialization */
    printf("\n[BOOT] === STEP 2.5: EPS Task Initialization ===\n");
    EPSTaskConfig_t eps_config = {
        .monitoring_period_ms = 500,
        .batt_voltage_threshold_safe = 6.0f,
        .batt_voltage_threshold_low = 6.5f,
        .batt_voltage_threshold_normal = 7.2f,
        .batt_voltage_threshold_charge = 8.2f,
        .i2c_bus_id = 1,
        .bq24295_address = 0x6A
    };
    if (eps_task_init(&eps_config) != 0) {
        printf("[ERROR] EPS task initialization failed!\n");
        return -1;
    }
    
    /* Step 3: Task Spawning */
    printf("\n[BOOT] === STEP 3: Spawning Tasks ===\n");
    if (system_spawn_tasks() != 0) {
        printf("[ERROR] Task spawning failed!\n");
        return -1;
    }
    
    /* System initialized flag */
    sys_ctx.system_initialized = 1;
    
    printf("\n[BOOT] === STEP 4: Starting FreeRTOS Scheduler ===\n");
    printf("[BOOT] Entering normal operation...\n\n");
    
    /* Start the FreeRTOS scheduler
     * This function NEVER RETURNS (runs forever)
     */
    vTaskStartScheduler();
    
    /* If scheduler returns, something is VERY wrong */
    printf("[CRITICAL] Scheduler terminated unexpectedly!\n");
    while (1) {
        /* Infinite loop - wait for watchdog reset */
    }
    
    return 0;  /* Never reached */
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * @brief System context'i oku (debugging için)
 */
SystemContext_t get_system_context(void) {
    return sys_ctx;
}

/**
 * @brief Queue handle'ları dış task'ların kullanması için export et
 */
QueueHandle_t get_telemetry_queue(void) {
    return telemetry_tx_queue;
}

QueueHandle_t get_command_queue(void) {
    return command_rx_queue;
}

QueueHandle_t get_gps_queue(void) {
    return gps_data_queue;
}

QueueHandle_t get_imu_queue(void) {
    return imu_data_queue;
}
