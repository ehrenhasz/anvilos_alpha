
 

#include <linux/pagewalk.h>
#include <linux/hugetlb.h>
#include <linux/shm.h>
#include <linux/mman.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/security.h>
#include <linux/mempolicy.h>
#include <linux/personality.h>
#include <linux/syscalls.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/mmu_notifier.h>
#include <linux/migrate.h>
#include <linux/perf_event.h>
#include <linux/pkeys.h>
#include <linux/ksm.h>
#include <linux/uaccess.h>
#include <linux/mm_inline.h>
#include <linux/pgtable.h>
#include <linux/sched/sysctl.h>
#include <linux/userfaultfd_k.h>
#include <linux/memory-tiers.h>
#include <asm/cacheflush.h>
#include <asm/mmu_context.h>
#include <asm/tlbflush.h>
#include <asm/tlb.h>

#include "internal.h"

bool can_change_pte_writable(struct vm_area_struct *vma, unsigned long addr,
			     pte_t pte)
{
	struct page *page;

	if (WARN_ON_ONCE(!(vma->vm_flags & VM_WRITE)))
		return false;

	 
	if (pte_protnone(pte))
		return false;

	 
	if (vma_soft_dirty_enabled(vma) && !pte_soft_dirty(pte))
		return false;

	 
	if (userfaultfd_pte_wp(vma, pte))
		return false;

	if (!(vma->vm_flags & VM_SHARED)) {
		 
		page = vm_normal_page(vma, addr, pte);
		return page && PageAnon(page) && PageAnonExclusive(page);
	}

	 
	return pte_dirty(pte);
}

static long change_pte_range(struct mmu_gather *tlb,
		struct vm_area_struct *vma, pmd_t *pmd, unsigned long addr,
		unsigned long end, pgprot_t newprot, unsigned long cp_flags)
{
	pte_t *pte, oldpte;
	spinlock_t *ptl;
	long pages = 0;
	int target_node = NUMA_NO_NODE;
	bool prot_numa = cp_flags & MM_CP_PROT_NUMA;
	bool uffd_wp = cp_flags & MM_CP_UFFD_WP;
	bool uffd_wp_resolve = cp_flags & MM_CP_UFFD_WP_RESOLVE;

	tlb_change_page_size(tlb, PAGE_SIZE);
	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	if (!pte)
		return -EAGAIN;

	 
	if (prot_numa && !(vma->vm_flags & VM_SHARED) &&
	    atomic_read(&vma->vm_mm->mm_users) == 1)
		target_node = numa_node_id();

	flush_tlb_batched_pending(vma->vm_mm);
	arch_enter_lazy_mmu_mode();
	do {
		oldpte = ptep_get(pte);
		if (pte_present(oldpte)) {
			pte_t ptent;

			 
			if (prot_numa) {
				struct page *page;
				int nid;
				bool toptier;

				 
				if (pte_protnone(oldpte))
					continue;

				page = vm_normal_page(vma, addr, oldpte);
				if (!page || is_zone_device_page(page) || PageKsm(page))
					continue;

				 
				if (is_cow_mapping(vma->vm_flags) &&
				    page_count(page) != 1)
					continue;

				 
				if (page_is_file_lru(page) && PageDirty(page))
					continue;

				 
				nid = page_to_nid(page);
				if (target_node == nid)
					continue;
				toptier = node_is_toptier(nid);

				 
				if (!(sysctl_numa_balancing_mode & NUMA_BALANCING_NORMAL) &&
				    toptier)
					continue;
				if (sysctl_numa_balancing_mode & NUMA_BALANCING_MEMORY_TIERING &&
				    !toptier)
					xchg_page_access_time(page,
						jiffies_to_msecs(jiffies));
			}

			oldpte = ptep_modify_prot_start(vma, addr, pte);
			ptent = pte_modify(oldpte, newprot);

			if (uffd_wp)
				ptent = pte_mkuffd_wp(ptent);
			else if (uffd_wp_resolve)
				ptent = pte_clear_uffd_wp(ptent);

			 
			if ((cp_flags & MM_CP_TRY_CHANGE_WRITABLE) &&
			    !pte_write(ptent) &&
			    can_change_pte_writable(vma, addr, ptent))
				ptent = pte_mkwrite(ptent, vma);

			ptep_modify_prot_commit(vma, addr, pte, oldpte, ptent);
			if (pte_needs_flush(oldpte, ptent))
				tlb_flush_pte_range(tlb, addr, PAGE_SIZE);
			pages++;
		} else if (is_swap_pte(oldpte)) {
			swp_entry_t entry = pte_to_swp_entry(oldpte);
			pte_t newpte;

			if (is_writable_migration_entry(entry)) {
				struct page *page = pfn_swap_entry_to_page(entry);

				 
				if (PageAnon(page))
					entry = make_readable_exclusive_migration_entry(
							     swp_offset(entry));
				else
					entry = make_readable_migration_entry(swp_offset(entry));
				newpte = swp_entry_to_pte(entry);
				if (pte_swp_soft_dirty(oldpte))
					newpte = pte_swp_mksoft_dirty(newpte);
			} else if (is_writable_device_private_entry(entry)) {
				 
				entry = make_readable_device_private_entry(
							swp_offset(entry));
				newpte = swp_entry_to_pte(entry);
				if (pte_swp_uffd_wp(oldpte))
					newpte = pte_swp_mkuffd_wp(newpte);
			} else if (is_writable_device_exclusive_entry(entry)) {
				entry = make_readable_device_exclusive_entry(
							swp_offset(entry));
				newpte = swp_entry_to_pte(entry);
				if (pte_swp_soft_dirty(oldpte))
					newpte = pte_swp_mksoft_dirty(newpte);
				if (pte_swp_uffd_wp(oldpte))
					newpte = pte_swp_mkuffd_wp(newpte);
			} else if (is_pte_marker_entry(entry)) {
				 
				if (is_poisoned_swp_entry(entry))
					continue;
				 
				if (uffd_wp_resolve) {
					pte_clear(vma->vm_mm, addr, pte);
					pages++;
				}
				continue;
			} else {
				newpte = oldpte;
			}

			if (uffd_wp)
				newpte = pte_swp_mkuffd_wp(newpte);
			else if (uffd_wp_resolve)
				newpte = pte_swp_clear_uffd_wp(newpte);

			if (!pte_same(oldpte, newpte)) {
				set_pte_at(vma->vm_mm, addr, pte, newpte);
				pages++;
			}
		} else {
			 
			WARN_ON_ONCE(!pte_none(oldpte));

			 
			if (likely(!uffd_wp))
				continue;

			if (userfaultfd_wp_use_markers(vma)) {
				 
				set_pte_at(vma->vm_mm, addr, pte,
					   make_pte_marker(PTE_MARKER_UFFD_WP));
				pages++;
			}
		}
	} while (pte++, addr += PAGE_SIZE, addr != end);
	arch_leave_lazy_mmu_mode();
	pte_unmap_unlock(pte - 1, ptl);

	return pages;
}

 
static inline bool
pgtable_split_needed(struct vm_area_struct *vma, unsigned long cp_flags)
{
	 
	return (cp_flags & MM_CP_UFFD_WP) && !vma_is_anonymous(vma);
}

 
static inline bool
pgtable_populate_needed(struct vm_area_struct *vma, unsigned long cp_flags)
{
	 
	if (!(cp_flags & MM_CP_UFFD_WP))
		return false;

	 
	return userfaultfd_wp_use_markers(vma);
}

 
#define  change_pmd_prepare(vma, pmd, cp_flags)				\
	({								\
		long err = 0;						\
		if (unlikely(pgtable_populate_needed(vma, cp_flags))) {	\
			if (pte_alloc(vma->vm_mm, pmd))			\
				err = -ENOMEM;				\
		}							\
		err;							\
	})

 
#define  change_prepare(vma, high, low, addr, cp_flags)			\
	  ({								\
		long err = 0;						\
		if (unlikely(pgtable_populate_needed(vma, cp_flags))) {	\
			low##_t *p = low##_alloc(vma->vm_mm, high, addr); \
			if (p == NULL)					\
				err = -ENOMEM;				\
		}							\
		err;							\
	})

static inline long change_pmd_range(struct mmu_gather *tlb,
		struct vm_area_struct *vma, pud_t *pud, unsigned long addr,
		unsigned long end, pgprot_t newprot, unsigned long cp_flags)
{
	pmd_t *pmd;
	unsigned long next;
	long pages = 0;
	unsigned long nr_huge_updates = 0;
	struct mmu_notifier_range range;

	range.start = 0;

	pmd = pmd_offset(pud, addr);
	do {
		long ret;
		pmd_t _pmd;
again:
		next = pmd_addr_end(addr, end);

		ret = change_pmd_prepare(vma, pmd, cp_flags);
		if (ret) {
			pages = ret;
			break;
		}

		if (pmd_none(*pmd))
			goto next;

		 
		if (!range.start) {
			mmu_notifier_range_init(&range,
				MMU_NOTIFY_PROTECTION_VMA, 0,
				vma->vm_mm, addr, end);
			mmu_notifier_invalidate_range_start(&range);
		}

		_pmd = pmdp_get_lockless(pmd);
		if (is_swap_pmd(_pmd) || pmd_trans_huge(_pmd) || pmd_devmap(_pmd)) {
			if ((next - addr != HPAGE_PMD_SIZE) ||
			    pgtable_split_needed(vma, cp_flags)) {
				__split_huge_pmd(vma, pmd, addr, false, NULL);
				 
				ret = change_pmd_prepare(vma, pmd, cp_flags);
				if (ret) {
					pages = ret;
					break;
				}
			} else {
				ret = change_huge_pmd(tlb, vma, pmd,
						addr, newprot, cp_flags);
				if (ret) {
					if (ret == HPAGE_PMD_NR) {
						pages += HPAGE_PMD_NR;
						nr_huge_updates++;
					}

					 
					goto next;
				}
			}
			 
		}

		ret = change_pte_range(tlb, vma, pmd, addr, next, newprot,
				       cp_flags);
		if (ret < 0)
			goto again;
		pages += ret;
next:
		cond_resched();
	} while (pmd++, addr = next, addr != end);

	if (range.start)
		mmu_notifier_invalidate_range_end(&range);

	if (nr_huge_updates)
		count_vm_numa_events(NUMA_HUGE_PTE_UPDATES, nr_huge_updates);
	return pages;
}

static inline long change_pud_range(struct mmu_gather *tlb,
		struct vm_area_struct *vma, p4d_t *p4d, unsigned long addr,
		unsigned long end, pgprot_t newprot, unsigned long cp_flags)
{
	pud_t *pud;
	unsigned long next;
	long pages = 0, ret;

	pud = pud_offset(p4d, addr);
	do {
		next = pud_addr_end(addr, end);
		ret = change_prepare(vma, pud, pmd, addr, cp_flags);
		if (ret)
			return ret;
		if (pud_none_or_clear_bad(pud))
			continue;
		pages += change_pmd_range(tlb, vma, pud, addr, next, newprot,
					  cp_flags);
	} while (pud++, addr = next, addr != end);

	return pages;
}

static inline long change_p4d_range(struct mmu_gather *tlb,
		struct vm_area_struct *vma, pgd_t *pgd, unsigned long addr,
		unsigned long end, pgprot_t newprot, unsigned long cp_flags)
{
	p4d_t *p4d;
	unsigned long next;
	long pages = 0, ret;

	p4d = p4d_offset(pgd, addr);
	do {
		next = p4d_addr_end(addr, end);
		ret = change_prepare(vma, p4d, pud, addr, cp_flags);
		if (ret)
			return ret;
		if (p4d_none_or_clear_bad(p4d))
			continue;
		pages += change_pud_range(tlb, vma, p4d, addr, next, newprot,
					  cp_flags);
	} while (p4d++, addr = next, addr != end);

	return pages;
}

static long change_protection_range(struct mmu_gather *tlb,
		struct vm_area_struct *vma, unsigned long addr,
		unsigned long end, pgprot_t newprot, unsigned long cp_flags)
{
	struct mm_struct *mm = vma->vm_mm;
	pgd_t *pgd;
	unsigned long next;
	long pages = 0, ret;

	BUG_ON(addr >= end);
	pgd = pgd_offset(mm, addr);
	tlb_start_vma(tlb, vma);
	do {
		next = pgd_addr_end(addr, end);
		ret = change_prepare(vma, pgd, p4d, addr, cp_flags);
		if (ret) {
			pages = ret;
			break;
		}
		if (pgd_none_or_clear_bad(pgd))
			continue;
		pages += change_p4d_range(tlb, vma, pgd, addr, next, newprot,
					  cp_flags);
	} while (pgd++, addr = next, addr != end);

	tlb_end_vma(tlb, vma);

	return pages;
}

long change_protection(struct mmu_gather *tlb,
		       struct vm_area_struct *vma, unsigned long start,
		       unsigned long end, unsigned long cp_flags)
{
	pgprot_t newprot = vma->vm_page_prot;
	long pages;

	BUG_ON((cp_flags & MM_CP_UFFD_WP_ALL) == MM_CP_UFFD_WP_ALL);

#ifdef CONFIG_NUMA_BALANCING
	 
	if (cp_flags & MM_CP_PROT_NUMA)
		newprot = PAGE_NONE;
#else
	WARN_ON_ONCE(cp_flags & MM_CP_PROT_NUMA);
#endif

	if (is_vm_hugetlb_page(vma))
		pages = hugetlb_change_protection(vma, start, end, newprot,
						  cp_flags);
	else
		pages = change_protection_range(tlb, vma, start, end, newprot,
						cp_flags);

	return pages;
}

static int prot_none_pte_entry(pte_t *pte, unsigned long addr,
			       unsigned long next, struct mm_walk *walk)
{
	return pfn_modify_allowed(pte_pfn(ptep_get(pte)),
				  *(pgprot_t *)(walk->private)) ?
		0 : -EACCES;
}

static int prot_none_hugetlb_entry(pte_t *pte, unsigned long hmask,
				   unsigned long addr, unsigned long next,
				   struct mm_walk *walk)
{
	return pfn_modify_allowed(pte_pfn(ptep_get(pte)),
				  *(pgprot_t *)(walk->private)) ?
		0 : -EACCES;
}

static int prot_none_test(unsigned long addr, unsigned long next,
			  struct mm_walk *walk)
{
	return 0;
}

static const struct mm_walk_ops prot_none_walk_ops = {
	.pte_entry		= prot_none_pte_entry,
	.hugetlb_entry		= prot_none_hugetlb_entry,
	.test_walk		= prot_none_test,
	.walk_lock		= PGWALK_WRLOCK,
};

int
mprotect_fixup(struct vma_iterator *vmi, struct mmu_gather *tlb,
	       struct vm_area_struct *vma, struct vm_area_struct **pprev,
	       unsigned long start, unsigned long end, unsigned long newflags)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long oldflags = vma->vm_flags;
	long nrpages = (end - start) >> PAGE_SHIFT;
	unsigned int mm_cp_flags = 0;
	unsigned long charged = 0;
	pgoff_t pgoff;
	int error;

	if (newflags == oldflags) {
		*pprev = vma;
		return 0;
	}

	 
	if (arch_has_pfn_modify_check() &&
	    (vma->vm_flags & (VM_PFNMAP|VM_MIXEDMAP)) &&
	    (newflags & VM_ACCESS_FLAGS) == 0) {
		pgprot_t new_pgprot = vm_get_page_prot(newflags);

		error = walk_page_range(current->mm, start, end,
				&prot_none_walk_ops, &new_pgprot);
		if (error)
			return error;
	}

	 
	if (newflags & VM_WRITE) {
		 
		if (!may_expand_vm(mm, newflags, nrpages) &&
				may_expand_vm(mm, oldflags, nrpages))
			return -ENOMEM;
		if (!(oldflags & (VM_ACCOUNT|VM_WRITE|VM_HUGETLB|
						VM_SHARED|VM_NORESERVE))) {
			charged = nrpages;
			if (security_vm_enough_memory_mm(mm, charged))
				return -ENOMEM;
			newflags |= VM_ACCOUNT;
		}
	}

	 
	pgoff = vma->vm_pgoff + ((start - vma->vm_start) >> PAGE_SHIFT);
	*pprev = vma_merge(vmi, mm, *pprev, start, end, newflags,
			   vma->anon_vma, vma->vm_file, pgoff, vma_policy(vma),
			   vma->vm_userfaultfd_ctx, anon_vma_name(vma));
	if (*pprev) {
		vma = *pprev;
		VM_WARN_ON((vma->vm_flags ^ newflags) & ~VM_SOFTDIRTY);
		goto success;
	}

	*pprev = vma;

	if (start != vma->vm_start) {
		error = split_vma(vmi, vma, start, 1);
		if (error)
			goto fail;
	}

	if (end != vma->vm_end) {
		error = split_vma(vmi, vma, end, 0);
		if (error)
			goto fail;
	}

success:
	 
	vma_start_write(vma);
	vm_flags_reset(vma, newflags);
	if (vma_wants_manual_pte_write_upgrade(vma))
		mm_cp_flags |= MM_CP_TRY_CHANGE_WRITABLE;
	vma_set_page_prot(vma);

	change_protection(tlb, vma, start, end, mm_cp_flags);

	 
	if ((oldflags & (VM_WRITE | VM_SHARED | VM_LOCKED)) == VM_LOCKED &&
			(newflags & VM_WRITE)) {
		populate_vma_page_range(vma, start, end, NULL);
	}

	vm_stat_account(mm, oldflags, -nrpages);
	vm_stat_account(mm, newflags, nrpages);
	perf_event_mmap(vma);
	return 0;

fail:
	vm_unacct_memory(charged);
	return error;
}

 
static int do_mprotect_pkey(unsigned long start, size_t len,
		unsigned long prot, int pkey)
{
	unsigned long nstart, end, tmp, reqprot;
	struct vm_area_struct *vma, *prev;
	int error;
	const int grows = prot & (PROT_GROWSDOWN|PROT_GROWSUP);
	const bool rier = (current->personality & READ_IMPLIES_EXEC) &&
				(prot & PROT_READ);
	struct mmu_gather tlb;
	struct vma_iterator vmi;

	start = untagged_addr(start);

	prot &= ~(PROT_GROWSDOWN|PROT_GROWSUP);
	if (grows == (PROT_GROWSDOWN|PROT_GROWSUP))  
		return -EINVAL;

	if (start & ~PAGE_MASK)
		return -EINVAL;
	if (!len)
		return 0;
	len = PAGE_ALIGN(len);
	end = start + len;
	if (end <= start)
		return -ENOMEM;
	if (!arch_validate_prot(prot, start))
		return -EINVAL;

	reqprot = prot;

	if (mmap_write_lock_killable(current->mm))
		return -EINTR;

	 
	error = -EINVAL;
	if ((pkey != -1) && !mm_pkey_is_allocated(current->mm, pkey))
		goto out;

	vma_iter_init(&vmi, current->mm, start);
	vma = vma_find(&vmi, end);
	error = -ENOMEM;
	if (!vma)
		goto out;

	if (unlikely(grows & PROT_GROWSDOWN)) {
		if (vma->vm_start >= end)
			goto out;
		start = vma->vm_start;
		error = -EINVAL;
		if (!(vma->vm_flags & VM_GROWSDOWN))
			goto out;
	} else {
		if (vma->vm_start > start)
			goto out;
		if (unlikely(grows & PROT_GROWSUP)) {
			end = vma->vm_end;
			error = -EINVAL;
			if (!(vma->vm_flags & VM_GROWSUP))
				goto out;
		}
	}

	prev = vma_prev(&vmi);
	if (start > vma->vm_start)
		prev = vma;

	tlb_gather_mmu(&tlb, current->mm);
	nstart = start;
	tmp = vma->vm_start;
	for_each_vma_range(vmi, vma, end) {
		unsigned long mask_off_old_flags;
		unsigned long newflags;
		int new_vma_pkey;

		if (vma->vm_start != tmp) {
			error = -ENOMEM;
			break;
		}

		 
		if (rier && (vma->vm_flags & VM_MAYEXEC))
			prot |= PROT_EXEC;

		 
		mask_off_old_flags = VM_ACCESS_FLAGS | VM_FLAGS_CLEAR;

		new_vma_pkey = arch_override_mprotect_pkey(vma, prot, pkey);
		newflags = calc_vm_prot_bits(prot, new_vma_pkey);
		newflags |= (vma->vm_flags & ~mask_off_old_flags);

		 
		if ((newflags & ~(newflags >> 4)) & VM_ACCESS_FLAGS) {
			error = -EACCES;
			break;
		}

		if (map_deny_write_exec(vma, newflags)) {
			error = -EACCES;
			break;
		}

		 
		if (!arch_validate_flags(newflags)) {
			error = -EINVAL;
			break;
		}

		error = security_file_mprotect(vma, reqprot, prot);
		if (error)
			break;

		tmp = vma->vm_end;
		if (tmp > end)
			tmp = end;

		if (vma->vm_ops && vma->vm_ops->mprotect) {
			error = vma->vm_ops->mprotect(vma, nstart, tmp, newflags);
			if (error)
				break;
		}

		error = mprotect_fixup(&vmi, &tlb, vma, &prev, nstart, tmp, newflags);
		if (error)
			break;

		tmp = vma_iter_end(&vmi);
		nstart = tmp;
		prot = reqprot;
	}
	tlb_finish_mmu(&tlb);

	if (!error && tmp < end)
		error = -ENOMEM;

out:
	mmap_write_unlock(current->mm);
	return error;
}

SYSCALL_DEFINE3(mprotect, unsigned long, start, size_t, len,
		unsigned long, prot)
{
	return do_mprotect_pkey(start, len, prot, -1);
}

#ifdef CONFIG_ARCH_HAS_PKEYS

SYSCALL_DEFINE4(pkey_mprotect, unsigned long, start, size_t, len,
		unsigned long, prot, int, pkey)
{
	return do_mprotect_pkey(start, len, prot, pkey);
}

SYSCALL_DEFINE2(pkey_alloc, unsigned long, flags, unsigned long, init_val)
{
	int pkey;
	int ret;

	 
	if (flags)
		return -EINVAL;
	 
	if (init_val & ~PKEY_ACCESS_MASK)
		return -EINVAL;

	mmap_write_lock(current->mm);
	pkey = mm_pkey_alloc(current->mm);

	ret = -ENOSPC;
	if (pkey == -1)
		goto out;

	ret = arch_set_user_pkey_access(current, pkey, init_val);
	if (ret) {
		mm_pkey_free(current->mm, pkey);
		goto out;
	}
	ret = pkey;
out:
	mmap_write_unlock(current->mm);
	return ret;
}

SYSCALL_DEFINE1(pkey_free, int, pkey)
{
	int ret;

	mmap_write_lock(current->mm);
	ret = mm_pkey_free(current->mm, pkey);
	mmap_write_unlock(current->mm);

	 
	return ret;
}

#endif  
