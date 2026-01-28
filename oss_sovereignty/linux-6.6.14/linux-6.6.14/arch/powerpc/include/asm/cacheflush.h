#ifndef _ASM_POWERPC_CACHEFLUSH_H
#define _ASM_POWERPC_CACHEFLUSH_H
#include <linux/mm.h>
#include <asm/cputable.h>
#include <asm/cpu_has_feature.h>
#define PG_dcache_clean PG_arch_1
#ifdef CONFIG_PPC_BOOK3S_64
static inline void flush_cache_vmap(unsigned long start, unsigned long end)
{
	asm volatile("ptesync" ::: "memory");
}
#define flush_cache_vmap flush_cache_vmap
#endif  
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1
static inline void flush_dcache_folio(struct folio *folio)
{
	if (cpu_has_feature(CPU_FTR_COHERENT_ICACHE))
		return;
	if (test_bit(PG_dcache_clean, &folio->flags))
		clear_bit(PG_dcache_clean, &folio->flags);
}
#define flush_dcache_folio flush_dcache_folio
static inline void flush_dcache_page(struct page *page)
{
	flush_dcache_folio(page_folio(page));
}
void flush_icache_range(unsigned long start, unsigned long stop);
#define flush_icache_range flush_icache_range
void flush_icache_user_page(struct vm_area_struct *vma, struct page *page,
		unsigned long addr, int len);
#define flush_icache_user_page flush_icache_user_page
void flush_dcache_icache_folio(struct folio *folio);
static inline void flush_dcache_range(unsigned long start, unsigned long stop)
{
	unsigned long shift = l1_dcache_shift();
	unsigned long bytes = l1_dcache_bytes();
	void *addr = (void *)(start & ~(bytes - 1));
	unsigned long size = stop - (unsigned long)addr + (bytes - 1);
	unsigned long i;
	if (IS_ENABLED(CONFIG_PPC64))
		mb();	 
	for (i = 0; i < size >> shift; i++, addr += bytes)
		dcbf(addr);
	mb();	 
}
static inline void clean_dcache_range(unsigned long start, unsigned long stop)
{
	unsigned long shift = l1_dcache_shift();
	unsigned long bytes = l1_dcache_bytes();
	void *addr = (void *)(start & ~(bytes - 1));
	unsigned long size = stop - (unsigned long)addr + (bytes - 1);
	unsigned long i;
	for (i = 0; i < size >> shift; i++, addr += bytes)
		dcbst(addr);
	mb();	 
}
static inline void invalidate_dcache_range(unsigned long start,
					   unsigned long stop)
{
	unsigned long shift = l1_dcache_shift();
	unsigned long bytes = l1_dcache_bytes();
	void *addr = (void *)(start & ~(bytes - 1));
	unsigned long size = stop - (unsigned long)addr + (bytes - 1);
	unsigned long i;
	for (i = 0; i < size >> shift; i++, addr += bytes)
		dcbi(addr);
	mb();	 
}
#ifdef CONFIG_4xx
static inline void flush_instruction_cache(void)
{
	iccci((void *)KERNELBASE);
	isync();
}
#else
void flush_instruction_cache(void);
#endif
#include <asm-generic/cacheflush.h>
#endif  
