

#define mftb()  ({unsigned long rval;                                   \
                  __asm__ volatile ("mftb %0" : "=r" (rval)); rval;})

#define TBFREQ 512000000

static inline mp_uint_t mp_hal_ticks_ms(void) {
    unsigned long tb = mftb();

    return tb * 1000 / TBFREQ;
}

static inline mp_uint_t mp_hal_ticks_us(void) {
    unsigned long tb = mftb();

    return tb * 1000000 / TBFREQ;
}

static inline void mp_hal_set_interrupt_char(char c) {
}
