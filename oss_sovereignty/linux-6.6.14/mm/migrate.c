
 

#include <linux/migrate.h>
#include <linux/export.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/pagemap.h>
#include <linux/buffer_head.h>
#include <linux/mm_inline.h>
#include <linux/nsproxy.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/topology.h>
#include <linux/cpu.h>
#include <linux/cpuset.h>
#include <linux/writeback.h>
#include <linux/mempolicy.h>
#include <linux/vmalloc.h>
#include <linux/security.h>
#include <linux/backing-dev.h>
#include <linux/compaction.h>
#include <linux/syscalls.h>
#include <linux/compat.h>
#include <linux/hugetlb.h>
#include <linux/hugetlb_cgroup.h>
#include <linux/gfp.h>
#include <linux/pfn_t.h>
#include <linux/memremap.h>
#include <linux/userfaultfd_k.h>
#include <linux/balloon_compaction.h>
#include <linux/page_idle.h>
#include <linux/page_owner.h>
#include <linux/sched/mm.h>
#include <linux/ptrace.h>
#include <linux/oom.h>
#include <linux/memory.h>
#include <linux/random.h>
#include <linux/sched/sysctl.h>
#include <linux/memory-tiers.h>

#include <asm/tlbflush.h>

#include <trace/events/migrate.h>

#include "internal.h"

bool isolate_movable_page(struct page *page, isolate_mode_t mode)
{
	struct folio *folio = folio_get_nontail_page(page);
	const struct movable_operations *mops;

	 
	if (!folio)
		goto out;

	if (unlikely(folio_test_slab(folio)))
		goto out_putfolio;
	 
	smp_rmb();
	 
	if (unlikely(!__folio_test_movable(folio)))
		goto out_putfolio;
	 
	smp_rmb();
	if (unlikely(folio_test_slab(folio)))
		goto out_putfolio;

	 
	if (unlikely(!folio_trylock(folio)))
		goto out_putfolio;

	if (!folio_test_movable(folio) || folio_test_isolated(folio))
		goto out_no_isolated;

	mops = folio_movable_ops(folio);
	VM_BUG_ON_FOLIO(!mops, folio);

	if (!mops->isolate_page(&folio->page, mode))
		goto out_no_isolated;

	 
	WARN_ON_ONCE(folio_test_isolated(folio));
	folio_set_isolated(folio);
	folio_unlock(folio);

	return true;

out_no_isolated:
	folio_unlock(folio);
out_putfolio:
	folio_put(folio);
out:
	return false;
}

static void putback_movable_folio(struct folio *folio)
{
	const struct movable_operations *mops = folio_movable_ops(folio);

	mops->putback_page(&folio->page);
	folio_clear_isolated(folio);
}

 
void putback_movable_pages(struct list_head *l)
{
	struct folio *folio;
	struct folio *folio2;

	list_for_each_entry_safe(folio, folio2, l, lru) {
		if (unlikely(folio_test_hugetlb(folio))) {
			folio_putback_active_hugetlb(folio);
			continue;
		}
		list_del(&folio->lru);
		 
		if (unlikely(__folio_test_movable(folio))) {
			VM_BUG_ON_FOLIO(!folio_test_isolated(folio), folio);
			folio_lock(folio);
			if (folio_test_movable(folio))
				putback_movable_folio(folio);
			else
				folio_clear_isolated(folio);
			folio_unlock(folio);
			folio_put(folio);
		} else {
			node_stat_mod_folio(folio, NR_ISOLATED_ANON +
					folio_is_file_lru(folio), -folio_nr_pages(folio));
			folio_putback_lru(folio);
		}
	}
}

 
static bool remove_migration_pte(struct folio *folio,
		struct vm_area_struct *vma, unsigned long addr, void *old)
{
	DEFINE_FOLIO_VMA_WALK(pvmw, old, vma, addr, PVMW_SYNC | PVMW_MIGRATION);

	while (page_vma_mapped_walk(&pvmw)) {
		rmap_t rmap_flags = RMAP_NONE;
		pte_t old_pte;
		pte_t pte;
		swp_entry_t entry;
		struct page *new;
		unsigned long idx = 0;

		 
		if (folio_test_large(folio) && !folio_test_hugetlb(folio))
			idx = linear_page_index(vma, pvmw.address) - pvmw.pgoff;
		new = folio_page(folio, idx);

#ifdef CONFIG_ARCH_ENABLE_THP_MIGRATION
		 
		if (!pvmw.pte) {
			VM_BUG_ON_FOLIO(folio_test_hugetlb(folio) ||
					!folio_test_pmd_mappable(folio), folio);
			remove_migration_pmd(&pvmw, new);
			continue;
		}
#endif

		folio_get(folio);
		pte = mk_pte(new, READ_ONCE(vma->vm_page_prot));
		old_pte = ptep_get(pvmw.pte);
		if (pte_swp_soft_dirty(old_pte))
			pte = pte_mksoft_dirty(pte);

		entry = pte_to_swp_entry(old_pte);
		if (!is_migration_entry_young(entry))
			pte = pte_mkold(pte);
		if (folio_test_dirty(folio) && is_migration_entry_dirty(entry))
			pte = pte_mkdirty(pte);
		if (is_writable_migration_entry(entry))
			pte = pte_mkwrite(pte, vma);
		else if (pte_swp_uffd_wp(old_pte))
			pte = pte_mkuffd_wp(pte);

		if (folio_test_anon(folio) && !is_readable_migration_entry(entry))
			rmap_flags |= RMAP_EXCLUSIVE;

		if (unlikely(is_device_private_page(new))) {
			if (pte_write(pte))
				entry = make_writable_device_private_entry(
							page_to_pfn(new));
			else
				entry = make_readable_device_private_entry(
							page_to_pfn(new));
			pte = swp_entry_to_pte(entry);
			if (pte_swp_soft_dirty(old_pte))
				pte = pte_swp_mksoft_dirty(pte);
			if (pte_swp_uffd_wp(old_pte))
				pte = pte_swp_mkuffd_wp(pte);
		}

#ifdef CONFIG_HUGETLB_PAGE
		if (folio_test_hugetlb(folio)) {
			struct hstate *h = hstate_vma(vma);
			unsigned int shift = huge_page_shift(h);
			unsigned long psize = huge_page_size(h);

			pte = arch_make_huge_pte(pte, shift, vma->vm_flags);
			if (folio_test_anon(folio))
				hugepage_add_anon_rmap(new, vma, pvmw.address,
						       rmap_flags);
			else
				page_dup_file_rmap(new, true);
			set_huge_pte_at(vma->vm_mm, pvmw.address, pvmw.pte, pte,
					psize);
		} else
#endif
		{
			if (folio_test_anon(folio))
				page_add_anon_rmap(new, vma, pvmw.address,
						   rmap_flags);
			else
				page_add_file_rmap(new, vma, false);
			set_pte_at(vma->vm_mm, pvmw.address, pvmw.pte, pte);
		}
		if (vma->vm_flags & VM_LOCKED)
			mlock_drain_local();

		trace_remove_migration_pte(pvmw.address, pte_val(pte),
					   compound_order(new));

		 
		update_mmu_cache(vma, pvmw.address, pvmw.pte);
	}

	return true;
}

 
void remove_migration_ptes(struct folio *src, struct folio *dst, bool locked)
{
	struct rmap_walk_control rwc = {
		.rmap_one = remove_migration_pte,
		.arg = src,
	};

	if (locked)
		rmap_walk_locked(dst, &rwc);
	else
		rmap_walk(dst, &rwc);
}

 
void migration_entry_wait(struct mm_struct *mm, pmd_t *pmd,
			  unsigned long address)
{
	spinlock_t *ptl;
	pte_t *ptep;
	pte_t pte;
	swp_entry_t entry;

	ptep = pte_offset_map_lock(mm, pmd, address, &ptl);
	if (!ptep)
		return;

	pte = ptep_get(ptep);
	pte_unmap(ptep);

	if (!is_swap_pte(pte))
		goto out;

	entry = pte_to_swp_entry(pte);
	if (!is_migration_entry(entry))
		goto out;

	migration_entry_wait_on_locked(entry, ptl);
	return;
out:
	spin_unlock(ptl);
}

#ifdef CONFIG_HUGETLB_PAGE
 
void migration_entry_wait_huge(struct vm_area_struct *vma, pte_t *ptep)
{
	spinlock_t *ptl = huge_pte_lockptr(hstate_vma(vma), vma->vm_mm, ptep);
	pte_t pte;

	hugetlb_vma_assert_locked(vma);
	spin_lock(ptl);
	pte = huge_ptep_get(ptep);

	if (unlikely(!is_hugetlb_entry_migration(pte))) {
		spin_unlock(ptl);
		hugetlb_vma_unlock_read(vma);
	} else {
		 
		hugetlb_vma_unlock_read(vma);
		migration_entry_wait_on_locked(pte_to_swp_entry(pte), ptl);
	}
}
#endif

#ifdef CONFIG_ARCH_ENABLE_THP_MIGRATION
void pmd_migration_entry_wait(struct mm_struct *mm, pmd_t *pmd)
{
	spinlock_t *ptl;

	ptl = pmd_lock(mm, pmd);
	if (!is_pmd_migration_entry(*pmd))
		goto unlock;
	migration_entry_wait_on_locked(pmd_to_swp_entry(*pmd), ptl);
	return;
unlock:
	spin_unlock(ptl);
}
#endif

static int folio_expected_refs(struct address_space *mapping,
		struct folio *folio)
{
	int refs = 1;
	if (!mapping)
		return refs;

	refs += folio_nr_pages(folio);
	if (folio_test_private(folio))
		refs++;

	return refs;
}

 
int folio_migrate_mapping(struct address_space *mapping,
		struct folio *newfolio, struct folio *folio, int extra_count)
{
	XA_STATE(xas, &mapping->i_pages, folio_index(folio));
	struct zone *oldzone, *newzone;
	int dirty;
	int expected_count = folio_expected_refs(mapping, folio) + extra_count;
	long nr = folio_nr_pages(folio);
	long entries, i;

	if (!mapping) {
		 
		if (folio_ref_count(folio) != expected_count)
			return -EAGAIN;

		 
		newfolio->index = folio->index;
		newfolio->mapping = folio->mapping;
		if (folio_test_swapbacked(folio))
			__folio_set_swapbacked(newfolio);

		return MIGRATEPAGE_SUCCESS;
	}

	oldzone = folio_zone(folio);
	newzone = folio_zone(newfolio);

	xas_lock_irq(&xas);
	if (!folio_ref_freeze(folio, expected_count)) {
		xas_unlock_irq(&xas);
		return -EAGAIN;
	}

	 
	newfolio->index = folio->index;
	newfolio->mapping = folio->mapping;
	folio_ref_add(newfolio, nr);  
	if (folio_test_swapbacked(folio)) {
		__folio_set_swapbacked(newfolio);
		if (folio_test_swapcache(folio)) {
			folio_set_swapcache(newfolio);
			newfolio->private = folio_get_private(folio);
		}
		entries = nr;
	} else {
		VM_BUG_ON_FOLIO(folio_test_swapcache(folio), folio);
		entries = 1;
	}

	 
	dirty = folio_test_dirty(folio);
	if (dirty) {
		folio_clear_dirty(folio);
		folio_set_dirty(newfolio);
	}

	 
	for (i = 0; i < entries; i++) {
		xas_store(&xas, newfolio);
		xas_next(&xas);
	}

	 
	folio_ref_unfreeze(folio, expected_count - nr);

	xas_unlock(&xas);
	 

	 
	if (newzone != oldzone) {
		struct lruvec *old_lruvec, *new_lruvec;
		struct mem_cgroup *memcg;

		memcg = folio_memcg(folio);
		old_lruvec = mem_cgroup_lruvec(memcg, oldzone->zone_pgdat);
		new_lruvec = mem_cgroup_lruvec(memcg, newzone->zone_pgdat);

		__mod_lruvec_state(old_lruvec, NR_FILE_PAGES, -nr);
		__mod_lruvec_state(new_lruvec, NR_FILE_PAGES, nr);
		if (folio_test_swapbacked(folio) && !folio_test_swapcache(folio)) {
			__mod_lruvec_state(old_lruvec, NR_SHMEM, -nr);
			__mod_lruvec_state(new_lruvec, NR_SHMEM, nr);

			if (folio_test_pmd_mappable(folio)) {
				__mod_lruvec_state(old_lruvec, NR_SHMEM_THPS, -nr);
				__mod_lruvec_state(new_lruvec, NR_SHMEM_THPS, nr);
			}
		}
#ifdef CONFIG_SWAP
		if (folio_test_swapcache(folio)) {
			__mod_lruvec_state(old_lruvec, NR_SWAPCACHE, -nr);
			__mod_lruvec_state(new_lruvec, NR_SWAPCACHE, nr);
		}
#endif
		if (dirty && mapping_can_writeback(mapping)) {
			__mod_lruvec_state(old_lruvec, NR_FILE_DIRTY, -nr);
			__mod_zone_page_state(oldzone, NR_ZONE_WRITE_PENDING, -nr);
			__mod_lruvec_state(new_lruvec, NR_FILE_DIRTY, nr);
			__mod_zone_page_state(newzone, NR_ZONE_WRITE_PENDING, nr);
		}
	}
	local_irq_enable();

	return MIGRATEPAGE_SUCCESS;
}
EXPORT_SYMBOL(folio_migrate_mapping);

 
int migrate_huge_page_move_mapping(struct address_space *mapping,
				   struct folio *dst, struct folio *src)
{
	XA_STATE(xas, &mapping->i_pages, folio_index(src));
	int expected_count;

	xas_lock_irq(&xas);
	expected_count = 2 + folio_has_private(src);
	if (!folio_ref_freeze(src, expected_count)) {
		xas_unlock_irq(&xas);
		return -EAGAIN;
	}

	dst->index = src->index;
	dst->mapping = src->mapping;

	folio_get(dst);

	xas_store(&xas, dst);

	folio_ref_unfreeze(src, expected_count - 1);

	xas_unlock_irq(&xas);

	return MIGRATEPAGE_SUCCESS;
}

 
void folio_migrate_flags(struct folio *newfolio, struct folio *folio)
{
	int cpupid;

	if (folio_test_error(folio))
		folio_set_error(newfolio);
	if (folio_test_referenced(folio))
		folio_set_referenced(newfolio);
	if (folio_test_uptodate(folio))
		folio_mark_uptodate(newfolio);
	if (folio_test_clear_active(folio)) {
		VM_BUG_ON_FOLIO(folio_test_unevictable(folio), folio);
		folio_set_active(newfolio);
	} else if (folio_test_clear_unevictable(folio))
		folio_set_unevictable(newfolio);
	if (folio_test_workingset(folio))
		folio_set_workingset(newfolio);
	if (folio_test_checked(folio))
		folio_set_checked(newfolio);
	 
	if (folio_test_mappedtodisk(folio))
		folio_set_mappedtodisk(newfolio);

	 
	if (folio_test_dirty(folio))
		folio_set_dirty(newfolio);

	if (folio_test_young(folio))
		folio_set_young(newfolio);
	if (folio_test_idle(folio))
		folio_set_idle(newfolio);

	 
	cpupid = page_cpupid_xchg_last(&folio->page, -1);
	 
	if (sysctl_numa_balancing_mode & NUMA_BALANCING_MEMORY_TIERING) {
		bool f_toptier = node_is_toptier(page_to_nid(&folio->page));
		bool t_toptier = node_is_toptier(page_to_nid(&newfolio->page));

		if (f_toptier != t_toptier)
			cpupid = -1;
	}
	page_cpupid_xchg_last(&newfolio->page, cpupid);

	folio_migrate_ksm(newfolio, folio);
	 
	if (folio_test_swapcache(folio))
		folio_clear_swapcache(folio);
	folio_clear_private(folio);

	 
	if (!folio_test_hugetlb(folio))
		folio->private = NULL;

	 
	if (folio_test_writeback(newfolio))
		folio_end_writeback(newfolio);

	 
	if (folio_test_readahead(folio))
		folio_set_readahead(newfolio);

	folio_copy_owner(newfolio, folio);

	if (!folio_test_hugetlb(folio))
		mem_cgroup_migrate(folio, newfolio);
}
EXPORT_SYMBOL(folio_migrate_flags);

void folio_migrate_copy(struct folio *newfolio, struct folio *folio)
{
	folio_copy(newfolio, folio);
	folio_migrate_flags(newfolio, folio);
}
EXPORT_SYMBOL(folio_migrate_copy);

 

int migrate_folio_extra(struct address_space *mapping, struct folio *dst,
		struct folio *src, enum migrate_mode mode, int extra_count)
{
	int rc;

	BUG_ON(folio_test_writeback(src));	 

	rc = folio_migrate_mapping(mapping, dst, src, extra_count);

	if (rc != MIGRATEPAGE_SUCCESS)
		return rc;

	if (mode != MIGRATE_SYNC_NO_COPY)
		folio_migrate_copy(dst, src);
	else
		folio_migrate_flags(dst, src);
	return MIGRATEPAGE_SUCCESS;
}

 
int migrate_folio(struct address_space *mapping, struct folio *dst,
		struct folio *src, enum migrate_mode mode)
{
	return migrate_folio_extra(mapping, dst, src, mode, 0);
}
EXPORT_SYMBOL(migrate_folio);

#ifdef CONFIG_BUFFER_HEAD
 
static bool buffer_migrate_lock_buffers(struct buffer_head *head,
							enum migrate_mode mode)
{
	struct buffer_head *bh = head;
	struct buffer_head *failed_bh;

	do {
		if (!trylock_buffer(bh)) {
			if (mode == MIGRATE_ASYNC)
				goto unlock;
			if (mode == MIGRATE_SYNC_LIGHT && !buffer_uptodate(bh))
				goto unlock;
			lock_buffer(bh);
		}

		bh = bh->b_this_page;
	} while (bh != head);

	return true;

unlock:
	 
	failed_bh = bh;
	bh = head;
	while (bh != failed_bh) {
		unlock_buffer(bh);
		bh = bh->b_this_page;
	}

	return false;
}

static int __buffer_migrate_folio(struct address_space *mapping,
		struct folio *dst, struct folio *src, enum migrate_mode mode,
		bool check_refs)
{
	struct buffer_head *bh, *head;
	int rc;
	int expected_count;

	head = folio_buffers(src);
	if (!head)
		return migrate_folio(mapping, dst, src, mode);

	 
	expected_count = folio_expected_refs(mapping, src);
	if (folio_ref_count(src) != expected_count)
		return -EAGAIN;

	if (!buffer_migrate_lock_buffers(head, mode))
		return -EAGAIN;

	if (check_refs) {
		bool busy;
		bool invalidated = false;

recheck_buffers:
		busy = false;
		spin_lock(&mapping->private_lock);
		bh = head;
		do {
			if (atomic_read(&bh->b_count)) {
				busy = true;
				break;
			}
			bh = bh->b_this_page;
		} while (bh != head);
		if (busy) {
			if (invalidated) {
				rc = -EAGAIN;
				goto unlock_buffers;
			}
			spin_unlock(&mapping->private_lock);
			invalidate_bh_lrus();
			invalidated = true;
			goto recheck_buffers;
		}
	}

	rc = folio_migrate_mapping(mapping, dst, src, 0);
	if (rc != MIGRATEPAGE_SUCCESS)
		goto unlock_buffers;

	folio_attach_private(dst, folio_detach_private(src));

	bh = head;
	do {
		folio_set_bh(bh, dst, bh_offset(bh));
		bh = bh->b_this_page;
	} while (bh != head);

	if (mode != MIGRATE_SYNC_NO_COPY)
		folio_migrate_copy(dst, src);
	else
		folio_migrate_flags(dst, src);

	rc = MIGRATEPAGE_SUCCESS;
unlock_buffers:
	if (check_refs)
		spin_unlock(&mapping->private_lock);
	bh = head;
	do {
		unlock_buffer(bh);
		bh = bh->b_this_page;
	} while (bh != head);

	return rc;
}

 
int buffer_migrate_folio(struct address_space *mapping,
		struct folio *dst, struct folio *src, enum migrate_mode mode)
{
	return __buffer_migrate_folio(mapping, dst, src, mode, false);
}
EXPORT_SYMBOL(buffer_migrate_folio);

 
int buffer_migrate_folio_norefs(struct address_space *mapping,
		struct folio *dst, struct folio *src, enum migrate_mode mode)
{
	return __buffer_migrate_folio(mapping, dst, src, mode, true);
}
EXPORT_SYMBOL_GPL(buffer_migrate_folio_norefs);
#endif  

int filemap_migrate_folio(struct address_space *mapping,
		struct folio *dst, struct folio *src, enum migrate_mode mode)
{
	int ret;

	ret = folio_migrate_mapping(mapping, dst, src, 0);
	if (ret != MIGRATEPAGE_SUCCESS)
		return ret;

	if (folio_get_private(src))
		folio_attach_private(dst, folio_detach_private(src));

	if (mode != MIGRATE_SYNC_NO_COPY)
		folio_migrate_copy(dst, src);
	else
		folio_migrate_flags(dst, src);
	return MIGRATEPAGE_SUCCESS;
}
EXPORT_SYMBOL_GPL(filemap_migrate_folio);

 
static int writeout(struct address_space *mapping, struct folio *folio)
{
	struct writeback_control wbc = {
		.sync_mode = WB_SYNC_NONE,
		.nr_to_write = 1,
		.range_start = 0,
		.range_end = LLONG_MAX,
		.for_reclaim = 1
	};
	int rc;

	if (!mapping->a_ops->writepage)
		 
		return -EINVAL;

	if (!folio_clear_dirty_for_io(folio))
		 
		return -EAGAIN;

	 
	remove_migration_ptes(folio, folio, false);

	rc = mapping->a_ops->writepage(&folio->page, &wbc);

	if (rc != AOP_WRITEPAGE_ACTIVATE)
		 
		folio_lock(folio);

	return (rc < 0) ? -EIO : -EAGAIN;
}

 
static int fallback_migrate_folio(struct address_space *mapping,
		struct folio *dst, struct folio *src, enum migrate_mode mode)
{
	if (folio_test_dirty(src)) {
		 
		switch (mode) {
		case MIGRATE_SYNC:
		case MIGRATE_SYNC_NO_COPY:
			break;
		default:
			return -EBUSY;
		}
		return writeout(mapping, src);
	}

	 
	if (!filemap_release_folio(src, GFP_KERNEL))
		return mode == MIGRATE_SYNC ? -EAGAIN : -EBUSY;

	return migrate_folio(mapping, dst, src, mode);
}

 
static int move_to_new_folio(struct folio *dst, struct folio *src,
				enum migrate_mode mode)
{
	int rc = -EAGAIN;
	bool is_lru = !__PageMovable(&src->page);

	VM_BUG_ON_FOLIO(!folio_test_locked(src), src);
	VM_BUG_ON_FOLIO(!folio_test_locked(dst), dst);

	if (likely(is_lru)) {
		struct address_space *mapping = folio_mapping(src);

		if (!mapping)
			rc = migrate_folio(mapping, dst, src, mode);
		else if (mapping->a_ops->migrate_folio)
			 
			rc = mapping->a_ops->migrate_folio(mapping, dst, src,
								mode);
		else
			rc = fallback_migrate_folio(mapping, dst, src, mode);
	} else {
		const struct movable_operations *mops;

		 
		VM_BUG_ON_FOLIO(!folio_test_isolated(src), src);
		if (!folio_test_movable(src)) {
			rc = MIGRATEPAGE_SUCCESS;
			folio_clear_isolated(src);
			goto out;
		}

		mops = folio_movable_ops(src);
		rc = mops->migrate_page(&dst->page, &src->page, mode);
		WARN_ON_ONCE(rc == MIGRATEPAGE_SUCCESS &&
				!folio_test_isolated(src));
	}

	 
	if (rc == MIGRATEPAGE_SUCCESS) {
		if (__PageMovable(&src->page)) {
			VM_BUG_ON_FOLIO(!folio_test_isolated(src), src);

			 
			folio_clear_isolated(src);
		}

		 
		if (!folio_mapping_flags(src))
			src->mapping = NULL;

		if (likely(!folio_is_zone_device(dst)))
			flush_dcache_folio(dst);
	}
out:
	return rc;
}

 
union migration_ptr {
	struct anon_vma *anon_vma;
	struct address_space *mapping;
};
static void __migrate_folio_record(struct folio *dst,
				   unsigned long page_was_mapped,
				   struct anon_vma *anon_vma)
{
	union migration_ptr ptr = { .anon_vma = anon_vma };
	dst->mapping = ptr.mapping;
	dst->private = (void *)page_was_mapped;
}

static void __migrate_folio_extract(struct folio *dst,
				   int *page_was_mappedp,
				   struct anon_vma **anon_vmap)
{
	union migration_ptr ptr = { .mapping = dst->mapping };
	*anon_vmap = ptr.anon_vma;
	*page_was_mappedp = (unsigned long)dst->private;
	dst->mapping = NULL;
	dst->private = NULL;
}

 
static void migrate_folio_undo_src(struct folio *src,
				   int page_was_mapped,
				   struct anon_vma *anon_vma,
				   bool locked,
				   struct list_head *ret)
{
	if (page_was_mapped)
		remove_migration_ptes(src, src, false);
	 
	if (anon_vma)
		put_anon_vma(anon_vma);
	if (locked)
		folio_unlock(src);
	if (ret)
		list_move_tail(&src->lru, ret);
}

 
static void migrate_folio_undo_dst(struct folio *dst, bool locked,
		free_folio_t put_new_folio, unsigned long private)
{
	if (locked)
		folio_unlock(dst);
	if (put_new_folio)
		put_new_folio(dst, private);
	else
		folio_put(dst);
}

 
static void migrate_folio_done(struct folio *src,
			       enum migrate_reason reason)
{
	 
	if (likely(!__folio_test_movable(src)))
		mod_node_page_state(folio_pgdat(src), NR_ISOLATED_ANON +
				    folio_is_file_lru(src), -folio_nr_pages(src));

	if (reason != MR_MEMORY_FAILURE)
		 
		folio_put(src);
}

 
static int migrate_folio_unmap(new_folio_t get_new_folio,
		free_folio_t put_new_folio, unsigned long private,
		struct folio *src, struct folio **dstp, enum migrate_mode mode,
		enum migrate_reason reason, struct list_head *ret)
{
	struct folio *dst;
	int rc = -EAGAIN;
	int page_was_mapped = 0;
	struct anon_vma *anon_vma = NULL;
	bool is_lru = !__PageMovable(&src->page);
	bool locked = false;
	bool dst_locked = false;

	if (folio_ref_count(src) == 1) {
		 
		folio_clear_active(src);
		folio_clear_unevictable(src);
		 
		list_del(&src->lru);
		migrate_folio_done(src, reason);
		return MIGRATEPAGE_SUCCESS;
	}

	dst = get_new_folio(src, private);
	if (!dst)
		return -ENOMEM;
	*dstp = dst;

	dst->private = NULL;

	if (!folio_trylock(src)) {
		if (mode == MIGRATE_ASYNC)
			goto out;

		 
		if (current->flags & PF_MEMALLOC)
			goto out;

		 
		if (mode == MIGRATE_SYNC_LIGHT && !folio_test_uptodate(src))
			goto out;

		folio_lock(src);
	}
	locked = true;

	if (folio_test_writeback(src)) {
		 
		switch (mode) {
		case MIGRATE_SYNC:
		case MIGRATE_SYNC_NO_COPY:
			break;
		default:
			rc = -EBUSY;
			goto out;
		}
		folio_wait_writeback(src);
	}

	 
	if (folio_test_anon(src) && !folio_test_ksm(src))
		anon_vma = folio_get_anon_vma(src);

	 
	if (unlikely(!folio_trylock(dst)))
		goto out;
	dst_locked = true;

	if (unlikely(!is_lru)) {
		__migrate_folio_record(dst, page_was_mapped, anon_vma);
		return MIGRATEPAGE_UNMAP;
	}

	 
	if (!src->mapping) {
		if (folio_test_private(src)) {
			try_to_free_buffers(src);
			goto out;
		}
	} else if (folio_mapped(src)) {
		 
		VM_BUG_ON_FOLIO(folio_test_anon(src) &&
			       !folio_test_ksm(src) && !anon_vma, src);
		try_to_migrate(src, mode == MIGRATE_ASYNC ? TTU_BATCH_FLUSH : 0);
		page_was_mapped = 1;
	}

	if (!folio_mapped(src)) {
		__migrate_folio_record(dst, page_was_mapped, anon_vma);
		return MIGRATEPAGE_UNMAP;
	}

out:
	 
	if (rc == -EAGAIN)
		ret = NULL;

	migrate_folio_undo_src(src, page_was_mapped, anon_vma, locked, ret);
	migrate_folio_undo_dst(dst, dst_locked, put_new_folio, private);

	return rc;
}

 
static int migrate_folio_move(free_folio_t put_new_folio, unsigned long private,
			      struct folio *src, struct folio *dst,
			      enum migrate_mode mode, enum migrate_reason reason,
			      struct list_head *ret)
{
	int rc;
	int page_was_mapped = 0;
	struct anon_vma *anon_vma = NULL;
	bool is_lru = !__PageMovable(&src->page);
	struct list_head *prev;

	__migrate_folio_extract(dst, &page_was_mapped, &anon_vma);
	prev = dst->lru.prev;
	list_del(&dst->lru);

	rc = move_to_new_folio(dst, src, mode);
	if (rc)
		goto out;

	if (unlikely(!is_lru))
		goto out_unlock_both;

	 
	folio_add_lru(dst);
	if (page_was_mapped)
		lru_add_drain();

	if (page_was_mapped)
		remove_migration_ptes(src, dst, false);

out_unlock_both:
	folio_unlock(dst);
	set_page_owner_migrate_reason(&dst->page, reason);
	 
	folio_put(dst);

	 
	list_del(&src->lru);
	 
	if (anon_vma)
		put_anon_vma(anon_vma);
	folio_unlock(src);
	migrate_folio_done(src, reason);

	return rc;
out:
	 
	if (rc == -EAGAIN) {
		list_add(&dst->lru, prev);
		__migrate_folio_record(dst, page_was_mapped, anon_vma);
		return rc;
	}

	migrate_folio_undo_src(src, page_was_mapped, anon_vma, true, ret);
	migrate_folio_undo_dst(dst, true, put_new_folio, private);

	return rc;
}

 
static int unmap_and_move_huge_page(new_folio_t get_new_folio,
		free_folio_t put_new_folio, unsigned long private,
		struct folio *src, int force, enum migrate_mode mode,
		int reason, struct list_head *ret)
{
	struct folio *dst;
	int rc = -EAGAIN;
	int page_was_mapped = 0;
	struct anon_vma *anon_vma = NULL;
	struct address_space *mapping = NULL;

	if (folio_ref_count(src) == 1) {
		 
		folio_putback_active_hugetlb(src);
		return MIGRATEPAGE_SUCCESS;
	}

	dst = get_new_folio(src, private);
	if (!dst)
		return -ENOMEM;

	if (!folio_trylock(src)) {
		if (!force)
			goto out;
		switch (mode) {
		case MIGRATE_SYNC:
		case MIGRATE_SYNC_NO_COPY:
			break;
		default:
			goto out;
		}
		folio_lock(src);
	}

	 
	if (hugetlb_folio_subpool(src) && !folio_mapping(src)) {
		rc = -EBUSY;
		goto out_unlock;
	}

	if (folio_test_anon(src))
		anon_vma = folio_get_anon_vma(src);

	if (unlikely(!folio_trylock(dst)))
		goto put_anon;

	if (folio_mapped(src)) {
		enum ttu_flags ttu = 0;

		if (!folio_test_anon(src)) {
			 
			mapping = hugetlb_page_mapping_lock_write(&src->page);
			if (unlikely(!mapping))
				goto unlock_put_anon;

			ttu = TTU_RMAP_LOCKED;
		}

		try_to_migrate(src, ttu);
		page_was_mapped = 1;

		if (ttu & TTU_RMAP_LOCKED)
			i_mmap_unlock_write(mapping);
	}

	if (!folio_mapped(src))
		rc = move_to_new_folio(dst, src, mode);

	if (page_was_mapped)
		remove_migration_ptes(src,
			rc == MIGRATEPAGE_SUCCESS ? dst : src, false);

unlock_put_anon:
	folio_unlock(dst);

put_anon:
	if (anon_vma)
		put_anon_vma(anon_vma);

	if (rc == MIGRATEPAGE_SUCCESS) {
		move_hugetlb_state(src, dst, reason);
		put_new_folio = NULL;
	}

out_unlock:
	folio_unlock(src);
out:
	if (rc == MIGRATEPAGE_SUCCESS)
		folio_putback_active_hugetlb(src);
	else if (rc != -EAGAIN)
		list_move_tail(&src->lru, ret);

	 
	if (put_new_folio)
		put_new_folio(dst, private);
	else
		folio_putback_active_hugetlb(dst);

	return rc;
}

static inline int try_split_folio(struct folio *folio, struct list_head *split_folios)
{
	int rc;

	folio_lock(folio);
	rc = split_folio_to_list(folio, split_folios);
	folio_unlock(folio);
	if (!rc)
		list_move_tail(&folio->lru, split_folios);

	return rc;
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
#define NR_MAX_BATCHED_MIGRATION	HPAGE_PMD_NR
#else
#define NR_MAX_BATCHED_MIGRATION	512
#endif
#define NR_MAX_MIGRATE_PAGES_RETRY	10
#define NR_MAX_MIGRATE_ASYNC_RETRY	3
#define NR_MAX_MIGRATE_SYNC_RETRY					\
	(NR_MAX_MIGRATE_PAGES_RETRY - NR_MAX_MIGRATE_ASYNC_RETRY)

struct migrate_pages_stats {
	int nr_succeeded;	 
	int nr_failed_pages;	 
	int nr_thp_succeeded;	 
	int nr_thp_failed;	 
	int nr_thp_split;	 
};

 
static int migrate_hugetlbs(struct list_head *from, new_folio_t get_new_folio,
			    free_folio_t put_new_folio, unsigned long private,
			    enum migrate_mode mode, int reason,
			    struct migrate_pages_stats *stats,
			    struct list_head *ret_folios)
{
	int retry = 1;
	int nr_failed = 0;
	int nr_retry_pages = 0;
	int pass = 0;
	struct folio *folio, *folio2;
	int rc, nr_pages;

	for (pass = 0; pass < NR_MAX_MIGRATE_PAGES_RETRY && retry; pass++) {
		retry = 0;
		nr_retry_pages = 0;

		list_for_each_entry_safe(folio, folio2, from, lru) {
			if (!folio_test_hugetlb(folio))
				continue;

			nr_pages = folio_nr_pages(folio);

			cond_resched();

			 
			if (!hugepage_migration_supported(folio_hstate(folio))) {
				nr_failed++;
				stats->nr_failed_pages += nr_pages;
				list_move_tail(&folio->lru, ret_folios);
				continue;
			}

			rc = unmap_and_move_huge_page(get_new_folio,
						      put_new_folio, private,
						      folio, pass > 2, mode,
						      reason, ret_folios);
			 
			switch(rc) {
			case -ENOMEM:
				 
				stats->nr_failed_pages += nr_pages + nr_retry_pages;
				return -ENOMEM;
			case -EAGAIN:
				retry++;
				nr_retry_pages += nr_pages;
				break;
			case MIGRATEPAGE_SUCCESS:
				stats->nr_succeeded += nr_pages;
				break;
			default:
				 
				nr_failed++;
				stats->nr_failed_pages += nr_pages;
				break;
			}
		}
	}
	 
	nr_failed += retry;
	stats->nr_failed_pages += nr_retry_pages;

	return nr_failed;
}

 
static int migrate_pages_batch(struct list_head *from,
		new_folio_t get_new_folio, free_folio_t put_new_folio,
		unsigned long private, enum migrate_mode mode, int reason,
		struct list_head *ret_folios, struct list_head *split_folios,
		struct migrate_pages_stats *stats, int nr_pass)
{
	int retry = 1;
	int thp_retry = 1;
	int nr_failed = 0;
	int nr_retry_pages = 0;
	int pass = 0;
	bool is_thp = false;
	struct folio *folio, *folio2, *dst = NULL, *dst2;
	int rc, rc_saved = 0, nr_pages;
	LIST_HEAD(unmap_folios);
	LIST_HEAD(dst_folios);
	bool nosplit = (reason == MR_NUMA_MISPLACED);

	VM_WARN_ON_ONCE(mode != MIGRATE_ASYNC &&
			!list_empty(from) && !list_is_singular(from));

	for (pass = 0; pass < nr_pass && retry; pass++) {
		retry = 0;
		thp_retry = 0;
		nr_retry_pages = 0;

		list_for_each_entry_safe(folio, folio2, from, lru) {
			is_thp = folio_test_large(folio) && folio_test_pmd_mappable(folio);
			nr_pages = folio_nr_pages(folio);

			cond_resched();

			 
			if (!thp_migration_supported() && is_thp) {
				nr_failed++;
				stats->nr_thp_failed++;
				if (!try_split_folio(folio, split_folios)) {
					stats->nr_thp_split++;
					continue;
				}
				stats->nr_failed_pages += nr_pages;
				list_move_tail(&folio->lru, ret_folios);
				continue;
			}

			rc = migrate_folio_unmap(get_new_folio, put_new_folio,
					private, folio, &dst, mode, reason,
					ret_folios);
			 
			switch(rc) {
			case -ENOMEM:
				 
				nr_failed++;
				stats->nr_thp_failed += is_thp;
				 
				if (folio_test_large(folio) && !nosplit) {
					int ret = try_split_folio(folio, split_folios);

					if (!ret) {
						stats->nr_thp_split += is_thp;
						break;
					} else if (reason == MR_LONGTERM_PIN &&
						   ret == -EAGAIN) {
						 
						retry++;
						thp_retry += is_thp;
						nr_retry_pages += nr_pages;
						 
						nr_failed--;
						stats->nr_thp_failed -= is_thp;
						break;
					}
				}

				stats->nr_failed_pages += nr_pages + nr_retry_pages;
				 
				stats->nr_thp_failed += thp_retry;
				rc_saved = rc;
				if (list_empty(&unmap_folios))
					goto out;
				else
					goto move;
			case -EAGAIN:
				retry++;
				thp_retry += is_thp;
				nr_retry_pages += nr_pages;
				break;
			case MIGRATEPAGE_SUCCESS:
				stats->nr_succeeded += nr_pages;
				stats->nr_thp_succeeded += is_thp;
				break;
			case MIGRATEPAGE_UNMAP:
				list_move_tail(&folio->lru, &unmap_folios);
				list_add_tail(&dst->lru, &dst_folios);
				break;
			default:
				 
				nr_failed++;
				stats->nr_thp_failed += is_thp;
				stats->nr_failed_pages += nr_pages;
				break;
			}
		}
	}
	nr_failed += retry;
	stats->nr_thp_failed += thp_retry;
	stats->nr_failed_pages += nr_retry_pages;
move:
	 
	try_to_unmap_flush();

	retry = 1;
	for (pass = 0; pass < nr_pass && retry; pass++) {
		retry = 0;
		thp_retry = 0;
		nr_retry_pages = 0;

		dst = list_first_entry(&dst_folios, struct folio, lru);
		dst2 = list_next_entry(dst, lru);
		list_for_each_entry_safe(folio, folio2, &unmap_folios, lru) {
			is_thp = folio_test_large(folio) && folio_test_pmd_mappable(folio);
			nr_pages = folio_nr_pages(folio);

			cond_resched();

			rc = migrate_folio_move(put_new_folio, private,
						folio, dst, mode,
						reason, ret_folios);
			 
			switch(rc) {
			case -EAGAIN:
				retry++;
				thp_retry += is_thp;
				nr_retry_pages += nr_pages;
				break;
			case MIGRATEPAGE_SUCCESS:
				stats->nr_succeeded += nr_pages;
				stats->nr_thp_succeeded += is_thp;
				break;
			default:
				nr_failed++;
				stats->nr_thp_failed += is_thp;
				stats->nr_failed_pages += nr_pages;
				break;
			}
			dst = dst2;
			dst2 = list_next_entry(dst, lru);
		}
	}
	nr_failed += retry;
	stats->nr_thp_failed += thp_retry;
	stats->nr_failed_pages += nr_retry_pages;

	rc = rc_saved ? : nr_failed;
out:
	 
	dst = list_first_entry(&dst_folios, struct folio, lru);
	dst2 = list_next_entry(dst, lru);
	list_for_each_entry_safe(folio, folio2, &unmap_folios, lru) {
		int page_was_mapped = 0;
		struct anon_vma *anon_vma = NULL;

		__migrate_folio_extract(dst, &page_was_mapped, &anon_vma);
		migrate_folio_undo_src(folio, page_was_mapped, anon_vma,
				       true, ret_folios);
		list_del(&dst->lru);
		migrate_folio_undo_dst(dst, true, put_new_folio, private);
		dst = dst2;
		dst2 = list_next_entry(dst, lru);
	}

	return rc;
}

static int migrate_pages_sync(struct list_head *from, new_folio_t get_new_folio,
		free_folio_t put_new_folio, unsigned long private,
		enum migrate_mode mode, int reason,
		struct list_head *ret_folios, struct list_head *split_folios,
		struct migrate_pages_stats *stats)
{
	int rc, nr_failed = 0;
	LIST_HEAD(folios);
	struct migrate_pages_stats astats;

	memset(&astats, 0, sizeof(astats));
	 
	rc = migrate_pages_batch(from, get_new_folio, put_new_folio, private, MIGRATE_ASYNC,
				 reason, &folios, split_folios, &astats,
				 NR_MAX_MIGRATE_ASYNC_RETRY);
	stats->nr_succeeded += astats.nr_succeeded;
	stats->nr_thp_succeeded += astats.nr_thp_succeeded;
	stats->nr_thp_split += astats.nr_thp_split;
	if (rc < 0) {
		stats->nr_failed_pages += astats.nr_failed_pages;
		stats->nr_thp_failed += astats.nr_thp_failed;
		list_splice_tail(&folios, ret_folios);
		return rc;
	}
	stats->nr_thp_failed += astats.nr_thp_split;
	nr_failed += astats.nr_thp_split;
	 
	list_splice_tail_init(&folios, from);
	while (!list_empty(from)) {
		list_move(from->next, &folios);
		rc = migrate_pages_batch(&folios, get_new_folio, put_new_folio,
					 private, mode, reason, ret_folios,
					 split_folios, stats, NR_MAX_MIGRATE_SYNC_RETRY);
		list_splice_tail_init(&folios, ret_folios);
		if (rc < 0)
			return rc;
		nr_failed += rc;
	}

	return nr_failed;
}

 
int migrate_pages(struct list_head *from, new_folio_t get_new_folio,
		free_folio_t put_new_folio, unsigned long private,
		enum migrate_mode mode, int reason, unsigned int *ret_succeeded)
{
	int rc, rc_gather;
	int nr_pages;
	struct folio *folio, *folio2;
	LIST_HEAD(folios);
	LIST_HEAD(ret_folios);
	LIST_HEAD(split_folios);
	struct migrate_pages_stats stats;

	trace_mm_migrate_pages_start(mode, reason);

	memset(&stats, 0, sizeof(stats));

	rc_gather = migrate_hugetlbs(from, get_new_folio, put_new_folio, private,
				     mode, reason, &stats, &ret_folios);
	if (rc_gather < 0)
		goto out;

again:
	nr_pages = 0;
	list_for_each_entry_safe(folio, folio2, from, lru) {
		 
		if (folio_test_hugetlb(folio)) {
			list_move_tail(&folio->lru, &ret_folios);
			continue;
		}

		nr_pages += folio_nr_pages(folio);
		if (nr_pages >= NR_MAX_BATCHED_MIGRATION)
			break;
	}
	if (nr_pages >= NR_MAX_BATCHED_MIGRATION)
		list_cut_before(&folios, from, &folio2->lru);
	else
		list_splice_init(from, &folios);
	if (mode == MIGRATE_ASYNC)
		rc = migrate_pages_batch(&folios, get_new_folio, put_new_folio,
				private, mode, reason, &ret_folios,
				&split_folios, &stats,
				NR_MAX_MIGRATE_PAGES_RETRY);
	else
		rc = migrate_pages_sync(&folios, get_new_folio, put_new_folio,
				private, mode, reason, &ret_folios,
				&split_folios, &stats);
	list_splice_tail_init(&folios, &ret_folios);
	if (rc < 0) {
		rc_gather = rc;
		list_splice_tail(&split_folios, &ret_folios);
		goto out;
	}
	if (!list_empty(&split_folios)) {
		 
		migrate_pages_batch(&split_folios, get_new_folio,
				put_new_folio, private, MIGRATE_ASYNC, reason,
				&ret_folios, NULL, &stats, 1);
		list_splice_tail_init(&split_folios, &ret_folios);
	}
	rc_gather += rc;
	if (!list_empty(from))
		goto again;
out:
	 
	list_splice(&ret_folios, from);

	 
	if (list_empty(from))
		rc_gather = 0;

	count_vm_events(PGMIGRATE_SUCCESS, stats.nr_succeeded);
	count_vm_events(PGMIGRATE_FAIL, stats.nr_failed_pages);
	count_vm_events(THP_MIGRATION_SUCCESS, stats.nr_thp_succeeded);
	count_vm_events(THP_MIGRATION_FAIL, stats.nr_thp_failed);
	count_vm_events(THP_MIGRATION_SPLIT, stats.nr_thp_split);
	trace_mm_migrate_pages(stats.nr_succeeded, stats.nr_failed_pages,
			       stats.nr_thp_succeeded, stats.nr_thp_failed,
			       stats.nr_thp_split, mode, reason);

	if (ret_succeeded)
		*ret_succeeded = stats.nr_succeeded;

	return rc_gather;
}

struct folio *alloc_migration_target(struct folio *src, unsigned long private)
{
	struct migration_target_control *mtc;
	gfp_t gfp_mask;
	unsigned int order = 0;
	int nid;
	int zidx;

	mtc = (struct migration_target_control *)private;
	gfp_mask = mtc->gfp_mask;
	nid = mtc->nid;
	if (nid == NUMA_NO_NODE)
		nid = folio_nid(src);

	if (folio_test_hugetlb(src)) {
		struct hstate *h = folio_hstate(src);

		gfp_mask = htlb_modify_alloc_mask(h, gfp_mask);
		return alloc_hugetlb_folio_nodemask(h, nid,
						mtc->nmask, gfp_mask);
	}

	if (folio_test_large(src)) {
		 
		gfp_mask &= ~__GFP_RECLAIM;
		gfp_mask |= GFP_TRANSHUGE;
		order = folio_order(src);
	}
	zidx = zone_idx(folio_zone(src));
	if (is_highmem_idx(zidx) || zidx == ZONE_MOVABLE)
		gfp_mask |= __GFP_HIGHMEM;

	return __folio_alloc(gfp_mask, order, nid, mtc->nmask);
}

#ifdef CONFIG_NUMA

static int store_status(int __user *status, int start, int value, int nr)
{
	while (nr-- > 0) {
		if (put_user(value, status + start))
			return -EFAULT;
		start++;
	}

	return 0;
}

static int do_move_pages_to_node(struct mm_struct *mm,
		struct list_head *pagelist, int node)
{
	int err;
	struct migration_target_control mtc = {
		.nid = node,
		.gfp_mask = GFP_HIGHUSER_MOVABLE | __GFP_THISNODE,
	};

	err = migrate_pages(pagelist, alloc_migration_target, NULL,
		(unsigned long)&mtc, MIGRATE_SYNC, MR_SYSCALL, NULL);
	if (err)
		putback_movable_pages(pagelist);
	return err;
}

 
static int add_page_for_migration(struct mm_struct *mm, const void __user *p,
		int node, struct list_head *pagelist, bool migrate_all)
{
	struct vm_area_struct *vma;
	unsigned long addr;
	struct page *page;
	int err;
	bool isolated;

	mmap_read_lock(mm);
	addr = (unsigned long)untagged_addr_remote(mm, p);

	err = -EFAULT;
	vma = vma_lookup(mm, addr);
	if (!vma || !vma_migratable(vma))
		goto out;

	 
	page = follow_page(vma, addr, FOLL_GET | FOLL_DUMP);

	err = PTR_ERR(page);
	if (IS_ERR(page))
		goto out;

	err = -ENOENT;
	if (!page)
		goto out;

	if (is_zone_device_page(page))
		goto out_putpage;

	err = 0;
	if (page_to_nid(page) == node)
		goto out_putpage;

	err = -EACCES;
	if (page_mapcount(page) > 1 && !migrate_all)
		goto out_putpage;

	if (PageHuge(page)) {
		if (PageHead(page)) {
			isolated = isolate_hugetlb(page_folio(page), pagelist);
			err = isolated ? 1 : -EBUSY;
		}
	} else {
		struct page *head;

		head = compound_head(page);
		isolated = isolate_lru_page(head);
		if (!isolated) {
			err = -EBUSY;
			goto out_putpage;
		}

		err = 1;
		list_add_tail(&head->lru, pagelist);
		mod_node_page_state(page_pgdat(head),
			NR_ISOLATED_ANON + page_is_file_lru(head),
			thp_nr_pages(head));
	}
out_putpage:
	 
	put_page(page);
out:
	mmap_read_unlock(mm);
	return err;
}

static int move_pages_and_store_status(struct mm_struct *mm, int node,
		struct list_head *pagelist, int __user *status,
		int start, int i, unsigned long nr_pages)
{
	int err;

	if (list_empty(pagelist))
		return 0;

	err = do_move_pages_to_node(mm, pagelist, node);
	if (err) {
		 
		if (err > 0)
			err += nr_pages - i;
		return err;
	}
	return store_status(status, start, node, i - start);
}

 
static int do_pages_move(struct mm_struct *mm, nodemask_t task_nodes,
			 unsigned long nr_pages,
			 const void __user * __user *pages,
			 const int __user *nodes,
			 int __user *status, int flags)
{
	compat_uptr_t __user *compat_pages = (void __user *)pages;
	int current_node = NUMA_NO_NODE;
	LIST_HEAD(pagelist);
	int start, i;
	int err = 0, err1;

	lru_cache_disable();

	for (i = start = 0; i < nr_pages; i++) {
		const void __user *p;
		int node;

		err = -EFAULT;
		if (in_compat_syscall()) {
			compat_uptr_t cp;

			if (get_user(cp, compat_pages + i))
				goto out_flush;

			p = compat_ptr(cp);
		} else {
			if (get_user(p, pages + i))
				goto out_flush;
		}
		if (get_user(node, nodes + i))
			goto out_flush;

		err = -ENODEV;
		if (node < 0 || node >= MAX_NUMNODES)
			goto out_flush;
		if (!node_state(node, N_MEMORY))
			goto out_flush;

		err = -EACCES;
		if (!node_isset(node, task_nodes))
			goto out_flush;

		if (current_node == NUMA_NO_NODE) {
			current_node = node;
			start = i;
		} else if (node != current_node) {
			err = move_pages_and_store_status(mm, current_node,
					&pagelist, status, start, i, nr_pages);
			if (err)
				goto out;
			start = i;
			current_node = node;
		}

		 
		err = add_page_for_migration(mm, p, current_node, &pagelist,
					     flags & MPOL_MF_MOVE_ALL);

		if (err > 0) {
			 
			continue;
		}

		 
		if (err == -EEXIST)
			err = -EFAULT;

		 
		err = store_status(status, i, err ? : current_node, 1);
		if (err)
			goto out_flush;

		err = move_pages_and_store_status(mm, current_node, &pagelist,
				status, start, i, nr_pages);
		if (err) {
			 
			if (err > 0)
				err--;
			goto out;
		}
		current_node = NUMA_NO_NODE;
	}
out_flush:
	 
	err1 = move_pages_and_store_status(mm, current_node, &pagelist,
				status, start, i, nr_pages);
	if (err >= 0)
		err = err1;
out:
	lru_cache_enable();
	return err;
}

 
static void do_pages_stat_array(struct mm_struct *mm, unsigned long nr_pages,
				const void __user **pages, int *status)
{
	unsigned long i;

	mmap_read_lock(mm);

	for (i = 0; i < nr_pages; i++) {
		unsigned long addr = (unsigned long)(*pages);
		struct vm_area_struct *vma;
		struct page *page;
		int err = -EFAULT;

		vma = vma_lookup(mm, addr);
		if (!vma)
			goto set_status;

		 
		page = follow_page(vma, addr, FOLL_GET | FOLL_DUMP);

		err = PTR_ERR(page);
		if (IS_ERR(page))
			goto set_status;

		err = -ENOENT;
		if (!page)
			goto set_status;

		if (!is_zone_device_page(page))
			err = page_to_nid(page);

		put_page(page);
set_status:
		*status = err;

		pages++;
		status++;
	}

	mmap_read_unlock(mm);
}

static int get_compat_pages_array(const void __user *chunk_pages[],
				  const void __user * __user *pages,
				  unsigned long chunk_nr)
{
	compat_uptr_t __user *pages32 = (compat_uptr_t __user *)pages;
	compat_uptr_t p;
	int i;

	for (i = 0; i < chunk_nr; i++) {
		if (get_user(p, pages32 + i))
			return -EFAULT;
		chunk_pages[i] = compat_ptr(p);
	}

	return 0;
}

 
static int do_pages_stat(struct mm_struct *mm, unsigned long nr_pages,
			 const void __user * __user *pages,
			 int __user *status)
{
#define DO_PAGES_STAT_CHUNK_NR 16UL
	const void __user *chunk_pages[DO_PAGES_STAT_CHUNK_NR];
	int chunk_status[DO_PAGES_STAT_CHUNK_NR];

	while (nr_pages) {
		unsigned long chunk_nr = min(nr_pages, DO_PAGES_STAT_CHUNK_NR);

		if (in_compat_syscall()) {
			if (get_compat_pages_array(chunk_pages, pages,
						   chunk_nr))
				break;
		} else {
			if (copy_from_user(chunk_pages, pages,
				      chunk_nr * sizeof(*chunk_pages)))
				break;
		}

		do_pages_stat_array(mm, chunk_nr, chunk_pages, chunk_status);

		if (copy_to_user(status, chunk_status, chunk_nr * sizeof(*status)))
			break;

		pages += chunk_nr;
		status += chunk_nr;
		nr_pages -= chunk_nr;
	}
	return nr_pages ? -EFAULT : 0;
}

static struct mm_struct *find_mm_struct(pid_t pid, nodemask_t *mem_nodes)
{
	struct task_struct *task;
	struct mm_struct *mm;

	 
	if (!pid) {
		mmget(current->mm);
		*mem_nodes = cpuset_mems_allowed(current);
		return current->mm;
	}

	 
	rcu_read_lock();
	task = find_task_by_vpid(pid);
	if (!task) {
		rcu_read_unlock();
		return ERR_PTR(-ESRCH);
	}
	get_task_struct(task);

	 
	if (!ptrace_may_access(task, PTRACE_MODE_READ_REALCREDS)) {
		rcu_read_unlock();
		mm = ERR_PTR(-EPERM);
		goto out;
	}
	rcu_read_unlock();

	mm = ERR_PTR(security_task_movememory(task));
	if (IS_ERR(mm))
		goto out;
	*mem_nodes = cpuset_mems_allowed(task);
	mm = get_task_mm(task);
out:
	put_task_struct(task);
	if (!mm)
		mm = ERR_PTR(-EINVAL);
	return mm;
}

 
static int kernel_move_pages(pid_t pid, unsigned long nr_pages,
			     const void __user * __user *pages,
			     const int __user *nodes,
			     int __user *status, int flags)
{
	struct mm_struct *mm;
	int err;
	nodemask_t task_nodes;

	 
	if (flags & ~(MPOL_MF_MOVE|MPOL_MF_MOVE_ALL))
		return -EINVAL;

	if ((flags & MPOL_MF_MOVE_ALL) && !capable(CAP_SYS_NICE))
		return -EPERM;

	mm = find_mm_struct(pid, &task_nodes);
	if (IS_ERR(mm))
		return PTR_ERR(mm);

	if (nodes)
		err = do_pages_move(mm, task_nodes, nr_pages, pages,
				    nodes, status, flags);
	else
		err = do_pages_stat(mm, nr_pages, pages, status);

	mmput(mm);
	return err;
}

SYSCALL_DEFINE6(move_pages, pid_t, pid, unsigned long, nr_pages,
		const void __user * __user *, pages,
		const int __user *, nodes,
		int __user *, status, int, flags)
{
	return kernel_move_pages(pid, nr_pages, pages, nodes, status, flags);
}

#ifdef CONFIG_NUMA_BALANCING
 
static bool migrate_balanced_pgdat(struct pglist_data *pgdat,
				   unsigned long nr_migrate_pages)
{
	int z;

	for (z = pgdat->nr_zones - 1; z >= 0; z--) {
		struct zone *zone = pgdat->node_zones + z;

		if (!managed_zone(zone))
			continue;

		 
		if (!zone_watermark_ok(zone, 0,
				       high_wmark_pages(zone) +
				       nr_migrate_pages,
				       ZONE_MOVABLE, 0))
			continue;
		return true;
	}
	return false;
}

static struct folio *alloc_misplaced_dst_folio(struct folio *src,
					   unsigned long data)
{
	int nid = (int) data;
	int order = folio_order(src);
	gfp_t gfp = __GFP_THISNODE;

	if (order > 0)
		gfp |= GFP_TRANSHUGE_LIGHT;
	else {
		gfp |= GFP_HIGHUSER_MOVABLE | __GFP_NOMEMALLOC | __GFP_NORETRY |
			__GFP_NOWARN;
		gfp &= ~__GFP_RECLAIM;
	}
	return __folio_alloc_node(gfp, order, nid);
}

static int numamigrate_isolate_page(pg_data_t *pgdat, struct page *page)
{
	int nr_pages = thp_nr_pages(page);
	int order = compound_order(page);

	VM_BUG_ON_PAGE(order && !PageTransHuge(page), page);

	 
	if (PageTransHuge(page) && total_mapcount(page) > 1)
		return 0;

	 
	if (!migrate_balanced_pgdat(pgdat, nr_pages)) {
		int z;

		if (!(sysctl_numa_balancing_mode & NUMA_BALANCING_MEMORY_TIERING))
			return 0;
		for (z = pgdat->nr_zones - 1; z >= 0; z--) {
			if (managed_zone(pgdat->node_zones + z))
				break;
		}
		wakeup_kswapd(pgdat->node_zones + z, 0, order, ZONE_MOVABLE);
		return 0;
	}

	if (!isolate_lru_page(page))
		return 0;

	mod_node_page_state(page_pgdat(page), NR_ISOLATED_ANON + page_is_file_lru(page),
			    nr_pages);

	 
	put_page(page);
	return 1;
}

 
int migrate_misplaced_page(struct page *page, struct vm_area_struct *vma,
			   int node)
{
	pg_data_t *pgdat = NODE_DATA(node);
	int isolated;
	int nr_remaining;
	unsigned int nr_succeeded;
	LIST_HEAD(migratepages);
	int nr_pages = thp_nr_pages(page);

	 
	if (page_mapcount(page) != 1 && page_is_file_lru(page) &&
	    (vma->vm_flags & VM_EXEC))
		goto out;

	 
	if (page_is_file_lru(page) && PageDirty(page))
		goto out;

	isolated = numamigrate_isolate_page(pgdat, page);
	if (!isolated)
		goto out;

	list_add(&page->lru, &migratepages);
	nr_remaining = migrate_pages(&migratepages, alloc_misplaced_dst_folio,
				     NULL, node, MIGRATE_ASYNC,
				     MR_NUMA_MISPLACED, &nr_succeeded);
	if (nr_remaining) {
		if (!list_empty(&migratepages)) {
			list_del(&page->lru);
			mod_node_page_state(page_pgdat(page), NR_ISOLATED_ANON +
					page_is_file_lru(page), -nr_pages);
			putback_lru_page(page);
		}
		isolated = 0;
	}
	if (nr_succeeded) {
		count_vm_numa_events(NUMA_PAGE_MIGRATE, nr_succeeded);
		if (!node_is_toptier(page_to_nid(page)) && node_is_toptier(node))
			mod_node_page_state(pgdat, PGPROMOTE_SUCCESS,
					    nr_succeeded);
	}
	BUG_ON(!list_empty(&migratepages));
	return isolated;

out:
	put_page(page);
	return 0;
}
#endif  
#endif  
