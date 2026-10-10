#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>

void adc_init(void)
{
	// Referencia AVCC (5V) con capacitor en AREF, canal ADC0[cite: 1, 7]
	ADMUX = (1 << REFS0);
	
	// Habilita ADC y Prescaler = 128 (16MHz / 128 = 125 kHz)[cite: 1, 7]
	ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
	
	// Deshabilita entrada digital en PC0[cite: 7]
	DIDR0 |= (1 << ADC0D);
}

uint16_t adc_read(void)
{
	// Inicia conversion[cite: 7]
	ADCSRA |= (1 << ADSC);
	
	// Polling esperando fin de conversion[cite: 1, 7]
	while (!(ADCSRA & (1 << ADIF)));
	
	// Limpia bandera ADIF[cite: 7]
	ADCSRA |= (1 << ADIF);
	
	return ADC;
}

void actualizar_displays(uint16_t valor)
{
	// Extrae los 4 nibbles del valor de 10 bits (0x000 a 0x3FF)
	uint8_t n0 = valor & 0x0F;         // Display 4 (menos significativo)
	uint8_t n1 = (valor >> 4) & 0x0F;  // Display 3
	uint8_t n2 = (valor >> 8) & 0x0F;  // Display 2
	uint8_t n3 = (valor >> 12) & 0x0F; // Display 1 (mas significativo)

	// 1. Cargar U4 (Displays 3 y 4)
	// Q4-Q7 = Display 3 (n1), Q0-Q3 = Display 4 (n0)
	PORTB = (n1 << 4) | n0;
	PORTC |= (1 << PC1);   // Pulso LE U4[cite: 1]
	asm("nop");
	PORTC &= ~(1 << PC1);

	// 2. Cargar U3 (Displays 1 y 2)
	// Q4-Q7 = Display 1 (n3), Q0-Q3 = Display 2 (n2)
	PORTB = (n3 << 4) | n2;
	PORTC |= (1 << PC2);   // Pulso LE U3[cite: 1]
	asm("nop");
	PORTC &= ~(1 << PC2);
}

int main(void)
{
	// Puerto B completo como salida para el bus de datos D0-D7[cite: 1]
	DDRB = 0xFF;
	PORTB = 0x00;

	// PC1 y PC2 como salidas para Latch Enable (LE)[cite: 1]
	DDRC |= (1 << PC1) | (1 << PC2);
	PORTC &= ~((1 << PC1) | (1 << PC2));

	adc_init();

	uint16_t lectura = 0;

	while (1)
	{
		lectura = adc_read();
		actualizar_displays(lectura);
		_delay_ms(100); // Tasa de refresco estable para lectura visual
	}

	return 0;
}