#ifndef _ASM_CACHEFLUSH_H
#define _ASM_CACHEFLUSH_H
#include <linux/mm_types.h>
#define LINESIZE	32
#define LINEBITS	5
extern void flush_dcache_range(unsigned long start, unsigned long end);
#define flush_dcache_range flush_dcache_range
extern void flush_icache_range(unsigned long start, unsigned long end);
#define flush_icache_range flush_icache_range
extern void flush_cache_all_hexagon(void);
static inline void update_mmu_cache_range(struct vm_fault *vmf,
		struct vm_area_struct *vma, unsigned long address,
		pte_t *ptep, unsigned int nr)
{
}
#define update_mmu_cache(vma, addr, ptep) \
	update_mmu_cache_range(NULL, vma, addr, ptep, 1)
void copy_to_user_page(struct vm_area_struct *vma, struct page *page,
		       unsigned long vaddr, void *dst, void *src, int len);
#define copy_to_user_page copy_to_user_page
#define copy_from_user_page(vma, page, vaddr, dst, src, len) \
	memcpy(dst, src, len)
extern void hexagon_inv_dcache_range(unsigned long start, unsigned long end);
extern void hexagon_clean_dcache_range(unsigned long start, unsigned long end);
#include <asm-generic/cacheflush.h>
#endif
