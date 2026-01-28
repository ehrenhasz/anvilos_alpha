#ifndef _ASM_OR1K_SETUP_H
#define _ASM_OR1K_SETUP_H
#include <linux/init.h>
#include <asm-generic/setup.h>
#ifndef __ASSEMBLY__
void __init or1k_early_setup(void *fdt);
#endif
#endif  
