#ifndef _ASM_ARC_SIGCONTEXT_H
#define _ASM_ARC_SIGCONTEXT_H
#include <asm/ptrace.h>
struct sigcontext {
	struct user_regs_struct regs;
	struct user_regs_arcv2 v2abi;
};
#endif  
