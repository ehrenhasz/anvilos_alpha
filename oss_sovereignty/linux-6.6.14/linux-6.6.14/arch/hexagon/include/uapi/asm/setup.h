#ifndef _ASM_SETUP_H
#define _ASM_SETUP_H
#ifdef __KERNEL__
#include <linux/init.h>
#else
#define __init
#endif
#include <asm-generic/setup.h>
extern char external_cmdline_buffer;
void __init setup_arch_memory(void);
#endif
