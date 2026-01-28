#ifndef _ASM_CACHEFLUSH_H
#define _ASM_CACHEFLUSH_H
#include <linux/mm.h>
#include <asm/cpu-features.h>
#define PG_dcache_dirty			PG_arch_1
#define folio_test_dcache_dirty(folio)		\
	test_bit(PG_dcache_dirty, &(folio)->flags)
#define folio_set_dcache_dirty(folio)	\
	set_bit(PG_dcache_dirty, &(folio)->flags)
#define folio_clear_dcache_dirty(folio)	\
	clear_bit(PG_dcache_dirty, &(folio)->flags)
extern void (*flush_cache_all)(void);
extern void (*__flush_cache_all)(void);
extern void (*flush_cache_mm)(struct mm_struct *mm);
#define flush_cache_dup_mm(mm)	do { (void) (mm); } while (0)
extern void (*flush_cache_range)(struct vm_area_struct *vma,
	unsigned long start, unsigned long end);
extern void (*flush_cache_page)(struct vm_area_struct *vma, unsigned long page, unsigned long pfn);
extern void __flush_dcache_pages(struct page *page, unsigned int nr);
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1
static inline void flush_dcache_folio(struct folio *folio)
{
	if (cpu_has_dc_aliases)
		__flush_dcache_pages(&folio->page, folio_nr_pages(folio));
	else if (!cpu_has_ic_fills_f_dc)
		folio_set_dcache_dirty(folio);
}
#define flush_dcache_folio flush_dcache_folio
static inline void flush_dcache_page(struct page *page)
{
	if (cpu_has_dc_aliases)
		__flush_dcache_pages(page, 1);
	else if (!cpu_has_ic_fills_f_dc)
		folio_set_dcache_dirty(page_folio(page));
}
#define flush_dcache_mmap_lock(mapping)		do { } while (0)
#define flush_dcache_mmap_unlock(mapping)	do { } while (0)
#define ARCH_HAS_FLUSH_ANON_PAGE
extern void __flush_anon_page(struct page *, unsigned long);
static inline void flush_anon_page(struct vm_area_struct *vma,
	struct page *page, unsigned long vmaddr)
{
	if (cpu_has_dc_aliases && PageAnon(page))
		__flush_anon_page(page, vmaddr);
}
extern void (*flush_icache_range)(unsigned long start, unsigned long end);
extern void (*local_flush_icache_range)(unsigned long start, unsigned long end);
extern void (*__flush_icache_user_range)(unsigned long start,
					 unsigned long end);
extern void (*__local_flush_icache_user_range)(unsigned long start,
					       unsigned long end);
extern void (*__flush_cache_vmap)(void);
static inline void flush_cache_vmap(unsigned long start, unsigned long end)
{
	if (cpu_has_dc_aliases)
		__flush_cache_vmap();
}
extern void (*__flush_cache_vunmap)(void);
static inline void flush_cache_vunmap(unsigned long start, unsigned long end)
{
	if (cpu_has_dc_aliases)
		__flush_cache_vunmap();
}
extern void copy_to_user_page(struct vm_area_struct *vma,
	struct page *page, unsigned long vaddr, void *dst, const void *src,
	unsigned long len);
extern void copy_from_user_page(struct vm_area_struct *vma,
	struct page *page, unsigned long vaddr, void *dst, const void *src,
	unsigned long len);
extern void (*flush_icache_all)(void);
extern void (*flush_data_cache_page)(unsigned long addr);
unsigned long run_uncached(void *func);
extern void *kmap_coherent(struct page *page, unsigned long addr);
extern void kunmap_coherent(void);
extern void *kmap_noncoherent(struct page *page, unsigned long addr);
static inline void kunmap_noncoherent(void)
{
	kunmap_coherent();
}
#define ARCH_IMPLEMENTS_FLUSH_KERNEL_VMAP_RANGE 1
extern void (*__flush_kernel_vmap_range)(unsigned long vaddr, int size);
static inline void flush_kernel_vmap_range(void *vaddr, int size)
{
	if (cpu_has_dc_aliases)
		__flush_kernel_vmap_range((unsigned long) vaddr, size);
}
static inline void invalidate_kernel_vmap_range(void *vaddr, int size)
{
	if (cpu_has_dc_aliases)
		__flush_kernel_vmap_range((unsigned long) vaddr, size);
}
#endif  
