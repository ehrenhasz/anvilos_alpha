
#include <linux/mm.h>
#include <linux/rmap.h>
#include <linux/hugetlb.h>
#include <linux/swap.h>
#include <linux/swapops.h>

#include "internal.h"

static inline bool not_found(struct page_vma_mapped_walk *pvmw)
{
	page_vma_mapped_walk_done(pvmw);
	return false;
}

static bool map_pte(struct page_vma_mapped_walk *pvmw, spinlock_t **ptlp)
{
	pte_t ptent;

	if (pvmw->flags & PVMW_SYNC) {
		 
		pvmw->pte = pte_offset_map_lock(pvmw->vma->vm_mm, pvmw->pmd,
						pvmw->address, &pvmw->ptl);
		*ptlp = pvmw->ptl;
		return !!pvmw->pte;
	}

	 
	pvmw->pte = pte_offset_map_nolock(pvmw->vma->vm_mm, pvmw->pmd,
					  pvmw->address, ptlp);
	if (!pvmw->pte)
		return false;

	ptent = ptep_get(pvmw->pte);

	if (pvmw->flags & PVMW_MIGRATION) {
		if (!is_swap_pte(ptent))
			return false;
	} else if (is_swap_pte(ptent)) {
		swp_entry_t entry;
		 
		entry = pte_to_swp_entry(ptent);
		if (!is_device_private_entry(entry) &&
		    !is_device_exclusive_entry(entry))
			return false;
	} else if (!pte_present(ptent)) {
		return false;
	}
	pvmw->ptl = *ptlp;
	spin_lock(pvmw->ptl);
	return true;
}

 
static bool check_pte(struct page_vma_mapped_walk *pvmw)
{
	unsigned long pfn;
	pte_t ptent = ptep_get(pvmw->pte);

	if (pvmw->flags & PVMW_MIGRATION) {
		swp_entry_t entry;
		if (!is_swap_pte(ptent))
			return false;
		entry = pte_to_swp_entry(ptent);

		if (!is_migration_entry(entry) &&
		    !is_device_exclusive_entry(entry))
			return false;

		pfn = swp_offset_pfn(entry);
	} else if (is_swap_pte(ptent)) {
		swp_entry_t entry;

		 
		entry = pte_to_swp_entry(ptent);
		if (!is_device_private_entry(entry) &&
		    !is_device_exclusive_entry(entry))
			return false;

		pfn = swp_offset_pfn(entry);
	} else {
		if (!pte_present(ptent))
			return false;

		pfn = pte_pfn(ptent);
	}

	return (pfn - pvmw->pfn) < pvmw->nr_pages;
}

 
static bool check_pmd(unsigned long pfn, struct page_vma_mapped_walk *pvmw)
{
	if ((pfn + HPAGE_PMD_NR - 1) < pvmw->pfn)
		return false;
	if (pfn > pvmw->pfn + pvmw->nr_pages - 1)
		return false;
	return true;
}

static void step_forward(struct page_vma_mapped_walk *pvmw, unsigned long size)
{
	pvmw->address = (pvmw->address + size) & ~(size - 1);
	if (!pvmw->address)
		pvmw->address = ULONG_MAX;
}

 
bool page_vma_mapped_walk(struct page_vma_mapped_walk *pvmw)
{
	struct vm_area_struct *vma = pvmw->vma;
	struct mm_struct *mm = vma->vm_mm;
	unsigned long end;
	spinlock_t *ptl;
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t pmde;

	 
	if (pvmw->pmd && !pvmw->pte)
		return not_found(pvmw);

	if (unlikely(is_vm_hugetlb_page(vma))) {
		struct hstate *hstate = hstate_vma(vma);
		unsigned long size = huge_page_size(hstate);
		 
		if (pvmw->pte)
			return not_found(pvmw);
		 
		pvmw->pte = hugetlb_walk(vma, pvmw->address, size);
		if (!pvmw->pte)
			return false;

		pvmw->ptl = huge_pte_lock(hstate, mm, pvmw->pte);
		if (!check_pte(pvmw))
			return not_found(pvmw);
		return true;
	}

	end = vma_address_end(pvmw);
	if (pvmw->pte)
		goto next_pte;
restart:
	do {
		pgd = pgd_offset(mm, pvmw->address);
		if (!pgd_present(*pgd)) {
			step_forward(pvmw, PGDIR_SIZE);
			continue;
		}
		p4d = p4d_offset(pgd, pvmw->address);
		if (!p4d_present(*p4d)) {
			step_forward(pvmw, P4D_SIZE);
			continue;
		}
		pud = pud_offset(p4d, pvmw->address);
		if (!pud_present(*pud)) {
			step_forward(pvmw, PUD_SIZE);
			continue;
		}

		pvmw->pmd = pmd_offset(pud, pvmw->address);
		 
		pmde = pmdp_get_lockless(pvmw->pmd);

		if (pmd_trans_huge(pmde) || is_pmd_migration_entry(pmde) ||
		    (pmd_present(pmde) && pmd_devmap(pmde))) {
			pvmw->ptl = pmd_lock(mm, pvmw->pmd);
			pmde = *pvmw->pmd;
			if (!pmd_present(pmde)) {
				swp_entry_t entry;

				if (!thp_migration_supported() ||
				    !(pvmw->flags & PVMW_MIGRATION))
					return not_found(pvmw);
				entry = pmd_to_swp_entry(pmde);
				if (!is_migration_entry(entry) ||
				    !check_pmd(swp_offset_pfn(entry), pvmw))
					return not_found(pvmw);
				return true;
			}
			if (likely(pmd_trans_huge(pmde) || pmd_devmap(pmde))) {
				if (pvmw->flags & PVMW_MIGRATION)
					return not_found(pvmw);
				if (!check_pmd(pmd_pfn(pmde), pvmw))
					return not_found(pvmw);
				return true;
			}
			 
			spin_unlock(pvmw->ptl);
			pvmw->ptl = NULL;
		} else if (!pmd_present(pmde)) {
			 
			if ((pvmw->flags & PVMW_SYNC) &&
			    transhuge_vma_suitable(vma, pvmw->address) &&
			    (pvmw->nr_pages >= HPAGE_PMD_NR)) {
				spinlock_t *ptl = pmd_lock(mm, pvmw->pmd);

				spin_unlock(ptl);
			}
			step_forward(pvmw, PMD_SIZE);
			continue;
		}
		if (!map_pte(pvmw, &ptl)) {
			if (!pvmw->pte)
				goto restart;
			goto next_pte;
		}
this_pte:
		if (check_pte(pvmw))
			return true;
next_pte:
		do {
			pvmw->address += PAGE_SIZE;
			if (pvmw->address >= end)
				return not_found(pvmw);
			 
			if ((pvmw->address & (PMD_SIZE - PAGE_SIZE)) == 0) {
				if (pvmw->ptl) {
					spin_unlock(pvmw->ptl);
					pvmw->ptl = NULL;
				}
				pte_unmap(pvmw->pte);
				pvmw->pte = NULL;
				goto restart;
			}
			pvmw->pte++;
		} while (pte_none(ptep_get(pvmw->pte)));

		if (!pvmw->ptl) {
			pvmw->ptl = ptl;
			spin_lock(pvmw->ptl);
		}
		goto this_pte;
	} while (pvmw->address < end);

	return false;
}

 
int page_mapped_in_vma(struct page *page, struct vm_area_struct *vma)
{
	struct page_vma_mapped_walk pvmw = {
		.pfn = page_to_pfn(page),
		.nr_pages = 1,
		.vma = vma,
		.flags = PVMW_SYNC,
	};

	pvmw.address = vma_address(page, vma);
	if (pvmw.address == -EFAULT)
		return 0;
	if (!page_vma_mapped_walk(&pvmw))
		return 0;
	page_vma_mapped_walk_done(&pvmw);
	return 1;
}
