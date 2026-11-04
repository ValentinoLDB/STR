#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/atomic.h>
#include <util/delay_basic.h>
#include "FreeRTOS.h"
#include "task.h"

/* =========================================================================
 * SELECTORES DE CONFIGURACION DEL BANCO DE PRUEBAS
 * - MODO_CALIBRACION_EJ3:
 *      1 -> Activa unicamente la Tarea D (C=1u=60ms, T=3u=180ms) [Ejercicio 3b]
 *      0 -> Activa las 4 tareas periodicas (A, B, C, D)          [Ejercicios 4 y 5]
 * - CONJUNTO_TAREAS:
 *      1 -> Conjunto de tareas del Ejercicio 1 (por defecto en Ej. 4 y 5)
 *      2 -> Conjunto de tareas del Ejercicio 2
 * (La politica Round Robin vs FIFO se selecciona en FreeRTOSConfig.h mediante
 *  configUSE_TIME_SLICING = 1 o 0).
 * ========================================================================= */
#ifndef MODO_CALIBRACION_EJ3
#define MODO_CALIBRACION_EJ3            0
#endif

#ifndef CONJUNTO_TAREAS
#define CONJUNTO_TAREAS                 1
#endif

/* Escala de tiempos: 1 tick = 15 ms | 1 unidad = 4 ticks = 60 ms */
#define MS_POR_TICK                     15UL
#define US_POR_TICK                     15000UL
#define TICKS_POR_UNIDAD                4U
#define MS_POR_UNIDAD                   60U

/* Calibracion del lazo de espera activa a 16 MHz:
 * _delay_loop_2(N) consume exactamente 4 * N ciclos de reloj de CPU.
 * Para 1 ms puro = 16000 ciclos -> N = 4000. Se ajusta a 3990 (59.85 ms en 60 iteraciones)
 * para compensar el pequeno overhead de las interrupciones de tick (~0.10 ms en 4 ticks)
 * de modo que el pulso real coincida en ~60.0 ms sin falsos desbordes de tick. */
#define ITERACIONES_LOOP2_POR_MS        3990U

/* Prioridades de FreeRTOS */
#define PRIORIDAD_TAREAS_PERIODICAS     ( tskIDLE_PRIORITY + 2U )
#define PRIORIDAD_TAREA_REPORTE         ( tskIDLE_PRIORITY + 1U )

/* Tamanos de stack (en bytes en AVR; max 255 porque StackType_t = uint8_t) */
#define STACK_TAREA_PERIODICA           150U
#define STACK_TAREA_REPORTE             180U

/* Mascaras de pines del Puerto B (PB0=A, PB1=B, PB2=C, PB3=D) */
#define MASCARA_PINES_TAREAS            0x0FU

typedef struct
{
    uint8_t  ucIndice;          /* 0=A, 1=B, 2=C, 3=D */
    char     cNombre;           /* 'A', 'B', 'C', 'D' */
    uint8_t  ucPinMask;         /* (1 << PBx) */
    uint8_t  ucComputoUnidades; /* Ci en unidades (1 unidad = 60 ms = 4 ticks) */
    uint8_t  ucPeriodoUnidades; /* Ti = Di en unidades */
} ConfigTarea_t;

typedef struct
{
    uint16_t usActivaciones;
    uint16_t usPlazosPerdidos;
    uint16_t usRespuestaMaxMs;
    uint16_t usJitterMaxMs;
} MetricasTarea_t;

#if ( CONJUNTO_TAREAS == 1 )
/* Conjunto del Ejercicio 1: H = 24 unidades = 96 ticks = 1440 ms */
#define HIPERPERIODO_UNIDADES           24U
static const ConfigTarea_t xConfigTareas[4] = {
    { 0U, 'A', (1U << PB0), 2U, 12U },
    { 1U, 'B', (1U << PB1), 2U, 24U },
    { 2U, 'C', (1U << PB2), 2U,  6U },
    { 3U, 'D', (1U << PB3), 1U,  3U }
};
#else
/* Conjunto del Ejercicio 2: H = 60 unidades = 240 ticks = 3600 ms */
#define HIPERPERIODO_UNIDADES           60U
static const ConfigTarea_t xConfigTareas[4] = {
    { 0U, 'A', (1U << PB0), 1U, 10U },
    { 1U, 'B', (1U << PB1), 3U, 12U },
    { 2U, 'C', (1U << PB2), 7U, 20U },
    { 3U, 'D', (1U << PB3), 1U,  5U }
};
#endif

/* Puntero interno de FreeRTOS al TCB en ejecucion actual */
extern volatile void * volatile pxCurrentTCB;

static TaskHandle_t             xHandlesTareas[4]    = { NULL, NULL, NULL, NULL };
static volatile uint8_t         ucMascaraEnRafaga[4] = { 0U, 0U, 0U, 0U };
static volatile MetricasTarea_t xMetricas[4];

/* -------------------------------------------------------------------------
 * Hook invocado automaticamente por FreeRTOS en cada cambio de contexto
 * (dentro de vTaskSwitchContext y vTaskStartScheduler, con IRQ deshabilitadas).
 * Garantiza que el pin PBx este en nivel ALTO unicamente mientras la tarea
 * esta consumiendo CPU efectivamente, bajandolo si es expropiada.
 * ------------------------------------------------------------------------- */
void vActualizarPinTareaActiva( void )
{
    uint8_t ucMask = 0U;
    const void *pvActual = (const void *)pxCurrentTCB;

    if      ( pvActual == (const void *)xHandlesTareas[0] ) { ucMask = ucMascaraEnRafaga[0]; }
    else if ( pvActual == (const void *)xHandlesTareas[1] ) { ucMask = ucMascaraEnRafaga[1]; }
    else if ( pvActual == (const void *)xHandlesTareas[2] ) { ucMask = ucMascaraEnRafaga[2]; }
    else if ( pvActual == (const void *)xHandlesTareas[3] ) { ucMask = ucMascaraEnRafaga[3]; }

    PORTB = ( uint8_t )( ( PORTB & ( uint8_t )( ~MASCARA_PINES_TAREAS ) ) | ucMask );
}

/* -------------------------------------------------------------------------
 * Mide con resolucion de 4 us el tiempo transcurrido (en microsegundos)
 * desde el tick de activacion teorica xTickActivacion hasta el instante actual,
 * combinando xTickCount y el contador hardware TCNT1 del Timer 1.
 * ------------------------------------------------------------------------- */
static uint32_t ulTiempoDesdeActivacionUs( TickType_t xTickActivacion )
{
    TickType_t xTicksActuales;
    uint16_t   usTimer1;
    uint8_t    ucFlags;

    ATOMIC_BLOCK( ATOMIC_RESTORESTATE )
    {
        xTicksActuales = xTaskGetTickCountFromISR();
        usTimer1       = TCNT1;
        ucFlags        = TIFR1;

        /* Si ocurrio el compare match de Timer 1 justo con CLI activo */
        if ( ( ucFlags & ( 1U << OCF1A ) ) && ( usTimer1 < 1875U ) )
        {
            xTicksActuales++;
        }
    }

    uint16_t usDeltaTicks = ( uint16_t )( xTicksActuales - xTickActivacion );
    return ( ( uint32_t )usDeltaTicks * US_POR_TICK ) + ( ( uint32_t )usTimer1 * 4UL );
}

/* -------------------------------------------------------------------------
 * Lazo de espera activa determinista.
 * Consume exactamente ucUnidades * 60 ms de tiempo efectivo de CPU.
 * Si la tarea es expropiada, sus registros de conteo se preservan en su stack
 * y reanuda exactamente donde quedo sin perder el computo ya realizado.
 * ------------------------------------------------------------------------- */
static void vConsumirCPU_Unidades( uint8_t ucUnidades )
{
    uint16_t usTotalMs = ( uint16_t )ucUnidades * ( uint16_t )MS_POR_UNIDAD;
    for ( uint16_t ms = 0U; ms < usTotalMs; ms++ )
    {
        _delay_loop_2( ITERACIONES_LOOP2_POR_MS );
    }
}

/* -------------------------------------------------------------------------
 * Inicializacion y utilidades livianas de USART a 9600 bps (8N1 @ 16 MHz)
 * ------------------------------------------------------------------------- */
static void vUsartInit( void )
{
    UCSR0A = 0U;
    UBRR0H = ( uint8_t )( 103U >> 8 );
    UBRR0L = ( uint8_t )103U;
    UCSR0B = ( 1U << TXEN0 );
    UCSR0C = ( 1U << UCSZ01 ) | ( 1U << UCSZ00 );
}

static void vUsartPutChar( char c )
{
    while ( !( UCSR0A & ( 1U << UDRE0 ) ) )
    {
        /* Espera activa con interrupciones habilitadas: puede ser expropiada
         * inmediatamente por cualquier tarea periodica de mayor prioridad */
    }
    UDR0 = ( uint8_t )c;
}

static void vUsartPrintStr( const char *pcStr )
{
    while ( *pcStr )
    {
        vUsartPutChar( *pcStr++ );
    }
}

static void vUsartPrintU16( uint16_t usVal, uint8_t ucAnchoMin )
{
    char cBuf[6];
    uint8_t ucPos = 0U;

    if ( usVal == 0U )
    {
        cBuf[ucPos++] = '0';
    }
    else
    {
        while ( usVal > 0U )
        {
            cBuf[ucPos++] = ( char )( '0' + ( usVal % 10U ) );
            usVal /= 10U;
        }
    }

    while ( ucPos < ucAnchoMin )
    {
        vUsartPutChar( ' ' );
        ucAnchoMin--;
    }

    while ( ucPos > 0U )
    {
        vUsartPutChar( cBuf[--ucPos] );
    }
}

/* -------------------------------------------------------------------------
 * Cuerpo comun de las tareas periodicas A, B, C y D
 * ------------------------------------------------------------------------- */
static void vTareaPeriodica( void *pvParameters )
{
    const ConfigTarea_t *pxCfg = ( const ConfigTarea_t * )pvParameters;
    const uint8_t  ucId           = pxCfg->ucIndice;
    const uint8_t  ucPinMask      = pxCfg->ucPinMask;
    const TickType_t xPeriodoTicks = ( TickType_t )( ( TickType_t )pxCfg->ucPeriodoUnidades * TICKS_POR_UNIDAD );
    const uint32_t ulPlazoUs      = ( uint32_t )xPeriodoTicks * US_POR_TICK;

    /* Todas las tareas parten sincronas desde el instante inicial t = 0 */
    TickType_t xTickActivacionTeorica = ( TickType_t )0U;

    for ( ;; )
    {
        /* 1. Medir jitter de activacion (desvio entre el inicio real y k * Ti)
         *    y elevar el pin PBx correspondiente al iniciar el consumo de CPU */
        uint32_t ulJitterUs = ulTiempoDesdeActivacionUs( xTickActivacionTeorica );

        ATOMIC_BLOCK( ATOMIC_RESTORESTATE )
        {
            ucMascaraEnRafaga[ucId] = ucPinMask;
            PORTB = ( uint8_t )( ( PORTB & ( uint8_t )( ~MASCARA_PINES_TAREAS ) ) | ucPinMask );
        }

        /* 2. Consumir CPU mediante lazo de espera activa */
        vConsumirCPU_Unidades( pxCfg->ucComputoUnidades );

        /* 3. Bajar el pin PBx al finalizar la rafaga y tomar marca temporal final */
        ATOMIC_BLOCK( ATOMIC_RESTORESTATE )
        {
            ucMascaraEnRafaga[ucId] = 0U;
            PORTB &= ( uint8_t )( ~ucPinMask );
        }

        uint32_t ulRespuestaUs = ulTiempoDesdeActivacionUs( xTickActivacionTeorica );
        uint16_t usJitterMs    = ( uint16_t )( ( ulJitterUs + 500UL ) / 1000UL );
        uint16_t usRespuestaMs = ( uint16_t )( ( ulRespuestaUs + 500UL ) / 1000UL );

        /* 4. Actualizar estadisticas de forma atomica */
        ATOMIC_BLOCK( ATOMIC_RESTORESTATE )
        {
            xMetricas[ucId].usActivaciones++;

            if ( usJitterMs > xMetricas[ucId].usJitterMaxMs )
            {
                xMetricas[ucId].usJitterMaxMs = usJitterMs;
            }
            if ( usRespuestaMs > xMetricas[ucId].usRespuestaMaxMs )
            {
                xMetricas[ucId].usRespuestaMaxMs = usRespuestaMs;
            }
            /* Margen de 1 ms (1000 us) para no computar el overhead de ISR como plazo perdido */
            if ( ulRespuestaUs > ( ulPlazoUs + 1000UL ) )
            {
                xMetricas[ucId].usPlazosPerdidos++;
            }
        }

        /* 5. Bloquear hasta la proxima activacion periodica teorica (k + 1) * Ti */
        ( void )xTaskDelayUntil( &xTickActivacionTeorica, xPeriodoTicks );
    }
}

/* -------------------------------------------------------------------------
 * Tarea de menor prioridad para reporte periodico por Terminal Serie
 * Reporta cada 2 hiperperiodos aprovechando los intervalos de CPU ociosa.
 * ------------------------------------------------------------------------- */
static void vTareaReporte( void *pvParameters )
{
    ( void )pvParameters;

    const TickType_t xTicksHiperperiodo = ( TickType_t )( ( TickType_t )HIPERPERIODO_UNIDADES * TICKS_POR_UNIDAD );
    const TickType_t xPeriodoReporte    = ( TickType_t )( 2U * xTicksHiperperiodo );
    TickType_t xUltimoDespertar         = ( TickType_t )0U;
    uint16_t   usNumHiperperiodos       = 0U;

    #if ( MODO_CALIBRACION_EJ3 == 1 )
    vUsartPrintStr( "\r\n=== Ej.3b Calibracion (Solo D) ===\r\n" );
    #elif ( configUSE_TIME_SLICING == 1 )
    vUsartPrintStr( "\r\n=== Ej.4 Round Robin (q=15ms) ===\r\n" );
    #else
    vUsartPrintStr( "\r\n=== Ej.5 FIFO (sin time slicing) ===\r\n" );
    #endif

    /* En el conjunto completo, el tramo final del 2do hiperperiodo [2760ms..2880ms]
     * (ticks 184..192) tiene la CPU 100% ociosa con todas las rafagas de 2H ya concluidas.
     * Despertar alli permite emitir el corte exacto de 2H, 4H, 6H... sin interferencias. */
    #if ( MODO_CALIBRACION_EJ3 == 0 ) && ( CONJUNTO_TAREAS == 1 )
    ( void )xTaskDelayUntil( &xUltimoDespertar, ( TickType_t )( xPeriodoReporte - 8U ) );
    #else
    ( void )xTaskDelayUntil( &xUltimoDespertar, ( TickType_t )( xPeriodoReporte - 4U ) );
    #endif

    for ( ;; )
    {
        MetricasTarea_t xCopia[4];
        usNumHiperperiodos = ( uint16_t )( usNumHiperperiodos + 2U );

        ATOMIC_BLOCK( ATOMIC_RESTORESTATE )
        {
            for ( uint8_t i = 0U; i < 4U; i++ )
            {
                xCopia[i] = xMetricas[i];
            }
        }

        vUsartPrintStr( "\r\n--- Reporte [" );
        vUsartPrintU16( usNumHiperperiodos, 1U );
        vUsartPrintStr( "H] ---\r\n T| Act|Perd|Rmax|Jmax (ms)\r\n" );

        for ( uint8_t i = 0U; i < 4U; i++ )
        {
            #if ( MODO_CALIBRACION_EJ3 == 1 )
            if ( i != 3U ) { continue; }
            #endif

            vUsartPutChar( ' ' );
            vUsartPutChar( xConfigTareas[i].cNombre );
            vUsartPutChar( '|' );
            vUsartPrintU16( xCopia[i].usActivaciones, 4U );
            vUsartPutChar( '|' );
            vUsartPrintU16( xCopia[i].usPlazosPerdidos, 4U );
            vUsartPutChar( '|' );
            vUsartPrintU16( xCopia[i].usRespuestaMaxMs, 4U );
            vUsartPutChar( '|' );
            vUsartPrintU16( xCopia[i].usJitterMaxMs, 4U );
            vUsartPrintStr( "\r\n" );
        }

        ( void )xTaskDelayUntil( &xUltimoDespertar, xPeriodoReporte );
    }
}

int main( void )
{
    /* Configurar PB0..PB3 como salidas inicialmente en nivel BAJO */
    DDRB  |= MASCARA_PINES_TAREAS;
    PORTB &= ( uint8_t )( ~MASCARA_PINES_TAREAS );

    /* Inicializar puerto serie a 9600 baudios */
    vUsartInit();

    #if ( MODO_CALIBRACION_EJ3 == 1 )
    /* Ejercicio 3(b): Unica tarea activa = Tarea D (PB3, C=1u, T=3u) */
    xTaskCreate( vTareaPeriodica, "D", STACK_TAREA_PERIODICA, ( void * )&xConfigTareas[3], PRIORIDAD_TAREAS_PERIODICAS, &xHandlesTareas[3] );
    #else
    /* Ejercicios 4 y 5: Creacion de las 4 tareas periodicas con IGUAL prioridad */
    xTaskCreate( vTareaPeriodica, "A", STACK_TAREA_PERIODICA, ( void * )&xConfigTareas[0], PRIORIDAD_TAREAS_PERIODICAS, &xHandlesTareas[0] );
    xTaskCreate( vTareaPeriodica, "B", STACK_TAREA_PERIODICA, ( void * )&xConfigTareas[1], PRIORIDAD_TAREAS_PERIODICAS, &xHandlesTareas[1] );
    xTaskCreate( vTareaPeriodica, "C", STACK_TAREA_PERIODICA, ( void * )&xConfigTareas[2], PRIORIDAD_TAREAS_PERIODICAS, &xHandlesTareas[2] );
    xTaskCreate( vTareaPeriodica, "D", STACK_TAREA_PERIODICA, ( void * )&xConfigTareas[3], PRIORIDAD_TAREAS_PERIODICAS, &xHandlesTareas[3] );
    #endif

    /* Tarea de menor prioridad para reporte por Virtual Terminal */
    xTaskCreate( vTareaReporte, "REP", STACK_TAREA_REPORTE, NULL, PRIORIDAD_TAREA_REPORTE, NULL );

    /* Iniciar planificador expropiativo de FreeRTOS */
    vTaskStartScheduler();

    while ( 1 )
    {
        /* Solo se alcanza si configTOTAL_HEAP_SIZE fuera insuficiente */
    }

    return 0;
}
