#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <avr/io.h>

/* Mapeo de compatibilidad de registros Timer 1 (ATmega323 -> ATmega328P) */
#ifndef TIMSK
#define TIMSK                                   TIMSK1
#endif
#ifndef TIFR
#define TIFR                                    TIFR1
#endif

/* =========================================================================
 * CONFIGURACION DEL PLANIFICADOR (Parte II - Practica 3)
 * - configUSE_PREEMPTION debe estar siempre habilitado (1).
 * - configUSE_TIME_SLICING:
 *      1 -> Reparto equitativo por Round Robin (Ejercicio 4)
 *      0 -> Planificacion FIFO entre tareas de igual prioridad (Ejercicio 5)
 * ========================================================================= */
#define configUSE_PREEMPTION                    1

#ifndef configUSE_TIME_SLICING
#define configUSE_TIME_SLICING                  1
#endif

/* =========================================================================
 * ESCALA DE TIEMPOS
 * - Reloj CPU: 16 MHz
 * - 1 tick de FreeRTOS = 15 ms exactos (Timer1 prescaler 64 -> OCR1A = 3749)
 * - 1 unidad de tiempo (Parte I) = 4 ticks = 60 ms
 * ========================================================================= */
#define configCPU_CLOCK_HZ                      ( ( unsigned long ) 16000000UL )
#define configTICK_PERIOD_MS_EXACT              15
#define configTICK_RATE_HZ                      ( ( TickType_t ) 67 )

#define pdMS_TO_TICKS( xTimeInMs )              ( ( TickType_t ) ( ( ( uint32_t ) ( xTimeInMs ) ) / 15UL ) )
#define pdTICKS_TO_MS( xTimeInTicks )           ( ( TickType_t ) ( ( ( uint32_t ) ( xTimeInTicks ) ) * 15UL ) )

#define configUSE_IDLE_HOOK                     0
#define configUSE_TICK_HOOK                     0
#define configMAX_PRIORITIES                    4
#define configMINIMAL_STACK_SIZE                ( ( unsigned short ) 75 )
#define configTOTAL_HEAP_SIZE                   ( ( size_t ) 1400 )
#define configMAX_TASK_NAME_LEN                 4
#define configUSE_16_BIT_TICKS                  1
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_MUTEXES                       0
#define configUSE_TASK_NOTIFICATIONS            0

#define INCLUDE_vTaskPrioritySet                0
#define INCLUDE_uxTaskPriorityGet               0
#define INCLUDE_vTaskDelete                     0
#define INCLUDE_vTaskSuspend                    0
#define INCLUDE_xTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1

/* Hook de cambio de contexto para que PB0..PB3 reflejen en el Logic Analyser
 * unicamente el intervalo en que cada tarea esta consumiendo efectivamente CPU */
#ifndef __ASSEMBLER__
extern void vActualizarPinTareaActiva( void );
#endif
#define traceTASK_SWITCHED_IN()                 vActualizarPinTareaActiva()

#endif /* FREERTOS_CONFIG_H */
