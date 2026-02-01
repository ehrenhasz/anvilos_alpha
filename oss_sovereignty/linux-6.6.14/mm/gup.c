
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/spinlock.h>

#include <linux/mm.h>
#include <linux/memremap.h>
#include <linux/pagemap.h>
#include <linux/rmap.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/secretmem.h>

#include <linux/sched/signal.h>
#include <linux/rwsem.h>
#include <linux/hugetlb.h>
#include <linux/migrate.h>
#include <linux/mm_inline.h>
#include <linux/sched/mm.h>
#include <linux/shmem_fs.h>

#include <asm/mmu_context.h>
#include <asm/tlbflush.h>

#include "internal.h"

struct follow_page_context {
	struct dev_pagemap *pgmap;
	unsigned int page_mask;
};

static inline void sanity_check_pinned_pages(struct page **pages,
					     unsigned long npages)
{
	if (!IS_ENABLED(CONFIG_DEBUG_VM))
		return;

	 
	for (; npages; npages--, pages++) {
		struct page *page = *pages;
		struct folio *folio = page_folio(page);

		if (is_zero_page(page) ||
		    !folio_test_anon(folio))
			continue;
		if (!folio_test_large(folio) || folio_test_hugetlb(folio))
			VM_BUG_ON_PAGE(!PageAnonExclusive(&folio->page), page);
		else
			 
			VM_BUG_ON_PAGE(!PageAnonExclusive(&folio->page) &&
				       !PageAnonExclusive(page), page);
	}
}

 
static inline struct folio *try_get_folio(struct page *page, int refs)
{
	struct folio *folio;

retry:
	folio = page_folio(page);
	if (WARN_ON_ONCE(folio_ref_count(folio) < 0))
		return NULL;
	if (unlikely(!folio_ref_try_add_rcu(folio, refs)))
		return NULL;

	 
	if (unlikely(page_folio(page) != folio)) {
		if (!put_devmap_managed_page_refs(&folio->page, refs))
			folio_put_refs(folio, refs);
		goto retry;
	}

	return folio;
}

 
struct folio *try_grab_folio(struct page *page, int refs, unsigned int flags)
{
	struct folio *folio;

	if (WARN_ON_ONCE((flags & (FOLL_GET | FOLL_PIN)) == 0))
		return NULL;

	if (unlikely(!(flags & FOLL_PCI_P2PDMA) && is_pci_p2pdma_page(page)))
		return NULL;

	if (flags & FOLL_GET)
		return try_get_folio(page, refs);

	 

	 
	if (is_zero_page(page))
		return page_folio(page);

	folio = try_get_folio(page, refs);
	if (!folio)
		return NULL;

	 
	if (unlikely((flags & FOLL_LONGTERM) &&
		     !folio_is_longterm_pinnable(folio))) {
		if (!put_devmap_managed_page_refs(&folio->page, refs))
			folio_put_refs(folio, refs);
		return NULL;
	}

	 
	if (folio_test_large(folio))
		atomic_add(refs, &folio->_pincount);
	else
		folio_ref_add(folio,
				refs * (GUP_PIN_COUNTING_BIAS - 1));
	 
	smp_mb__after_atomic();

	node_stat_mod_folio(folio, NR_FOLL_PIN_ACQUIRED, refs);

	return folio;
}

static void gup_put_folio(struct folio *folio, int refs, unsigned int flags)
{
	if (flags & FOLL_PIN) {
		if (is_zero_folio(folio))
			return;
		node_stat_mod_folio(folio, NR_FOLL_PIN_RELEASED, refs);
		if (folio_test_large(folio))
			atomic_sub(refs, &folio->_pincount);
		else
			refs *= GUP_PIN_COUNTING_BIAS;
	}

	if (!put_devmap_managed_page_refs(&folio->page, refs))
		folio_put_refs(folio, refs);
}

 
int __must_check try_grab_page(struct page *page, unsigned int flags)
{
	struct folio *folio = page_folio(page);

	if (WARN_ON_ONCE(folio_ref_count(folio) <= 0))
		return -ENOMEM;

	if (unlikely(!(flags & FOLL_PCI_P2PDMA) && is_pci_p2pdma_page(page)))
		return -EREMOTEIO;

	if (flags & FOLL_GET)
		folio_ref_inc(folio);
	else if (flags & FOLL_PIN) {
		 
		if (is_zero_page(page))
			return 0;

		 
		if (folio_test_large(folio)) {
			folio_ref_add(folio, 1);
			atomic_add(1, &folio->_pincount);
		} else {
			folio_ref_add(folio, GUP_PIN_COUNTING_BIAS);
		}

		node_stat_mod_folio(folio, NR_FOLL_PIN_ACQUIRED, 1);
	}

	return 0;
}

 
void unpin_user_page(struct page *page)
{
	sanity_check_pinned_pages(&page, 1);
	gup_put_folio(page_folio(page), 1, FOLL_PIN);
}
EXPORT_SYMBOL(unpin_user_page);

 
void folio_add_pin(struct folio *folio)
{
	if (is_zero_folio(folio))
		return;

	 
	if (folio_test_large(folio)) {
		WARN_ON_ONCE(atomic_read(&folio->_pincount) < 1);
		folio_ref_inc(folio);
		atomic_inc(&folio->_pincount);
	} else {
		WARN_ON_ONCE(folio_ref_count(folio) < GUP_PIN_COUNTING_BIAS);
		folio_ref_add(folio, GUP_PIN_COUNTING_BIAS);
	}
}

static inline struct folio *gup_folio_range_next(struct page *start,
		unsigned long npages, unsigned long i, unsigned int *ntails)
{
	struct page *next = nth_page(start, i);
	struct folio *folio = page_folio(next);
	unsigned int nr = 1;

	if (folio_test_large(folio))
		nr = min_t(unsigned int, npages - i,
			   folio_nr_pages(folio) - folio_page_idx(folio, next));

	*ntails = nr;
	return folio;
}

static inline struct folio *gup_folio_next(struct page **list,
		unsigned long npages, unsigned long i, unsigned int *ntails)
{
	struct folio *folio = page_folio(list[i]);
	unsigned int nr;

	for (nr = i + 1; nr < npages; nr++) {
		if (page_folio(list[nr]) != folio)
			break;
	}

	*ntails = nr - i;
	return folio;
}

 
void unpin_user_pages_dirty_lock(struct page **pages, unsigned long npages,
				 bool make_dirty)
{
	unsigned long i;
	struct folio *folio;
	unsigned int nr;

	if (!make_dirty) {
		unpin_user_pages(pages, npages);
		return;
	}

	sanity_check_pinned_pages(pages, npages);
	for (i = 0; i < npages; i += nr) {
		folio = gup_folio_next(pages, npages, i, &nr);
		 
		if (!folio_test_dirty(folio)) {
			folio_lock(folio);
			folio_mark_dirty(folio);
			folio_unlock(folio);
		}
		gup_put_folio(folio, nr, FOLL_PIN);
	}
}
EXPORT_SYMBOL(unpin_user_pages_dirty_lock);

 
void unpin_user_page_range_dirty_lock(struct page *page, unsigned long npages,
				      bool make_dirty)
{
	unsigned long i;
	struct folio *folio;
	unsigned int nr;

	for (i = 0; i < npages; i += nr) {
		folio = gup_folio_range_next(page, npages, i, &nr);
		if (make_dirty && !folio_test_dirty(folio)) {
			folio_lock(folio);
			folio_mark_dirty(folio);
			folio_unlock(folio);
		}
		gup_put_folio(folio, nr, FOLL_PIN);
	}
}
EXPORT_SYMBOL(unpin_user_page_range_dirty_lock);

static void unpin_user_pages_lockless(struct page **pages, unsigned long npages)
{
	unsigned long i;
	struct folio *folio;
	unsigned int nr;

	 
	for (i = 0; i < npages; i += nr) {
		folio = gup_folio_next(pages, npages, i, &nr);
		gup_put_folio(folio, nr, FOLL_PIN);
	}
}

 
void unpin_user_pages(struct page **pages, unsigned long npages)
{
	unsigned long i;
	struct folio *folio;
	unsigned int nr;

	 
	if (WARN_ON(IS_ERR_VALUE(npages)))
		return;

	sanity_check_pinned_pages(pages, npages);
	for (i = 0; i < npages; i += nr) {
		folio = gup_folio_next(pages, npages, i, &nr);
		gup_put_folio(folio, nr, FOLL_PIN);
	}
}
EXPORT_SYMBOL(unpin_user_pages);

 
static inline void mm_set_has_pinned_flag(unsigned long *mm_flags)
{
	if (!test_bit(MMF_HAS_PINNED, mm_flags))
		set_bit(MMF_HAS_PINNED, mm_flags);
}

#ifdef CONFIG_MMU
static struct page *no_page_table(struct vm_area_struct *vma,
		unsigned int flags)
{
	 
	if ((flags & FOLL_DUMP) &&
			(vma_is_anonymous(vma) || !vma->vm_ops->fault))
		return ERR_PTR(-EFAULT);
	return NULL;
}

static int follow_pfn_pte(struct vm_area_struct *vma, unsigned long address,
		pte_t *pte, unsigned int flags)
{
	if (flags & FOLL_TOUCH) {
		pte_t orig_entry = ptep_get(pte);
		pte_t entry = orig_entry;

		if (flags & FOLL_WRITE)
			entry = pte_mkdirty(entry);
		entry = pte_mkyoung(entry);

		if (!pte_same(orig_entry, entry)) {
			set_pte_at(vma->vm_mm, address, pte, entry);
			update_mmu_cache(vma, address, pte);
		}
	}

	 
	return -EEXIST;
}

 
static inline bool can_follow_write_pte(pte_t pte, struct page *page,
					struct vm_area_struct *vma,
					unsigned int flags)
{
	 
	if (pte_write(pte))
		return true;

	 
	if (!(flags & FOLL_FORCE))
		return false;

	 
	if (vma->vm_flags & (VM_MAYSHARE | VM_SHARED))
		return false;

	 
	if (!(vma->vm_flags & VM_MAYWRITE))
		return false;

	 
	if (vma->vm_flags & VM_WRITE)
		return false;

	 
	if (!page || !PageAnon(page) || !PageAnonExclusive(page))
		return false;

	 
	if (vma_soft_dirty_enabled(vma) && !pte_soft_dirty(pte))
		return false;
	return !userfaultfd_pte_wp(vma, pte);
}

static struct page *follow_page_pte(struct vm_area_struct *vma,
		unsigned long address, pmd_t *pmd, unsigned int flags,
		struct dev_pagemap **pgmap)
{
	struct mm_struct *mm = vma->vm_mm;
	struct page *page;
	spinlock_t *ptl;
	pte_t *ptep, pte;
	int ret;

	 
	if (WARN_ON_ONCE((flags & (FOLL_PIN | FOLL_GET)) ==
			 (FOLL_PIN | FOLL_GET)))
		return ERR_PTR(-EINVAL);

	ptep = pte_offset_map_lock(mm, pmd, address, &ptl);
	if (!ptep)
		return no_page_table(vma, flags);
	pte = ptep_get(ptep);
	if (!pte_present(pte))
		goto no_page;
	if (pte_protnone(pte) && !gup_can_follow_protnone(vma, flags))
		goto no_page;

	page = vm_normal_page(vma, address, pte);

	 
	if ((flags & FOLL_WRITE) &&
	    !can_follow_write_pte(pte, page, vma, flags)) {
		page = NULL;
		goto out;
	}

	if (!page && pte_devmap(pte) && (flags & (FOLL_GET | FOLL_PIN))) {
		 
		*pgmap = get_dev_pagemap(pte_pfn(pte), *pgmap);
		if (*pgmap)
			page = pte_page(pte);
		else
			goto no_page;
	} else if (unlikely(!page)) {
		if (flags & FOLL_DUMP) {
			 
			page = ERR_PTR(-EFAULT);
			goto out;
		}

		if (is_zero_pfn(pte_pfn(pte))) {
			page = pte_page(pte);
		} else {
			ret = follow_pfn_pte(vma, address, ptep, flags);
			page = ERR_PTR(ret);
			goto out;
		}
	}

	if (!pte_write(pte) && gup_must_unshare(vma, flags, page)) {
		page = ERR_PTR(-EMLINK);
		goto out;
	}

	VM_BUG_ON_PAGE((flags & FOLL_PIN) && PageAnon(page) &&
		       !PageAnonExclusive(page), page);

	 
	ret = try_grab_page(page, flags);
	if (unlikely(ret)) {
		page = ERR_PTR(ret);
		goto out;
	}

	 
	if (flags & FOLL_PIN) {
		ret = arch_make_page_accessible(page);
		if (ret) {
			unpin_user_page(page);
			page = ERR_PTR(ret);
			goto out;
		}
	}
	if (flags & FOLL_TOUCH) {
		if ((flags & FOLL_WRITE) &&
		    !pte_dirty(pte) && !PageDirty(page))
			set_page_dirty(page);
		 
		mark_page_accessed(page);
	}
out:
	pte_unmap_unlock(ptep, ptl);
	return page;
no_page:
	pte_unmap_unlock(ptep, ptl);
	if (!pte_none(pte))
		return NULL;
	return no_page_table(vma, flags);
}

static struct page *follow_pmd_mask(struct vm_area_struct *vma,
				    unsigned long address, pud_t *pudp,
				    unsigned int flags,
				    struct follow_page_context *ctx)
{
	pmd_t *pmd, pmdval;
	spinlock_t *ptl;
	struct page *page;
	struct mm_struct *mm = vma->vm_mm;

	pmd = pmd_offset(pudp, address);
	pmdval = pmdp_get_lockless(pmd);
	if (pmd_none(pmdval))
		return no_page_table(vma, flags);
	if (!pmd_present(pmdval))
		return no_page_table(vma, flags);
	if (pmd_devmap(pmdval)) {
		ptl = pmd_lock(mm, pmd);
		page = follow_devmap_pmd(vma, address, pmd, flags, &ctx->pgmap);
		spin_unlock(ptl);
		if (page)
			return page;
	}
	if (likely(!pmd_trans_huge(pmdval)))
		return follow_page_pte(vma, address, pmd, flags, &ctx->pgmap);

	if (pmd_protnone(pmdval) && !gup_can_follow_protnone(vma, flags))
		return no_page_table(vma, flags);

	ptl = pmd_lock(mm, pmd);
	if (unlikely(!pmd_present(*pmd))) {
		spin_unlock(ptl);
		return no_page_table(vma, flags);
	}
	if (unlikely(!pmd_trans_huge(*pmd))) {
		spin_unlock(ptl);
		return follow_page_pte(vma, address, pmd, flags, &ctx->pgmap);
	}
	if (flags & FOLL_SPLIT_PMD) {
		spin_unlock(ptl);
		split_huge_pmd(vma, pmd, address);
		 
		return pte_alloc(mm, pmd) ? ERR_PTR(-ENOMEM) :
			follow_page_pte(vma, address, pmd, flags, &ctx->pgmap);
	}
	page = follow_trans_huge_pmd(vma, address, pmd, flags);
	spin_unlock(ptl);
	ctx->page_mask = HPAGE_PMD_NR - 1;
	return page;
}

static struct page *follow_pud_mask(struct vm_area_struct *vma,
				    unsigned long address, p4d_t *p4dp,
				    unsigned int flags,
				    struct follow_page_context *ctx)
{
	pud_t *pud;
	spinlock_t *ptl;
	struct page *page;
	struct mm_struct *mm = vma->vm_mm;

	pud = pud_offset(p4dp, address);
	if (pud_none(*pud))
		return no_page_table(vma, flags);
	if (pud_devmap(*pud)) {
		ptl = pud_lock(mm, pud);
		page = follow_devmap_pud(vma, address, pud, flags, &ctx->pgmap);
		spin_unlock(ptl);
		if (page)
			return page;
	}
	if (unlikely(pud_bad(*pud)))
		return no_page_table(vma, flags);

	return follow_pmd_mask(vma, address, pud, flags, ctx);
}

static struct page *follow_p4d_mask(struct vm_area_struct *vma,
				    unsigned long address, pgd_t *pgdp,
				    unsigned int flags,
				    struct follow_page_context *ctx)
{
	p4d_t *p4d;

	p4d = p4d_offset(pgdp, address);
	if (p4d_none(*p4d))
		return no_page_table(vma, flags);
	BUILD_BUG_ON(p4d_huge(*p4d));
	if (unlikely(p4d_bad(*p4d)))
		return no_page_table(vma, flags);

	return follow_pud_mask(vma, address, p4d, flags, ctx);
}

 
static struct page *follow_page_mask(struct vm_area_struct *vma,
			      unsigned long address, unsigned int flags,
			      struct follow_page_context *ctx)
{
	pgd_t *pgd;
	struct mm_struct *mm = vma->vm_mm;

	ctx->page_mask = 0;

	 
	if (is_vm_hugetlb_page(vma))
		return hugetlb_follow_page_mask(vma, address, flags,
						&ctx->page_mask);

	pgd = pgd_offset(mm, address);

	if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd)))
		return no_page_table(vma, flags);

	return follow_p4d_mask(vma, address, pgd, flags, ctx);
}

struct page *follow_page(struct vm_area_struct *vma, unsigned long address,
			 unsigned int foll_flags)
{
	struct follow_page_context ctx = { NULL };
	struct page *page;

	if (vma_is_secretmem(vma))
		return NULL;

	if (WARN_ON_ONCE(foll_flags & FOLL_PIN))
		return NULL;

	 
	page = follow_page_mask(vma, address, foll_flags, &ctx);
	if (ctx.pgmap)
		put_dev_pagemap(ctx.pgmap);
	return page;
}

static int get_gate_page(struct mm_struct *mm, unsigned long address,
		unsigned int gup_flags, struct vm_area_struct **vma,
		struct page **page)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;
	pte_t entry;
	int ret = -EFAULT;

	 
	if (gup_flags & FOLL_WRITE)
		return -EFAULT;
	if (address > TASK_SIZE)
		pgd = pgd_offset_k(address);
	else
		pgd = pgd_offset_gate(mm, address);
	if (pgd_none(*pgd))
		return -EFAULT;
	p4d = p4d_offset(pgd, address);
	if (p4d_none(*p4d))
		return -EFAULT;
	pud = pud_offset(p4d, address);
	if (pud_none(*pud))
		return -EFAULT;
	pmd = pmd_offset(pud, address);
	if (!pmd_present(*pmd))
		return -EFAULT;
	pte = pte_offset_map(pmd, address);
	if (!pte)
		return -EFAULT;
	entry = ptep_get(pte);
	if (pte_none(entry))
		goto unmap;
	*vma = get_gate_vma(mm);
	if (!page)
		goto out;
	*page = vm_normal_page(*vma, address, entry);
	if (!*page) {
		if ((gup_flags & FOLL_DUMP) || !is_zero_pfn(pte_pfn(entry)))
			goto unmap;
		*page = pte_page(entry);
	}
	ret = try_grab_page(*page, gup_flags);
	if (unlikely(ret))
		goto unmap;
out:
	ret = 0;
unmap:
	pte_unmap(pte);
	return ret;
}

 
static int faultin_page(struct vm_area_struct *vma,
		unsigned long address, unsigned int *flags, bool unshare,
		int *locked)
{
	unsigned int fault_flags = 0;
	vm_fault_t ret;

	if (*flags & FOLL_NOFAULT)
		return -EFAULT;
	if (*flags & FOLL_WRITE)
		fault_flags |= FAULT_FLAG_WRITE;
	if (*flags & FOLL_REMOTE)
		fault_flags |= FAULT_FLAG_REMOTE;
	if (*flags & FOLL_UNLOCKABLE) {
		fault_flags |= FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_KILLABLE;
		 
		if (*flags & FOLL_INTERRUPTIBLE)
			fault_flags |= FAULT_FLAG_INTERRUPTIBLE;
	}
	if (*flags & FOLL_NOWAIT)
		fault_flags |= FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_RETRY_NOWAIT;
	if (*flags & FOLL_TRIED) {
		 
		fault_flags |= FAULT_FLAG_TRIED;
	}
	if (unshare) {
		fault_flags |= FAULT_FLAG_UNSHARE;
		 
		VM_BUG_ON(fault_flags & FAULT_FLAG_WRITE);
	}

	ret = handle_mm_fault(vma, address, fault_flags, NULL);

	if (ret & VM_FAULT_COMPLETED) {
		 
		WARN_ON_ONCE(fault_flags & FAULT_FLAG_RETRY_NOWAIT);
		*locked = 0;

		 
		return -EAGAIN;
	}

	if (ret & VM_FAULT_ERROR) {
		int err = vm_fault_to_errno(ret, *flags);

		if (err)
			return err;
		BUG();
	}

	if (ret & VM_FAULT_RETRY) {
		if (!(fault_flags & FAULT_FLAG_RETRY_NOWAIT))
			*locked = 0;
		return -EBUSY;
	}

	return 0;
}

 
static bool writable_file_mapping_allowed(struct vm_area_struct *vma,
					  unsigned long gup_flags)
{
	 
	if ((gup_flags & (FOLL_PIN | FOLL_LONGTERM)) !=
	    (FOLL_PIN | FOLL_LONGTERM))
		return true;

	 
	return !vma_needs_dirty_tracking(vma);
}

static int check_vma_flags(struct vm_area_struct *vma, unsigned long gup_flags)
{
	vm_flags_t vm_flags = vma->vm_flags;
	int write = (gup_flags & FOLL_WRITE);
	int foreign = (gup_flags & FOLL_REMOTE);
	bool vma_anon = vma_is_anonymous(vma);

	if (vm_flags & (VM_IO | VM_PFNMAP))
		return -EFAULT;

	if ((gup_flags & FOLL_ANON) && !vma_anon)
		return -EFAULT;

	if ((gup_flags & FOLL_LONGTERM) && vma_is_fsdax(vma))
		return -EOPNOTSUPP;

	if (vma_is_secretmem(vma))
		return -EFAULT;

	if (write) {
		if (!vma_anon &&
		    !writable_file_mapping_allowed(vma, gup_flags))
			return -EFAULT;

		if (!(vm_flags & VM_WRITE) || (vm_flags & VM_SHADOW_STACK)) {
			if (!(gup_flags & FOLL_FORCE))
				return -EFAULT;
			 
			if (is_vm_hugetlb_page(vma))
				return -EFAULT;
			 
			if (!is_cow_mapping(vm_flags))
				return -EFAULT;
		}
	} else if (!(vm_flags & VM_READ)) {
		if (!(gup_flags & FOLL_FORCE))
			return -EFAULT;
		 
		if (!(vm_flags & VM_MAYREAD))
			return -EFAULT;
	}
	 
	if (!arch_vma_access_permitted(vma, write, false, foreign))
		return -EFAULT;
	return 0;
}

 
static struct vm_area_struct *gup_vma_lookup(struct mm_struct *mm,
	 unsigned long addr)
{
#ifdef CONFIG_STACK_GROWSUP
	return vma_lookup(mm, addr);
#else
	static volatile unsigned long next_warn;
	struct vm_area_struct *vma;
	unsigned long now, next;

	vma = find_vma(mm, addr);
	if (!vma || (addr >= vma->vm_start))
		return vma;

	 
	if (!(vma->vm_flags & VM_GROWSDOWN))
		return NULL;
	if (vma->vm_start - addr > 65536)
		return NULL;

	 
	now = jiffies; next = next_warn;
	if (next && time_before(now, next))
		return NULL;
	next_warn = now + 60*60*HZ;

	 
	pr_warn("GUP no longer grows the stack in %s (%d): %lx-%lx (%lx)\n",
		current->comm, task_pid_nr(current),
		vma->vm_start, vma->vm_end, addr);
	dump_stack();
	return NULL;
#endif
}

 
static long __get_user_pages(struct mm_struct *mm,
		unsigned long start, unsigned long nr_pages,
		unsigned int gup_flags, struct page **pages,
		int *locked)
{
	long ret = 0, i = 0;
	struct vm_area_struct *vma = NULL;
	struct follow_page_context ctx = { NULL };

	if (!nr_pages)
		return 0;

	start = untagged_addr_remote(mm, start);

	VM_BUG_ON(!!pages != !!(gup_flags & (FOLL_GET | FOLL_PIN)));

	do {
		struct page *page;
		unsigned int foll_flags = gup_flags;
		unsigned int page_increm;

		 
		if (!vma || start >= vma->vm_end) {
			vma = gup_vma_lookup(mm, start);
			if (!vma && in_gate_area(mm, start)) {
				ret = get_gate_page(mm, start & PAGE_MASK,
						gup_flags, &vma,
						pages ? &page : NULL);
				if (ret)
					goto out;
				ctx.page_mask = 0;
				goto next_page;
			}

			if (!vma) {
				ret = -EFAULT;
				goto out;
			}
			ret = check_vma_flags(vma, gup_flags);
			if (ret)
				goto out;
		}
retry:
		 
		if (fatal_signal_pending(current)) {
			ret = -EINTR;
			goto out;
		}
		cond_resched();

		page = follow_page_mask(vma, start, foll_flags, &ctx);
		if (!page || PTR_ERR(page) == -EMLINK) {
			ret = faultin_page(vma, start, &foll_flags,
					   PTR_ERR(page) == -EMLINK, locked);
			switch (ret) {
			case 0:
				goto retry;
			case -EBUSY:
			case -EAGAIN:
				ret = 0;
				fallthrough;
			case -EFAULT:
			case -ENOMEM:
			case -EHWPOISON:
				goto out;
			}
			BUG();
		} else if (PTR_ERR(page) == -EEXIST) {
			 
			if (pages) {
				ret = PTR_ERR(page);
				goto out;
			}
		} else if (IS_ERR(page)) {
			ret = PTR_ERR(page);
			goto out;
		}
next_page:
		page_increm = 1 + (~(start >> PAGE_SHIFT) & ctx.page_mask);
		if (page_increm > nr_pages)
			page_increm = nr_pages;

		if (pages) {
			struct page *subpage;
			unsigned int j;

			 
			if (page_increm > 1) {
				struct folio *folio;

				 
				folio = try_grab_folio(page, page_increm - 1,
						       foll_flags);
				if (WARN_ON_ONCE(!folio)) {
					 
					gup_put_folio(page_folio(page), 1,
						      foll_flags);
					ret = -EFAULT;
					goto out;
				}
			}

			for (j = 0; j < page_increm; j++) {
				subpage = nth_page(page, j);
				pages[i + j] = subpage;
				flush_anon_page(vma, subpage, start + j * PAGE_SIZE);
				flush_dcache_page(subpage);
			}
		}

		i += page_increm;
		start += page_increm * PAGE_SIZE;
		nr_pages -= page_increm;
	} while (nr_pages);
out:
	if (ctx.pgmap)
		put_dev_pagemap(ctx.pgmap);
	return i ? i : ret;
}

static bool vma_permits_fault(struct vm_area_struct *vma,
			      unsigned int fault_flags)
{
	bool write   = !!(fault_flags & FAULT_FLAG_WRITE);
	bool foreign = !!(fault_flags & FAULT_FLAG_REMOTE);
	vm_flags_t vm_flags = write ? VM_WRITE : VM_READ;

	if (!(vm_flags & vma->vm_flags))
		return false;

	 
	if (!arch_vma_access_permitted(vma, write, false, foreign))
		return false;

	return true;
}

 
int fixup_user_fault(struct mm_struct *mm,
		     unsigned long address, unsigned int fault_flags,
		     bool *unlocked)
{
	struct vm_area_struct *vma;
	vm_fault_t ret;

	address = untagged_addr_remote(mm, address);

	if (unlocked)
		fault_flags |= FAULT_FLAG_ALLOW_RETRY | FAULT_FLAG_KILLABLE;

retry:
	vma = gup_vma_lookup(mm, address);
	if (!vma)
		return -EFAULT;

	if (!vma_permits_fault(vma, fault_flags))
		return -EFAULT;

	if ((fault_flags & FAULT_FLAG_KILLABLE) &&
	    fatal_signal_pending(current))
		return -EINTR;

	ret = handle_mm_fault(vma, address, fault_flags, NULL);

	if (ret & VM_FAULT_COMPLETED) {
		 
		mmap_read_lock(mm);
		*unlocked = true;
		return 0;
	}

	if (ret & VM_FAULT_ERROR) {
		int err = vm_fault_to_errno(ret, 0);

		if (err)
			return err;
		BUG();
	}

	if (ret & VM_FAULT_RETRY) {
		mmap_read_lock(mm);
		*unlocked = true;
		fault_flags |= FAULT_FLAG_TRIED;
		goto retry;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(fixup_user_fault);

 
static bool gup_signal_pending(unsigned int flags)
{
	if (fatal_signal_pending(current))
		return true;

	if (!(flags & FOLL_INTERRUPTIBLE))
		return false;

	return signal_pending(current);
}

 
static __always_inline long __get_user_pages_locked(struct mm_struct *mm,
						unsigned long start,
						unsigned long nr_pages,
						struct page **pages,
						int *locked,
						unsigned int flags)
{
	long ret, pages_done;
	bool must_unlock = false;

	 
	if (!*locked) {
		if (mmap_read_lock_killable(mm))
			return -EAGAIN;
		must_unlock = true;
		*locked = 1;
	}
	else
		mmap_assert_locked(mm);

	if (flags & FOLL_PIN)
		mm_set_has_pinned_flag(&mm->flags);

	 
	if (pages && !(flags & FOLL_PIN))
		flags |= FOLL_GET;

	pages_done = 0;
	for (;;) {
		ret = __get_user_pages(mm, start, nr_pages, flags, pages,
				       locked);
		if (!(flags & FOLL_UNLOCKABLE)) {
			 
			pages_done = ret;
			break;
		}

		 
		if (!*locked) {
			BUG_ON(ret < 0);
			BUG_ON(ret >= nr_pages);
		}

		if (ret > 0) {
			nr_pages -= ret;
			pages_done += ret;
			if (!nr_pages)
				break;
		}
		if (*locked) {
			 
			if (!pages_done)
				pages_done = ret;
			break;
		}
		 
		if (likely(pages))
			pages += ret;
		start += ret << PAGE_SHIFT;

		 
		must_unlock = true;

retry:
		 
		if (gup_signal_pending(flags)) {
			if (!pages_done)
				pages_done = -EINTR;
			break;
		}

		ret = mmap_read_lock_killable(mm);
		if (ret) {
			BUG_ON(ret > 0);
			if (!pages_done)
				pages_done = ret;
			break;
		}

		*locked = 1;
		ret = __get_user_pages(mm, start, 1, flags | FOLL_TRIED,
				       pages, locked);
		if (!*locked) {
			 
			BUG_ON(ret != 0);
			goto retry;
		}
		if (ret != 1) {
			BUG_ON(ret > 1);
			if (!pages_done)
				pages_done = ret;
			break;
		}
		nr_pages--;
		pages_done++;
		if (!nr_pages)
			break;
		if (likely(pages))
			pages++;
		start += PAGE_SIZE;
	}
	if (must_unlock && *locked) {
		 
		mmap_read_unlock(mm);
		*locked = 0;
	}
	return pages_done;
}

 
long populate_vma_page_range(struct vm_area_struct *vma,
		unsigned long start, unsigned long end, int *locked)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long nr_pages = (end - start) / PAGE_SIZE;
	int local_locked = 1;
	int gup_flags;
	long ret;

	VM_BUG_ON(!PAGE_ALIGNED(start));
	VM_BUG_ON(!PAGE_ALIGNED(end));
	VM_BUG_ON_VMA(start < vma->vm_start, vma);
	VM_BUG_ON_VMA(end   > vma->vm_end, vma);
	mmap_assert_locked(mm);

	 
	if (vma->vm_flags & VM_LOCKONFAULT)
		return nr_pages;

	gup_flags = FOLL_TOUCH;
	 
	if ((vma->vm_flags & (VM_WRITE | VM_SHARED)) == VM_WRITE)
		gup_flags |= FOLL_WRITE;

	 
	if (vma_is_accessible(vma))
		gup_flags |= FOLL_FORCE;

	if (locked)
		gup_flags |= FOLL_UNLOCKABLE;

	 
	ret = __get_user_pages(mm, start, nr_pages, gup_flags,
			       NULL, locked ? locked : &local_locked);
	lru_add_drain();
	return ret;
}

 
long faultin_vma_page_range(struct vm_area_struct *vma, unsigned long start,
			    unsigned long end, bool write, int *locked)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long nr_pages = (end - start) / PAGE_SIZE;
	int gup_flags;
	long ret;

	VM_BUG_ON(!PAGE_ALIGNED(start));
	VM_BUG_ON(!PAGE_ALIGNED(end));
	VM_BUG_ON_VMA(start < vma->vm_start, vma);
	VM_BUG_ON_VMA(end > vma->vm_end, vma);
	mmap_assert_locked(mm);

	 
	gup_flags = FOLL_TOUCH | FOLL_HWPOISON | FOLL_UNLOCKABLE;
	if (write)
		gup_flags |= FOLL_WRITE;

	 
	if (check_vma_flags(vma, gup_flags))
		return -EINVAL;

	ret = __get_user_pages(mm, start, nr_pages, gup_flags,
			       NULL, locked);
	lru_add_drain();
	return ret;
}

 
int __mm_populate(unsigned long start, unsigned long len, int ignore_errors)
{
	struct mm_struct *mm = current->mm;
	unsigned long end, nstart, nend;
	struct vm_area_struct *vma = NULL;
	int locked = 0;
	long ret = 0;

	end = start + len;

	for (nstart = start; nstart < end; nstart = nend) {
		 
		if (!locked) {
			locked = 1;
			mmap_read_lock(mm);
			vma = find_vma_intersection(mm, nstart, end);
		} else if (nstart >= vma->vm_end)
			vma = find_vma_intersection(mm, vma->vm_end, end);

		if (!vma)
			break;
		 
		nend = min(end, vma->vm_end);
		if (vma->vm_flags & (VM_IO | VM_PFNMAP))
			continue;
		if (nstart < vma->vm_start)
			nstart = vma->vm_start;
		 
		ret = populate_vma_page_range(vma, nstart, nend, &locked);
		if (ret < 0) {
			if (ignore_errors) {
				ret = 0;
				continue;	 
			}
			break;
		}
		nend = nstart + ret * PAGE_SIZE;
		ret = 0;
	}
	if (locked)
		mmap_read_unlock(mm);
	return ret;	 
}
#else  
static long __get_user_pages_locked(struct mm_struct *mm, unsigned long start,
		unsigned long nr_pages, struct page **pages,
		int *locked, unsigned int foll_flags)
{
	struct vm_area_struct *vma;
	bool must_unlock = false;
	unsigned long vm_flags;
	long i;

	if (!nr_pages)
		return 0;

	 
	if (!*locked) {
		if (mmap_read_lock_killable(mm))
			return -EAGAIN;
		must_unlock = true;
		*locked = 1;
	}

	 
	vm_flags  = (foll_flags & FOLL_WRITE) ?
			(VM_WRITE | VM_MAYWRITE) : (VM_READ | VM_MAYREAD);
	vm_flags &= (foll_flags & FOLL_FORCE) ?
			(VM_MAYREAD | VM_MAYWRITE) : (VM_READ | VM_WRITE);

	for (i = 0; i < nr_pages; i++) {
		vma = find_vma(mm, start);
		if (!vma)
			break;

		 
		if ((vma->vm_flags & (VM_IO | VM_PFNMAP)) ||
		    !(vm_flags & vma->vm_flags))
			break;

		if (pages) {
			pages[i] = virt_to_page((void *)start);
			if (pages[i])
				get_page(pages[i]);
		}

		start = (start + PAGE_SIZE) & PAGE_MASK;
	}

	if (must_unlock && *locked) {
		mmap_read_unlock(mm);
		*locked = 0;
	}

	return i ? : -EFAULT;
}
#endif  

 
size_t fault_in_writeable(char __user *uaddr, size_t size)
{
	char __user *start = uaddr, *end;

	if (unlikely(size == 0))
		return 0;
	if (!user_write_access_begin(uaddr, size))
		return size;
	if (!PAGE_ALIGNED(uaddr)) {
		unsafe_put_user(0, uaddr, out);
		uaddr = (char __user *)PAGE_ALIGN((unsigned long)uaddr);
	}
	end = (char __user *)PAGE_ALIGN((unsigned long)start + size);
	if (unlikely(end < start))
		end = NULL;
	while (uaddr != end) {
		unsafe_put_user(0, uaddr, out);
		uaddr += PAGE_SIZE;
	}

out:
	user_write_access_end();
	if (size > uaddr - start)
		return size - (uaddr - start);
	return 0;
}
EXPORT_SYMBOL(fault_in_writeable);

 
size_t fault_in_subpage_writeable(char __user *uaddr, size_t size)
{
	size_t faulted_in;

	 
	faulted_in = size - fault_in_writeable(uaddr, size);
	if (faulted_in)
		faulted_in -= probe_subpage_writeable(uaddr, faulted_in);

	return size - faulted_in;
}
EXPORT_SYMBOL(fault_in_subpage_writeable);

 
size_t fault_in_safe_writeable(const char __user *uaddr, size_t size)
{
	unsigned long start = (unsigned long)uaddr, end;
	struct mm_struct *mm = current->mm;
	bool unlocked = false;

	if (unlikely(size == 0))
		return 0;
	end = PAGE_ALIGN(start + size);
	if (end < start)
		end = 0;

	mmap_read_lock(mm);
	do {
		if (fixup_user_fault(mm, start, FAULT_FLAG_WRITE, &unlocked))
			break;
		start = (start + PAGE_SIZE) & PAGE_MASK;
	} while (start != end);
	mmap_read_unlock(mm);

	if (size > (unsigned long)uaddr - start)
		return size - ((unsigned long)uaddr - start);
	return 0;
}
EXPORT_SYMBOL(fault_in_safe_writeable);

 
size_t fault_in_readable(const char __user *uaddr, size_t size)
{
	const char __user *start = uaddr, *end;
	volatile char c;

	if (unlikely(size == 0))
		return 0;
	if (!user_read_access_begin(uaddr, size))
		return size;
	if (!PAGE_ALIGNED(uaddr)) {
		unsafe_get_user(c, uaddr, out);
		uaddr = (const char __user *)PAGE_ALIGN((unsigned long)uaddr);
	}
	end = (const char __user *)PAGE_ALIGN((unsigned long)start + size);
	if (unlikely(end < start))
		end = NULL;
	while (uaddr != end) {
		unsafe_get_user(c, uaddr, out);
		uaddr += PAGE_SIZE;
	}

out:
	user_read_access_end();
	(void)c;
	if (size > uaddr - start)
		return size - (uaddr - start);
	return 0;
}
EXPORT_SYMBOL(fault_in_readable);

 
#ifdef CONFIG_ELF_CORE
struct page *get_dump_page(unsigned long addr)
{
	struct page *page;
	int locked = 0;
	int ret;

	ret = __get_user_pages_locked(current->mm, addr, 1, &page, &locked,
				      FOLL_FORCE | FOLL_DUMP | FOLL_GET);
	return (ret == 1) ? page : NULL;
}
#endif  

#ifdef CONFIG_MIGRATION
 
static unsigned long collect_longterm_unpinnable_pages(
					struct list_head *movable_page_list,
					unsigned long nr_pages,
					struct page **pages)
{
	unsigned long i, collected = 0;
	struct folio *prev_folio = NULL;
	bool drain_allow = true;

	for (i = 0; i < nr_pages; i++) {
		struct folio *folio = page_folio(pages[i]);

		if (folio == prev_folio)
			continue;
		prev_folio = folio;

		if (folio_is_longterm_pinnable(folio))
			continue;

		collected++;

		if (folio_is_device_coherent(folio))
			continue;

		if (folio_test_hugetlb(folio)) {
			isolate_hugetlb(folio, movable_page_list);
			continue;
		}

		if (!folio_test_lru(folio) && drain_allow) {
			lru_add_drain_all();
			drain_allow = false;
		}

		if (!folio_isolate_lru(folio))
			continue;

		list_add_tail(&folio->lru, movable_page_list);
		node_stat_mod_folio(folio,
				    NR_ISOLATED_ANON + folio_is_file_lru(folio),
				    folio_nr_pages(folio));
	}

	return collected;
}

 
static int migrate_longterm_unpinnable_pages(
					struct list_head *movable_page_list,
					unsigned long nr_pages,
					struct page **pages)
{
	int ret;
	unsigned long i;

	for (i = 0; i < nr_pages; i++) {
		struct folio *folio = page_folio(pages[i]);

		if (folio_is_device_coherent(folio)) {
			 
			pages[i] = NULL;
			folio_get(folio);
			gup_put_folio(folio, 1, FOLL_PIN);

			if (migrate_device_coherent_page(&folio->page)) {
				ret = -EBUSY;
				goto err;
			}

			continue;
		}

		 
		unpin_user_page(pages[i]);
		pages[i] = NULL;
	}

	if (!list_empty(movable_page_list)) {
		struct migration_target_control mtc = {
			.nid = NUMA_NO_NODE,
			.gfp_mask = GFP_USER | __GFP_NOWARN,
		};

		if (migrate_pages(movable_page_list, alloc_migration_target,
				  NULL, (unsigned long)&mtc, MIGRATE_SYNC,
				  MR_LONGTERM_PIN, NULL)) {
			ret = -ENOMEM;
			goto err;
		}
	}

	putback_movable_pages(movable_page_list);

	return -EAGAIN;

err:
	for (i = 0; i < nr_pages; i++)
		if (pages[i])
			unpin_user_page(pages[i]);
	putback_movable_pages(movable_page_list);

	return ret;
}

 
static long check_and_migrate_movable_pages(unsigned long nr_pages,
					    struct page **pages)
{
	unsigned long collected;
	LIST_HEAD(movable_page_list);

	collected = collect_longterm_unpinnable_pages(&movable_page_list,
						nr_pages, pages);
	if (!collected)
		return 0;

	return migrate_longterm_unpinnable_pages(&movable_page_list, nr_pages,
						pages);
}
#else
static long check_and_migrate_movable_pages(unsigned long nr_pages,
					    struct page **pages)
{
	return 0;
}
#endif  

 
static long __gup_longterm_locked(struct mm_struct *mm,
				  unsigned long start,
				  unsigned long nr_pages,
				  struct page **pages,
				  int *locked,
				  unsigned int gup_flags)
{
	unsigned int flags;
	long rc, nr_pinned_pages;

	if (!(gup_flags & FOLL_LONGTERM))
		return __get_user_pages_locked(mm, start, nr_pages, pages,
					       locked, gup_flags);

	flags = memalloc_pin_save();
	do {
		nr_pinned_pages = __get_user_pages_locked(mm, start, nr_pages,
							  pages, locked,
							  gup_flags);
		if (nr_pinned_pages <= 0) {
			rc = nr_pinned_pages;
			break;
		}

		 
		rc = check_and_migrate_movable_pages(nr_pinned_pages, pages);
	} while (rc == -EAGAIN);
	memalloc_pin_restore(flags);
	return rc ? rc : nr_pinned_pages;
}

 
static bool is_valid_gup_args(struct page **pages, int *locked,
			      unsigned int *gup_flags_p, unsigned int to_set)
{
	unsigned int gup_flags = *gup_flags_p;

	 
	if (WARN_ON_ONCE(gup_flags & (FOLL_PIN | FOLL_TRIED | FOLL_UNLOCKABLE |
				      FOLL_REMOTE | FOLL_FAST_ONLY)))
		return false;

	gup_flags |= to_set;
	if (locked) {
		 
		if (WARN_ON_ONCE(*locked != 1))
			return false;

		gup_flags |= FOLL_UNLOCKABLE;
	}

	 
	if (WARN_ON_ONCE((gup_flags & (FOLL_PIN | FOLL_GET)) ==
			 (FOLL_PIN | FOLL_GET)))
		return false;

	 
	if (WARN_ON_ONCE(!(gup_flags & FOLL_PIN) && (gup_flags & FOLL_LONGTERM)))
		return false;

	 
	if (WARN_ON_ONCE((gup_flags & (FOLL_GET | FOLL_PIN)) && !pages))
		return false;

	 
	if (WARN_ON_ONCE((gup_flags & FOLL_LONGTERM) &&
			 (gup_flags & FOLL_PCI_P2PDMA)))
		return false;

	*gup_flags_p = gup_flags;
	return true;
}

#ifdef CONFIG_MMU
 
long get_user_pages_remote(struct mm_struct *mm,
		unsigned long start, unsigned long nr_pages,
		unsigned int gup_flags, struct page **pages,
		int *locked)
{
	int local_locked = 1;

	if (!is_valid_gup_args(pages, locked, &gup_flags,
			       FOLL_TOUCH | FOLL_REMOTE))
		return -EINVAL;

	return __get_user_pages_locked(mm, start, nr_pages, pages,
				       locked ? locked : &local_locked,
				       gup_flags);
}
EXPORT_SYMBOL(get_user_pages_remote);

#else  
long get_user_pages_remote(struct mm_struct *mm,
			   unsigned long start, unsigned long nr_pages,
			   unsigned int gup_flags, struct page **pages,
			   int *locked)
{
	return 0;
}
#endif  

 
long get_user_pages(unsigned long start, unsigned long nr_pages,
		    unsigned int gup_flags, struct page **pages)
{
	int locked = 1;

	if (!is_valid_gup_args(pages, NULL, &gup_flags, FOLL_TOUCH))
		return -EINVAL;

	return __get_user_pages_locked(current->mm, start, nr_pages, pages,
				       &locked, gup_flags);
}
EXPORT_SYMBOL(get_user_pages);

 
long get_user_pages_unlocked(unsigned long start, unsigned long nr_pages,
			     struct page **pages, unsigned int gup_flags)
{
	int locked = 0;

	if (!is_valid_gup_args(pages, NULL, &gup_flags,
			       FOLL_TOUCH | FOLL_UNLOCKABLE))
		return -EINVAL;

	return __get_user_pages_locked(current->mm, start, nr_pages, pages,
				       &locked, gup_flags);
}
EXPORT_SYMBOL(get_user_pages_unlocked);

 
#ifdef CONFIG_HAVE_FAST_GUP

 
static bool folio_fast_pin_allowed(struct folio *folio, unsigned int flags)
{
	struct address_space *mapping;
	unsigned long mapping_flags;

	 
	if ((flags & (FOLL_PIN | FOLL_LONGTERM | FOLL_WRITE)) !=
	    (FOLL_PIN | FOLL_LONGTERM | FOLL_WRITE))
		return true;

	 

	if (WARN_ON_ONCE(folio_test_slab(folio)))
		return false;

	 
	if (folio_test_hugetlb(folio))
		return true;

	 
	lockdep_assert_irqs_disabled();

	 
	mapping = READ_ONCE(folio->mapping);

	 
	if (!mapping)
		return false;

	 
	mapping_flags = (unsigned long)mapping & PAGE_MAPPING_FLAGS;
	if (mapping_flags)
		return mapping_flags & PAGE_MAPPING_ANON;

	 
	return shmem_mapping(mapping);
}

static void __maybe_unused undo_dev_pagemap(int *nr, int nr_start,
					    unsigned int flags,
					    struct page **pages)
{
	while ((*nr) - nr_start) {
		struct page *page = pages[--(*nr)];

		ClearPageReferenced(page);
		if (flags & FOLL_PIN)
			unpin_user_page(page);
		else
			put_page(page);
	}
}

#ifdef CONFIG_ARCH_HAS_PTE_SPECIAL
 
static int gup_pte_range(pmd_t pmd, pmd_t *pmdp, unsigned long addr,
			 unsigned long end, unsigned int flags,
			 struct page **pages, int *nr)
{
	struct dev_pagemap *pgmap = NULL;
	int nr_start = *nr, ret = 0;
	pte_t *ptep, *ptem;

	ptem = ptep = pte_offset_map(&pmd, addr);
	if (!ptep)
		return 0;
	do {
		pte_t pte = ptep_get_lockless(ptep);
		struct page *page;
		struct folio *folio;

		 
		if (pte_protnone(pte))
			goto pte_unmap;

		if (!pte_access_permitted(pte, flags & FOLL_WRITE))
			goto pte_unmap;

		if (pte_devmap(pte)) {
			if (unlikely(flags & FOLL_LONGTERM))
				goto pte_unmap;

			pgmap = get_dev_pagemap(pte_pfn(pte), pgmap);
			if (unlikely(!pgmap)) {
				undo_dev_pagemap(nr, nr_start, flags, pages);
				goto pte_unmap;
			}
		} else if (pte_special(pte))
			goto pte_unmap;

		VM_BUG_ON(!pfn_valid(pte_pfn(pte)));
		page = pte_page(pte);

		folio = try_grab_folio(page, 1, flags);
		if (!folio)
			goto pte_unmap;

		if (unlikely(folio_is_secretmem(folio))) {
			gup_put_folio(folio, 1, flags);
			goto pte_unmap;
		}

		if (unlikely(pmd_val(pmd) != pmd_val(*pmdp)) ||
		    unlikely(pte_val(pte) != pte_val(ptep_get(ptep)))) {
			gup_put_folio(folio, 1, flags);
			goto pte_unmap;
		}

		if (!folio_fast_pin_allowed(folio, flags)) {
			gup_put_folio(folio, 1, flags);
			goto pte_unmap;
		}

		if (!pte_write(pte) && gup_must_unshare(NULL, flags, page)) {
			gup_put_folio(folio, 1, flags);
			goto pte_unmap;
		}

		 
		if (flags & FOLL_PIN) {
			ret = arch_make_page_accessible(page);
			if (ret) {
				gup_put_folio(folio, 1, flags);
				goto pte_unmap;
			}
		}
		folio_set_referenced(folio);
		pages[*nr] = page;
		(*nr)++;
	} while (ptep++, addr += PAGE_SIZE, addr != end);

	ret = 1;

pte_unmap:
	if (pgmap)
		put_dev_pagemap(pgmap);
	pte_unmap(ptem);
	return ret;
}
#else

 
static int gup_pte_range(pmd_t pmd, pmd_t *pmdp, unsigned long addr,
			 unsigned long end, unsigned int flags,
			 struct page **pages, int *nr)
{
	return 0;
}
#endif  

#if defined(CONFIG_ARCH_HAS_PTE_DEVMAP) && defined(CONFIG_TRANSPARENT_HUGEPAGE)
static int __gup_device_huge(unsigned long pfn, unsigned long addr,
			     unsigned long end, unsigned int flags,
			     struct page **pages, int *nr)
{
	int nr_start = *nr;
	struct dev_pagemap *pgmap = NULL;

	do {
		struct page *page = pfn_to_page(pfn);

		pgmap = get_dev_pagemap(pfn, pgmap);
		if (unlikely(!pgmap)) {
			undo_dev_pagemap(nr, nr_start, flags, pages);
			break;
		}

		if (!(flags & FOLL_PCI_P2PDMA) && is_pci_p2pdma_page(page)) {
			undo_dev_pagemap(nr, nr_start, flags, pages);
			break;
		}

		SetPageReferenced(page);
		pages[*nr] = page;
		if (unlikely(try_grab_page(page, flags))) {
			undo_dev_pagemap(nr, nr_start, flags, pages);
			break;
		}
		(*nr)++;
		pfn++;
	} while (addr += PAGE_SIZE, addr != end);

	put_dev_pagemap(pgmap);
	return addr == end;
}

static int __gup_device_huge_pmd(pmd_t orig, pmd_t *pmdp, unsigned long addr,
				 unsigned long end, unsigned int flags,
				 struct page **pages, int *nr)
{
	unsigned long fault_pfn;
	int nr_start = *nr;

	fault_pfn = pmd_pfn(orig) + ((addr & ~PMD_MASK) >> PAGE_SHIFT);
	if (!__gup_device_huge(fault_pfn, addr, end, flags, pages, nr))
		return 0;

	if (unlikely(pmd_val(orig) != pmd_val(*pmdp))) {
		undo_dev_pagemap(nr, nr_start, flags, pages);
		return 0;
	}
	return 1;
}

static int __gup_device_huge_pud(pud_t orig, pud_t *pudp, unsigned long addr,
				 unsigned long end, unsigned int flags,
				 struct page **pages, int *nr)
{
	unsigned long fault_pfn;
	int nr_start = *nr;

	fault_pfn = pud_pfn(orig) + ((addr & ~PUD_MASK) >> PAGE_SHIFT);
	if (!__gup_device_huge(fault_pfn, addr, end, flags, pages, nr))
		return 0;

	if (unlikely(pud_val(orig) != pud_val(*pudp))) {
		undo_dev_pagemap(nr, nr_start, flags, pages);
		return 0;
	}
	return 1;
}
#else
static int __gup_device_huge_pmd(pmd_t orig, pmd_t *pmdp, unsigned long addr,
				 unsigned long end, unsigned int flags,
				 struct page **pages, int *nr)
{
	BUILD_BUG();
	return 0;
}

static int __gup_device_huge_pud(pud_t pud, pud_t *pudp, unsigned long addr,
				 unsigned long end, unsigned int flags,
				 struct page **pages, int *nr)
{
	BUILD_BUG();
	return 0;
}
#endif

static int record_subpages(struct page *page, unsigned long addr,
			   unsigned long end, struct page **pages)
{
	int nr;

	for (nr = 0; addr != end; nr++, addr += PAGE_SIZE)
		pages[nr] = nth_page(page, nr);

	return nr;
}

#ifdef CONFIG_ARCH_HAS_HUGEPD
static unsigned long hugepte_addr_end(unsigned long addr, unsigned long end,
				      unsigned long sz)
{
	unsigned long __boundary = (addr + sz) & ~(sz-1);
	return (__boundary - 1 < end - 1) ? __boundary : end;
}

static int gup_hugepte(pte_t *ptep, unsigned long sz, unsigned long addr,
		       unsigned long end, unsigned int flags,
		       struct page **pages, int *nr)
{
	unsigned long pte_end;
	struct page *page;
	struct folio *folio;
	pte_t pte;
	int refs;

	pte_end = (addr + sz) & ~(sz-1);
	if (pte_end < end)
		end = pte_end;

	pte = huge_ptep_get(ptep);

	if (!pte_access_permitted(pte, flags & FOLL_WRITE))
		return 0;

	 
	VM_BUG_ON(!pfn_valid(pte_pfn(pte)));

	page = nth_page(pte_page(pte), (addr & (sz - 1)) >> PAGE_SHIFT);
	refs = record_subpages(page, addr, end, pages + *nr);

	folio = try_grab_folio(page, refs, flags);
	if (!folio)
		return 0;

	if (unlikely(pte_val(pte) != pte_val(ptep_get(ptep)))) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	if (!folio_fast_pin_allowed(folio, flags)) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	if (!pte_write(pte) && gup_must_unshare(NULL, flags, &folio->page)) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	*nr += refs;
	folio_set_referenced(folio);
	return 1;
}

static int gup_huge_pd(hugepd_t hugepd, unsigned long addr,
		unsigned int pdshift, unsigned long end, unsigned int flags,
		struct page **pages, int *nr)
{
	pte_t *ptep;
	unsigned long sz = 1UL << hugepd_shift(hugepd);
	unsigned long next;

	ptep = hugepte_offset(hugepd, addr, pdshift);
	do {
		next = hugepte_addr_end(addr, end, sz);
		if (!gup_hugepte(ptep, sz, addr, end, flags, pages, nr))
			return 0;
	} while (ptep++, addr = next, addr != end);

	return 1;
}
#else
static inline int gup_huge_pd(hugepd_t hugepd, unsigned long addr,
		unsigned int pdshift, unsigned long end, unsigned int flags,
		struct page **pages, int *nr)
{
	return 0;
}
#endif  

static int gup_huge_pmd(pmd_t orig, pmd_t *pmdp, unsigned long addr,
			unsigned long end, unsigned int flags,
			struct page **pages, int *nr)
{
	struct page *page;
	struct folio *folio;
	int refs;

	if (!pmd_access_permitted(orig, flags & FOLL_WRITE))
		return 0;

	if (pmd_devmap(orig)) {
		if (unlikely(flags & FOLL_LONGTERM))
			return 0;
		return __gup_device_huge_pmd(orig, pmdp, addr, end, flags,
					     pages, nr);
	}

	page = nth_page(pmd_page(orig), (addr & ~PMD_MASK) >> PAGE_SHIFT);
	refs = record_subpages(page, addr, end, pages + *nr);

	folio = try_grab_folio(page, refs, flags);
	if (!folio)
		return 0;

	if (unlikely(pmd_val(orig) != pmd_val(*pmdp))) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	if (!folio_fast_pin_allowed(folio, flags)) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}
	if (!pmd_write(orig) && gup_must_unshare(NULL, flags, &folio->page)) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	*nr += refs;
	folio_set_referenced(folio);
	return 1;
}

static int gup_huge_pud(pud_t orig, pud_t *pudp, unsigned long addr,
			unsigned long end, unsigned int flags,
			struct page **pages, int *nr)
{
	struct page *page;
	struct folio *folio;
	int refs;

	if (!pud_access_permitted(orig, flags & FOLL_WRITE))
		return 0;

	if (pud_devmap(orig)) {
		if (unlikely(flags & FOLL_LONGTERM))
			return 0;
		return __gup_device_huge_pud(orig, pudp, addr, end, flags,
					     pages, nr);
	}

	page = nth_page(pud_page(orig), (addr & ~PUD_MASK) >> PAGE_SHIFT);
	refs = record_subpages(page, addr, end, pages + *nr);

	folio = try_grab_folio(page, refs, flags);
	if (!folio)
		return 0;

	if (unlikely(pud_val(orig) != pud_val(*pudp))) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	if (!folio_fast_pin_allowed(folio, flags)) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	if (!pud_write(orig) && gup_must_unshare(NULL, flags, &folio->page)) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	*nr += refs;
	folio_set_referenced(folio);
	return 1;
}

static int gup_huge_pgd(pgd_t orig, pgd_t *pgdp, unsigned long addr,
			unsigned long end, unsigned int flags,
			struct page **pages, int *nr)
{
	int refs;
	struct page *page;
	struct folio *folio;

	if (!pgd_access_permitted(orig, flags & FOLL_WRITE))
		return 0;

	BUILD_BUG_ON(pgd_devmap(orig));

	page = nth_page(pgd_page(orig), (addr & ~PGDIR_MASK) >> PAGE_SHIFT);
	refs = record_subpages(page, addr, end, pages + *nr);

	folio = try_grab_folio(page, refs, flags);
	if (!folio)
		return 0;

	if (unlikely(pgd_val(orig) != pgd_val(*pgdp))) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	if (!pgd_write(orig) && gup_must_unshare(NULL, flags, &folio->page)) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	if (!folio_fast_pin_allowed(folio, flags)) {
		gup_put_folio(folio, refs, flags);
		return 0;
	}

	*nr += refs;
	folio_set_referenced(folio);
	return 1;
}

static int gup_pmd_range(pud_t *pudp, pud_t pud, unsigned long addr, unsigned long end,
		unsigned int flags, struct page **pages, int *nr)
{
	unsigned long next;
	pmd_t *pmdp;

	pmdp = pmd_offset_lockless(pudp, pud, addr);
	do {
		pmd_t pmd = pmdp_get_lockless(pmdp);

		next = pmd_addr_end(addr, end);
		if (!pmd_present(pmd))
			return 0;

		if (unlikely(pmd_trans_huge(pmd) || pmd_huge(pmd) ||
			     pmd_devmap(pmd))) {
			 
			if (pmd_protnone(pmd))
				return 0;

			if (!gup_huge_pmd(pmd, pmdp, addr, next, flags,
				pages, nr))
				return 0;

		} else if (unlikely(is_hugepd(__hugepd(pmd_val(pmd))))) {
			 
			if (!gup_huge_pd(__hugepd(pmd_val(pmd)), addr,
					 PMD_SHIFT, next, flags, pages, nr))
				return 0;
		} else if (!gup_pte_range(pmd, pmdp, addr, next, flags, pages, nr))
			return 0;
	} while (pmdp++, addr = next, addr != end);

	return 1;
}

static int gup_pud_range(p4d_t *p4dp, p4d_t p4d, unsigned long addr, unsigned long end,
			 unsigned int flags, struct page **pages, int *nr)
{
	unsigned long next;
	pud_t *pudp;

	pudp = pud_offset_lockless(p4dp, p4d, addr);
	do {
		pud_t pud = READ_ONCE(*pudp);

		next = pud_addr_end(addr, end);
		if (unlikely(!pud_present(pud)))
			return 0;
		if (unlikely(pud_huge(pud) || pud_devmap(pud))) {
			if (!gup_huge_pud(pud, pudp, addr, next, flags,
					  pages, nr))
				return 0;
		} else if (unlikely(is_hugepd(__hugepd(pud_val(pud))))) {
			if (!gup_huge_pd(__hugepd(pud_val(pud)), addr,
					 PUD_SHIFT, next, flags, pages, nr))
				return 0;
		} else if (!gup_pmd_range(pudp, pud, addr, next, flags, pages, nr))
			return 0;
	} while (pudp++, addr = next, addr != end);

	return 1;
}

static int gup_p4d_range(pgd_t *pgdp, pgd_t pgd, unsigned long addr, unsigned long end,
			 unsigned int flags, struct page **pages, int *nr)
{
	unsigned long next;
	p4d_t *p4dp;

	p4dp = p4d_offset_lockless(pgdp, pgd, addr);
	do {
		p4d_t p4d = READ_ONCE(*p4dp);

		next = p4d_addr_end(addr, end);
		if (p4d_none(p4d))
			return 0;
		BUILD_BUG_ON(p4d_huge(p4d));
		if (unlikely(is_hugepd(__hugepd(p4d_val(p4d))))) {
			if (!gup_huge_pd(__hugepd(p4d_val(p4d)), addr,
					 P4D_SHIFT, next, flags, pages, nr))
				return 0;
		} else if (!gup_pud_range(p4dp, p4d, addr, next, flags, pages, nr))
			return 0;
	} while (p4dp++, addr = next, addr != end);

	return 1;
}

static void gup_pgd_range(unsigned long addr, unsigned long end,
		unsigned int flags, struct page **pages, int *nr)
{
	unsigned long next;
	pgd_t *pgdp;

	pgdp = pgd_offset(current->mm, addr);
	do {
		pgd_t pgd = READ_ONCE(*pgdp);

		next = pgd_addr_end(addr, end);
		if (pgd_none(pgd))
			return;
		if (unlikely(pgd_huge(pgd))) {
			if (!gup_huge_pgd(pgd, pgdp, addr, next, flags,
					  pages, nr))
				return;
		} else if (unlikely(is_hugepd(__hugepd(pgd_val(pgd))))) {
			if (!gup_huge_pd(__hugepd(pgd_val(pgd)), addr,
					 PGDIR_SHIFT, next, flags, pages, nr))
				return;
		} else if (!gup_p4d_range(pgdp, pgd, addr, next, flags, pages, nr))
			return;
	} while (pgdp++, addr = next, addr != end);
}
#else
static inline void gup_pgd_range(unsigned long addr, unsigned long end,
		unsigned int flags, struct page **pages, int *nr)
{
}
#endif  

#ifndef gup_fast_permitted
 
static bool gup_fast_permitted(unsigned long start, unsigned long end)
{
	return true;
}
#endif

static unsigned long lockless_pages_from_mm(unsigned long start,
					    unsigned long end,
					    unsigned int gup_flags,
					    struct page **pages)
{
	unsigned long flags;
	int nr_pinned = 0;
	unsigned seq;

	if (!IS_ENABLED(CONFIG_HAVE_FAST_GUP) ||
	    !gup_fast_permitted(start, end))
		return 0;

	if (gup_flags & FOLL_PIN) {
		seq = raw_read_seqcount(&current->mm->write_protect_seq);
		if (seq & 1)
			return 0;
	}

	 
	local_irq_save(flags);
	gup_pgd_range(start, end, gup_flags, pages, &nr_pinned);
	local_irq_restore(flags);

	 
	if (gup_flags & FOLL_PIN) {
		if (read_seqcount_retry(&current->mm->write_protect_seq, seq)) {
			unpin_user_pages_lockless(pages, nr_pinned);
			return 0;
		} else {
			sanity_check_pinned_pages(pages, nr_pinned);
		}
	}
	return nr_pinned;
}

static int internal_get_user_pages_fast(unsigned long start,
					unsigned long nr_pages,
					unsigned int gup_flags,
					struct page **pages)
{
	unsigned long len, end;
	unsigned long nr_pinned;
	int locked = 0;
	int ret;

	if (WARN_ON_ONCE(gup_flags & ~(FOLL_WRITE | FOLL_LONGTERM |
				       FOLL_FORCE | FOLL_PIN | FOLL_GET |
				       FOLL_FAST_ONLY | FOLL_NOFAULT |
				       FOLL_PCI_P2PDMA | FOLL_HONOR_NUMA_FAULT)))
		return -EINVAL;

	if (gup_flags & FOLL_PIN)
		mm_set_has_pinned_flag(&current->mm->flags);

	if (!(gup_flags & FOLL_FAST_ONLY))
		might_lock_read(&current->mm->mmap_lock);

	start = untagged_addr(start) & PAGE_MASK;
	len = nr_pages << PAGE_SHIFT;
	if (check_add_overflow(start, len, &end))
		return -EOVERFLOW;
	if (end > TASK_SIZE_MAX)
		return -EFAULT;
	if (unlikely(!access_ok((void __user *)start, len)))
		return -EFAULT;

	nr_pinned = lockless_pages_from_mm(start, end, gup_flags, pages);
	if (nr_pinned == nr_pages || gup_flags & FOLL_FAST_ONLY)
		return nr_pinned;

	 
	start += nr_pinned << PAGE_SHIFT;
	pages += nr_pinned;
	ret = __gup_longterm_locked(current->mm, start, nr_pages - nr_pinned,
				    pages, &locked,
				    gup_flags | FOLL_TOUCH | FOLL_UNLOCKABLE);
	if (ret < 0) {
		 
		if (nr_pinned)
			return nr_pinned;
		return ret;
	}
	return ret + nr_pinned;
}

 
int get_user_pages_fast_only(unsigned long start, int nr_pages,
			     unsigned int gup_flags, struct page **pages)
{
	 
	if (!is_valid_gup_args(pages, NULL, &gup_flags,
			       FOLL_GET | FOLL_FAST_ONLY))
		return -EINVAL;

	return internal_get_user_pages_fast(start, nr_pages, gup_flags, pages);
}
EXPORT_SYMBOL_GPL(get_user_pages_fast_only);

 
int get_user_pages_fast(unsigned long start, int nr_pages,
			unsigned int gup_flags, struct page **pages)
{
	 
	if (!is_valid_gup_args(pages, NULL, &gup_flags, FOLL_GET))
		return -EINVAL;
	return internal_get_user_pages_fast(start, nr_pages, gup_flags, pages);
}
EXPORT_SYMBOL_GPL(get_user_pages_fast);

 
int pin_user_pages_fast(unsigned long start, int nr_pages,
			unsigned int gup_flags, struct page **pages)
{
	if (!is_valid_gup_args(pages, NULL, &gup_flags, FOLL_PIN))
		return -EINVAL;
	return internal_get_user_pages_fast(start, nr_pages, gup_flags, pages);
}
EXPORT_SYMBOL_GPL(pin_user_pages_fast);

 
long pin_user_pages_remote(struct mm_struct *mm,
			   unsigned long start, unsigned long nr_pages,
			   unsigned int gup_flags, struct page **pages,
			   int *locked)
{
	int local_locked = 1;

	if (!is_valid_gup_args(pages, locked, &gup_flags,
			       FOLL_PIN | FOLL_TOUCH | FOLL_REMOTE))
		return 0;
	return __gup_longterm_locked(mm, start, nr_pages, pages,
				     locked ? locked : &local_locked,
				     gup_flags);
}
EXPORT_SYMBOL(pin_user_pages_remote);

 
long pin_user_pages(unsigned long start, unsigned long nr_pages,
		    unsigned int gup_flags, struct page **pages)
{
	int locked = 1;

	if (!is_valid_gup_args(pages, NULL, &gup_flags, FOLL_PIN))
		return 0;
	return __gup_longterm_locked(current->mm, start, nr_pages,
				     pages, &locked, gup_flags);
}
EXPORT_SYMBOL(pin_user_pages);

 
long pin_user_pages_unlocked(unsigned long start, unsigned long nr_pages,
			     struct page **pages, unsigned int gup_flags)
{
	int locked = 0;

	if (!is_valid_gup_args(pages, NULL, &gup_flags,
			       FOLL_PIN | FOLL_TOUCH | FOLL_UNLOCKABLE))
		return 0;

	return __gup_longterm_locked(current->mm, start, nr_pages, pages,
				     &locked, gup_flags);
}
EXPORT_SYMBOL(pin_user_pages_unlocked);
