#ifndef _UAPI__ASM_SIGCONTEXT_H
#define _UAPI__ASM_SIGCONTEXT_H
#include <linux/types.h>
#define MCONTEXT_VERSION 2
struct sigcontext {
	int version;
	unsigned long gregs[32];
};
#endif
