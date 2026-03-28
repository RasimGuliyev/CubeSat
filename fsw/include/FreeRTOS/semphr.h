#ifndef SEMPHR_H
#define SEMPHR_H

#include "FreeRTOS.h"
#include "queue.h"

// Semaphore handle is just a queue handle in FreeRTOS
typedef QueueHandle_t SemaphoreHandle_t;

// Semaphore creation functions
#define xSemaphoreCreateMutex() xQueueCreateMutex()
#define xSemaphoreTake(sem, timeout) xQueueSemaphoreTake(sem, timeout)
#define xSemaphoreGive(sem) xQueueSendToBack(sem, NULL, 0)

#endif // SEMPHR_H