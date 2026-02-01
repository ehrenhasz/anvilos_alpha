
 

 
#include <linux/pagemap.h>
#include <linux/gfp.h>
#include <linux/pagewalk.h>
#include <linux/mman.h>
#include <linux/syscalls.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/shmem_fs.h>
#include <linux/hugetlb.h>
#include <linux/pgtable.h>

#include <linux/uaccess.h>
#include "swap.h"

static int mincore_hugetlb(pte_t *pte, unsigned long hmask, unsigned long addr,
			unsigned long end, struct mm_walk *walk)
{
#ifdef CONFIG_HUGETLB_PAGE
	unsigned char present;
	unsigned char *vec = walk->private;

	 
	present = pte && !huge_pte_none_mostly(huge_ptep_get(pte));
	for (; addr != end; vec++, addr += PAGE_SIZE)
		*vec = present;
	walk->private = vec;
#else
	BUG();
#endif
	return 0;
}

 
static unsigned char mincore_page(struct address_space *mapping, pgoff_t index)
{
	unsigned char present = 0;
	struct folio *folio;

	 
	folio = filemap_get_incore_folio(mapping, index);
	if (!IS_ERR(folio)) {
		present = folio_test_uptodate(folio);
		folio_put(folio);
	}

	return present;
}

static int __mincore_unmapped_range(unsigned long addr, unsigned long end,
				struct vm_area_struct *vma, unsigned char *vec)
{
	unsigned long nr = (end - addr) >> PAGE_SHIFT;
	int i;

	if (vma->vm_file) {
		pgoff_t pgoff;

		pgoff = linear_page_index(vma, addr);
		for (i = 0; i < nr; i++, pgoff++)
			vec[i] = mincore_page(vma->vm_file->f_mapping, pgoff);
	} else {
		for (i = 0; i < nr; i++)
			vec[i] = 0;
	}
	return nr;
}

static int mincore_unmapped_range(unsigned long addr, unsigned long end,
				   __always_unused int depth,
				   struct mm_walk *walk)
{
	walk->private += __mincore_unmapped_range(addr, end,
						  walk->vma, walk->private);
	return 0;
}

static int mincore_pte_range(pmd_t *pmd, unsigned long addr, unsigned long end,
			struct mm_walk *walk)
{
	spinlock_t *ptl;
	struct vm_area_struct *vma = walk->vma;
	pte_t *ptep;
	unsigned char *vec = walk->private;
	int nr = (end - addr) >> PAGE_SHIFT;

	ptl = pmd_trans_huge_lock(pmd, vma);
	if (ptl) {
		memset(vec, 1, nr);
		spin_unlock(ptl);
		goto out;
	}

	ptep = pte_offset_map_lock(walk->mm, pmd, addr, &ptl);
	if (!ptep) {
		walk->action = ACTION_AGAIN;
		return 0;
	}
	for (; addr != end; ptep++, addr += PAGE_SIZE) {
		pte_t pte = ptep_get(ptep);

		 
		if (pte_none_mostly(pte))
			__mincore_unmapped_range(addr, addr + PAGE_SIZE,
						 vma, vec);
		else if (pte_present(pte))
			*vec = 1;
		else {  
			swp_entry_t entry = pte_to_swp_entry(pte);

			if (non_swap_entry(entry)) {
				 
				*vec = 1;
			} else {
#ifdef CONFIG_SWAP
				*vec = mincore_page(swap_address_space(entry),
						    swp_offset(entry));
#else
				WARN_ON(1);
				*vec = 1;
#endif
			}
		}
		vec++;
	}
	pte_unmap_unlock(ptep - 1, ptl);
out:
	walk->private += nr;
	cond_resched();
	return 0;
}

static inline bool can_do_mincore(struct vm_area_struct *vma)
{
	if (vma_is_anonymous(vma))
		return true;
	if (!vma->vm_file)
		return false;
	 
	return inode_owner_or_capable(&nop_mnt_idmap,
				      file_inode(vma->vm_file)) ||
	       file_permission(vma->vm_file, MAY_WRITE) == 0;
}

static const struct mm_walk_ops mincore_walk_ops = {
	.pmd_entry		= mincore_pte_range,
	.pte_hole		= mincore_unmapped_range,
	.hugetlb_entry		= mincore_hugetlb,
	.walk_lock		= PGWALK_RDLOCK,
};

 
static long do_mincore(unsigned long addr, unsigned long pages, unsigned char *vec)
{
	struct vm_area_struct *vma;
	unsigned long end;
	int err;

	vma = vma_lookup(current->mm, addr);
	if (!vma)
		return -ENOMEM;
	end = min(vma->vm_end, addr + (pages << PAGE_SHIFT));
	if (!can_do_mincore(vma)) {
		unsigned long pages = DIV_ROUND_UP(end - addr, PAGE_SIZE);
		memset(vec, 1, pages);
		return pages;
	}
	err = walk_page_range(vma->vm_mm, addr, end, &mincore_walk_ops, vec);
	if (err < 0)
		return err;
	return (end - addr) >> PAGE_SHIFT;
}

 
SYSCALL_DEFINE3(mincore, unsigned long, start, size_t, len,
		unsigned char __user *, vec)
{
	long retval;
	unsigned long pages;
	unsigned char *tmp;

	start = untagged_addr(start);

	 
	if (start & ~PAGE_MASK)
		return -EINVAL;

	 
	if (!access_ok((void __user *) start, len))
		return -ENOMEM;

	 
	pages = len >> PAGE_SHIFT;
	pages += (offset_in_page(len)) != 0;

	if (!access_ok(vec, pages))
		return -EFAULT;

	tmp = (void *) __get_free_page(GFP_USER);
	if (!tmp)
		return -EAGAIN;

	retval = 0;
	while (pages) {
		 
		mmap_read_lock(current->mm);
		retval = do_mincore(start, min(pages, PAGE_SIZE), tmp);
		mmap_read_unlock(current->mm);

		if (retval <= 0)
			break;
		if (copy_to_user(vec, tmp, retval)) {
			retval = -EFAULT;
			break;
		}
		pages -= retval;
		vec += retval;
		start += retval << PAGE_SHIFT;
		retval = 0;
	}
	free_page((unsigned long) tmp);
	return retval;
}
