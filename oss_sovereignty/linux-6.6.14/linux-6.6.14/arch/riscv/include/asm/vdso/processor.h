#ifndef __ASM_VDSO_PROCESSOR_H
#define __ASM_VDSO_PROCESSOR_H
#ifndef __ASSEMBLY__
#include <asm/barrier.h>
static inline void cpu_relax(void)
{
#ifdef __riscv_muldiv
	int dummy;
	__asm__ __volatile__ ("div %0, %0, zero" : "=r" (dummy));
#endif
#ifdef CONFIG_TOOLCHAIN_HAS_ZIHINTPAUSE
	__asm__ __volatile__ ("pause");
#else
	__asm__ __volatile__ (".4byte 0x100000F");
#endif
	barrier();
}
#endif  
#endif  
