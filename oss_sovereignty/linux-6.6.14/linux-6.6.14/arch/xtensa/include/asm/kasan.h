#ifndef __ASM_KASAN_H
#define __ASM_KASAN_H
#ifndef __ASSEMBLY__
#ifdef CONFIG_KASAN
#include <linux/kernel.h>
#include <linux/sizes.h>
#include <asm/kmem_layout.h>
#define KASAN_SHADOW_SCALE_SHIFT 3
#define KASAN_START_VADDR __XTENSA_UL_CONST(0x90000000)
#define KASAN_SHADOW_START (XCHAL_PAGE_TABLE_VADDR + XCHAL_PAGE_TABLE_SIZE)
#define KASAN_SHADOW_SIZE (-KASAN_START_VADDR >> KASAN_SHADOW_SCALE_SHIFT)
#define KASAN_SHADOW_OFFSET __XTENSA_UL_CONST(CONFIG_KASAN_SHADOW_OFFSET)
void __init kasan_early_init(void);
void __init kasan_init(void);
#else
static inline void kasan_early_init(void)
{
}
static inline void kasan_init(void)
{
}
#endif
#endif
#endif
