/* cpu/timer.h */
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

void init_timer(uint32_t freq);

/* Allow kernel to read the tick count */
extern volatile uint32_t tick;

#endif
