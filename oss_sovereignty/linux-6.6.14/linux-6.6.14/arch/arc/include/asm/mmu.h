#ifndef _ASM_ARC_MMU_H
#define _ASM_ARC_MMU_H
#ifndef __ASSEMBLY__
#include <linux/threads.h>	 
typedef struct {
	unsigned long asid[NR_CPUS];	 
} mm_context_t;
extern void do_tlb_overlap_fault(unsigned long, unsigned long, struct pt_regs *);
#endif
#include <asm/mmu-arcv2.h>
#endif
