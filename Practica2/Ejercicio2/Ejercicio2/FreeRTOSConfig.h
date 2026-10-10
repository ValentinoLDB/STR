#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <avr/io.h>

// Mapeo de compatibilidad de registros Timer 1 (ATmega323 -> ATmega328P)
#ifndef TIMSK
#define TIMSK                                   TIMSK1
#endif
#ifndef TIFR
#define TIFR                                    TIFR1
#endif

#define configUSE_PREEMPTION                    1
#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 16000000UL )
#define configTICK_RATE_HZ                      ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES                    4
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 70 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) 1100 )
#define configMAX_TASK_NAME_LEN                 8
#define configUSE_16_BIT_TICKS                  1
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       1

#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1

#endif /* FREERTOS_CONFIG_H */