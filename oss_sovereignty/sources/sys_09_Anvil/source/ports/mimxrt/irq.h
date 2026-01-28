
#ifndef MICROPY_INCLUDED_MIMXRT_IRQ_H
#define MICROPY_INCLUDED_MIMXRT_IRQ_H

#define NVIC_PRIORITYGROUP_4    ((uint32_t)0x00000003)

static inline uint32_t query_irq(void) {
    return __get_PRIMASK();
}

static inline uint32_t raise_irq_pri(uint32_t pri) {
    uint32_t basepri = __get_BASEPRI();
    
    
    
    
    
    pri <<= (8 - __NVIC_PRIO_BITS);
    __ASM volatile ("msr basepri_max, %0" : : "r" (pri) : "memory");
    return basepri;
}


static inline void restore_irq_pri(uint32_t basepri) {
    __set_BASEPRI(basepri);
}



















#define IRQ_PRI_SYSTICK         NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 0, 0)

#define IRQ_PRI_OTG_HS          NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 6, 0)

#define IRQ_PRI_EXTINT          NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 14, 0)



#define IRQ_PRI_PENDSV          NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 15, 0)

#endif 
