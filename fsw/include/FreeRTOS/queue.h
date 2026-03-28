#ifndef QUEUE_H
#define QUEUE_H

/* FreeRTOS Queue API Mock Header */

#include "FreeRTOS.h"

typedef void* QueueHandle_t;

/* Static queue type for CSP compatibility */
typedef struct {
    uint8_t dummy;
} StaticQueue_t;

#define queueQUEUE_TYPE_BASE  (0)
#define queueQUEUE_TYPE_MUTEX  (1)

/* Queue creation */
QueueHandle_t xQueueCreate(
    UBaseType_t uxQueueLength,
    UBaseType_t uxItemSize
);

/* Send to queue */
BaseType_t xQueueSend(
    QueueHandle_t xQueue,
    const void *pvItemToQueue,
    TickType_t xTicksToWait
);

/* Receive from queue */
BaseType_t xQueueReceive(
    QueueHandle_t xQueue,
    void *pvBuffer,
    TickType_t xTicksToWait
);

/* Queue length */
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t xQueue);

/* Delete queue */
void vQueueDelete(QueueHandle_t xQueue);

/* Send from ISR */
BaseType_t xQueueSendFromISR(
    QueueHandle_t xQueue,
    const void *pvItemToQueue,
    BaseType_t *pxHigherPriorityTaskWoken
);

#define xQueueSendToBack xQueueSend
#define xQueueSendToFront(queue, buf, timeout) xQueueSend(queue, buf, timeout)

/* Peek queue */
BaseType_t xQueuePeek(
    QueueHandle_t xQueue,
    void *pvBuffer,
    TickType_t xTicksToWait
);

/* Mutex queue creation */
QueueHandle_t xQueueCreateMutex(void);

/* Semaphore take */
BaseType_t xQueueSemaphoreTake(
    QueueHandle_t xQueue,
    TickType_t xTicksToWait
);

/* Peek queue */
BaseType_t xQueuePeek(
    QueueHandle_t xQueue,
    void *pvBuffer,
    TickType_t xTicksToWait
);

#endif /* QUEUE_H */
