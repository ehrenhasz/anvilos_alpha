#ifndef _ASM_POWERPC_VDSO_TIMEBASE_H
#define _ASM_POWERPC_VDSO_TIMEBASE_H
#include <asm/reg.h>
#if defined(__powerpc64__) && (defined(CONFIG_PPC_CELL) || defined(CONFIG_PPC_E500))
#define mftb()		({unsigned long rval;				\
			asm volatile(					\
				"90:	mfspr %0, %2;\n"		\
				ASM_FTR_IFSET(				\
					"97:	cmpwi %0,0;\n"		\
					"	beq- 90b;\n", "", %1)	\
			: "=r" (rval) \
			: "i" (CPU_FTR_CELL_TB_BUG), "i" (SPRN_TBRL) : "cr0"); \
			rval;})
#elif defined(CONFIG_PPC_8xx)
#define mftb()		({unsigned long rval;	\
			asm volatile("mftbl %0" : "=r" (rval)); rval;})
#else
#define mftb()		({unsigned long rval;	\
			asm volatile("mfspr %0, %1" : \
				     "=r" (rval) : "i" (SPRN_TBRL)); rval;})
#endif  
#if defined(CONFIG_PPC_8xx)
#define mftbu()		({unsigned long rval;	\
			asm volatile("mftbu %0" : "=r" (rval)); rval;})
#else
#define mftbu()		({unsigned long rval;	\
			asm volatile("mfspr %0, %1" : "=r" (rval) : \
				"i" (SPRN_TBRU)); rval;})
#endif
#define mttbl(v)	asm volatile("mttbl %0":: "r"(v))
#define mttbu(v)	asm volatile("mttbu %0":: "r"(v))
static __always_inline u64 get_tb(void)
{
	unsigned int tbhi, tblo, tbhi2;
	if (__is_defined(__powerpc64__))
		return mftb();
	do {
		tbhi = mftbu();
		tblo = mftb();
		tbhi2 = mftbu();
	} while (tbhi != tbhi2);
	return ((u64)tbhi << 32) | tblo;
}
static inline void set_tb(unsigned int upper, unsigned int lower)
{
	mtspr(SPRN_TBWL, 0);
	mtspr(SPRN_TBWU, upper);
	mtspr(SPRN_TBWL, lower);
}
#endif  
