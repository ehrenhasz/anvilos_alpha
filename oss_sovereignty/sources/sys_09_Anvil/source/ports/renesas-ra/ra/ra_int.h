

#ifndef RA_RA_INT_H_
#define RA_RA_INT_H_

#include <stdint.h>

#if defined(RA4M1) | defined(RA4W1)
#define IRQ_MAX 48
#elif defined(RA6M1) | defined(RA6M2) | defined(RA6M3) | defined(RA6M5)
#define IRQ_MAX 128
#else
#error "CMSIS MCU Series is not specified."
#endif

extern uint8_t irq_to_ch[IRQ_MAX];

void ra_int_init(void);

#endif 
