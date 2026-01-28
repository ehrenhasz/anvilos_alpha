#ifndef _XTENSA_MMU_H
#define _XTENSA_MMU_H
#ifndef CONFIG_MMU
#include <asm-generic/mmu.h>
#else
typedef struct {
	unsigned long asid[NR_CPUS];
	unsigned int cpu;
} mm_context_t;
#endif  
#endif	 
