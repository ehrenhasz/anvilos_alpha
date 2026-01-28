#ifndef _ASM_CMPXCHG_H
#define _ASM_CMPXCHG_H
static inline unsigned long
__arch_xchg(unsigned long x, volatile void *ptr, int size)
{
	unsigned long retval;
	if (size != 4) do { asm volatile("brkpt;\n"); } while (1);
	__asm__ __volatile__ (
	"1:	%0 = memw_locked(%1);\n"     
	"	memw_locked(%1,P0) = %2;\n"  
	"	if (!P0) jump 1b;\n"
	: "=&r" (retval)
	: "r" (ptr), "r" (x)
	: "memory", "p0"
	);
	return retval;
}
#define arch_xchg(ptr, v) ((__typeof__(*(ptr)))__arch_xchg((unsigned long)(v), (ptr), \
							   sizeof(*(ptr))))
#define arch_cmpxchg(ptr, old, new)				\
({								\
	__typeof__(ptr) __ptr = (ptr);				\
	__typeof__(*(ptr)) __old = (old);			\
	__typeof__(*(ptr)) __new = (new);			\
	__typeof__(*(ptr)) __oldval = 0;			\
								\
	asm volatile(						\
		"1:	%0 = memw_locked(%1);\n"		\
		"	{ P0 = cmp.eq(%0,%2);\n"		\
		"	  if (!P0.new) jump:nt 2f; }\n"		\
		"	memw_locked(%1,p0) = %3;\n"		\
		"	if (!P0) jump 1b;\n"			\
		"2:\n"						\
		: "=&r" (__oldval)				\
		: "r" (__ptr), "r" (__old), "r" (__new)		\
		: "memory", "p0"				\
	);							\
	__oldval;						\
})
#endif  
