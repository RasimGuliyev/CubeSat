#ifndef SEMPHR_H
#define SEMPHR_H

#include "FreeRTOS.h"
#include "queue.h"

// Semaphore handle is just a queue handle in FreeRTOS
typedef QueueHandle_t SemaphoreHandle_t;

// Semaphore creation functions
#define xSemaphoreCreateMutex() xQueueCreateMutex()
#define xSemaphoreTake(sem, timeout) xQueueSemaphoreTake(sem, timeout)
#define xSemaphoreGive(sem) xQueueGenericSend(sem, NULL, 0, queueSEND_TO_BACK)

#endif // SEMPHR_H