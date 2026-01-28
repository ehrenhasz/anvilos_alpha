#ifndef __ASM_ARC_UDELAY_H
#define __ASM_ARC_UDELAY_H
#include <asm-generic/types.h>
#include <asm/param.h>		 
extern unsigned long loops_per_jiffy;
static inline void __delay(unsigned long loops)
{
	__asm__ __volatile__(
	"	mov lp_count, %0	\n"
	"	lp  1f			\n"
	"	nop			\n"
	"1:				\n"
	:
        : "r"(loops)
        : "lp_count");
}
extern void __bad_udelay(void);
static inline void __udelay(unsigned long usecs)
{
	unsigned long loops;
	loops = ((u64) usecs * 4295 * HZ * loops_per_jiffy) >> 32;
	__delay(loops);
}
#define udelay(n) (__builtin_constant_p(n) ? ((n) > 20000 ? __bad_udelay() \
				: __udelay(n)) : __udelay(n))
#endif  
