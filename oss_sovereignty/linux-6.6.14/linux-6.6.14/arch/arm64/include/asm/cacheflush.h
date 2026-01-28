#ifndef __ASM_CACHEFLUSH_H
#define __ASM_CACHEFLUSH_H
#include <linux/kgdb.h>
#include <linux/mm.h>
#define PG_dcache_clean PG_arch_1
extern void caches_clean_inval_pou(unsigned long start, unsigned long end);
extern void icache_inval_pou(unsigned long start, unsigned long end);
extern void dcache_clean_inval_poc(unsigned long start, unsigned long end);
extern void dcache_inval_poc(unsigned long start, unsigned long end);
extern void dcache_clean_poc(unsigned long start, unsigned long end);
extern void dcache_clean_pop(unsigned long start, unsigned long end);
extern void dcache_clean_pou(unsigned long start, unsigned long end);
extern long caches_clean_inval_user_pou(unsigned long start, unsigned long end);
extern void sync_icache_aliases(unsigned long start, unsigned long end);
static inline void flush_icache_range(unsigned long start, unsigned long end)
{
	caches_clean_inval_pou(start, end);
	if (in_dbg_master())
		return;
	kick_all_cpus_sync();
}
#define flush_icache_range flush_icache_range
extern void copy_to_user_page(struct vm_area_struct *, struct page *,
	unsigned long, void *, const void *, unsigned long);
#define copy_to_user_page copy_to_user_page
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1
extern void flush_dcache_page(struct page *);
void flush_dcache_folio(struct folio *);
#define flush_dcache_folio flush_dcache_folio
static __always_inline void icache_inval_all_pou(void)
{
	if (cpus_have_const_cap(ARM64_HAS_CACHE_DIC))
		return;
	asm("ic	ialluis");
	dsb(ish);
}
#include <asm-generic/cacheflush.h>
#endif  
