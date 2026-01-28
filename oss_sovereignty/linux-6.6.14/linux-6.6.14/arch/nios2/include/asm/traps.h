#ifndef _ASM_NIOS2_TRAPS_H
#define _ASM_NIOS2_TRAPS_H
#define TRAP_ID_SYSCALL		0
#ifndef __ASSEMBLY__
void _exception(int signo, struct pt_regs *regs, int code, unsigned long addr);
#endif
#endif  
