#ifndef _ASM_RISCV_CACHEFLUSH_H
#define _ASM_RISCV_CACHEFLUSH_H
#include <linux/mm.h>
static inline void local_flush_icache_all(void)
{
	asm volatile ("fence.i" ::: "memory");
}
#define PG_dcache_clean PG_arch_1
static inline void flush_dcache_folio(struct folio *folio)
{
	if (test_bit(PG_dcache_clean, &folio->flags))
		clear_bit(PG_dcache_clean, &folio->flags);
}
#define flush_dcache_folio flush_dcache_folio
#define ARCH_IMPLEMENTS_FLUSH_DCACHE_PAGE 1
static inline void flush_dcache_page(struct page *page)
{
	flush_dcache_folio(page_folio(page));
}
#define flush_icache_range(start, end) flush_icache_all()
#define flush_icache_user_page(vma, pg, addr, len) \
	flush_icache_mm(vma->vm_mm, 0)
#ifdef CONFIG_64BIT
#define flush_cache_vmap(start, end)	flush_tlb_kernel_range(start, end)
#endif
#ifndef CONFIG_SMP
#define flush_icache_all() local_flush_icache_all()
#define flush_icache_mm(mm, local) flush_icache_all()
#else  
void flush_icache_all(void);
void flush_icache_mm(struct mm_struct *mm, bool local);
#endif  
extern unsigned int riscv_cbom_block_size;
extern unsigned int riscv_cboz_block_size;
void riscv_init_cbo_blocksizes(void);
#ifdef CONFIG_RISCV_DMA_NONCOHERENT
void riscv_noncoherent_supported(void);
void __init riscv_set_dma_cache_alignment(void);
#else
static inline void riscv_noncoherent_supported(void) {}
static inline void riscv_set_dma_cache_alignment(void) {}
#endif
#define SYS_RISCV_FLUSH_ICACHE_LOCAL 1UL
#define SYS_RISCV_FLUSH_ICACHE_ALL   (SYS_RISCV_FLUSH_ICACHE_LOCAL)
#include <asm-generic/cacheflush.h>
#endif  
