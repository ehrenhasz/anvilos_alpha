
#include <linux/pagewalk.h>
#include <linux/highmem.h>
#include <linux/sched.h>
#include <linux/hugetlb.h>

 
static int real_depth(int depth)
{
	if (depth == 3 && PTRS_PER_PMD == 1)
		depth = 2;
	if (depth == 2 && PTRS_PER_PUD == 1)
		depth = 1;
	if (depth == 1 && PTRS_PER_P4D == 1)
		depth = 0;
	return depth;
}

static int walk_pte_range_inner(pte_t *pte, unsigned long addr,
				unsigned long end, struct mm_walk *walk)
{
	const struct mm_walk_ops *ops = walk->ops;
	int err = 0;

	for (;;) {
		err = ops->pte_entry(pte, addr, addr + PAGE_SIZE, walk);
		if (err)
		       break;
		if (addr >= end - PAGE_SIZE)
			break;
		addr += PAGE_SIZE;
		pte++;
	}
	return err;
}

static int walk_pte_range(pmd_t *pmd, unsigned long addr, unsigned long end,
			  struct mm_walk *walk)
{
	pte_t *pte;
	int err = 0;
	spinlock_t *ptl;

	if (walk->no_vma) {
		 
		if (walk->mm == &init_mm || addr >= TASK_SIZE)
			pte = pte_offset_kernel(pmd, addr);
		else
			pte = pte_offset_map(pmd, addr);
		if (pte) {
			err = walk_pte_range_inner(pte, addr, end, walk);
			if (walk->mm != &init_mm && addr < TASK_SIZE)
				pte_unmap(pte);
		}
	} else {
		pte = pte_offset_map_lock(walk->mm, pmd, addr, &ptl);
		if (pte) {
			err = walk_pte_range_inner(pte, addr, end, walk);
			pte_unmap_unlock(pte, ptl);
		}
	}
	if (!pte)
		walk->action = ACTION_AGAIN;
	return err;
}

#ifdef CONFIG_ARCH_HAS_HUGEPD
static int walk_hugepd_range(hugepd_t *phpd, unsigned long addr,
			     unsigned long end, struct mm_walk *walk, int pdshift)
{
	int err = 0;
	const struct mm_walk_ops *ops = walk->ops;
	int shift = hugepd_shift(*phpd);
	int page_size = 1 << shift;

	if (!ops->pte_entry)
		return 0;

	if (addr & (page_size - 1))
		return 0;

	for (;;) {
		pte_t *pte;

		spin_lock(&walk->mm->page_table_lock);
		pte = hugepte_offset(*phpd, addr, pdshift);
		err = ops->pte_entry(pte, addr, addr + page_size, walk);
		spin_unlock(&walk->mm->page_table_lock);

		if (err)
			break;
		if (addr >= end - page_size)
			break;
		addr += page_size;
	}
	return err;
}
#else
static int walk_hugepd_range(hugepd_t *phpd, unsigned long addr,
			     unsigned long end, struct mm_walk *walk, int pdshift)
{
	return 0;
}
#endif

static int walk_pmd_range(pud_t *pud, unsigned long addr, unsigned long end,
			  struct mm_walk *walk)
{
	pmd_t *pmd;
	unsigned long next;
	const struct mm_walk_ops *ops = walk->ops;
	int err = 0;
	int depth = real_depth(3);

	pmd = pmd_offset(pud, addr);
	do {
again:
		next = pmd_addr_end(addr, end);
		if (pmd_none(*pmd)) {
			if (ops->pte_hole)
				err = ops->pte_hole(addr, next, depth, walk);
			if (err)
				break;
			continue;
		}

		walk->action = ACTION_SUBTREE;

		 
		if (ops->pmd_entry)
			err = ops->pmd_entry(pmd, addr, next, walk);
		if (err)
			break;

		if (walk->action == ACTION_AGAIN)
			goto again;

		 
		if ((!walk->vma && (pmd_leaf(*pmd) || !pmd_present(*pmd))) ||
		    walk->action == ACTION_CONTINUE ||
		    !(ops->pte_entry))
			continue;

		if (walk->vma)
			split_huge_pmd(walk->vma, pmd, addr);

		if (is_hugepd(__hugepd(pmd_val(*pmd))))
			err = walk_hugepd_range((hugepd_t *)pmd, addr, next, walk, PMD_SHIFT);
		else
			err = walk_pte_range(pmd, addr, next, walk);
		if (err)
			break;

		if (walk->action == ACTION_AGAIN)
			goto again;

	} while (pmd++, addr = next, addr != end);

	return err;
}

static int walk_pud_range(p4d_t *p4d, unsigned long addr, unsigned long end,
			  struct mm_walk *walk)
{
	pud_t *pud;
	unsigned long next;
	const struct mm_walk_ops *ops = walk->ops;
	int err = 0;
	int depth = real_depth(2);

	pud = pud_offset(p4d, addr);
	do {
 again:
		next = pud_addr_end(addr, end);
		if (pud_none(*pud)) {
			if (ops->pte_hole)
				err = ops->pte_hole(addr, next, depth, walk);
			if (err)
				break;
			continue;
		}

		walk->action = ACTION_SUBTREE;

		if (ops->pud_entry)
			err = ops->pud_entry(pud, addr, next, walk);
		if (err)
			break;

		if (walk->action == ACTION_AGAIN)
			goto again;

		if ((!walk->vma && (pud_leaf(*pud) || !pud_present(*pud))) ||
		    walk->action == ACTION_CONTINUE ||
		    !(ops->pmd_entry || ops->pte_entry))
			continue;

		if (walk->vma)
			split_huge_pud(walk->vma, pud, addr);
		if (pud_none(*pud))
			goto again;

		if (is_hugepd(__hugepd(pud_val(*pud))))
			err = walk_hugepd_range((hugepd_t *)pud, addr, next, walk, PUD_SHIFT);
		else
			err = walk_pmd_range(pud, addr, next, walk);
		if (err)
			break;
	} while (pud++, addr = next, addr != end);

	return err;
}

static int walk_p4d_range(pgd_t *pgd, unsigned long addr, unsigned long end,
			  struct mm_walk *walk)
{
	p4d_t *p4d;
	unsigned long next;
	const struct mm_walk_ops *ops = walk->ops;
	int err = 0;
	int depth = real_depth(1);

	p4d = p4d_offset(pgd, addr);
	do {
		next = p4d_addr_end(addr, end);
		if (p4d_none_or_clear_bad(p4d)) {
			if (ops->pte_hole)
				err = ops->pte_hole(addr, next, depth, walk);
			if (err)
				break;
			continue;
		}
		if (ops->p4d_entry) {
			err = ops->p4d_entry(p4d, addr, next, walk);
			if (err)
				break;
		}
		if (is_hugepd(__hugepd(p4d_val(*p4d))))
			err = walk_hugepd_range((hugepd_t *)p4d, addr, next, walk, P4D_SHIFT);
		else if (ops->pud_entry || ops->pmd_entry || ops->pte_entry)
			err = walk_pud_range(p4d, addr, next, walk);
		if (err)
			break;
	} while (p4d++, addr = next, addr != end);

	return err;
}

static int walk_pgd_range(unsigned long addr, unsigned long end,
			  struct mm_walk *walk)
{
	pgd_t *pgd;
	unsigned long next;
	const struct mm_walk_ops *ops = walk->ops;
	int err = 0;

	if (walk->pgd)
		pgd = walk->pgd + pgd_index(addr);
	else
		pgd = pgd_offset(walk->mm, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(pgd)) {
			if (ops->pte_hole)
				err = ops->pte_hole(addr, next, 0, walk);
			if (err)
				break;
			continue;
		}
		if (ops->pgd_entry) {
			err = ops->pgd_entry(pgd, addr, next, walk);
			if (err)
				break;
		}
		if (is_hugepd(__hugepd(pgd_val(*pgd))))
			err = walk_hugepd_range((hugepd_t *)pgd, addr, next, walk, PGDIR_SHIFT);
		else if (ops->p4d_entry || ops->pud_entry || ops->pmd_entry || ops->pte_entry)
			err = walk_p4d_range(pgd, addr, next, walk);
		if (err)
			break;
	} while (pgd++, addr = next, addr != end);

	return err;
}

#ifdef CONFIG_HUGETLB_PAGE
static unsigned long hugetlb_entry_end(struct hstate *h, unsigned long addr,
				       unsigned long end)
{
	unsigned long boundary = (addr & huge_page_mask(h)) + huge_page_size(h);
	return boundary < end ? boundary : end;
}

static int walk_hugetlb_range(unsigned long addr, unsigned long end,
			      struct mm_walk *walk)
{
	struct vm_area_struct *vma = walk->vma;
	struct hstate *h = hstate_vma(vma);
	unsigned long next;
	unsigned long hmask = huge_page_mask(h);
	unsigned long sz = huge_page_size(h);
	pte_t *pte;
	const struct mm_walk_ops *ops = walk->ops;
	int err = 0;

	hugetlb_vma_lock_read(vma);
	do {
		next = hugetlb_entry_end(h, addr, end);
		pte = hugetlb_walk(vma, addr & hmask, sz);
		if (pte)
			err = ops->hugetlb_entry(pte, hmask, addr, next, walk);
		else if (ops->pte_hole)
			err = ops->pte_hole(addr, next, -1, walk);
		if (err)
			break;
	} while (addr = next, addr != end);
	hugetlb_vma_unlock_read(vma);

	return err;
}

#else  
static int walk_hugetlb_range(unsigned long addr, unsigned long end,
			      struct mm_walk *walk)
{
	return 0;
}

#endif  

 
static int walk_page_test(unsigned long start, unsigned long end,
			struct mm_walk *walk)
{
	struct vm_area_struct *vma = walk->vma;
	const struct mm_walk_ops *ops = walk->ops;

	if (ops->test_walk)
		return ops->test_walk(start, end, walk);

	 
	if (vma->vm_flags & VM_PFNMAP) {
		int err = 1;
		if (ops->pte_hole)
			err = ops->pte_hole(start, end, -1, walk);
		return err ? err : 1;
	}
	return 0;
}

static int __walk_page_range(unsigned long start, unsigned long end,
			struct mm_walk *walk)
{
	int err = 0;
	struct vm_area_struct *vma = walk->vma;
	const struct mm_walk_ops *ops = walk->ops;

	if (ops->pre_vma) {
		err = ops->pre_vma(start, end, walk);
		if (err)
			return err;
	}

	if (is_vm_hugetlb_page(vma)) {
		if (ops->hugetlb_entry)
			err = walk_hugetlb_range(start, end, walk);
	} else
		err = walk_pgd_range(start, end, walk);

	if (ops->post_vma)
		ops->post_vma(walk);

	return err;
}

static inline void process_mm_walk_lock(struct mm_struct *mm,
					enum page_walk_lock walk_lock)
{
	if (walk_lock == PGWALK_RDLOCK)
		mmap_assert_locked(mm);
	else
		mmap_assert_write_locked(mm);
}

static inline void process_vma_walk_lock(struct vm_area_struct *vma,
					 enum page_walk_lock walk_lock)
{
#ifdef CONFIG_PER_VMA_LOCK
	switch (walk_lock) {
	case PGWALK_WRLOCK:
		vma_start_write(vma);
		break;
	case PGWALK_WRLOCK_VERIFY:
		vma_assert_write_locked(vma);
		break;
	case PGWALK_RDLOCK:
		 
		break;
	}
#endif
}

 
int walk_page_range(struct mm_struct *mm, unsigned long start,
		unsigned long end, const struct mm_walk_ops *ops,
		void *private)
{
	int err = 0;
	unsigned long next;
	struct vm_area_struct *vma;
	struct mm_walk walk = {
		.ops		= ops,
		.mm		= mm,
		.private	= private,
	};

	if (start >= end)
		return -EINVAL;

	if (!walk.mm)
		return -EINVAL;

	process_mm_walk_lock(walk.mm, ops->walk_lock);

	vma = find_vma(walk.mm, start);
	do {
		if (!vma) {  
			walk.vma = NULL;
			next = end;
			if (ops->pte_hole)
				err = ops->pte_hole(start, next, -1, &walk);
		} else if (start < vma->vm_start) {  
			walk.vma = NULL;
			next = min(end, vma->vm_start);
			if (ops->pte_hole)
				err = ops->pte_hole(start, next, -1, &walk);
		} else {  
			process_vma_walk_lock(vma, ops->walk_lock);
			walk.vma = vma;
			next = min(end, vma->vm_end);
			vma = find_vma(mm, vma->vm_end);

			err = walk_page_test(start, next, &walk);
			if (err > 0) {
				 
				err = 0;
				continue;
			}
			if (err < 0)
				break;
			err = __walk_page_range(start, next, &walk);
		}
		if (err)
			break;
	} while (start = next, start < end);
	return err;
}

 
int walk_page_range_novma(struct mm_struct *mm, unsigned long start,
			  unsigned long end, const struct mm_walk_ops *ops,
			  pgd_t *pgd,
			  void *private)
{
	struct mm_walk walk = {
		.ops		= ops,
		.mm		= mm,
		.pgd		= pgd,
		.private	= private,
		.no_vma		= true
	};

	if (start >= end || !walk.mm)
		return -EINVAL;

	mmap_assert_write_locked(walk.mm);

	return walk_pgd_range(start, end, &walk);
}

int walk_page_range_vma(struct vm_area_struct *vma, unsigned long start,
			unsigned long end, const struct mm_walk_ops *ops,
			void *private)
{
	struct mm_walk walk = {
		.ops		= ops,
		.mm		= vma->vm_mm,
		.vma		= vma,
		.private	= private,
	};

	if (start >= end || !walk.mm)
		return -EINVAL;
	if (start < vma->vm_start || end > vma->vm_end)
		return -EINVAL;

	process_mm_walk_lock(walk.mm, ops->walk_lock);
	process_vma_walk_lock(vma, ops->walk_lock);
	return __walk_page_range(start, end, &walk);
}

int walk_page_vma(struct vm_area_struct *vma, const struct mm_walk_ops *ops,
		void *private)
{
	struct mm_walk walk = {
		.ops		= ops,
		.mm		= vma->vm_mm,
		.vma		= vma,
		.private	= private,
	};

	if (!walk.mm)
		return -EINVAL;

	process_mm_walk_lock(walk.mm, ops->walk_lock);
	process_vma_walk_lock(vma, ops->walk_lock);
	return __walk_page_range(vma->vm_start, vma->vm_end, &walk);
}

 
int walk_page_mapping(struct address_space *mapping, pgoff_t first_index,
		      pgoff_t nr, const struct mm_walk_ops *ops,
		      void *private)
{
	struct mm_walk walk = {
		.ops		= ops,
		.private	= private,
	};
	struct vm_area_struct *vma;
	pgoff_t vba, vea, cba, cea;
	unsigned long start_addr, end_addr;
	int err = 0;

	lockdep_assert_held(&mapping->i_mmap_rwsem);
	vma_interval_tree_foreach(vma, &mapping->i_mmap, first_index,
				  first_index + nr - 1) {
		 
		vba = vma->vm_pgoff;
		vea = vba + vma_pages(vma);
		cba = first_index;
		cba = max(cba, vba);
		cea = first_index + nr;
		cea = min(cea, vea);

		start_addr = ((cba - vba) << PAGE_SHIFT) + vma->vm_start;
		end_addr = ((cea - vba) << PAGE_SHIFT) + vma->vm_start;
		if (start_addr >= end_addr)
			continue;

		walk.vma = vma;
		walk.mm = vma->vm_mm;

		err = walk_page_test(vma->vm_start, vma->vm_end, &walk);
		if (err > 0) {
			err = 0;
			break;
		} else if (err < 0)
			break;

		err = __walk_page_range(start_addr, end_addr, &walk);
		if (err)
			break;
	}

	return err;
}
