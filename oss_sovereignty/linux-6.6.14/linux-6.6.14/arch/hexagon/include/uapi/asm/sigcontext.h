#ifndef _ASM_SIGCONTEXT_H
#define _ASM_SIGCONTEXT_H
#include <asm/user.h>
struct sigcontext {
	struct user_regs_struct sc_regs;
} __aligned(8);
#endif
