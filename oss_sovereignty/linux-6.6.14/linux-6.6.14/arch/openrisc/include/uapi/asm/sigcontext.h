#ifndef __ASM_OPENRISC_SIGCONTEXT_H
#define __ASM_OPENRISC_SIGCONTEXT_H
#include <asm/ptrace.h>
struct sigcontext {
	struct user_regs_struct regs;   
	union {
		unsigned long fpcsr;
		unsigned long oldmask;	 
	};
};
#endif  
