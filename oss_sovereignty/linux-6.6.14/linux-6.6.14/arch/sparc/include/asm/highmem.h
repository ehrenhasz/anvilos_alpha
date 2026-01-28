#ifndef _ASM_HIGHMEM_H
#define _ASM_HIGHMEM_H
#ifdef __KERNEL__
#include <linux/interrupt.h>
#include <linux/pgtable.h>
#include <asm/vaddrs.h>
#include <asm/pgtsrmmu.h>
extern unsigned long highstart_pfn, highend_pfn;
#define kmap_prot __pgprot(SRMMU_ET_PTE | SRMMU_PRIV | SRMMU_CACHE)
extern pte_t *pkmap_page_table;
#define LAST_PKMAP 1024
#define PKMAP_SIZE (LAST_PKMAP << PAGE_SHIFT)
#define PKMAP_BASE PMD_ALIGN(SRMMU_NOCACHE_VADDR + (SRMMU_MAX_NOCACHE_PAGES << PAGE_SHIFT))
#define LAST_PKMAP_MASK (LAST_PKMAP - 1)
#define PKMAP_NR(virt)  ((virt - PKMAP_BASE) >> PAGE_SHIFT)
#define PKMAP_ADDR(nr)  (PKMAP_BASE + ((nr) << PAGE_SHIFT))
#define PKMAP_END (PKMAP_ADDR(LAST_PKMAP))
#define flush_cache_kmaps()	flush_cache_all()
#define arch_kmap_local_pre_map(vaddr, pteval)	flush_cache_all()
#define arch_kmap_local_pre_unmap(vaddr)	flush_cache_all()
#define arch_kmap_local_post_map(vaddr, pteval)	flush_tlb_all()
#define arch_kmap_local_post_unmap(vaddr)	flush_tlb_all()
#endif  
#endif  
