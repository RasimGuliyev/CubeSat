// fsw/include/FreeRTOS/FreeRTOS.c
// Mock FreeRTOS implementations for compilation testing
// In production, use actual FreeRTOS library

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include <stdio.h>
#include <stdlib.h>

// Mock task handle
typedef struct {
    char name[32];
    TaskFunction_t function;
    void *parameters;
} MockTask_t;

static MockTask_t mock_tasks[10];
static int task_count = 0;

// Mock queue handle
typedef struct {
    char name[32];
    int max_items;
    int item_count;
} MockQueue_t;

static MockQueue_t mock_queues[10];
static int queue_count = 0;

// Mock semaphore handle
typedef struct {
    char name[32];
    int count;
} MockSemaphore_t;

static MockSemaphore_t mock_semaphores[10];
static int semaphore_count = 0;

// Global tick count
static TickType_t global_tick_count = 0;

// ============================================================================
// TASK FUNCTIONS
// ============================================================================

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode, const char * const pcName,
                      uint16_t usStackDepth, void *pvParameters,
                      UBaseType_t uxPriority, TaskHandle_t *pxCreatedTask) {
    (void)usStackDepth; // Unused in mock
    (void)uxPriority;   // Unused in mock

    if (task_count >= 10) return pdFAIL;

    MockTask_t *task = &mock_tasks[task_count++];
    snprintf(task->name, sizeof(task->name), "%s", pcName);
    task->function = pxTaskCode;
    task->parameters = pvParameters;

    if (pxCreatedTask) {
        *pxCreatedTask = (TaskHandle_t)task;
    }

    printf("Mock FreeRTOS: Created task '%s'\n", pcName);
    return pdPASS;
}

void vTaskDelete(TaskHandle_t xTaskToDelete) {
    (void)xTaskToDelete; // Mock implementation - do nothing
    printf("Mock FreeRTOS: Task deleted\n");
}

void vTaskDelay(TickType_t xTicksToDelay) {
    global_tick_count += xTicksToDelay;
    printf("Mock FreeRTOS: Delayed %lu ticks\n", xTicksToDelay);
}

TickType_t xTaskGetTickCount(void) {
    return global_tick_count;
}

UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t xTask) {
    (void)xTask; // Unused in mock
    return 1000; // Mock stack high water mark
}

void vTaskSuspend(TaskHandle_t xTaskToSuspend) {
    (void)xTaskToSuspend; // Mock implementation
    printf("Mock FreeRTOS: Task suspended\n");
}

void vTaskResume(TaskHandle_t xTaskToResume) {
    (void)xTaskToResume; // Mock implementation
    printf("Mock FreeRTOS: Task resumed\n");
}

void vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority) {
    (void)xTask; // Unused in mock
    (void)uxNewPriority; // Unused in mock
    printf("Mock FreeRTOS: Task priority set\n");
}

TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return (TaskHandle_t)&mock_tasks[0]; // Mock current task
}

void vTaskStartScheduler(void) {
    printf("Mock FreeRTOS: Scheduler started\n");
    printf("Mock FreeRTOS: Running %d tasks...\n", task_count);

    // Simulate running tasks briefly
    for (int i = 0; i < task_count; i++) {
        if (mock_tasks[i].function) {
            printf("Mock FreeRTOS: Executing task '%s'\n", mock_tasks[i].name);
            // Don't actually call the function to avoid infinite loops
        }
    }

    printf("Mock FreeRTOS: Scheduler simulation complete\n");
}

// ============================================================================
// QUEUE FUNCTIONS
// ============================================================================

QueueHandle_t xQueueCreate(UBaseType_t uxQueueLength, UBaseType_t uxItemSize) {
    (void)uxItemSize; // Unused in mock

    if (queue_count >= 10) return NULL;

    MockQueue_t *queue = &mock_queues[queue_count++];
    snprintf(queue->name, sizeof(queue->name), "Queue%d", queue_count);
    queue->max_items = uxQueueLength;
    queue->item_count = 0;

    printf("Mock FreeRTOS: Created queue with length %lu\n", uxQueueLength);
    return (QueueHandle_t)queue;
}

BaseType_t xQueueSend(QueueHandle_t xQueue, const void * pvItemToQueue, TickType_t xTicksToWait) {
    (void)pvItemToQueue; // Unused in mock
    (void)xTicksToWait;  // Unused in mock

    MockQueue_t *queue = (MockQueue_t *)xQueue;
    if (!queue || queue->item_count >= queue->max_items) {
        return pdFAIL;
    }

    queue->item_count++;
    printf("Mock FreeRTOS: Item sent to queue '%s' (%d/%d)\n",
           queue->name, queue->item_count, queue->max_items);
    return pdPASS;
}

BaseType_t xQueueReceive(QueueHandle_t xQueue, void *pvBuffer, TickType_t xTicksToWait) {
    (void)pvBuffer;     // Unused in mock
    (void)xTicksToWait; // Unused in mock

    MockQueue_t *queue = (MockQueue_t *)xQueue;
    if (!queue || queue->item_count == 0) {
        return pdFAIL;
    }

    queue->item_count--;
    printf("Mock FreeRTOS: Item received from queue '%s' (%d/%d)\n",
           queue->name, queue->item_count, queue->max_items);
    return pdPASS;
}

UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue) {
    MockQueue_t *queue = (MockQueue_t *)xQueue;
    return queue ? queue->item_count : 0;
}

// ============================================================================
// SEMAPHORE FUNCTIONS
// ============================================================================

SemaphoreHandle_t xQueueCreateMutex(void) {
    if (semaphore_count >= 10) return NULL;

    MockSemaphore_t *sem = &mock_semaphores[semaphore_count++];
    snprintf(sem->name, sizeof(sem->name), "Mutex%d", semaphore_count);
    sem->count = 1; // Binary semaphore

    printf("Mock FreeRTOS: Created mutex '%s'\n", sem->name);
    return (SemaphoreHandle_t)sem;
}

BaseType_t xQueueSemaphoreTake(SemaphoreHandle_t xSemaphore, TickType_t xTicksToWait) {
    (void)xTicksToWait; // Unused in mock

    MockSemaphore_t *sem = (MockSemaphore_t *)xSemaphore;
    if (!sem || sem->count == 0) {
        return pdFAIL;
    }

    sem->count--;
    printf("Mock FreeRTOS: Semaphore '%s' taken\n", sem->name);
    return pdPASS;
}

BaseType_t xQueueGive(SemaphoreHandle_t xSemaphore) {
    MockSemaphore_t *sem = (MockSemaphore_t *)xSemaphore;
    if (!sem) return pdFAIL;

    sem->count++;
    printf("Mock FreeRTOS: Semaphore '%s' given\n", sem->name);
    return pdPASS;
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// pdMS_TO_TICKS is defined as a macro in FreeRTOS.h