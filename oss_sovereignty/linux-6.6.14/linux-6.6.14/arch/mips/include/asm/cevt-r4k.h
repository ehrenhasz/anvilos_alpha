#ifndef __ASM_CEVT_R4K_H
#define __ASM_CEVT_R4K_H
#include <linux/clockchips.h>
#include <asm/time.h>
DECLARE_PER_CPU(struct clock_event_device, mips_clockevent_device);
void mips_event_handler(struct clock_event_device *dev);
int c0_compare_int_usable(void);
irqreturn_t c0_compare_interrupt(int, void *);
extern struct irqaction c0_compare_irqaction;
extern int cp0_timer_irq_installed;
#endif  
