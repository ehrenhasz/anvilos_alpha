
#ifndef MICROPY_INCLUDED_STM32_IRQ_H
#define MICROPY_INCLUDED_STM32_IRQ_H



#define IRQn_NONNEG(pri) ((pri) & 0x7f)


#define IRQ_STATE_DISABLED (0x00000001)
#define IRQ_STATE_ENABLED  (0x00000000)



#define IRQ_ENABLE_STATS (0)

#if IRQ_ENABLE_STATS
#if defined(STM32H7)
#define IRQ_STATS_MAX   (256)
#else
#define IRQ_STATS_MAX   (128)
#endif
extern uint32_t irq_stats[IRQ_STATS_MAX];
#define IRQ_ENTER(irq) ++irq_stats[irq]
#define IRQ_EXIT(irq)
#else
#define IRQ_ENTER(irq)
#define IRQ_EXIT(irq)
#endif








static inline uint32_t query_irq(void) {
    return __get_PRIMASK();
}

static inline void enable_irq(mp_uint_t state) {
    __set_PRIMASK(state);
}

static inline mp_uint_t disable_irq(void) {
    mp_uint_t state = __get_PRIMASK();
    __disable_irq();
    return state;
}

#if __CORTEX_M >= 0x03



static inline uint32_t raise_irq_pri(uint32_t pri) {
    uint32_t basepri = __get_BASEPRI();
    
    
    
    
    
    pri <<= (8 - __NVIC_PRIO_BITS);
    __ASM volatile ("msr basepri_max, %0" : : "r" (pri) : "memory");
    return basepri;
}


static inline void restore_irq_pri(uint32_t basepri) {
    __set_BASEPRI(basepri);
}

#else

static inline uint32_t raise_irq_pri(uint32_t pri) {
    return disable_irq();
}


static inline void restore_irq_pri(uint32_t state) {
    enable_irq(state);
}

#endif






















#if __CORTEX_M == 0

#define IRQ_PRI_SYSTICK         0
#define IRQ_PRI_UART            1
#define IRQ_PRI_SDIO            1
#define IRQ_PRI_DMA             1
#define IRQ_PRI_FLASH           2
#define IRQ_PRI_OTG_FS          2
#define IRQ_PRI_OTG_HS          2
#define IRQ_PRI_TIM5            2
#define IRQ_PRI_CAN             2
#define IRQ_PRI_TIMX            2
#define IRQ_PRI_EXTINT          2
#define IRQ_PRI_PENDSV          3
#define IRQ_PRI_RTC_WKUP        3

#else

#define IRQ_PRI_SYSTICK         NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 0, 0)



#define IRQ_PRI_UART            NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 1, 0)


#define IRQ_PRI_SDIO            NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 4, 0)



#define IRQ_PRI_DMA             NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 5, 0)




#define IRQ_PRI_FLASH           NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 6, 0)

#define IRQ_PRI_OTG_FS          NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 6, 0)
#define IRQ_PRI_OTG_HS          NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 6, 0)
#define IRQ_PRI_TIM5            NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 6, 0)

#define IRQ_PRI_CAN             NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 7, 0)

#define IRQ_PRI_SUBGHZ_RADIO    NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 8, 0)

#define IRQ_PRI_SPI             NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 8, 0)

#define IRQ_PRI_HSEM            NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 10, 0)


#define IRQ_PRI_TIMX            NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 13, 0)

#define IRQ_PRI_EXTINT          NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 14, 0)



#define IRQ_PRI_PENDSV          NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 15, 0)
#define IRQ_PRI_RTC_WKUP        NVIC_EncodePriority(NVIC_PRIORITYGROUP_4, 15, 0)

#endif

#endif 
