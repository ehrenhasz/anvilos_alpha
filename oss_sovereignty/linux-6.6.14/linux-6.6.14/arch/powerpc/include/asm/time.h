#ifndef __POWERPC_TIME_H
#define __POWERPC_TIME_H
#ifdef __KERNEL__
#include <linux/types.h>
#include <linux/percpu.h>
#include <asm/processor.h>
#include <asm/cpu_has_feature.h>
#include <asm/vdso/timebase.h>
extern u64 decrementer_max;
extern unsigned long tb_ticks_per_jiffy;
extern unsigned long tb_ticks_per_usec;
extern unsigned long tb_ticks_per_sec;
extern struct clock_event_device decrementer_clockevent;
extern u64 decrementer_max;
extern void generic_calibrate_decr(void);
extern unsigned long ppc_proc_freq;
#define DEFAULT_PROC_FREQ	(DEFAULT_TB_FREQ * 8)
extern unsigned long ppc_tb_freq;
#define DEFAULT_TB_FREQ		125000000UL
extern bool tb_invalid;
struct div_result {
	u64 result_high;
	u64 result_low;
};
static inline u64 get_vtb(void)
{
	if (cpu_has_feature(CPU_FTR_ARCH_207S))
		return mfspr(SPRN_VTB);
	return 0;
}
static inline u64 get_dec(void)
{
	if (IS_ENABLED(CONFIG_40x))
		return mfspr(SPRN_PIT);
	return mfspr(SPRN_DEC);
}
static inline void set_dec(u64 val)
{
	if (IS_ENABLED(CONFIG_40x))
		mtspr(SPRN_PIT, (u32)val);
	else if (IS_ENABLED(CONFIG_BOOKE))
		mtspr(SPRN_DEC, val);
	else
		mtspr(SPRN_DEC, val - 1);
}
static inline unsigned long tb_ticks_since(unsigned long tstamp)
{
	return mftb() - tstamp;
}
#define mulhwu(x,y) \
({unsigned z; asm ("mulhwu %0,%1,%2" : "=r" (z) : "r" (x), "r" (y)); z;})
#ifdef CONFIG_PPC64
#define mulhdu(x,y) \
({unsigned long z; asm ("mulhdu %0,%1,%2" : "=r" (z) : "r" (x), "r" (y)); z;})
#else
extern u64 mulhdu(u64, u64);
#endif
extern void div128_by_32(u64 dividend_high, u64 dividend_low,
			 unsigned divisor, struct div_result *dr);
extern void secondary_cpu_time_init(void);
extern void __init time_init(void);
DECLARE_PER_CPU(u64, decrementers_next_tb);
static inline u64 timer_get_next_tb(void)
{
	return __this_cpu_read(decrementers_next_tb);
}
#ifdef CONFIG_KVM_BOOK3S_HV_POSSIBLE
void timer_rearm_host_dec(u64 now);
#endif
unsigned long long tb_to_ns(unsigned long long tb_ticks);
void timer_broadcast_interrupt(void);
void pseries_accumulate_stolen_time(void);
u64 pseries_calculate_stolen_time(u64 stop_tb);
#endif  
#endif  
