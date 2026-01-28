#ifndef __ASM_KFENCE_H
#define __ASM_KFENCE_H
#include <asm/set_memory.h>
static inline bool arch_kfence_init_pool(void) { return true; }
static inline bool kfence_protect_page(unsigned long addr, bool protect)
{
	set_memory_valid(addr, 1, !protect);
	return true;
}
#ifdef CONFIG_KFENCE
extern bool kfence_early_init;
static inline bool arm64_kfence_can_set_direct_map(void)
{
	return !kfence_early_init;
}
#else  
static inline bool arm64_kfence_can_set_direct_map(void) { return false; }
#endif  
#endif  
