#ifndef _ASM_CACHEFLUSH_H
#define _ASM_CACHEFLUSH_H
#include <linux/mm.h>
#include <asm/shmparam.h>
void flush_cache_all(void);
void flush_icache_range(unsigned long kstart, unsigned long kend);
void __sync_icache_dcache(phys_addr_t paddr, unsigned long vaddr, int len);
void __inv_icache_pages(phys_addr_t paddr, unsigned long vaddr, unsigned nr);
void __flush_dcache_pages(phys_addr_t paddr, unsigned long vaddr, unsigned nr);
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1
void flush_dcache_page(struct page *page);
void flush_dcache_folio(struct folio *folio);
#define flush_dcache_folio flush_dcache_folio
void dma_cache_wback_inv(phys_addr_t start, unsigned long sz);
void dma_cache_inv(phys_addr_t start, unsigned long sz);
void dma_cache_wback(phys_addr_t start, unsigned long sz);
#define flush_dcache_mmap_lock(mapping)		do { } while (0)
#define flush_dcache_mmap_unlock(mapping)	do { } while (0)
#define flush_cache_vmap(start, end)		flush_cache_all()
#define flush_cache_vunmap(start, end)		flush_cache_all()
#define flush_cache_dup_mm(mm)			 
#ifndef CONFIG_ARC_CACHE_VIPT_ALIASING
#define flush_cache_mm(mm)			 
#define flush_cache_range(mm, u_vstart, u_vend)
#define flush_cache_page(vma, u_vaddr, pfn)	 
#else	 
void flush_cache_mm(struct mm_struct *mm);
void flush_cache_range(struct vm_area_struct *vma,
	unsigned long start,unsigned long end);
void flush_cache_page(struct vm_area_struct *vma,
	unsigned long user_addr, unsigned long page);
#define ARCH_HAS_FLUSH_ANON_PAGE
void flush_anon_page(struct vm_area_struct *vma,
	struct page *page, unsigned long u_vaddr);
#endif	 
#define PG_dc_clean	PG_arch_1
#define CACHE_COLORS_NUM	4
#define CACHE_COLORS_MSK	(CACHE_COLORS_NUM - 1)
#define CACHE_COLOR(addr)	(((unsigned long)(addr) >> (PAGE_SHIFT)) & CACHE_COLORS_MSK)
static inline int cache_is_vipt_aliasing(void)
{
	return IS_ENABLED(CONFIG_ARC_CACHE_VIPT_ALIASING);
}
#define addr_not_cache_congruent(addr1, addr2)				\
({									\
	cache_is_vipt_aliasing() ? 					\
		(CACHE_COLOR(addr1) != CACHE_COLOR(addr2)) : 0;		\
})
#define copy_to_user_page(vma, page, vaddr, dst, src, len)		\
do {									\
	memcpy(dst, src, len);						\
	if (vma->vm_flags & VM_EXEC)					\
		__sync_icache_dcache((unsigned long)(dst), vaddr, len);	\
} while (0)
#define copy_from_user_page(vma, page, vaddr, dst, src, len)		\
	memcpy(dst, src, len);						\
#endif
