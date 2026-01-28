#ifndef _ASM_MICROBLAZE_SIGCONTEXT_H
#define _ASM_MICROBLAZE_SIGCONTEXT_H
#include <asm/ptrace.h>
struct sigcontext {
	struct pt_regs regs;
	unsigned long oldmask;
};
#endif  
