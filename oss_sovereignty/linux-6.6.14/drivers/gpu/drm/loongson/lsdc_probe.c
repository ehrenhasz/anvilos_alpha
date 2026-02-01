
 

#include "lsdc_drv.h"
#include "lsdc_probe.h"

 
#define LOONGSON_CPU_IMP_MASK           0xff00
#define LOONGSON_CPU_IMP_SHIFT          8

#define LOONGARCH_CPU_IMP_LS2K1000      0xa0
#define LOONGARCH_CPU_IMP_LS2K2000      0xb0
#define LOONGARCH_CPU_IMP_LS3A5000      0xc0

#define LOONGSON_CPU_MIPS_IMP_LS2K      0x61  

 
#define LOONGSON_CPU_REV_MASK           0x00ff

#define LOONGARCH_CPUCFG_PRID_REG       0x0

 

unsigned int loongson_cpu_get_prid(u8 *imp, u8 *rev)
{
	unsigned int prid = 0;

#if defined(__loongarch__)
	__asm__ volatile("cpucfg %0, %1\n\t"
			: "=&r"(prid)
			: "r"(LOONGARCH_CPUCFG_PRID_REG)
			);
#endif

#if defined(__mips__)
	__asm__ volatile("mfc0\t%0, $15\n\t"
			: "=r" (prid)
			);
#endif

	if (imp)
		*imp = (prid & LOONGSON_CPU_IMP_MASK) >> LOONGSON_CPU_IMP_SHIFT;

	if (rev)
		*rev = prid & LOONGSON_CPU_REV_MASK;

	return prid;
}
