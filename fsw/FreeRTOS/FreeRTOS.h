#ifndef FREERTOS_H
#define FREERTOS_H

/* FreeRTOS Mock Header - Temporal compile fix */

/* Task states */
#define tskRUNNING_CHAR  ( 'X' )
#define tskBLOCKED_CHAR  ( 'B' )
#define tskREADY_CHAR    ( 'R' )
#define tskDELETED_CHAR  ( 'D' )
#define tskSUSPENDED_CHAR ( 'S' )

/* Tick period */
#define portTICK_PERIOD_MS ( ( TickType_t ) 1 )

/* Task handle */
typedef void* TaskHandle_t;

/* Tick type */
typedef unsigned long TickType_t;

/* Base type */
typedef long BaseType_t;
#define pdTRUE  ( ( BaseType_t ) 1 )
#define pdFALSE ( ( BaseType_t ) 0 )

/* Queue handle */
typedef void* QueueHandle_t;

/* Error codes */
#define pdPASS   ( BaseType_t ) 1
#define pdFAIL   ( BaseType_t ) 0

#if !defined(configASSERT)
    #define configASSERT(x) if(!(x)){while(1);}
#endif

/* Macro utility */
#define pdMS_TO_TICKS(ms) ((ms) / portTICK_PERIOD_MS)

#endif /* FREERTOS_H */
