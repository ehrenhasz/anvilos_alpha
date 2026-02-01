
#include <linux/pagewalk.h>
#include <linux/hugetlb.h>
#include <linux/bitops.h>
#include <linux/mmu_notifier.h>
#include <linux/mm_inline.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>

 
struct wp_walk {
	struct mmu_notifier_range range;
	unsigned long tlbflush_start;
	unsigned long tlbflush_end;
	unsigned long total;
};

 
static int wp_pte(pte_t *pte, unsigned long addr, unsigned long end,
		  struct mm_walk *walk)
{
	struct wp_walk *wpwalk = walk->private;
	pte_t ptent = ptep_get(pte);

	if (pte_write(ptent)) {
		pte_t old_pte = ptep_modify_prot_start(walk->vma, addr, pte);

		ptent = pte_wrprotect(old_pte);
		ptep_modify_prot_commit(walk->vma, addr, pte, old_pte, ptent);
		wpwalk->total++;
		wpwalk->tlbflush_start = min(wpwalk->tlbflush_start, addr);
		wpwalk->tlbflush_end = max(wpwalk->tlbflush_end,
					   addr + PAGE_SIZE);
	}

	return 0;
}

 
struct clean_walk {
	struct wp_walk base;
	pgoff_t bitmap_pgoff;
	unsigned long *bitmap;
	pgoff_t start;
	pgoff_t end;
};

#define to_clean_walk(_wpwalk) container_of(_wpwalk, struct clean_walk, base)

 
static int clean_record_pte(pte_t *pte, unsigned long addr,
			    unsigned long end, struct mm_walk *walk)
{
	struct wp_walk *wpwalk = walk->private;
	struct clean_walk *cwalk = to_clean_walk(wpwalk);
	pte_t ptent = ptep_get(pte);

	if (pte_dirty(ptent)) {
		pgoff_t pgoff = ((addr - walk->vma->vm_start) >> PAGE_SHIFT) +
			walk->vma->vm_pgoff - cwalk->bitmap_pgoff;
		pte_t old_pte = ptep_modify_prot_start(walk->vma, addr, pte);

		ptent = pte_mkclean(old_pte);
		ptep_modify_prot_commit(walk->vma, addr, pte, old_pte, ptent);

		wpwalk->total++;
		wpwalk->tlbflush_start = min(wpwalk->tlbflush_start, addr);
		wpwalk->tlbflush_end = max(wpwalk->tlbflush_end,
					   addr + PAGE_SIZE);

		__set_bit(pgoff, cwalk->bitmap);
		cwalk->start = min(cwalk->start, pgoff);
		cwalk->end = max(cwalk->end, pgoff + 1);
	}

	return 0;
}

 
static int wp_clean_pmd_entry(pmd_t *pmd, unsigned long addr, unsigned long end,
			      struct mm_walk *walk)
{
	pmd_t pmdval = pmdp_get_lockless(pmd);

	 
	if (pmd_trans_huge(pmdval) || pmd_devmap(pmdval)) {
		WARN_ON(pmd_write(pmdval) || pmd_dirty(pmdval));
		walk->action = ACTION_CONTINUE;
	}
	return 0;
}

 
static int wp_clean_pud_entry(pud_t *pud, unsigned long addr, unsigned long end,
			      struct mm_walk *walk)
{
#ifdef CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD
	pud_t pudval = READ_ONCE(*pud);

	 
	if (pud_trans_huge(pudval) || pud_devmap(pudval)) {
		WARN_ON(pud_write(pudval) || pud_dirty(pudval));
		walk->action = ACTION_CONTINUE;
	}
#endif
	return 0;
}

 
static int wp_clean_pre_vma(unsigned long start, unsigned long end,
			    struct mm_walk *walk)
{
	struct wp_walk *wpwalk = walk->private;

	wpwalk->tlbflush_start = end;
	wpwalk->tlbflush_end = start;

	mmu_notifier_range_init(&wpwalk->range, MMU_NOTIFY_PROTECTION_PAGE, 0,
				walk->mm, start, end);
	mmu_notifier_invalidate_range_start(&wpwalk->range);
	flush_cache_range(walk->vma, start, end);

	 
	inc_tlb_flush_pending(walk->mm);

	return 0;
}

 
static void wp_clean_post_vma(struct mm_walk *walk)
{
	struct wp_walk *wpwalk = walk->private;

	if (mm_tlb_flush_nested(walk->mm))
		flush_tlb_range(walk->vma, wpwalk->range.start,
				wpwalk->range.end);
	else if (wpwalk->tlbflush_end > wpwalk->tlbflush_start)
		flush_tlb_range(walk->vma, wpwalk->tlbflush_start,
				wpwalk->tlbflush_end);

	mmu_notifier_invalidate_range_end(&wpwalk->range);
	dec_tlb_flush_pending(walk->mm);
}

 
static int wp_clean_test_walk(unsigned long start, unsigned long end,
			      struct mm_walk *walk)
{
	unsigned long vm_flags = READ_ONCE(walk->vma->vm_flags);

	 
	if ((vm_flags & (VM_SHARED | VM_MAYWRITE | VM_HUGETLB)) !=
	    (VM_SHARED | VM_MAYWRITE))
		return 1;

	return 0;
}

static const struct mm_walk_ops clean_walk_ops = {
	.pte_entry = clean_record_pte,
	.pmd_entry = wp_clean_pmd_entry,
	.pud_entry = wp_clean_pud_entry,
	.test_walk = wp_clean_test_walk,
	.pre_vma = wp_clean_pre_vma,
	.post_vma = wp_clean_post_vma
};

static const struct mm_walk_ops wp_walk_ops = {
	.pte_entry = wp_pte,
	.pmd_entry = wp_clean_pmd_entry,
	.pud_entry = wp_clean_pud_entry,
	.test_walk = wp_clean_test_walk,
	.pre_vma = wp_clean_pre_vma,
	.post_vma = wp_clean_post_vma
};

 
unsigned long wp_shared_mapping_range(struct address_space *mapping,
				      pgoff_t first_index, pgoff_t nr)
{
	struct wp_walk wpwalk = { .total = 0 };

	i_mmap_lock_read(mapping);
	WARN_ON(walk_page_mapping(mapping, first_index, nr, &wp_walk_ops,
				  &wpwalk));
	i_mmap_unlock_read(mapping);

	return wpwalk.total;
}
EXPORT_SYMBOL_GPL(wp_shared_mapping_range);

 
unsigned long clean_record_shared_mapping_range(struct address_space *mapping,
						pgoff_t first_index, pgoff_t nr,
						pgoff_t bitmap_pgoff,
						unsigned long *bitmap,
						pgoff_t *start,
						pgoff_t *end)
{
	bool none_set = (*start >= *end);
	struct clean_walk cwalk = {
		.base = { .total = 0 },
		.bitmap_pgoff = bitmap_pgoff,
		.bitmap = bitmap,
		.start = none_set ? nr : *start,
		.end = none_set ? 0 : *end,
	};

	i_mmap_lock_read(mapping);
	WARN_ON(walk_page_mapping(mapping, first_index, nr, &clean_walk_ops,
				  &cwalk.base));
	i_mmap_unlock_read(mapping);

	*start = cwalk.start;
	*end = cwalk.end;

	return cwalk.base.total;
}
EXPORT_SYMBOL_GPL(clean_record_shared_mapping_range);
