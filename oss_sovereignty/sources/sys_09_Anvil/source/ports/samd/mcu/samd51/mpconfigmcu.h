
#include "samd51.h"

#define MICROPY_CONFIG_ROM_LEVEL        (MICROPY_CONFIG_ROM_LEVEL_FULL_FEATURES)


#define MICROPY_EMIT_THUMB              (1)
#define MICROPY_EMIT_INLINE_THUMB       (1)


#define MICROPY_ENABLE_EMERGENCY_EXCEPTION_BUF  (1)

#define MICROPY_PY_OS_SYNC              (1)
#define MICROPY_PY_OS_URANDOM           (1)
#define MICROPY_PY_ONEWIRE              (1)
#define MICROPY_PY_RANDOM_SEED_INIT_FUNC (trng_random_u32())
unsigned long trng_random_u32(void);


#define MICROPY_FATFS_ENABLE_LFN            (1)
#define MICROPY_FATFS_RPATH                 (2)
#define MICROPY_FATFS_MAX_SS                (4096)
#define MICROPY_FATFS_LFN_CODE_PAGE         437 

#define VFS_BLOCK_SIZE_BYTES            (1536) 

#ifndef MICROPY_HW_UART_TXBUF
#define MICROPY_HW_UART_TXBUF           (1)
#endif
#ifndef MICROPY_HW_UART_RTSCTS
#define MICROPY_HW_UART_RTSCTS          (1)
#endif

#define CPU_FREQ                        (120000000)
#define DFLL48M_FREQ                    (48000000)
#define MAX_CPU_FREQ                    (200000000)
#define DPLLx_REF_FREQ                  (32768)

#define NVIC_PRIORITYGROUP_4            ((uint32_t)0x00000003)
#define IRQ_PRI_PENDSV                  NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 7, 0)

static inline uint32_t raise_irq_pri(uint32_t pri) {
    uint32_t basepri = __get_BASEPRI();
    
    
    
    
    
    pri <<= (8 - __NVIC_PRIO_BITS);
    __ASM volatile ("msr basepri_max, %0" : : "r" (pri) : "memory");
    return basepri;
}


static inline void restore_irq_pri(uint32_t basepri) {
    __set_BASEPRI(basepri);
}
