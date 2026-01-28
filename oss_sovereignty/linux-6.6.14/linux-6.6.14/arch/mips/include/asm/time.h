#ifndef _ASM_TIME_H
#define _ASM_TIME_H
#include <linux/rtc.h>
#include <linux/spinlock.h>
#include <linux/clockchips.h>
#include <linux/clocksource.h>
extern spinlock_t rtc_lock;
extern void plat_time_init(void);
extern unsigned int mips_hpt_frequency;
extern int (*perf_irq)(void);
extern int __weak get_c0_perfcount_int(void);
extern unsigned int get_c0_compare_int(void);
extern int r4k_clockevent_init(void);
static inline int mips_clockevent_init(void)
{
#ifdef CONFIG_CEVT_R4K
	return r4k_clockevent_init();
#else
	return -ENXIO;
#endif
}
extern int init_r4k_clocksource(void);
static inline int init_mips_clocksource(void)
{
#ifdef CONFIG_CSRC_R4K
	return init_r4k_clocksource();
#else
	return 0;
#endif
}
static inline void clockevent_set_clock(struct clock_event_device *cd,
					unsigned int clock)
{
	clockevents_calc_mult_shift(cd, clock, 4);
}
#endif  
