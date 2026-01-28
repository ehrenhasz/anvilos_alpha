#ifndef _ASM_MICROBLAZE_DELAY_H
#define _ASM_MICROBLAZE_DELAY_H
#include <linux/param.h>
static inline void __delay(unsigned long loops)
{
	asm volatile ("# __delay		\n\t"		\
			"1: addi	%0, %0, -1\t\n"		\
			"bneid	%0, 1b		\t\n"		\
			"nop			\t\n"
			: "=r" (loops)
			: "0" (loops));
}
#define __MAX_UDELAY	(226050910UL/HZ)	 
#define __MAX_NDELAY	(4294967295UL/HZ)	 
extern unsigned long loops_per_jiffy;
static inline void __udelay(unsigned int x)
{
	unsigned long long tmp =
		(unsigned long long)x * (unsigned long long)loops_per_jiffy \
			* 226LL;
	unsigned loops = tmp >> 32;
	__delay(loops);
}
extern void __bad_udelay(void);		 
extern void __bad_ndelay(void);		 
#define udelay(n)						\
	({							\
		if (__builtin_constant_p(n)) {			\
			if ((n) / __MAX_UDELAY >= 1)		\
				__bad_udelay();			\
			else					\
				__udelay((n) * (19 * HZ));	\
		} else {					\
			__udelay((n) * (19 * HZ));		\
		}						\
	})
#define ndelay(n)						\
	({							\
		if (__builtin_constant_p(n)) {			\
			if ((n) / __MAX_NDELAY >= 1)		\
				__bad_ndelay();			\
			else					\
				__udelay((n) * HZ);		\
		} else {					\
			__udelay((n) * HZ);			\
		}						\
	})
#define muldiv(a, b, c)		(((a)*(b))/(c))
#endif  
