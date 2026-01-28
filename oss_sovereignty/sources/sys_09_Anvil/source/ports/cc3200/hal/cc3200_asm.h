

#ifndef CC3200_ASM_H_
#define CC3200_ASM_H_









#define IRQ_STATE_DISABLED (0x00000001)
#define IRQ_STATE_ENABLED  (0x00000000)

#ifndef __disable_irq
#define __disable_irq() __asm__ volatile ("cpsid i");
#endif

#ifndef DEBUG
__attribute__(( always_inline ))
static inline void __WFI(void) {
    __asm volatile ("    dsb      \n"
                    "    isb      \n"
                    "    wfi      \n");
}
#else

__attribute__(( always_inline ))
static inline void __WFI(void) {
    __asm volatile ("    dsb      \n"
                    "    isb      \n");
}
#endif

__attribute__(( always_inline ))
static inline uint32_t __get_PRIMASK(void) {
    uint32_t result;
    __asm volatile ("mrs %0, primask" : "=r" (result));
    return(result);
}

__attribute__(( always_inline ))
static inline void __set_PRIMASK(uint32_t priMask) {
    __asm volatile ("msr primask, %0" : : "r" (priMask) : "memory");
}

__attribute__(( always_inline ))
static inline uint32_t __get_BASEPRI(void) {
    uint32_t result;
    __asm volatile ("mrs %0, basepri" : "=r" (result));
    return(result);
}

__attribute__(( always_inline ))
static inline void __set_BASEPRI(uint32_t value) {
    __asm volatile ("msr basepri, %0" : : "r" (value) : "memory");
}

__attribute__(( always_inline ))
static inline uint32_t query_irq(void) {
    return __get_PRIMASK();
}

__attribute__(( always_inline ))
static inline void enable_irq(mp_uint_t state) {
    __set_PRIMASK(state);
}

__attribute__(( always_inline ))
static inline mp_uint_t disable_irq(void) {
    mp_uint_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

#endif 
