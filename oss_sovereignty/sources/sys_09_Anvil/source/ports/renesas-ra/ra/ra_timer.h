

#ifndef RA_RA_TIMER_H_
#define RA_RA_TIMER_H_

#include <stdint.h>

void SysTick_Handler(void);
uint32_t HAL_GetTick(void);

#define DEF_CLKDEV    2
#define TENUSEC_COUNT (PCLK / DEF_CLKDEV / 100000)
#define MSEC_COUNT    (PCLK / DEF_CLKDEV / 100)

typedef void (*AGT_TIMER_CB)(void *);

void ra_agt_timer_set_callback(uint32_t ch, AGT_TIMER_CB cb, void *param);
void ra_agt_int_isr0(void);
void ra_agt_int_isr1(void);
void ra_agt_timer_start(uint32_t ch);
void ra_agt_timer_stop(uint32_t ch);
void ra_agt_timer_set_freq(uint32_t ch, float freq);
float ra_agt_timer_get_freq(uint32_t ch);
void ra_agt_timer_init(uint32_t ch, float freq);
void ra_agt_timer_deinit(uint32_t ch);
__WEAK void agt_int_isr(void);
uint32_t mtick();

#endif 
