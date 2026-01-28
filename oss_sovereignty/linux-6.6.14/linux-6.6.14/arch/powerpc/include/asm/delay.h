#ifndef _ASM_POWERPC_DELAY_H
#define _ASM_POWERPC_DELAY_H
#ifdef __KERNEL__
#include <linux/processor.h>
#include <asm/time.h>
extern void __delay(unsigned long loops);
extern void udelay(unsigned long usecs);
#ifdef CONFIG_PPC64
#define mdelay(n)	udelay((n) * 1000)
#endif
#define spin_event_timeout(condition, timeout, delay)                          \
({                                                                             \
	typeof(condition) __ret;                                               \
	unsigned long __loops = tb_ticks_per_usec * timeout;                   \
	unsigned long __start = mftb();                                     \
                                                                               \
	if (delay) {                                                           \
		while (!(__ret = (condition)) &&                               \
				(tb_ticks_since(__start) <= __loops))          \
			udelay(delay);                                         \
	} else {                                                               \
		spin_begin();                                                  \
		while (!(__ret = (condition)) &&                               \
				(tb_ticks_since(__start) <= __loops))          \
			spin_cpu_relax();                                      \
		spin_end();                                                    \
	}                                                                      \
	if (!__ret)                                                            \
		__ret = (condition);                                           \
	__ret;		                                                       \
})
#endif  
#endif  
