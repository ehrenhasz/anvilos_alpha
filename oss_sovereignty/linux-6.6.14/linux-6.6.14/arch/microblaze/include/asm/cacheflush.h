#ifndef _ASM_MICROBLAZE_CACHEFLUSH_H
#define _ASM_MICROBLAZE_CACHEFLUSH_H
#include <linux/mm.h>
#include <linux/io.h>
struct scache {
	void (*ie)(void);  
	void (*id)(void);  
	void (*ifl)(void);  
	void (*iflr)(unsigned long a, unsigned long b);
	void (*iin)(void);  
	void (*iinr)(unsigned long a, unsigned long b);
	void (*de)(void);  
	void (*dd)(void);  
	void (*dfl)(void);  
	void (*dflr)(unsigned long a, unsigned long b);
	void (*din)(void);  
	void (*dinr)(unsigned long a, unsigned long b);
};
extern struct scache *mbc;
void microblaze_cache_init(void);
#define enable_icache()					mbc->ie();
#define disable_icache()				mbc->id();
#define flush_icache()					mbc->ifl();
#define flush_icache_range(start, end)			mbc->iflr(start, end);
#define invalidate_icache()				mbc->iin();
#define invalidate_icache_range(start, end)		mbc->iinr(start, end);
#define enable_dcache()					mbc->de();
#define disable_dcache()				mbc->dd();
#define invalidate_dcache()				mbc->din();
#define invalidate_dcache_range(start, end)		mbc->dinr(start, end);
#define flush_dcache()					mbc->dfl();
#define flush_dcache_range(start, end)			mbc->dflr(start, end);
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1
#define flush_dcache_page(page) \
do { \
	unsigned long addr = (unsigned long) page_address(page);   \
	addr = (u32)virt_to_phys((void *)addr); \
	flush_dcache_range((unsigned) (addr), (unsigned) (addr) + PAGE_SIZE); \
} while (0);
static inline void flush_dcache_folio(struct folio *folio)
{
	unsigned long addr = folio_pfn(folio) << PAGE_SHIFT;
	flush_dcache_range(addr, addr + folio_size(folio));
}
#define flush_dcache_folio flush_dcache_folio
#define flush_cache_page(vma, vmaddr, pfn) \
	flush_dcache_range(pfn << PAGE_SHIFT, (pfn << PAGE_SHIFT) + PAGE_SIZE);
static inline void copy_to_user_page(struct vm_area_struct *vma,
				     struct page *page, unsigned long vaddr,
				     void *dst, void *src, int len)
{
	u32 addr = virt_to_phys(dst);
	memcpy(dst, src, len);
	if (vma->vm_flags & VM_EXEC) {
		invalidate_icache_range(addr, addr + PAGE_SIZE);
		flush_dcache_range(addr, addr + PAGE_SIZE);
	}
}
#define copy_to_user_page copy_to_user_page
#include <asm-generic/cacheflush.h>
#endif  
