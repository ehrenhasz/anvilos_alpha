 
#ifndef _ASM_X86_CFI_H
#define _ASM_X86_CFI_H

 

#include <linux/cfi.h>

#ifdef CONFIG_CFI_CLANG
enum bug_trap_type handle_cfi_failure(struct pt_regs *regs);
#else
static inline enum bug_trap_type handle_cfi_failure(struct pt_regs *regs)
{
	return BUG_TRAP_TYPE_NONE;
}
#endif  

#endif  
