#ifndef _ASM_HIGHMEM_H
#define _ASM_HIGHMEM_H
#ifdef __KERNEL__
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <asm/fixmap.h>
extern pte_t *pkmap_page_table;
#define PKMAP_ORDER	PTE_SHIFT
#define LAST_PKMAP	(1 << PKMAP_ORDER)
#define PKMAP_BASE	((FIXADDR_START - PAGE_SIZE * (LAST_PKMAP + 1)) \
								& PMD_MASK)
#define LAST_PKMAP_MASK	(LAST_PKMAP - 1)
#define PKMAP_NR(virt)  ((virt - PKMAP_BASE) >> PAGE_SHIFT)
#define PKMAP_ADDR(nr)  (PKMAP_BASE + ((nr) << PAGE_SHIFT))
#define flush_cache_kmaps()	{ flush_icache(); flush_dcache(); }
#define arch_kmap_local_post_map(vaddr, pteval)	\
	local_flush_tlb_page(NULL, vaddr);
#define arch_kmap_local_post_unmap(vaddr)	\
	local_flush_tlb_page(NULL, vaddr);
#endif  
#endif  
