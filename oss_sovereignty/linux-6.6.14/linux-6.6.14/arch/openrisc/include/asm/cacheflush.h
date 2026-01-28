#ifndef __ASM_CACHEFLUSH_H
#define __ASM_CACHEFLUSH_H
#include <linux/mm.h>
extern void local_dcache_page_flush(struct page *page);
extern void local_icache_page_inv(struct page *page);
#ifndef CONFIG_SMP
#define dcache_page_flush(page)      local_dcache_page_flush(page)
#define icache_page_inv(page)        local_icache_page_inv(page)
#else   
#define dcache_page_flush(page)      local_dcache_page_flush(page)
#define icache_page_inv(page)        smp_icache_page_inv(page)
extern void smp_icache_page_inv(struct page *page);
#endif  
static inline void sync_icache_dcache(struct page *page)
{
	if (!IS_ENABLED(CONFIG_DCACHE_WRITETHROUGH))
		dcache_page_flush(page);
	icache_page_inv(page);
}
#define PG_dc_clean                  PG_arch_1
static inline void flush_dcache_folio(struct folio *folio)
{
	clear_bit(PG_dc_clean, &folio->flags);
}
#define flush_dcache_folio flush_dcache_folio
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1
static inline void flush_dcache_page(struct page *page)
{
	flush_dcache_folio(page_folio(page));
}
#define flush_icache_user_page(vma, page, addr, len)	\
do {							\
	if (vma->vm_flags & VM_EXEC)			\
		sync_icache_dcache(page);		\
} while (0)
#include <asm-generic/cacheflush.h>
#endif  
