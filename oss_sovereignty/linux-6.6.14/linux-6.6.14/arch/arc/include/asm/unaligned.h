#ifndef _ASM_ARC_UNALIGNED_H
#define _ASM_ARC_UNALIGNED_H
#include <asm-generic/unaligned.h>
#include <asm/ptrace.h>
#ifdef CONFIG_ARC_EMUL_UNALIGNED
int misaligned_fixup(unsigned long address, struct pt_regs *regs,
		     struct callee_regs *cregs);
#else
static inline int
misaligned_fixup(unsigned long address, struct pt_regs *regs,
		 struct callee_regs *cregs)
{
	return 1;
}
#endif
#endif  
