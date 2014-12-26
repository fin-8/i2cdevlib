#include "msec.h"

uint32_t msec = 0;

void initMsecCounter(void) {
	TCCR2A = (1 << WGM21) | (0 << WGM20); // CTC mode
	OCR2A = 249; // 1kHz @ prescaler 64
	TIMSK2 = (1 << TOIE2); // interupt on overflow

	msec = 0;
	TCCR2B = (1 << CS22) | (0 << CS21) | (0 << CS20); // prescaler 64; start timer

	// interrupts still have to be enabled using "sei();" !
}

void stopMsecCounter(void) {
	TIMSK2 &= ~(1 << TOIE2); // interupt on overflow
	TCCR2B &= ~((1 << CS22) | (1 << CS21) | (1 << CS20)); // prescaler 64; start timer
}

ISR (TIMER2_OVF_vect) {msec++;}

uint32_t millis(void) {
	return msec;
}

