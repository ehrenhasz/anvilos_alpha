#ifndef _ASM_HIGHMEM_H
#define _ASM_HIGHMEM_H
#ifdef __KERNEL__
#include <linux/interrupt.h>
#include <asm/cacheflush.h>
#include <asm/page.h>
#include <asm/fixmap.h>
extern pte_t *pkmap_page_table;
#ifdef CONFIG_PPC_4K_PAGES
#define PKMAP_ORDER	PTE_SHIFT
#else
#define PKMAP_ORDER	9
#endif
#define LAST_PKMAP	(1 << PKMAP_ORDER)
#ifndef CONFIG_PPC_4K_PAGES
#define PKMAP_BASE	(FIXADDR_START - PAGE_SIZE*(LAST_PKMAP + 1))
#else
#define PKMAP_BASE	((FIXADDR_START - PAGE_SIZE*(LAST_PKMAP + 1)) & PMD_MASK)
#endif
#define LAST_PKMAP_MASK	(LAST_PKMAP-1)
#define PKMAP_NR(virt)  ((virt-PKMAP_BASE) >> PAGE_SHIFT)
#define PKMAP_ADDR(nr)  (PKMAP_BASE + ((nr) << PAGE_SHIFT))
#define flush_cache_kmaps()	flush_cache_all()
#define arch_kmap_local_set_pte(mm, vaddr, ptep, ptev)	\
	__set_pte_at(mm, vaddr, ptep, ptev, 1)
#define arch_kmap_local_post_map(vaddr, pteval)	\
	local_flush_tlb_page(NULL, vaddr)
#define arch_kmap_local_post_unmap(vaddr)	\
	local_flush_tlb_page(NULL, vaddr)
#endif  
#endif  
