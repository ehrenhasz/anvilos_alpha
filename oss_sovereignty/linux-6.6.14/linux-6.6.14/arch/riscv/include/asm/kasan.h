#ifndef __ASM_KASAN_H
#define __ASM_KASAN_H
#ifndef __ASSEMBLY__
#ifdef CONFIG_KASAN
#define KASAN_SHADOW_SCALE_SHIFT	3
#define KASAN_SHADOW_SIZE	(UL(1) << ((VA_BITS - 1) - KASAN_SHADOW_SCALE_SHIFT))
#define KASAN_SHADOW_START	((KASAN_SHADOW_END - KASAN_SHADOW_SIZE) & PGDIR_MASK)
#define KASAN_SHADOW_END	MODULES_LOWEST_VADDR
#define KASAN_SHADOW_OFFSET	_AC(CONFIG_KASAN_SHADOW_OFFSET, UL)
void kasan_init(void);
asmlinkage void kasan_early_init(void);
void kasan_swapper_init(void);
#endif
#endif
#endif  
