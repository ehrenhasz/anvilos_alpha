#ifndef __ASM_KASAN_H
#define __ASM_KASAN_H
#ifndef __ASSEMBLY__
#include <linux/linkage.h>
#include <asm/memory.h>
#include <asm/mte-kasan.h>
#include <asm/pgtable-types.h>
#define arch_kasan_set_tag(addr, tag)	__tag_set(addr, tag)
#define arch_kasan_reset_tag(addr)	__tag_reset(addr)
#define arch_kasan_get_tag(addr)	__tag_get(addr)
#if defined(CONFIG_KASAN_GENERIC) || defined(CONFIG_KASAN_SW_TAGS)
void kasan_init(void);
#define _KASAN_SHADOW_START(va)	(KASAN_SHADOW_END - (1UL << ((va) - KASAN_SHADOW_SCALE_SHIFT)))
#define KASAN_SHADOW_START      _KASAN_SHADOW_START(vabits_actual)
void kasan_copy_shadow(pgd_t *pgdir);
asmlinkage void kasan_early_init(void);
#else
static inline void kasan_init(void) { }
static inline void kasan_copy_shadow(pgd_t *pgdir) { }
#endif
#endif
#endif
