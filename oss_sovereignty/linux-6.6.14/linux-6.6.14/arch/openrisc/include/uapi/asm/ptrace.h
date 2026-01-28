#ifndef _UAPI__ASM_OPENRISC_PTRACE_H
#define _UAPI__ASM_OPENRISC_PTRACE_H
#ifndef __ASSEMBLY__
struct user_regs_struct {
	unsigned long gpr[32];
	unsigned long pc;
	unsigned long sr;
};
struct __or1k_fpu_state {
	unsigned long fpcsr;
};
#endif
#endif  
