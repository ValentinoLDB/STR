#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>

int main(void) {
	// Configuración de puertos
	DDRB |= (1 << PB0) | (1 << PB1);//salidas
	DDRD &= ~((1 << PD2) | (1 << PD3));//entradas
	PORTD |= (1 << PD2) | (1 << PD3);// pull-ups
	PORTB |= (1 << PB0) | (1 << PB1);//Los 2 prendidos
	
	// Mantiene el estado inicial hasta que se toque cualquiera de los 2 botones
	while ((PIND & (1 << PD2)) && (PIND & (1 << PD3))) {}
	PORTB |= (1 << PB0);   // PB0 encendido
	PORTB &= ~(1 << PB1);  // PB1 apagado
	_delay_ms(250);        // Retardo inicial
	
	while (1) {
		PORTB ^= (1 << PB0) | (1 << PB1);
		_delay_ms(250);
	}
	
	return 0;
}