#ifndef _ASM_HIGHMEM_H
#define _ASM_HIGHMEM_H
#ifdef __KERNEL__
#include <linux/bug.h>
#include <linux/interrupt.h>
#include <linux/uaccess.h>
#include <asm/cpu-features.h>
#include <asm/kmap_size.h>
extern unsigned long highstart_pfn, highend_pfn;
extern pte_t *pkmap_page_table;
#if defined(CONFIG_PHYS_ADDR_T_64BIT) || defined(CONFIG_MIPS_HUGE_TLB_SUPPORT)
#define LAST_PKMAP 512
#else
#define LAST_PKMAP 1024
#endif
#define LAST_PKMAP_MASK (LAST_PKMAP-1)
#define PKMAP_NR(virt)	((virt-PKMAP_BASE) >> PAGE_SHIFT)
#define PKMAP_ADDR(nr)	(PKMAP_BASE + ((nr) << PAGE_SHIFT))
#define ARCH_HAS_KMAP_FLUSH_TLB
extern void kmap_flush_tlb(unsigned long addr);
#define flush_cache_kmaps()	BUG_ON(cpu_has_dc_aliases)
#define arch_kmap_local_set_pte(mm, vaddr, ptep, ptev)	set_pte(ptep, ptev)
#define arch_kmap_local_post_map(vaddr, pteval)	local_flush_tlb_one(vaddr)
#define arch_kmap_local_post_unmap(vaddr)	local_flush_tlb_one(vaddr)
#endif  
#endif  
