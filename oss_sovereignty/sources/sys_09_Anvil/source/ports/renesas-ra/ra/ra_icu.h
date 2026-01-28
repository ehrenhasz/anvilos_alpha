

#ifndef RA_RA_ICU_H_
#define RA_RA_ICU_H_

#include <stdint.h>
#include <stdbool.h>


#define VECTOR_NUMBER_NONE (0xff)

typedef struct ra_irq_pin {
    uint8_t irq_no;
    uint32_t pin;
} ra_icu_pin_t;

typedef void (*ICU_CB)(void *);

bool ra_icu_find_irq_no(uint32_t pin, uint8_t *irq_no);
void ra_icu_set_pin(uint32_t pin, bool irq_enable, bool pullup);
void ra_icu_enable_irq_no(uint8_t irq_no);
void ra_icu_disable_irq_no(uint8_t irq_no);
void ra_icu_enable_pin(uint32_t pin);
void ra_icu_disable_pin(uint32_t pin);
void ra_icu_priority_irq_no(uint8_t irq_no, uint32_t ipl);
void ra_icu_priority_pin(uint32_t pin, uint32_t ipl);
void ra_icu_set_callback(uint8_t irq_no, ICU_CB func, void *param);
void ra_icu_trigger_irq_no(uint8_t irq_no, uint32_t cond);
void ra_icu_trigger_pin(uint32_t pin, uint32_t cond);
void ra_icu_set_bounce(uint8_t irq_no, uint32_t bounce);
void ra_icu_init(void);
void ra_icu_deinit(void);
void ra_icu_swint(uint8_t irq_no);
void r_icu_isr(void);

#endif 
