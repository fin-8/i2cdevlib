#ifndef __MSEC_H__
#define __MSEC_H__

#include <avr/io.h>
#include <avr/interrupt.h>

void initMsecCounter(void);
void stopMsecCounter(void);
uint32_t millis(void);

#endif

