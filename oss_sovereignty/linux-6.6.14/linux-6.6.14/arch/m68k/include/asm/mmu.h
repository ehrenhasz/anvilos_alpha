#ifndef __MMU_H
#define __MMU_H
#ifdef CONFIG_MMU
typedef unsigned long mm_context_t;
#else
#include <asm-generic/mmu.h>
#endif
#endif
