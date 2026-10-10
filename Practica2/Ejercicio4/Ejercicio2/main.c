#include <avr/io.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

// ==========================================================
// SELECCIÓN DE SECUENCIA:
// 1 -> Secuencia A: Tarea 1 -> Tarea 3 -> Tarea 2
// 2 -> Secuencia B: Tarea 2 -> Tarea 2 -> Tarea 3 -> Tarea 1
// 3 -> Secuencia C: Tarea 3 -> Tarea 3 -> Tarea 3 -> Tarea 1 -> Tarea 2
// ==========================================================
#define SECUENCIA 1

SemaphoreHandle_t xSem1;
SemaphoreHandle_t xSem2;
SemaphoreHandle_t xSem3;

// Inicialización USART a 9600 baudios (16 MHz)
void usart_init(void)
{
	UCSR0A = 0;
	UBRR0H = 0;
	UBRR0L = 103;
	UCSR0B = (1 << RXEN0) | (1 << TXEN0);
	UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

void usart_print(const char *str)
{
	while (*str)
	{
		while (!(UCSR0A & (1 << UDRE0)));
		UDR0 = *str++;
	}
}

// ----------------------------------------------------------
// IMPLEMENTACIÓN DE TAREAS
// ----------------------------------------------------------

void vTarea1(void *pvParameters)
{
	for (;;)
	{
		if (xSemaphoreTake(xSem1, portMAX_DELAY) == pdTRUE)
		{
			usart_print("Tarea 1\r\n");

			#if (SECUENCIA == 1)
			// A: 1 -> 3 -> 2
			xSemaphoreGive(xSem3);

			#elif (SECUENCIA == 2)
			// B: 2 -> 2 -> 3 -> 1 -> (repite 2)
			xSemaphoreGive(xSem2);

			#elif (SECUENCIA == 3)
			// C: 3 -> 3 -> 3 -> 1 -> 2 -> (repite 3)
			xSemaphoreGive(xSem2);
			#endif
		}
	}
}

void vTarea2(void *pvParameters)
{
	#if (SECUENCIA == 2)
	uint8_t cuenta2 = 0;
	#endif

	for (;;)
	{
		if (xSemaphoreTake(xSem2, portMAX_DELAY) == pdTRUE)
		{
			usart_print("Tarea 2\r\n");

			#if (SECUENCIA == 1)
			// A: 1 -> 3 -> 2 -> (repite 1)
			xSemaphoreGive(xSem1);

			#elif (SECUENCIA == 2)
			// B: Imprime 2 veces antes de pasar a la 3
			cuenta2++;
			if (cuenta2 < 2)
			{
				xSemaphoreGive(xSem2); // Vuelve a auto-habilitarse
			}
			else
			{
				cuenta2 = 0;
				xSemaphoreGive(xSem3); // Pasa a la Tarea 3
			}

			#elif (SECUENCIA == 3)
			// C: 3 -> 3 -> 3 -> 1 -> 2 -> (repite 3)
			xSemaphoreGive(xSem3);
			#endif
		}
	}
}

void vTarea3(void *pvParameters)
{
	#if (SECUENCIA == 3)
	uint8_t cuenta3 = 0;
	#endif

	for (;;)
	{
		if (xSemaphoreTake(xSem3, portMAX_DELAY) == pdTRUE)
		{
			usart_print("Tarea 3\r\n");

			#if (SECUENCIA == 1)
			// A: 1 -> 3 -> 2
			xSemaphoreGive(xSem2);

			#elif (SECUENCIA == 2)
			// B: 2 -> 2 -> 3 -> 1
			xSemaphoreGive(xSem1);

			#elif (SECUENCIA == 3)
			// C: Imprime 3 veces antes de pasar a la 1
			cuenta3++;
			if (cuenta3 < 3)
			{
				xSemaphoreGive(xSem3); // Vuelve a auto-habilitarse
			}
			else
			{
				cuenta3 = 0;
				xSemaphoreGive(xSem1); // Pasa a la Tarea 1
			}
			#endif
		}
	}
}

// ----------------------------------------------------------
// MAIN
// ----------------------------------------------------------
int main(void)
{
	usart_init();

	xSem1 = xSemaphoreCreateBinary();
	xSem2 = xSemaphoreCreateBinary();
	xSem3 = xSemaphoreCreateBinary();

	if (xSem1 != NULL && xSem2 != NULL && xSem3 != NULL)
	{
		// Disparo del semáforo inicial según la secuencia elegida
		#if (SECUENCIA == 1)
		xSemaphoreGive(xSem1); // Arranca Tarea 1
		#elif (SECUENCIA == 2)
		xSemaphoreGive(xSem2); // Arranca Tarea 2
		#elif (SECUENCIA == 3)
		xSemaphoreGive(xSem3); // Arranca Tarea 3
		#endif

		// Todas las tareas tienen idéntica prioridad (1)
		xTaskCreate(vTarea1, "T1", 85, NULL, 1, NULL);
		xTaskCreate(vTarea2, "T2", 85, NULL, 1, NULL);
		xTaskCreate(vTarea3, "T3", 85, NULL, 1, NULL);

		vTaskStartScheduler();
	}

	while (1);
	return 0;
}