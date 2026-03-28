#ifndef TASK_H
#define TASK_H

/* FreeRTOS Task API Mock Header */

#include "FreeRTOS.h"

typedef struct {
    TickType_t notificationValue;
} StaticTask_t;

/* Task function signature */
typedef void (*TaskFunction_t)(void *);

/* Task priority type */
typedef int UBaseType_t;

/* Task creation */
BaseType_t xTaskCreate(
    TaskFunction_t pxTaskCode,
    const char * const pcName,
    uint16_t usStackDepth,
    void *pvParameters,
    UBaseType_t uxPriority,
    TaskHandle_t *pxCreatedTask
);

/* Task deletion */
void vTaskDelete(TaskHandle_t xTaskToDelete);

/* Task delay */
void vTaskDelay(TickType_t xTicksToDelay);

/* Get tick count */
TickType_t xTaskGetTickCount(void);

/* Suspend task */
void vTaskSuspend(TaskHandle_t xTaskToSuspend);

/* Resume task */
void vTaskResume(TaskHandle_t xTaskToResume);

/* Priority set */
void vTaskPrioritySet(TaskHandle_t xTask, UBaseType_t uxNewPriority);

/* Get current task */
TaskHandle_t xTaskGetCurrentTaskHandle(void);

#endif /* TASK_H */
