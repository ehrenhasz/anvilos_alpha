#ifndef _ASM_PGALLOC_H
#define _ASM_PGALLOC_H
#include <asm/mem-layout.h>
#include <asm/atomic.h>
#include <asm-generic/pgalloc.h>
extern unsigned long long kmap_generation;
static inline pgd_t *pgd_alloc(struct mm_struct *mm)
{
	pgd_t *pgd;
	pgd = (pgd_t *)__get_free_page(GFP_KERNEL | __GFP_ZERO);
	memcpy(pgd, swapper_pg_dir, PTRS_PER_PGD*sizeof(pgd_t));
	mm->context.generation = kmap_generation;
	mm->context.ptbase = __pa(pgd);
	return pgd;
}
static inline void pmd_populate(struct mm_struct *mm, pmd_t *pmd,
				pgtable_t pte)
{
	set_pmd(pmd, __pmd(((unsigned long)page_to_pfn(pte) << PAGE_SHIFT) |
		HEXAGON_L1_PTE_SIZE));
}
static inline void pmd_populate_kernel(struct mm_struct *mm, pmd_t *pmd,
				       pte_t *pte)
{
	extern spinlock_t kmap_gen_lock;
	pmd_t *ppmd;
	int pmdindex;
	spin_lock(&kmap_gen_lock);
	kmap_generation++;
	mm->context.generation = kmap_generation;
	current->active_mm->context.generation = kmap_generation;
	spin_unlock(&kmap_gen_lock);
	set_pmd(pmd, __pmd(((unsigned long)__pa(pte)) | HEXAGON_L1_PTE_SIZE));
	pmdindex = (pgd_t *)pmd - mm->pgd;
	ppmd = (pmd_t *)current->active_mm->pgd + pmdindex;
	set_pmd(ppmd, __pmd(((unsigned long)__pa(pte)) | HEXAGON_L1_PTE_SIZE));
	if (pmdindex > max_kernel_seg)
		max_kernel_seg = pmdindex;
}
#define __pte_free_tlb(tlb, pte, addr)				\
do {								\
	pagetable_pte_dtor((page_ptdesc(pte)));			\
	tlb_remove_page_ptdesc((tlb), (page_ptdesc(pte)));	\
} while (0)
#endif
