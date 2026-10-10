#define F_CPU 16000000UL

#include <avr/io.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// Mutex para sincronizar el acceso al puerto serie y evitar condiciones de carrera
static SemaphoreHandle_t xUsartMutex = NULL;

// Configuracion del USART a 9600 baudios para 16 MHz (UBRR = 103, U2X0 = 0)
void usart_init(void)
{
	// Asegura U2X0 = 0 (velocidad normal, 16x oversampling) y limpia flags de error
	UCSR0A = 0;

	// UBRR para 9600 baudios a 16 MHz con U2X0 = 0 es 103
	UBRR0H = (uint8_t)(103 >> 8);
	UBRR0L = (uint8_t)103;

	// Habilita transmision (TX) y recepcion (RX)
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);

	// Formato de trama: 8 bits de datos, 1 bit de parada, sin paridad
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

// Transmision de cadenas de texto:
// - Protegida por Mutex para evitar intercalacion de caracteres entre tareas
// - Mantiene las interrupciones globales activas durante el sondeo de UDRE0
void usart_print(const char* str)
{
	if (xUsartMutex != NULL)
	{
		if (xSemaphoreTake(xUsartMutex, portMAX_DELAY) == pdTRUE)
		{
			while (*str)
			{
				// Espera que el buffer de transmision este libre (sin deshabilitar interrupciones)
				while (!(UCSR0A & (1 << UDRE0)));
				UDR0 = *str++;
			}
			xSemaphoreGive(xUsartMutex);
		}
	}
	else
	{
		while (*str)
		{
			while (!(UCSR0A & (1 << UDRE0)));
			UDR0 = *str++;
		}
	}
}

// Tarea generica: imprime su mensaje y se bloquea durante el tiempo establecido
void vTareaImprimir(void *pvParameters)
{
	const char *mensaje = (const char *)pvParameters;

	while (1)
	{
		usart_print(mensaje);

		// Retardo de 100 ms usando ticks del planificador
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

int main(void)
{
	// Inicializacion del USART a 9600 baudios
	usart_init();

	// Creacion del mutex para proteger el recurso USART
	xUsartMutex = xSemaphoreCreateMutex();

	// Creacion de las 3 tareas con prioridad 1:
	// Stack de 100 bytes (StackType_t = uint8_t): suficiente para el contexto de 35 bytes
	// (32 registros + SREG + PC) mas las llamadas de funcion, sin desbordar el heap.
	xTaskCreate(vTareaImprimir, "T1", 100, (void *)"Tarea 1\r\n", 3, NULL);
	xTaskCreate(vTareaImprimir, "T2", 100, (void *)"Tarea 2\r\n", 2, NULL);
	xTaskCreate(vTareaImprimir, "T3", 100, (void *)"Tarea 3\r\n", 1, NULL);

	// Inicia el planificador preemptivo de FreeRTOS
	vTaskStartScheduler();

	// Si llega a este punto es porque hubo insuficiencia de memoria heap al crear la tarea Idle
	while (1);

	return 0;
}