#ifndef __PLAT_TIME_H
#define __PLAT_TIME_H
void orion_time_set_base(void __iomem *timer_base);
void orion_time_init(void __iomem *bridge_base, u32 bridge_timer1_clr_mask,
		     unsigned int irq, unsigned int tclk);
#endif
