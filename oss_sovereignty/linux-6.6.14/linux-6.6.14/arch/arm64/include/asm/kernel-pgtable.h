#ifndef __ASM_KERNEL_PGTABLE_H
#define __ASM_KERNEL_PGTABLE_H
#include <asm/boot.h>
#include <asm/pgtable-hwdef.h>
#include <asm/sparsemem.h>
#ifdef CONFIG_ARM64_4K_PAGES
#define SWAPPER_PGTABLE_LEVELS	(CONFIG_PGTABLE_LEVELS - 1)
#else
#define SWAPPER_PGTABLE_LEVELS	(CONFIG_PGTABLE_LEVELS)
#endif
#ifdef CONFIG_RANDOMIZE_BASE
#define EARLY_KASLR	(1)
#else
#define EARLY_KASLR	(0)
#endif
#define SPAN_NR_ENTRIES(vstart, vend, shift) \
	((((vend) - 1) >> (shift)) - ((vstart) >> (shift)) + 1)
#define EARLY_ENTRIES(vstart, vend, shift, add) \
	(SPAN_NR_ENTRIES(vstart, vend, shift) + (add))
#define EARLY_PGDS(vstart, vend, add) (EARLY_ENTRIES(vstart, vend, PGDIR_SHIFT, add))
#if SWAPPER_PGTABLE_LEVELS > 3
#define EARLY_PUDS(vstart, vend, add) (EARLY_ENTRIES(vstart, vend, PUD_SHIFT, add))
#else
#define EARLY_PUDS(vstart, vend, add) (0)
#endif
#if SWAPPER_PGTABLE_LEVELS > 2
#define EARLY_PMDS(vstart, vend, add) (EARLY_ENTRIES(vstart, vend, SWAPPER_TABLE_SHIFT, add))
#else
#define EARLY_PMDS(vstart, vend, add) (0)
#endif
#define EARLY_PAGES(vstart, vend, add) ( 1 			 				\
			+ EARLY_PGDS((vstart), (vend), add) 	 	\
			+ EARLY_PUDS((vstart), (vend), add)	 	\
			+ EARLY_PMDS((vstart), (vend), add))	 
#define INIT_DIR_SIZE (PAGE_SIZE * EARLY_PAGES(KIMAGE_VADDR, _end, EARLY_KASLR))
#if VA_BITS < 48
#define INIT_IDMAP_DIR_SIZE	((INIT_IDMAP_DIR_PAGES + 2) * PAGE_SIZE)
#else
#define INIT_IDMAP_DIR_SIZE	(INIT_IDMAP_DIR_PAGES * PAGE_SIZE)
#endif
#define INIT_IDMAP_DIR_PAGES	EARLY_PAGES(KIMAGE_VADDR, _end + MAX_FDT_SIZE + SWAPPER_BLOCK_SIZE, 1)
#ifdef CONFIG_ARM64_4K_PAGES
#define SWAPPER_BLOCK_SHIFT	PMD_SHIFT
#define SWAPPER_BLOCK_SIZE	PMD_SIZE
#define SWAPPER_TABLE_SHIFT	PUD_SHIFT
#else
#define SWAPPER_BLOCK_SHIFT	PAGE_SHIFT
#define SWAPPER_BLOCK_SIZE	PAGE_SIZE
#define SWAPPER_TABLE_SHIFT	PMD_SHIFT
#endif
#define SWAPPER_PTE_FLAGS	(PTE_TYPE_PAGE | PTE_AF | PTE_SHARED | PTE_UXN)
#define SWAPPER_PMD_FLAGS	(PMD_TYPE_SECT | PMD_SECT_AF | PMD_SECT_S | PTE_UXN)
#ifdef CONFIG_ARM64_4K_PAGES
#define SWAPPER_RW_MMUFLAGS	(PMD_ATTRINDX(MT_NORMAL) | SWAPPER_PMD_FLAGS | PTE_WRITE)
#define SWAPPER_RX_MMUFLAGS	(SWAPPER_RW_MMUFLAGS | PMD_SECT_RDONLY)
#else
#define SWAPPER_RW_MMUFLAGS	(PTE_ATTRINDX(MT_NORMAL) | SWAPPER_PTE_FLAGS | PTE_WRITE)
#define SWAPPER_RX_MMUFLAGS	(SWAPPER_RW_MMUFLAGS | PTE_RDONLY)
#endif
#endif	 
