#include <linux/module.h>
#include <linux/signal.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/swap.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/gfp.h>
#include <asm/setup.h>
#include <linux/uaccess.h>
#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/traps.h>
#include <asm/machdep.h>
#include <asm/io.h>
#ifdef CONFIG_ATARI
#include <asm/atari_stram.h>
#endif
#include <asm/sections.h>
#include <asm/tlb.h>
void *empty_zero_page;
EXPORT_SYMBOL(empty_zero_page);
#ifdef CONFIG_MMU
int m68k_virt_to_node_shift;
void __init m68k_setup_node(int node)
{
	node_set_online(node);
}
#else  
void __init paging_init(void)
{
	unsigned long end_mem = memory_end & PAGE_MASK;
	unsigned long max_zone_pfn[MAX_NR_ZONES] = { 0, };
	high_memory = (void *) end_mem;
	empty_zero_page = memblock_alloc(PAGE_SIZE, PAGE_SIZE);
	if (!empty_zero_page)
		panic("%s: Failed to allocate %lu bytes align=0x%lx\n",
		      __func__, PAGE_SIZE, PAGE_SIZE);
	max_zone_pfn[ZONE_DMA] = end_mem >> PAGE_SHIFT;
	free_area_init(max_zone_pfn);
}
#endif  
void free_initmem(void)
{
#ifndef CONFIG_MMU_SUN3
	free_initmem_default(-1);
#endif  
}
#if defined(CONFIG_MMU) && !defined(CONFIG_COLDFIRE)
#define VECTORS	&vectors[0]
#else
#define VECTORS	_ramvec
#endif
static inline void init_pointer_tables(void)
{
#if defined(CONFIG_MMU) && !defined(CONFIG_SUN3) && !defined(CONFIG_COLDFIRE)
	int i, j;
	init_pointer_table(kernel_pg_dir, TABLE_PGD);
	for (i = 0; i < PTRS_PER_PGD; i++) {
		pud_t *pud = (pud_t *)&kernel_pg_dir[i];
		pmd_t *pmd_dir;
		if (!pud_present(*pud))
			continue;
		pmd_dir = (pmd_t *)pgd_page_vaddr(kernel_pg_dir[i]);
		init_pointer_table(pmd_dir, TABLE_PMD);
		for (j = 0; j < PTRS_PER_PMD; j++) {
			pmd_t *pmd = &pmd_dir[j];
			pte_t *pte_dir;
			if (!pmd_present(*pmd))
				continue;
			pte_dir = (pte_t *)pmd_page_vaddr(*pmd);
			init_pointer_table(pte_dir, TABLE_PTE);
		}
	}
#endif
}
void __init mem_init(void)
{
	memblock_free_all();
	init_pointer_tables();
}
