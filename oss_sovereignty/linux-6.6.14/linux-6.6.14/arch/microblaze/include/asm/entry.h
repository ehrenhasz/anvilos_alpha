#ifndef _ASM_MICROBLAZE_ENTRY_H
#define _ASM_MICROBLAZE_ENTRY_H
#include <asm/percpu.h>
#include <asm/ptrace.h>
#include <linux/linkage.h>
#define PER_CPU(var) var
# ifndef __ASSEMBLY__
DECLARE_PER_CPU(unsigned int, KSP);  
DECLARE_PER_CPU(unsigned int, KM);  
DECLARE_PER_CPU(unsigned int, ENTRY_SP);  
DECLARE_PER_CPU(unsigned int, R11_SAVE);  
DECLARE_PER_CPU(unsigned int, CURRENT_SAVE);  
extern asmlinkage void do_notify_resume(struct pt_regs *regs, int in_syscall);
# endif  
#endif  
