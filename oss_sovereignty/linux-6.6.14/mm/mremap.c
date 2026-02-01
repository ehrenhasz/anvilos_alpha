
 

#include <linux/mm.h>
#include <linux/mm_inline.h>
#include <linux/hugetlb.h>
#include <linux/shm.h>
#include <linux/ksm.h>
#include <linux/mman.h>
#include <linux/swap.h>
#include <linux/capability.h>
#include <linux/fs.h>
#include <linux/swapops.h>
#include <linux/highmem.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/mmu_notifier.h>
#include <linux/uaccess.h>
#include <linux/userfaultfd_k.h>
#include <linux/mempolicy.h>

#include <asm/cacheflush.h>
#include <asm/tlb.h>
#include <asm/pgalloc.h>

#include "internal.h"

static pud_t *get_old_pud(struct mm_struct *mm, unsigned long addr)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;

	pgd = pgd_offset(mm, addr);
	if (pgd_none_or_clear_bad(pgd))
		return NULL;

	p4d = p4d_offset(pgd, addr);
	if (p4d_none_or_clear_bad(p4d))
		return NULL;

	pud = pud_offset(p4d, addr);
	if (pud_none_or_clear_bad(pud))
		return NULL;

	return pud;
}

static pmd_t *get_old_pmd(struct mm_struct *mm, unsigned long addr)
{
	pud_t *pud;
	pmd_t *pmd;

	pud = get_old_pud(mm, addr);
	if (!pud)
		return NULL;

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return NULL;

	return pmd;
}

static pud_t *alloc_new_pud(struct mm_struct *mm, struct vm_area_struct *vma,
			    unsigned long addr)
{
	pgd_t *pgd;
	p4d_t *p4d;

	pgd = pgd_offset(mm, addr);
	p4d = p4d_alloc(mm, pgd, addr);
	if (!p4d)
		return NULL;

	return pud_alloc(mm, p4d, addr);
}

static pmd_t *alloc_new_pmd(struct mm_struct *mm, struct vm_area_struct *vma,
			    unsigned long addr)
{
	pud_t *pud;
	pmd_t *pmd;

	pud = alloc_new_pud(mm, vma, addr);
	if (!pud)
		return NULL;

	pmd = pmd_alloc(mm, pud, addr);
	if (!pmd)
		return NULL;

	VM_BUG_ON(pmd_trans_huge(*pmd));

	return pmd;
}

static void take_rmap_locks(struct vm_area_struct *vma)
{
	if (vma->vm_file)
		i_mmap_lock_write(vma->vm_file->f_mapping);
	if (vma->anon_vma)
		anon_vma_lock_write(vma->anon_vma);
}

static void drop_rmap_locks(struct vm_area_struct *vma)
{
	if (vma->anon_vma)
		anon_vma_unlock_write(vma->anon_vma);
	if (vma->vm_file)
		i_mmap_unlock_write(vma->vm_file->f_mapping);
}

static pte_t move_soft_dirty_pte(pte_t pte)
{
	 
#ifdef CONFIG_MEM_SOFT_DIRTY
	if (pte_present(pte))
		pte = pte_mksoft_dirty(pte);
	else if (is_swap_pte(pte))
		pte = pte_swp_mksoft_dirty(pte);
#endif
	return pte;
}

static int move_ptes(struct vm_area_struct *vma, pmd_t *old_pmd,
		unsigned long old_addr, unsigned long old_end,
		struct vm_area_struct *new_vma, pmd_t *new_pmd,
		unsigned long new_addr, bool need_rmap_locks)
{
	struct mm_struct *mm = vma->vm_mm;
	pte_t *old_pte, *new_pte, pte;
	spinlock_t *old_ptl, *new_ptl;
	bool force_flush = false;
	unsigned long len = old_end - old_addr;
	int err = 0;

	 
	if (need_rmap_locks)
		take_rmap_locks(vma);

	 
	old_pte = pte_offset_map_lock(mm, old_pmd, old_addr, &old_ptl);
	if (!old_pte) {
		err = -EAGAIN;
		goto out;
	}
	new_pte = pte_offset_map_nolock(mm, new_pmd, new_addr, &new_ptl);
	if (!new_pte) {
		pte_unmap_unlock(old_pte, old_ptl);
		err = -EAGAIN;
		goto out;
	}
	if (new_ptl != old_ptl)
		spin_lock_nested(new_ptl, SINGLE_DEPTH_NESTING);
	flush_tlb_batched_pending(vma->vm_mm);
	arch_enter_lazy_mmu_mode();

	for (; old_addr < old_end; old_pte++, old_addr += PAGE_SIZE,
				   new_pte++, new_addr += PAGE_SIZE) {
		if (pte_none(ptep_get(old_pte)))
			continue;

		pte = ptep_get_and_clear(mm, old_addr, old_pte);
		 
		if (pte_present(pte))
			force_flush = true;
		pte = move_pte(pte, new_vma->vm_page_prot, old_addr, new_addr);
		pte = move_soft_dirty_pte(pte);
		set_pte_at(mm, new_addr, new_pte, pte);
	}

	arch_leave_lazy_mmu_mode();
	if (force_flush)
		flush_tlb_range(vma, old_end - len, old_end);
	if (new_ptl != old_ptl)
		spin_unlock(new_ptl);
	pte_unmap(new_pte - 1);
	pte_unmap_unlock(old_pte - 1, old_ptl);
out:
	if (need_rmap_locks)
		drop_rmap_locks(vma);
	return err;
}

#ifndef arch_supports_page_table_move
#define arch_supports_page_table_move arch_supports_page_table_move
static inline bool arch_supports_page_table_move(void)
{
	return IS_ENABLED(CONFIG_HAVE_MOVE_PMD) ||
		IS_ENABLED(CONFIG_HAVE_MOVE_PUD);
}
#endif

#ifdef CONFIG_HAVE_MOVE_PMD
static bool move_normal_pmd(struct vm_area_struct *vma, unsigned long old_addr,
		  unsigned long new_addr, pmd_t *old_pmd, pmd_t *new_pmd)
{
	spinlock_t *old_ptl, *new_ptl;
	struct mm_struct *mm = vma->vm_mm;
	pmd_t pmd;

	if (!arch_supports_page_table_move())
		return false;
	 
	if (WARN_ON_ONCE(!pmd_none(*new_pmd)))
		return false;

	 
	old_ptl = pmd_lock(vma->vm_mm, old_pmd);
	new_ptl = pmd_lockptr(mm, new_pmd);
	if (new_ptl != old_ptl)
		spin_lock_nested(new_ptl, SINGLE_DEPTH_NESTING);

	 
	pmd = *old_pmd;
	pmd_clear(old_pmd);

	VM_BUG_ON(!pmd_none(*new_pmd));

	pmd_populate(mm, new_pmd, pmd_pgtable(pmd));
	flush_tlb_range(vma, old_addr, old_addr + PMD_SIZE);
	if (new_ptl != old_ptl)
		spin_unlock(new_ptl);
	spin_unlock(old_ptl);

	return true;
}
#else
static inline bool move_normal_pmd(struct vm_area_struct *vma,
		unsigned long old_addr, unsigned long new_addr, pmd_t *old_pmd,
		pmd_t *new_pmd)
{
	return false;
}
#endif

#if CONFIG_PGTABLE_LEVELS > 2 && defined(CONFIG_HAVE_MOVE_PUD)
static bool move_normal_pud(struct vm_area_struct *vma, unsigned long old_addr,
		  unsigned long new_addr, pud_t *old_pud, pud_t *new_pud)
{
	spinlock_t *old_ptl, *new_ptl;
	struct mm_struct *mm = vma->vm_mm;
	pud_t pud;

	if (!arch_supports_page_table_move())
		return false;
	 
	if (WARN_ON_ONCE(!pud_none(*new_pud)))
		return false;

	 
	old_ptl = pud_lock(vma->vm_mm, old_pud);
	new_ptl = pud_lockptr(mm, new_pud);
	if (new_ptl != old_ptl)
		spin_lock_nested(new_ptl, SINGLE_DEPTH_NESTING);

	 
	pud = *old_pud;
	pud_clear(old_pud);

	VM_BUG_ON(!pud_none(*new_pud));

	pud_populate(mm, new_pud, pud_pgtable(pud));
	flush_tlb_range(vma, old_addr, old_addr + PUD_SIZE);
	if (new_ptl != old_ptl)
		spin_unlock(new_ptl);
	spin_unlock(old_ptl);

	return true;
}
#else
static inline bool move_normal_pud(struct vm_area_struct *vma,
		unsigned long old_addr, unsigned long new_addr, pud_t *old_pud,
		pud_t *new_pud)
{
	return false;
}
#endif

#if defined(CONFIG_TRANSPARENT_HUGEPAGE) && defined(CONFIG_HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD)
static bool move_huge_pud(struct vm_area_struct *vma, unsigned long old_addr,
			  unsigned long new_addr, pud_t *old_pud, pud_t *new_pud)
{
	spinlock_t *old_ptl, *new_ptl;
	struct mm_struct *mm = vma->vm_mm;
	pud_t pud;

	 
	if (WARN_ON_ONCE(!pud_none(*new_pud)))
		return false;

	 
	old_ptl = pud_lock(vma->vm_mm, old_pud);
	new_ptl = pud_lockptr(mm, new_pud);
	if (new_ptl != old_ptl)
		spin_lock_nested(new_ptl, SINGLE_DEPTH_NESTING);

	 
	pud = *old_pud;
	pud_clear(old_pud);

	VM_BUG_ON(!pud_none(*new_pud));

	 
	 
	set_pud_at(mm, new_addr, new_pud, pud);
	flush_pud_tlb_range(vma, old_addr, old_addr + HPAGE_PUD_SIZE);
	if (new_ptl != old_ptl)
		spin_unlock(new_ptl);
	spin_unlock(old_ptl);

	return true;
}
#else
static bool move_huge_pud(struct vm_area_struct *vma, unsigned long old_addr,
			  unsigned long new_addr, pud_t *old_pud, pud_t *new_pud)
{
	WARN_ON_ONCE(1);
	return false;

}
#endif

enum pgt_entry {
	NORMAL_PMD,
	HPAGE_PMD,
	NORMAL_PUD,
	HPAGE_PUD,
};

 
static __always_inline unsigned long get_extent(enum pgt_entry entry,
			unsigned long old_addr, unsigned long old_end,
			unsigned long new_addr)
{
	unsigned long next, extent, mask, size;

	switch (entry) {
	case HPAGE_PMD:
	case NORMAL_PMD:
		mask = PMD_MASK;
		size = PMD_SIZE;
		break;
	case HPAGE_PUD:
	case NORMAL_PUD:
		mask = PUD_MASK;
		size = PUD_SIZE;
		break;
	default:
		BUILD_BUG();
		break;
	}

	next = (old_addr + size) & mask;
	 
	extent = next - old_addr;
	if (extent > old_end - old_addr)
		extent = old_end - old_addr;
	next = (new_addr + size) & mask;
	if (extent > next - new_addr)
		extent = next - new_addr;
	return extent;
}

 
static bool move_pgt_entry(enum pgt_entry entry, struct vm_area_struct *vma,
			unsigned long old_addr, unsigned long new_addr,
			void *old_entry, void *new_entry, bool need_rmap_locks)
{
	bool moved = false;

	 
	if (need_rmap_locks)
		take_rmap_locks(vma);

	switch (entry) {
	case NORMAL_PMD:
		moved = move_normal_pmd(vma, old_addr, new_addr, old_entry,
					new_entry);
		break;
	case NORMAL_PUD:
		moved = move_normal_pud(vma, old_addr, new_addr, old_entry,
					new_entry);
		break;
	case HPAGE_PMD:
		moved = IS_ENABLED(CONFIG_TRANSPARENT_HUGEPAGE) &&
			move_huge_pmd(vma, old_addr, new_addr, old_entry,
				      new_entry);
		break;
	case HPAGE_PUD:
		moved = IS_ENABLED(CONFIG_TRANSPARENT_HUGEPAGE) &&
			move_huge_pud(vma, old_addr, new_addr, old_entry,
				      new_entry);
		break;

	default:
		WARN_ON_ONCE(1);
		break;
	}

	if (need_rmap_locks)
		drop_rmap_locks(vma);

	return moved;
}

unsigned long move_page_tables(struct vm_area_struct *vma,
		unsigned long old_addr, struct vm_area_struct *new_vma,
		unsigned long new_addr, unsigned long len,
		bool need_rmap_locks)
{
	unsigned long extent, old_end;
	struct mmu_notifier_range range;
	pmd_t *old_pmd, *new_pmd;
	pud_t *old_pud, *new_pud;

	if (!len)
		return 0;

	old_end = old_addr + len;

	if (is_vm_hugetlb_page(vma))
		return move_hugetlb_page_tables(vma, new_vma, old_addr,
						new_addr, len);

	flush_cache_range(vma, old_addr, old_end);
	mmu_notifier_range_init(&range, MMU_NOTIFY_UNMAP, 0, vma->vm_mm,
				old_addr, old_end);
	mmu_notifier_invalidate_range_start(&range);

	for (; old_addr < old_end; old_addr += extent, new_addr += extent) {
		cond_resched();
		 
		extent = get_extent(NORMAL_PUD, old_addr, old_end, new_addr);

		old_pud = get_old_pud(vma->vm_mm, old_addr);
		if (!old_pud)
			continue;
		new_pud = alloc_new_pud(vma->vm_mm, vma, new_addr);
		if (!new_pud)
			break;
		if (pud_trans_huge(*old_pud) || pud_devmap(*old_pud)) {
			if (extent == HPAGE_PUD_SIZE) {
				move_pgt_entry(HPAGE_PUD, vma, old_addr, new_addr,
					       old_pud, new_pud, need_rmap_locks);
				 
				continue;
			}
		} else if (IS_ENABLED(CONFIG_HAVE_MOVE_PUD) && extent == PUD_SIZE) {

			if (move_pgt_entry(NORMAL_PUD, vma, old_addr, new_addr,
					   old_pud, new_pud, true))
				continue;
		}

		extent = get_extent(NORMAL_PMD, old_addr, old_end, new_addr);
		old_pmd = get_old_pmd(vma->vm_mm, old_addr);
		if (!old_pmd)
			continue;
		new_pmd = alloc_new_pmd(vma->vm_mm, vma, new_addr);
		if (!new_pmd)
			break;
again:
		if (is_swap_pmd(*old_pmd) || pmd_trans_huge(*old_pmd) ||
		    pmd_devmap(*old_pmd)) {
			if (extent == HPAGE_PMD_SIZE &&
			    move_pgt_entry(HPAGE_PMD, vma, old_addr, new_addr,
					   old_pmd, new_pmd, need_rmap_locks))
				continue;
			split_huge_pmd(vma, old_pmd, old_addr);
		} else if (IS_ENABLED(CONFIG_HAVE_MOVE_PMD) &&
			   extent == PMD_SIZE) {
			 
			if (move_pgt_entry(NORMAL_PMD, vma, old_addr, new_addr,
					   old_pmd, new_pmd, true))
				continue;
		}
		if (pmd_none(*old_pmd))
			continue;
		if (pte_alloc(new_vma->vm_mm, new_pmd))
			break;
		if (move_ptes(vma, old_pmd, old_addr, old_addr + extent,
			      new_vma, new_pmd, new_addr, need_rmap_locks) < 0)
			goto again;
	}

	mmu_notifier_invalidate_range_end(&range);

	return len + old_addr - old_end;	 
}

static unsigned long move_vma(struct vm_area_struct *vma,
		unsigned long old_addr, unsigned long old_len,
		unsigned long new_len, unsigned long new_addr,
		bool *locked, unsigned long flags,
		struct vm_userfaultfd_ctx *uf, struct list_head *uf_unmap)
{
	long to_account = new_len - old_len;
	struct mm_struct *mm = vma->vm_mm;
	struct vm_area_struct *new_vma;
	unsigned long vm_flags = vma->vm_flags;
	unsigned long new_pgoff;
	unsigned long moved_len;
	unsigned long account_start = 0;
	unsigned long account_end = 0;
	unsigned long hiwater_vm;
	int err = 0;
	bool need_rmap_locks;
	struct vma_iterator vmi;

	 
	if (mm->map_count >= sysctl_max_map_count - 3)
		return -ENOMEM;

	if (unlikely(flags & MREMAP_DONTUNMAP))
		to_account = new_len;

	if (vma->vm_ops && vma->vm_ops->may_split) {
		if (vma->vm_start != old_addr)
			err = vma->vm_ops->may_split(vma, old_addr);
		if (!err && vma->vm_end != old_addr + old_len)
			err = vma->vm_ops->may_split(vma, old_addr + old_len);
		if (err)
			return err;
	}

	 
	err = ksm_madvise(vma, old_addr, old_addr + old_len,
						MADV_UNMERGEABLE, &vm_flags);
	if (err)
		return err;

	if (vm_flags & VM_ACCOUNT) {
		if (security_vm_enough_memory_mm(mm, to_account >> PAGE_SHIFT))
			return -ENOMEM;
	}

	vma_start_write(vma);
	new_pgoff = vma->vm_pgoff + ((old_addr - vma->vm_start) >> PAGE_SHIFT);
	new_vma = copy_vma(&vma, new_addr, new_len, new_pgoff,
			   &need_rmap_locks);
	if (!new_vma) {
		if (vm_flags & VM_ACCOUNT)
			vm_unacct_memory(to_account >> PAGE_SHIFT);
		return -ENOMEM;
	}

	moved_len = move_page_tables(vma, old_addr, new_vma, new_addr, old_len,
				     need_rmap_locks);
	if (moved_len < old_len) {
		err = -ENOMEM;
	} else if (vma->vm_ops && vma->vm_ops->mremap) {
		err = vma->vm_ops->mremap(new_vma);
	}

	if (unlikely(err)) {
		 
		move_page_tables(new_vma, new_addr, vma, old_addr, moved_len,
				 true);
		vma = new_vma;
		old_len = new_len;
		old_addr = new_addr;
		new_addr = err;
	} else {
		mremap_userfaultfd_prep(new_vma, uf);
	}

	if (is_vm_hugetlb_page(vma)) {
		clear_vma_resv_huge_pages(vma);
	}

	 
	if (vm_flags & VM_ACCOUNT && !(flags & MREMAP_DONTUNMAP)) {
		vm_flags_clear(vma, VM_ACCOUNT);
		if (vma->vm_start < old_addr)
			account_start = vma->vm_start;
		if (vma->vm_end > old_addr + old_len)
			account_end = vma->vm_end;
	}

	 
	hiwater_vm = mm->hiwater_vm;
	vm_stat_account(mm, vma->vm_flags, new_len >> PAGE_SHIFT);

	 
	if (unlikely(vma->vm_flags & VM_PFNMAP))
		untrack_pfn_clear(vma);

	if (unlikely(!err && (flags & MREMAP_DONTUNMAP))) {
		 
		vm_flags_clear(vma, VM_LOCKED_MASK);

		 
		if (new_vma != vma && vma->vm_start == old_addr &&
			vma->vm_end == (old_addr + old_len))
			unlink_anon_vmas(vma);

		 
		return new_addr;
	}

	vma_iter_init(&vmi, mm, old_addr);
	if (do_vmi_munmap(&vmi, mm, old_addr, old_len, uf_unmap, false) < 0) {
		 
		if (vm_flags & VM_ACCOUNT && !(flags & MREMAP_DONTUNMAP))
			vm_acct_memory(old_len >> PAGE_SHIFT);
		account_start = account_end = 0;
	}

	if (vm_flags & VM_LOCKED) {
		mm->locked_vm += new_len >> PAGE_SHIFT;
		*locked = true;
	}

	mm->hiwater_vm = hiwater_vm;

	 
	if (account_start) {
		vma = vma_prev(&vmi);
		vm_flags_set(vma, VM_ACCOUNT);
	}

	if (account_end) {
		vma = vma_next(&vmi);
		vm_flags_set(vma, VM_ACCOUNT);
	}

	return new_addr;
}

static struct vm_area_struct *vma_to_resize(unsigned long addr,
	unsigned long old_len, unsigned long new_len, unsigned long flags)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned long pgoff;

	vma = vma_lookup(mm, addr);
	if (!vma)
		return ERR_PTR(-EFAULT);

	 
	if (!old_len && !(vma->vm_flags & (VM_SHARED | VM_MAYSHARE))) {
		pr_warn_once("%s (%d): attempted to duplicate a private mapping with mremap.  This is not supported.\n", current->comm, current->pid);
		return ERR_PTR(-EINVAL);
	}

	if ((flags & MREMAP_DONTUNMAP) &&
			(vma->vm_flags & (VM_DONTEXPAND | VM_PFNMAP)))
		return ERR_PTR(-EINVAL);

	 
	if (old_len > vma->vm_end - addr)
		return ERR_PTR(-EFAULT);

	if (new_len == old_len)
		return vma;

	 
	pgoff = (addr - vma->vm_start) >> PAGE_SHIFT;
	pgoff += vma->vm_pgoff;
	if (pgoff + (new_len >> PAGE_SHIFT) < pgoff)
		return ERR_PTR(-EINVAL);

	if (vma->vm_flags & (VM_DONTEXPAND | VM_PFNMAP))
		return ERR_PTR(-EFAULT);

	if (!mlock_future_ok(mm, vma->vm_flags, new_len - old_len))
		return ERR_PTR(-EAGAIN);

	if (!may_expand_vm(mm, vma->vm_flags,
				(new_len - old_len) >> PAGE_SHIFT))
		return ERR_PTR(-ENOMEM);

	return vma;
}

static unsigned long mremap_to(unsigned long addr, unsigned long old_len,
		unsigned long new_addr, unsigned long new_len, bool *locked,
		unsigned long flags, struct vm_userfaultfd_ctx *uf,
		struct list_head *uf_unmap_early,
		struct list_head *uf_unmap)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned long ret = -EINVAL;
	unsigned long map_flags = 0;

	if (offset_in_page(new_addr))
		goto out;

	if (new_len > TASK_SIZE || new_addr > TASK_SIZE - new_len)
		goto out;

	 
	if (addr + old_len > new_addr && new_addr + new_len > addr)
		goto out;

	 
	if ((mm->map_count + 2) >= sysctl_max_map_count - 3)
		return -ENOMEM;

	if (flags & MREMAP_FIXED) {
		ret = do_munmap(mm, new_addr, new_len, uf_unmap_early);
		if (ret)
			goto out;
	}

	if (old_len > new_len) {
		ret = do_munmap(mm, addr+new_len, old_len - new_len, uf_unmap);
		if (ret)
			goto out;
		old_len = new_len;
	}

	vma = vma_to_resize(addr, old_len, new_len, flags);
	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		goto out;
	}

	 
	if (flags & MREMAP_DONTUNMAP &&
		!may_expand_vm(mm, vma->vm_flags, old_len >> PAGE_SHIFT)) {
		ret = -ENOMEM;
		goto out;
	}

	if (flags & MREMAP_FIXED)
		map_flags |= MAP_FIXED;

	if (vma->vm_flags & VM_MAYSHARE)
		map_flags |= MAP_SHARED;

	ret = get_unmapped_area(vma->vm_file, new_addr, new_len, vma->vm_pgoff +
				((addr - vma->vm_start) >> PAGE_SHIFT),
				map_flags);
	if (IS_ERR_VALUE(ret))
		goto out;

	 
	if (!(flags & MREMAP_FIXED))
		new_addr = ret;

	ret = move_vma(vma, addr, old_len, new_len, new_addr, locked, flags, uf,
		       uf_unmap);

out:
	return ret;
}

static int vma_expandable(struct vm_area_struct *vma, unsigned long delta)
{
	unsigned long end = vma->vm_end + delta;

	if (end < vma->vm_end)  
		return 0;
	if (find_vma_intersection(vma->vm_mm, vma->vm_end, end))
		return 0;
	if (get_unmapped_area(NULL, vma->vm_start, end - vma->vm_start,
			      0, MAP_FIXED) & ~PAGE_MASK)
		return 0;
	return 1;
}

 
SYSCALL_DEFINE5(mremap, unsigned long, addr, unsigned long, old_len,
		unsigned long, new_len, unsigned long, flags,
		unsigned long, new_addr)
{
	struct mm_struct *mm = current->mm;
	struct vm_area_struct *vma;
	unsigned long ret = -EINVAL;
	bool locked = false;
	struct vm_userfaultfd_ctx uf = NULL_VM_UFFD_CTX;
	LIST_HEAD(uf_unmap_early);
	LIST_HEAD(uf_unmap);

	 
	addr = untagged_addr(addr);

	if (flags & ~(MREMAP_FIXED | MREMAP_MAYMOVE | MREMAP_DONTUNMAP))
		return ret;

	if (flags & MREMAP_FIXED && !(flags & MREMAP_MAYMOVE))
		return ret;

	 
	if (flags & MREMAP_DONTUNMAP &&
			(!(flags & MREMAP_MAYMOVE) || old_len != new_len))
		return ret;


	if (offset_in_page(addr))
		return ret;

	old_len = PAGE_ALIGN(old_len);
	new_len = PAGE_ALIGN(new_len);

	 
	if (!new_len)
		return ret;

	if (mmap_write_lock_killable(current->mm))
		return -EINTR;
	vma = vma_lookup(mm, addr);
	if (!vma) {
		ret = -EFAULT;
		goto out;
	}

	if (is_vm_hugetlb_page(vma)) {
		struct hstate *h __maybe_unused = hstate_vma(vma);

		old_len = ALIGN(old_len, huge_page_size(h));
		new_len = ALIGN(new_len, huge_page_size(h));

		 
		if (addr & ~huge_page_mask(h))
			goto out;
		if (new_addr & ~huge_page_mask(h))
			goto out;

		 
		if (new_len > old_len)
			goto out;
	}

	if (flags & (MREMAP_FIXED | MREMAP_DONTUNMAP)) {
		ret = mremap_to(addr, old_len, new_addr, new_len,
				&locked, flags, &uf, &uf_unmap_early,
				&uf_unmap);
		goto out;
	}

	 
	if (old_len >= new_len) {
		VMA_ITERATOR(vmi, mm, addr + new_len);

		if (old_len == new_len) {
			ret = addr;
			goto out;
		}

		ret = do_vmi_munmap(&vmi, mm, addr + new_len, old_len - new_len,
				    &uf_unmap, true);
		if (ret)
			goto out;

		ret = addr;
		goto out_unlocked;
	}

	 
	vma = vma_to_resize(addr, old_len, new_len, flags);
	if (IS_ERR(vma)) {
		ret = PTR_ERR(vma);
		goto out;
	}

	 
	if (old_len == vma->vm_end - addr) {
		 
		if (vma_expandable(vma, new_len - old_len)) {
			long pages = (new_len - old_len) >> PAGE_SHIFT;
			unsigned long extension_start = addr + old_len;
			unsigned long extension_end = addr + new_len;
			pgoff_t extension_pgoff = vma->vm_pgoff +
				((extension_start - vma->vm_start) >> PAGE_SHIFT);
			VMA_ITERATOR(vmi, mm, extension_start);

			if (vma->vm_flags & VM_ACCOUNT) {
				if (security_vm_enough_memory_mm(mm, pages)) {
					ret = -ENOMEM;
					goto out;
				}
			}

			 
			vma = vma_merge(&vmi, mm, vma, extension_start,
				extension_end, vma->vm_flags, vma->anon_vma,
				vma->vm_file, extension_pgoff, vma_policy(vma),
				vma->vm_userfaultfd_ctx, anon_vma_name(vma));
			if (!vma) {
				vm_unacct_memory(pages);
				ret = -ENOMEM;
				goto out;
			}

			vm_stat_account(mm, vma->vm_flags, pages);
			if (vma->vm_flags & VM_LOCKED) {
				mm->locked_vm += pages;
				locked = true;
				new_addr = addr;
			}
			ret = addr;
			goto out;
		}
	}

	 
	ret = -ENOMEM;
	if (flags & MREMAP_MAYMOVE) {
		unsigned long map_flags = 0;
		if (vma->vm_flags & VM_MAYSHARE)
			map_flags |= MAP_SHARED;

		new_addr = get_unmapped_area(vma->vm_file, 0, new_len,
					vma->vm_pgoff +
					((addr - vma->vm_start) >> PAGE_SHIFT),
					map_flags);
		if (IS_ERR_VALUE(new_addr)) {
			ret = new_addr;
			goto out;
		}

		ret = move_vma(vma, addr, old_len, new_len, new_addr,
			       &locked, flags, &uf, &uf_unmap);
	}
out:
	if (offset_in_page(ret))
		locked = false;
	mmap_write_unlock(current->mm);
	if (locked && new_len > old_len)
		mm_populate(new_addr + old_len, new_len - old_len);
out_unlocked:
	userfaultfd_unmap_complete(mm, &uf_unmap_early);
	mremap_userfaultfd_complete(&uf, addr, ret, old_len);
	userfaultfd_unmap_complete(mm, &uf_unmap);
	return ret;
}
