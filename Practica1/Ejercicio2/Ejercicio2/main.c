#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>

volatile uint8_t flag_toggle = 0;

void timer0_init(void)
{
	// Modo CTC, Prescaler de 64, interrupción en Compare Match A
	TCCR0A = (1 << WGM01);
	TCCR0B = (1 << CS01) | (1 << CS00);
	OCR0A = 249;
	TIMSK0 = (1 << OCIE0A);
}

ISR(TIMER0_COMPA_vect)
{
	static uint8_t contador_1ms = 0;
	
	contador_1ms++;
	
	if (contador_1ms >= 250)
	{
		flag_toggle = 1;
		contador_1ms = 0;
	}
}

int main(void)
{
	// Configuración de puertos
	DDRB |= (1 << PB0) | (1 << PB1);
	DDRD &= ~((1 << PD2) | (1 << PD3));
	PORTD |= (1 << PD2) | (1 << PD3);
	
	// Condición inicial: Ambos LEDs encendidos
	PORTB |= (1 << PB0) | (1 << PB1);

	// Trampa bloqueante (espera la presión del botón)
	while ((PIND & (1 << PD2)) && (PIND & (1 << PD3)))
	{
	}
	
	// Configuración inicial del parpadeo alternado
	PORTB |= (1 << PB0);
	PORTB &= ~(1 << PB1);
	
	// Recién acá arrancamos el Timer y permitimos que interrumpa
	timer0_init();
	sei();

	// Superloop infinito
	while (1)
	{
		if (flag_toggle)
		{
			PORTB ^= (1 << PB0) | (1 << PB1);
			flag_toggle = 0;
		}
	}
	
	return 0;
}