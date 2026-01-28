
#ifndef MICROPY_INCLUDED_ESP8266_XTIRQ_H
#define MICROPY_INCLUDED_ESP8266_XTIRQ_H

#include <stdint.h>


static inline uint32_t query_irq(void) {
    uint32_t ps;
    __asm__ volatile ("rsr %0, ps" : "=a" (ps));
    return ps & 0xf;
}



static inline uint32_t raise_irq_pri(uint32_t intlevel) {
    uint32_t old_ps;
    __asm__ volatile ("rsil %0, %1" : "=a" (old_ps) : "I" (intlevel));
    return old_ps;
}


static inline void restore_irq_pri(uint32_t ps) {
    __asm__ volatile ("wsr %0, ps; rsync" : : "a" (ps));
}

static inline uint32_t disable_irq(void) {
    return raise_irq_pri(15);
}

static inline void enable_irq(uint32_t irq_state) {
    restore_irq_pri(irq_state);
}

#endif 
