
 
#include <linux/export.h>
#include <linux/memremap.h>
#include <linux/migrate.h>
#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/mmu_notifier.h>
#include <linux/oom.h>
#include <linux/pagewalk.h>
#include <linux/rmap.h>
#include <linux/swapops.h>
#include <asm/tlbflush.h>
#include "internal.h"

static int migrate_vma_collect_skip(unsigned long start,
				    unsigned long end,
				    struct mm_walk *walk)
{
	struct migrate_vma *migrate = walk->private;
	unsigned long addr;

	for (addr = start; addr < end; addr += PAGE_SIZE) {
		migrate->dst[migrate->npages] = 0;
		migrate->src[migrate->npages++] = 0;
	}

	return 0;
}

static int migrate_vma_collect_hole(unsigned long start,
				    unsigned long end,
				    __always_unused int depth,
				    struct mm_walk *walk)
{
	struct migrate_vma *migrate = walk->private;
	unsigned long addr;

	 
	if (!vma_is_anonymous(walk->vma))
		return migrate_vma_collect_skip(start, end, walk);

	for (addr = start; addr < end; addr += PAGE_SIZE) {
		migrate->src[migrate->npages] = MIGRATE_PFN_MIGRATE;
		migrate->dst[migrate->npages] = 0;
		migrate->npages++;
		migrate->cpages++;
	}

	return 0;
}

static int migrate_vma_collect_pmd(pmd_t *pmdp,
				   unsigned long start,
				   unsigned long end,
				   struct mm_walk *walk)
{
	struct migrate_vma *migrate = walk->private;
	struct vm_area_struct *vma = walk->vma;
	struct mm_struct *mm = vma->vm_mm;
	unsigned long addr = start, unmapped = 0;
	spinlock_t *ptl;
	pte_t *ptep;

again:
	if (pmd_none(*pmdp))
		return migrate_vma_collect_hole(start, end, -1, walk);

	if (pmd_trans_huge(*pmdp)) {
		struct page *page;

		ptl = pmd_lock(mm, pmdp);
		if (unlikely(!pmd_trans_huge(*pmdp))) {
			spin_unlock(ptl);
			goto again;
		}

		page = pmd_page(*pmdp);
		if (is_huge_zero_page(page)) {
			spin_unlock(ptl);
			split_huge_pmd(vma, pmdp, addr);
		} else {
			int ret;

			get_page(page);
			spin_unlock(ptl);
			if (unlikely(!trylock_page(page)))
				return migrate_vma_collect_skip(start, end,
								walk);
			ret = split_huge_page(page);
			unlock_page(page);
			put_page(page);
			if (ret)
				return migrate_vma_collect_skip(start, end,
								walk);
		}
	}

	ptep = pte_offset_map_lock(mm, pmdp, addr, &ptl);
	if (!ptep)
		goto again;
	arch_enter_lazy_mmu_mode();

	for (; addr < end; addr += PAGE_SIZE, ptep++) {
		unsigned long mpfn = 0, pfn;
		struct page *page;
		swp_entry_t entry;
		pte_t pte;

		pte = ptep_get(ptep);

		if (pte_none(pte)) {
			if (vma_is_anonymous(vma)) {
				mpfn = MIGRATE_PFN_MIGRATE;
				migrate->cpages++;
			}
			goto next;
		}

		if (!pte_present(pte)) {
			 
			entry = pte_to_swp_entry(pte);
			if (!is_device_private_entry(entry))
				goto next;

			page = pfn_swap_entry_to_page(entry);
			if (!(migrate->flags &
				MIGRATE_VMA_SELECT_DEVICE_PRIVATE) ||
			    page->pgmap->owner != migrate->pgmap_owner)
				goto next;

			mpfn = migrate_pfn(page_to_pfn(page)) |
					MIGRATE_PFN_MIGRATE;
			if (is_writable_device_private_entry(entry))
				mpfn |= MIGRATE_PFN_WRITE;
		} else {
			pfn = pte_pfn(pte);
			if (is_zero_pfn(pfn) &&
			    (migrate->flags & MIGRATE_VMA_SELECT_SYSTEM)) {
				mpfn = MIGRATE_PFN_MIGRATE;
				migrate->cpages++;
				goto next;
			}
			page = vm_normal_page(migrate->vma, addr, pte);
			if (page && !is_zone_device_page(page) &&
			    !(migrate->flags & MIGRATE_VMA_SELECT_SYSTEM))
				goto next;
			else if (page && is_device_coherent_page(page) &&
			    (!(migrate->flags & MIGRATE_VMA_SELECT_DEVICE_COHERENT) ||
			     page->pgmap->owner != migrate->pgmap_owner))
				goto next;
			mpfn = migrate_pfn(pfn) | MIGRATE_PFN_MIGRATE;
			mpfn |= pte_write(pte) ? MIGRATE_PFN_WRITE : 0;
		}

		 
		if (!page || !page->mapping || PageTransCompound(page)) {
			mpfn = 0;
			goto next;
		}

		 
		get_page(page);

		 
		if (trylock_page(page)) {
			bool anon_exclusive;
			pte_t swp_pte;

			flush_cache_page(vma, addr, pte_pfn(pte));
			anon_exclusive = PageAnon(page) && PageAnonExclusive(page);
			if (anon_exclusive) {
				pte = ptep_clear_flush(vma, addr, ptep);

				if (page_try_share_anon_rmap(page)) {
					set_pte_at(mm, addr, ptep, pte);
					unlock_page(page);
					put_page(page);
					mpfn = 0;
					goto next;
				}
			} else {
				pte = ptep_get_and_clear(mm, addr, ptep);
			}

			migrate->cpages++;

			 
			if (pte_dirty(pte))
				folio_mark_dirty(page_folio(page));

			 
			if (mpfn & MIGRATE_PFN_WRITE)
				entry = make_writable_migration_entry(
							page_to_pfn(page));
			else if (anon_exclusive)
				entry = make_readable_exclusive_migration_entry(
							page_to_pfn(page));
			else
				entry = make_readable_migration_entry(
							page_to_pfn(page));
			if (pte_present(pte)) {
				if (pte_young(pte))
					entry = make_migration_entry_young(entry);
				if (pte_dirty(pte))
					entry = make_migration_entry_dirty(entry);
			}
			swp_pte = swp_entry_to_pte(entry);
			if (pte_present(pte)) {
				if (pte_soft_dirty(pte))
					swp_pte = pte_swp_mksoft_dirty(swp_pte);
				if (pte_uffd_wp(pte))
					swp_pte = pte_swp_mkuffd_wp(swp_pte);
			} else {
				if (pte_swp_soft_dirty(pte))
					swp_pte = pte_swp_mksoft_dirty(swp_pte);
				if (pte_swp_uffd_wp(pte))
					swp_pte = pte_swp_mkuffd_wp(swp_pte);
			}
			set_pte_at(mm, addr, ptep, swp_pte);

			 
			page_remove_rmap(page, vma, false);
			put_page(page);

			if (pte_present(pte))
				unmapped++;
		} else {
			put_page(page);
			mpfn = 0;
		}

next:
		migrate->dst[migrate->npages] = 0;
		migrate->src[migrate->npages++] = mpfn;
	}

	 
	if (unmapped)
		flush_tlb_range(walk->vma, start, end);

	arch_leave_lazy_mmu_mode();
	pte_unmap_unlock(ptep - 1, ptl);

	return 0;
}

static const struct mm_walk_ops migrate_vma_walk_ops = {
	.pmd_entry		= migrate_vma_collect_pmd,
	.pte_hole		= migrate_vma_collect_hole,
	.walk_lock		= PGWALK_RDLOCK,
};

 
static void migrate_vma_collect(struct migrate_vma *migrate)
{
	struct mmu_notifier_range range;

	 
	mmu_notifier_range_init_owner(&range, MMU_NOTIFY_MIGRATE, 0,
		migrate->vma->vm_mm, migrate->start, migrate->end,
		migrate->pgmap_owner);
	mmu_notifier_invalidate_range_start(&range);

	walk_page_range(migrate->vma->vm_mm, migrate->start, migrate->end,
			&migrate_vma_walk_ops, migrate);

	mmu_notifier_invalidate_range_end(&range);
	migrate->end = migrate->start + (migrate->npages << PAGE_SHIFT);
}

 
static bool migrate_vma_check_page(struct page *page, struct page *fault_page)
{
	 
	int extra = 1 + (page == fault_page);

	 
	if (PageCompound(page))
		return false;

	 
	if (is_zone_device_page(page))
		extra++;

	 
	if (page_mapping(page))
		extra += 1 + page_has_private(page);

	if ((page_count(page) - extra) > page_mapcount(page))
		return false;

	return true;
}

 
static unsigned long migrate_device_unmap(unsigned long *src_pfns,
					  unsigned long npages,
					  struct page *fault_page)
{
	unsigned long i, restore = 0;
	bool allow_drain = true;
	unsigned long unmapped = 0;

	lru_add_drain();

	for (i = 0; i < npages; i++) {
		struct page *page = migrate_pfn_to_page(src_pfns[i]);
		struct folio *folio;

		if (!page) {
			if (src_pfns[i] & MIGRATE_PFN_MIGRATE)
				unmapped++;
			continue;
		}

		 
		if (!is_zone_device_page(page)) {
			if (!PageLRU(page) && allow_drain) {
				 
				lru_add_drain_all();
				allow_drain = false;
			}

			if (!isolate_lru_page(page)) {
				src_pfns[i] &= ~MIGRATE_PFN_MIGRATE;
				restore++;
				continue;
			}

			 
			put_page(page);
		}

		folio = page_folio(page);
		if (folio_mapped(folio))
			try_to_migrate(folio, 0);

		if (page_mapped(page) ||
		    !migrate_vma_check_page(page, fault_page)) {
			if (!is_zone_device_page(page)) {
				get_page(page);
				putback_lru_page(page);
			}

			src_pfns[i] &= ~MIGRATE_PFN_MIGRATE;
			restore++;
			continue;
		}

		unmapped++;
	}

	for (i = 0; i < npages && restore; i++) {
		struct page *page = migrate_pfn_to_page(src_pfns[i]);
		struct folio *folio;

		if (!page || (src_pfns[i] & MIGRATE_PFN_MIGRATE))
			continue;

		folio = page_folio(page);
		remove_migration_ptes(folio, folio, false);

		src_pfns[i] = 0;
		folio_unlock(folio);
		folio_put(folio);
		restore--;
	}

	return unmapped;
}

 
static void migrate_vma_unmap(struct migrate_vma *migrate)
{
	migrate->cpages = migrate_device_unmap(migrate->src, migrate->npages,
					migrate->fault_page);
}

 
int migrate_vma_setup(struct migrate_vma *args)
{
	long nr_pages = (args->end - args->start) >> PAGE_SHIFT;

	args->start &= PAGE_MASK;
	args->end &= PAGE_MASK;
	if (!args->vma || is_vm_hugetlb_page(args->vma) ||
	    (args->vma->vm_flags & VM_SPECIAL) || vma_is_dax(args->vma))
		return -EINVAL;
	if (nr_pages <= 0)
		return -EINVAL;
	if (args->start < args->vma->vm_start ||
	    args->start >= args->vma->vm_end)
		return -EINVAL;
	if (args->end <= args->vma->vm_start || args->end > args->vma->vm_end)
		return -EINVAL;
	if (!args->src || !args->dst)
		return -EINVAL;
	if (args->fault_page && !is_device_private_page(args->fault_page))
		return -EINVAL;

	memset(args->src, 0, sizeof(*args->src) * nr_pages);
	args->cpages = 0;
	args->npages = 0;

	migrate_vma_collect(args);

	if (args->cpages)
		migrate_vma_unmap(args);

	 
	return 0;

}
EXPORT_SYMBOL(migrate_vma_setup);

 
static void migrate_vma_insert_page(struct migrate_vma *migrate,
				    unsigned long addr,
				    struct page *page,
				    unsigned long *src)
{
	struct vm_area_struct *vma = migrate->vma;
	struct mm_struct *mm = vma->vm_mm;
	bool flush = false;
	spinlock_t *ptl;
	pte_t entry;
	pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	pte_t orig_pte;

	 
	if (!vma_is_anonymous(vma))
		goto abort;

	pgdp = pgd_offset(mm, addr);
	p4dp = p4d_alloc(mm, pgdp, addr);
	if (!p4dp)
		goto abort;
	pudp = pud_alloc(mm, p4dp, addr);
	if (!pudp)
		goto abort;
	pmdp = pmd_alloc(mm, pudp, addr);
	if (!pmdp)
		goto abort;
	if (pmd_trans_huge(*pmdp) || pmd_devmap(*pmdp))
		goto abort;
	if (pte_alloc(mm, pmdp))
		goto abort;
	if (unlikely(anon_vma_prepare(vma)))
		goto abort;
	if (mem_cgroup_charge(page_folio(page), vma->vm_mm, GFP_KERNEL))
		goto abort;

	 
	__SetPageUptodate(page);

	if (is_device_private_page(page)) {
		swp_entry_t swp_entry;

		if (vma->vm_flags & VM_WRITE)
			swp_entry = make_writable_device_private_entry(
						page_to_pfn(page));
		else
			swp_entry = make_readable_device_private_entry(
						page_to_pfn(page));
		entry = swp_entry_to_pte(swp_entry);
	} else {
		if (is_zone_device_page(page) &&
		    !is_device_coherent_page(page)) {
			pr_warn_once("Unsupported ZONE_DEVICE page type.\n");
			goto abort;
		}
		entry = mk_pte(page, vma->vm_page_prot);
		if (vma->vm_flags & VM_WRITE)
			entry = pte_mkwrite(pte_mkdirty(entry), vma);
	}

	ptep = pte_offset_map_lock(mm, pmdp, addr, &ptl);
	if (!ptep)
		goto abort;
	orig_pte = ptep_get(ptep);

	if (check_stable_address_space(mm))
		goto unlock_abort;

	if (pte_present(orig_pte)) {
		unsigned long pfn = pte_pfn(orig_pte);

		if (!is_zero_pfn(pfn))
			goto unlock_abort;
		flush = true;
	} else if (!pte_none(orig_pte))
		goto unlock_abort;

	 
	if (userfaultfd_missing(vma))
		goto unlock_abort;

	inc_mm_counter(mm, MM_ANONPAGES);
	page_add_new_anon_rmap(page, vma, addr);
	if (!is_zone_device_page(page))
		lru_cache_add_inactive_or_unevictable(page, vma);
	get_page(page);

	if (flush) {
		flush_cache_page(vma, addr, pte_pfn(orig_pte));
		ptep_clear_flush(vma, addr, ptep);
		set_pte_at_notify(mm, addr, ptep, entry);
		update_mmu_cache(vma, addr, ptep);
	} else {
		 
		set_pte_at(mm, addr, ptep, entry);
		update_mmu_cache(vma, addr, ptep);
	}

	pte_unmap_unlock(ptep, ptl);
	*src = MIGRATE_PFN_MIGRATE;
	return;

unlock_abort:
	pte_unmap_unlock(ptep, ptl);
abort:
	*src &= ~MIGRATE_PFN_MIGRATE;
}

static void __migrate_device_pages(unsigned long *src_pfns,
				unsigned long *dst_pfns, unsigned long npages,
				struct migrate_vma *migrate)
{
	struct mmu_notifier_range range;
	unsigned long i;
	bool notified = false;

	for (i = 0; i < npages; i++) {
		struct page *newpage = migrate_pfn_to_page(dst_pfns[i]);
		struct page *page = migrate_pfn_to_page(src_pfns[i]);
		struct address_space *mapping;
		int r;

		if (!newpage) {
			src_pfns[i] &= ~MIGRATE_PFN_MIGRATE;
			continue;
		}

		if (!page) {
			unsigned long addr;

			if (!(src_pfns[i] & MIGRATE_PFN_MIGRATE))
				continue;

			 
			VM_BUG_ON(!migrate);
			addr = migrate->start + i*PAGE_SIZE;
			if (!notified) {
				notified = true;

				mmu_notifier_range_init_owner(&range,
					MMU_NOTIFY_MIGRATE, 0,
					migrate->vma->vm_mm, addr, migrate->end,
					migrate->pgmap_owner);
				mmu_notifier_invalidate_range_start(&range);
			}
			migrate_vma_insert_page(migrate, addr, newpage,
						&src_pfns[i]);
			continue;
		}

		mapping = page_mapping(page);

		if (is_device_private_page(newpage) ||
		    is_device_coherent_page(newpage)) {
			if (mapping) {
				struct folio *folio;

				folio = page_folio(page);

				 
				if (!folio_test_anon(folio) ||
				    !folio_free_swap(folio)) {
					src_pfns[i] &= ~MIGRATE_PFN_MIGRATE;
					continue;
				}
			}
		} else if (is_zone_device_page(newpage)) {
			 
			src_pfns[i] &= ~MIGRATE_PFN_MIGRATE;
			continue;
		}

		if (migrate && migrate->fault_page == page)
			r = migrate_folio_extra(mapping, page_folio(newpage),
						page_folio(page),
						MIGRATE_SYNC_NO_COPY, 1);
		else
			r = migrate_folio(mapping, page_folio(newpage),
					page_folio(page), MIGRATE_SYNC_NO_COPY);
		if (r != MIGRATEPAGE_SUCCESS)
			src_pfns[i] &= ~MIGRATE_PFN_MIGRATE;
	}

	if (notified)
		mmu_notifier_invalidate_range_end(&range);
}

 
void migrate_device_pages(unsigned long *src_pfns, unsigned long *dst_pfns,
			unsigned long npages)
{
	__migrate_device_pages(src_pfns, dst_pfns, npages, NULL);
}
EXPORT_SYMBOL(migrate_device_pages);

 
void migrate_vma_pages(struct migrate_vma *migrate)
{
	__migrate_device_pages(migrate->src, migrate->dst, migrate->npages, migrate);
}
EXPORT_SYMBOL(migrate_vma_pages);

 
void migrate_device_finalize(unsigned long *src_pfns,
			unsigned long *dst_pfns, unsigned long npages)
{
	unsigned long i;

	for (i = 0; i < npages; i++) {
		struct folio *dst, *src;
		struct page *newpage = migrate_pfn_to_page(dst_pfns[i]);
		struct page *page = migrate_pfn_to_page(src_pfns[i]);

		if (!page) {
			if (newpage) {
				unlock_page(newpage);
				put_page(newpage);
			}
			continue;
		}

		if (!(src_pfns[i] & MIGRATE_PFN_MIGRATE) || !newpage) {
			if (newpage) {
				unlock_page(newpage);
				put_page(newpage);
			}
			newpage = page;
		}

		src = page_folio(page);
		dst = page_folio(newpage);
		remove_migration_ptes(src, dst, false);
		folio_unlock(src);

		if (is_zone_device_page(page))
			put_page(page);
		else
			putback_lru_page(page);

		if (newpage != page) {
			unlock_page(newpage);
			if (is_zone_device_page(newpage))
				put_page(newpage);
			else
				putback_lru_page(newpage);
		}
	}
}
EXPORT_SYMBOL(migrate_device_finalize);

 
void migrate_vma_finalize(struct migrate_vma *migrate)
{
	migrate_device_finalize(migrate->src, migrate->dst, migrate->npages);
}
EXPORT_SYMBOL(migrate_vma_finalize);

 
int migrate_device_range(unsigned long *src_pfns, unsigned long start,
			unsigned long npages)
{
	unsigned long i, pfn;

	for (pfn = start, i = 0; i < npages; pfn++, i++) {
		struct page *page = pfn_to_page(pfn);

		if (!get_page_unless_zero(page)) {
			src_pfns[i] = 0;
			continue;
		}

		if (!trylock_page(page)) {
			src_pfns[i] = 0;
			put_page(page);
			continue;
		}

		src_pfns[i] = migrate_pfn(pfn) | MIGRATE_PFN_MIGRATE;
	}

	migrate_device_unmap(src_pfns, npages, NULL);

	return 0;
}
EXPORT_SYMBOL(migrate_device_range);

 
int migrate_device_coherent_page(struct page *page)
{
	unsigned long src_pfn, dst_pfn = 0;
	struct page *dpage;

	WARN_ON_ONCE(PageCompound(page));

	lock_page(page);
	src_pfn = migrate_pfn(page_to_pfn(page)) | MIGRATE_PFN_MIGRATE;

	 
	migrate_device_unmap(&src_pfn, 1, NULL);
	if (!(src_pfn & MIGRATE_PFN_MIGRATE))
		return -EBUSY;

	dpage = alloc_page(GFP_USER | __GFP_NOWARN);
	if (dpage) {
		lock_page(dpage);
		dst_pfn = migrate_pfn(page_to_pfn(dpage));
	}

	migrate_device_pages(&src_pfn, &dst_pfn, 1);
	if (src_pfn & MIGRATE_PFN_MIGRATE)
		copy_highpage(dpage, page);
	migrate_device_finalize(&src_pfn, &dst_pfn, 1);

	if (src_pfn & MIGRATE_PFN_MIGRATE)
		return 0;
	return -EBUSY;
}
