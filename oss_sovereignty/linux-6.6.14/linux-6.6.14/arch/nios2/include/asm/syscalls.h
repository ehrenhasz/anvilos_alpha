#ifndef __ASM_NIOS2_SYSCALLS_H
#define __ASM_NIOS2_SYSCALLS_H
int sys_cacheflush(unsigned long addr, unsigned long len,
				unsigned int op);
#include <asm-generic/syscalls.h>
#endif  
