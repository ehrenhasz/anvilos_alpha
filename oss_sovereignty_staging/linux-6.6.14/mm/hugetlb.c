
 
#include <linux/list.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/sysctl.h>
#include <linux/highmem.h>
#include <linux/mmu_notifier.h>
#include <linux/nodemask.h>
#include <linux/pagemap.h>
#include <linux/mempolicy.h>
#include <linux/compiler.h>
#include <linux/cpuset.h>
#include <linux/mutex.h>
#include <linux/memblock.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/sched/mm.h>
#include <linux/mmdebug.h>
#include <linux/sched/signal.h>
#include <linux/rmap.h>
#include <linux/string_helpers.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/jhash.h>
#include <linux/numa.h>
#include <linux/llist.h>
#include <linux/cma.h>
#include <linux/migrate.h>
#include <linux/nospec.h>
#include <linux/delayacct.h>
#include <linux/memory.h>
#include <linux/mm_inline.h>

#include <asm/page.h>
#include <asm/pgalloc.h>
#include <asm/tlb.h>

#include <linux/io.h>
#include <linux/hugetlb.h>
#include <linux/hugetlb_cgroup.h>
#include <linux/node.h>
#include <linux/page_owner.h>
#include "internal.h"
#include "hugetlb_vmemmap.h"

int hugetlb_max_hstate __read_mostly;
unsigned int default_hstate_idx;
struct hstate hstates[HUGE_MAX_HSTATE];

#ifdef CONFIG_CMA
static struct cma *hugetlb_cma[MAX_NUMNODES];
static unsigned long hugetlb_cma_size_in_node[MAX_NUMNODES] __initdata;
static bool hugetlb_cma_folio(struct folio *folio, unsigned int order)
{
	return cma_pages_valid(hugetlb_cma[folio_nid(folio)], &folio->page,
				1 << order);
}
#else
static bool hugetlb_cma_folio(struct folio *folio, unsigned int order)
{
	return false;
}
#endif
static unsigned long hugetlb_cma_size __initdata;

__initdata LIST_HEAD(huge_boot_pages);

 
static struct hstate * __initdata parsed_hstate;
static unsigned long __initdata default_hstate_max_huge_pages;
static bool __initdata parsed_valid_hugepagesz = true;
static bool __initdata parsed_default_hugepagesz;
static unsigned int default_hugepages_in_node[MAX_NUMNODES] __initdata;

 
DEFINE_SPINLOCK(hugetlb_lock);

 
static int num_fault_mutexes;
struct mutex *hugetlb_fault_mutex_table ____cacheline_aligned_in_smp;

 
static int hugetlb_acct_memory(struct hstate *h, long delta);
static void hugetlb_vma_lock_free(struct vm_area_struct *vma);
static void hugetlb_vma_lock_alloc(struct vm_area_struct *vma);
static void __hugetlb_vma_unlock_write_free(struct vm_area_struct *vma);
static void hugetlb_unshare_pmds(struct vm_area_struct *vma,
		unsigned long start, unsigned long end);
static struct resv_map *vma_resv_map(struct vm_area_struct *vma);

static inline bool subpool_is_free(struct hugepage_subpool *spool)
{
	if (spool->count)
		return false;
	if (spool->max_hpages != -1)
		return spool->used_hpages == 0;
	if (spool->min_hpages != -1)
		return spool->rsv_hpages == spool->min_hpages;

	return true;
}

static inline void unlock_or_release_subpool(struct hugepage_subpool *spool,
						unsigned long irq_flags)
{
	spin_unlock_irqrestore(&spool->lock, irq_flags);

	 
	if (subpool_is_free(spool)) {
		if (spool->min_hpages != -1)
			hugetlb_acct_memory(spool->hstate,
						-spool->min_hpages);
		kfree(spool);
	}
}

struct hugepage_subpool *hugepage_new_subpool(struct hstate *h, long max_hpages,
						long min_hpages)
{
	struct hugepage_subpool *spool;

	spool = kzalloc(sizeof(*spool), GFP_KERNEL);
	if (!spool)
		return NULL;

	spin_lock_init(&spool->lock);
	spool->count = 1;
	spool->max_hpages = max_hpages;
	spool->hstate = h;
	spool->min_hpages = min_hpages;

	if (min_hpages != -1 && hugetlb_acct_memory(h, min_hpages)) {
		kfree(spool);
		return NULL;
	}
	spool->rsv_hpages = min_hpages;

	return spool;
}

void hugepage_put_subpool(struct hugepage_subpool *spool)
{
	unsigned long flags;

	spin_lock_irqsave(&spool->lock, flags);
	BUG_ON(!spool->count);
	spool->count--;
	unlock_or_release_subpool(spool, flags);
}

 
static long hugepage_subpool_get_pages(struct hugepage_subpool *spool,
				      long delta)
{
	long ret = delta;

	if (!spool)
		return ret;

	spin_lock_irq(&spool->lock);

	if (spool->max_hpages != -1) {		 
		if ((spool->used_hpages + delta) <= spool->max_hpages)
			spool->used_hpages += delta;
		else {
			ret = -ENOMEM;
			goto unlock_ret;
		}
	}

	 
	if (spool->min_hpages != -1 && spool->rsv_hpages) {
		if (delta > spool->rsv_hpages) {
			 
			ret = delta - spool->rsv_hpages;
			spool->rsv_hpages = 0;
		} else {
			ret = 0;	 
			spool->rsv_hpages -= delta;
		}
	}

unlock_ret:
	spin_unlock_irq(&spool->lock);
	return ret;
}

 
static long hugepage_subpool_put_pages(struct hugepage_subpool *spool,
				       long delta)
{
	long ret = delta;
	unsigned long flags;

	if (!spool)
		return delta;

	spin_lock_irqsave(&spool->lock, flags);

	if (spool->max_hpages != -1)		 
		spool->used_hpages -= delta;

	  
	if (spool->min_hpages != -1 && spool->used_hpages < spool->min_hpages) {
		if (spool->rsv_hpages + delta <= spool->min_hpages)
			ret = 0;
		else
			ret = spool->rsv_hpages + delta - spool->min_hpages;

		spool->rsv_hpages += delta;
		if (spool->rsv_hpages > spool->min_hpages)
			spool->rsv_hpages = spool->min_hpages;
	}

	 
	unlock_or_release_subpool(spool, flags);

	return ret;
}

static inline struct hugepage_subpool *subpool_inode(struct inode *inode)
{
	return HUGETLBFS_SB(inode->i_sb)->spool;
}

static inline struct hugepage_subpool *subpool_vma(struct vm_area_struct *vma)
{
	return subpool_inode(file_inode(vma->vm_file));
}

 
void hugetlb_vma_lock_read(struct vm_area_struct *vma)
{
	if (__vma_shareable_lock(vma)) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		down_read(&vma_lock->rw_sema);
	} else if (__vma_private_lock(vma)) {
		struct resv_map *resv_map = vma_resv_map(vma);

		down_read(&resv_map->rw_sema);
	}
}

void hugetlb_vma_unlock_read(struct vm_area_struct *vma)
{
	if (__vma_shareable_lock(vma)) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		up_read(&vma_lock->rw_sema);
	} else if (__vma_private_lock(vma)) {
		struct resv_map *resv_map = vma_resv_map(vma);

		up_read(&resv_map->rw_sema);
	}
}

void hugetlb_vma_lock_write(struct vm_area_struct *vma)
{
	if (__vma_shareable_lock(vma)) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		down_write(&vma_lock->rw_sema);
	} else if (__vma_private_lock(vma)) {
		struct resv_map *resv_map = vma_resv_map(vma);

		down_write(&resv_map->rw_sema);
	}
}

void hugetlb_vma_unlock_write(struct vm_area_struct *vma)
{
	if (__vma_shareable_lock(vma)) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		up_write(&vma_lock->rw_sema);
	} else if (__vma_private_lock(vma)) {
		struct resv_map *resv_map = vma_resv_map(vma);

		up_write(&resv_map->rw_sema);
	}
}

int hugetlb_vma_trylock_write(struct vm_area_struct *vma)
{

	if (__vma_shareable_lock(vma)) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		return down_write_trylock(&vma_lock->rw_sema);
	} else if (__vma_private_lock(vma)) {
		struct resv_map *resv_map = vma_resv_map(vma);

		return down_write_trylock(&resv_map->rw_sema);
	}

	return 1;
}

void hugetlb_vma_assert_locked(struct vm_area_struct *vma)
{
	if (__vma_shareable_lock(vma)) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		lockdep_assert_held(&vma_lock->rw_sema);
	} else if (__vma_private_lock(vma)) {
		struct resv_map *resv_map = vma_resv_map(vma);

		lockdep_assert_held(&resv_map->rw_sema);
	}
}

void hugetlb_vma_lock_release(struct kref *kref)
{
	struct hugetlb_vma_lock *vma_lock = container_of(kref,
			struct hugetlb_vma_lock, refs);

	kfree(vma_lock);
}

static void __hugetlb_vma_unlock_write_put(struct hugetlb_vma_lock *vma_lock)
{
	struct vm_area_struct *vma = vma_lock->vma;

	 
	vma_lock->vma = NULL;
	vma->vm_private_data = NULL;
	up_write(&vma_lock->rw_sema);
	kref_put(&vma_lock->refs, hugetlb_vma_lock_release);
}

static void __hugetlb_vma_unlock_write_free(struct vm_area_struct *vma)
{
	if (__vma_shareable_lock(vma)) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		__hugetlb_vma_unlock_write_put(vma_lock);
	} else if (__vma_private_lock(vma)) {
		struct resv_map *resv_map = vma_resv_map(vma);

		 
		up_write(&resv_map->rw_sema);
	}
}

static void hugetlb_vma_lock_free(struct vm_area_struct *vma)
{
	 
	if (!vma || !__vma_shareable_lock(vma))
		return;

	if (vma->vm_private_data) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		down_write(&vma_lock->rw_sema);
		__hugetlb_vma_unlock_write_put(vma_lock);
	}
}

static void hugetlb_vma_lock_alloc(struct vm_area_struct *vma)
{
	struct hugetlb_vma_lock *vma_lock;

	 
	if (!vma || !(vma->vm_flags & VM_MAYSHARE))
		return;

	 
	if (vma->vm_private_data)
		return;

	vma_lock = kmalloc(sizeof(*vma_lock), GFP_KERNEL);
	if (!vma_lock) {
		 
		pr_warn_once("HugeTLB: unable to allocate vma specific lock\n");
		return;
	}

	kref_init(&vma_lock->refs);
	init_rwsem(&vma_lock->rw_sema);
	vma_lock->vma = vma;
	vma->vm_private_data = vma_lock;
}

 
static struct file_region *
get_file_region_entry_from_cache(struct resv_map *resv, long from, long to)
{
	struct file_region *nrg;

	VM_BUG_ON(resv->region_cache_count <= 0);

	resv->region_cache_count--;
	nrg = list_first_entry(&resv->region_cache, struct file_region, link);
	list_del(&nrg->link);

	nrg->from = from;
	nrg->to = to;

	return nrg;
}

static void copy_hugetlb_cgroup_uncharge_info(struct file_region *nrg,
					      struct file_region *rg)
{
#ifdef CONFIG_CGROUP_HUGETLB
	nrg->reservation_counter = rg->reservation_counter;
	nrg->css = rg->css;
	if (rg->css)
		css_get(rg->css);
#endif
}

 
static void record_hugetlb_cgroup_uncharge_info(struct hugetlb_cgroup *h_cg,
						struct hstate *h,
						struct resv_map *resv,
						struct file_region *nrg)
{
#ifdef CONFIG_CGROUP_HUGETLB
	if (h_cg) {
		nrg->reservation_counter =
			&h_cg->rsvd_hugepage[hstate_index(h)];
		nrg->css = &h_cg->css;
		 
		css_get(&h_cg->css);
		if (!resv->pages_per_hpage)
			resv->pages_per_hpage = pages_per_huge_page(h);
		 
		VM_BUG_ON(resv->pages_per_hpage != pages_per_huge_page(h));
	} else {
		nrg->reservation_counter = NULL;
		nrg->css = NULL;
	}
#endif
}

static void put_uncharge_info(struct file_region *rg)
{
#ifdef CONFIG_CGROUP_HUGETLB
	if (rg->css)
		css_put(rg->css);
#endif
}

static bool has_same_uncharge_info(struct file_region *rg,
				   struct file_region *org)
{
#ifdef CONFIG_CGROUP_HUGETLB
	return rg->reservation_counter == org->reservation_counter &&
	       rg->css == org->css;

#else
	return true;
#endif
}

static void coalesce_file_region(struct resv_map *resv, struct file_region *rg)
{
	struct file_region *nrg, *prg;

	prg = list_prev_entry(rg, link);
	if (&prg->link != &resv->regions && prg->to == rg->from &&
	    has_same_uncharge_info(prg, rg)) {
		prg->to = rg->to;

		list_del(&rg->link);
		put_uncharge_info(rg);
		kfree(rg);

		rg = prg;
	}

	nrg = list_next_entry(rg, link);
	if (&nrg->link != &resv->regions && nrg->from == rg->to &&
	    has_same_uncharge_info(nrg, rg)) {
		nrg->from = rg->from;

		list_del(&rg->link);
		put_uncharge_info(rg);
		kfree(rg);
	}
}

static inline long
hugetlb_resv_map_add(struct resv_map *map, struct list_head *rg, long from,
		     long to, struct hstate *h, struct hugetlb_cgroup *cg,
		     long *regions_needed)
{
	struct file_region *nrg;

	if (!regions_needed) {
		nrg = get_file_region_entry_from_cache(map, from, to);
		record_hugetlb_cgroup_uncharge_info(cg, h, map, nrg);
		list_add(&nrg->link, rg);
		coalesce_file_region(map, nrg);
	} else
		*regions_needed += 1;

	return to - from;
}

 
static long add_reservation_in_range(struct resv_map *resv, long f, long t,
				     struct hugetlb_cgroup *h_cg,
				     struct hstate *h, long *regions_needed)
{
	long add = 0;
	struct list_head *head = &resv->regions;
	long last_accounted_offset = f;
	struct file_region *iter, *trg = NULL;
	struct list_head *rg = NULL;

	if (regions_needed)
		*regions_needed = 0;

	 
	list_for_each_entry_safe(iter, trg, head, link) {
		 
		if (iter->from < f) {
			 
			if (iter->to > last_accounted_offset)
				last_accounted_offset = iter->to;
			continue;
		}

		 
		if (iter->from >= t) {
			rg = iter->link.prev;
			break;
		}

		 
		if (iter->from > last_accounted_offset)
			add += hugetlb_resv_map_add(resv, iter->link.prev,
						    last_accounted_offset,
						    iter->from, h, h_cg,
						    regions_needed);

		last_accounted_offset = iter->to;
	}

	 
	if (!rg)
		rg = head->prev;
	if (last_accounted_offset < t)
		add += hugetlb_resv_map_add(resv, rg, last_accounted_offset,
					    t, h, h_cg, regions_needed);

	return add;
}

 
static int allocate_file_region_entries(struct resv_map *resv,
					int regions_needed)
	__must_hold(&resv->lock)
{
	LIST_HEAD(allocated_regions);
	int to_allocate = 0, i = 0;
	struct file_region *trg = NULL, *rg = NULL;

	VM_BUG_ON(regions_needed < 0);

	 
	while (resv->region_cache_count <
	       (resv->adds_in_progress + regions_needed)) {
		to_allocate = resv->adds_in_progress + regions_needed -
			      resv->region_cache_count;

		 
		VM_BUG_ON(resv->region_cache_count < resv->adds_in_progress);

		spin_unlock(&resv->lock);
		for (i = 0; i < to_allocate; i++) {
			trg = kmalloc(sizeof(*trg), GFP_KERNEL);
			if (!trg)
				goto out_of_memory;
			list_add(&trg->link, &allocated_regions);
		}

		spin_lock(&resv->lock);

		list_splice(&allocated_regions, &resv->region_cache);
		resv->region_cache_count += to_allocate;
	}

	return 0;

out_of_memory:
	list_for_each_entry_safe(rg, trg, &allocated_regions, link) {
		list_del(&rg->link);
		kfree(rg);
	}
	return -ENOMEM;
}

 
static long region_add(struct resv_map *resv, long f, long t,
		       long in_regions_needed, struct hstate *h,
		       struct hugetlb_cgroup *h_cg)
{
	long add = 0, actual_regions_needed = 0;

	spin_lock(&resv->lock);
retry:

	 
	add_reservation_in_range(resv, f, t, NULL, NULL,
				 &actual_regions_needed);

	 
	if (actual_regions_needed > in_regions_needed &&
	    resv->region_cache_count <
		    resv->adds_in_progress +
			    (actual_regions_needed - in_regions_needed)) {
		 
		VM_BUG_ON(t - f <= 1);

		if (allocate_file_region_entries(
			    resv, actual_regions_needed - in_regions_needed)) {
			return -ENOMEM;
		}

		goto retry;
	}

	add = add_reservation_in_range(resv, f, t, h_cg, h, NULL);

	resv->adds_in_progress -= in_regions_needed;

	spin_unlock(&resv->lock);
	return add;
}

 
static long region_chg(struct resv_map *resv, long f, long t,
		       long *out_regions_needed)
{
	long chg = 0;

	spin_lock(&resv->lock);

	 
	chg = add_reservation_in_range(resv, f, t, NULL, NULL,
				       out_regions_needed);

	if (*out_regions_needed == 0)
		*out_regions_needed = 1;

	if (allocate_file_region_entries(resv, *out_regions_needed))
		return -ENOMEM;

	resv->adds_in_progress += *out_regions_needed;

	spin_unlock(&resv->lock);
	return chg;
}

 
static void region_abort(struct resv_map *resv, long f, long t,
			 long regions_needed)
{
	spin_lock(&resv->lock);
	VM_BUG_ON(!resv->region_cache_count);
	resv->adds_in_progress -= regions_needed;
	spin_unlock(&resv->lock);
}

 
static long region_del(struct resv_map *resv, long f, long t)
{
	struct list_head *head = &resv->regions;
	struct file_region *rg, *trg;
	struct file_region *nrg = NULL;
	long del = 0;

retry:
	spin_lock(&resv->lock);
	list_for_each_entry_safe(rg, trg, head, link) {
		 
		if (rg->to <= f && (rg->to != rg->from || rg->to != f))
			continue;

		if (rg->from >= t)
			break;

		if (f > rg->from && t < rg->to) {  
			 
			if (!nrg &&
			    resv->region_cache_count > resv->adds_in_progress) {
				nrg = list_first_entry(&resv->region_cache,
							struct file_region,
							link);
				list_del(&nrg->link);
				resv->region_cache_count--;
			}

			if (!nrg) {
				spin_unlock(&resv->lock);
				nrg = kmalloc(sizeof(*nrg), GFP_KERNEL);
				if (!nrg)
					return -ENOMEM;
				goto retry;
			}

			del += t - f;
			hugetlb_cgroup_uncharge_file_region(
				resv, rg, t - f, false);

			 
			nrg->from = t;
			nrg->to = rg->to;

			copy_hugetlb_cgroup_uncharge_info(nrg, rg);

			INIT_LIST_HEAD(&nrg->link);

			 
			rg->to = f;

			list_add(&nrg->link, &rg->link);
			nrg = NULL;
			break;
		}

		if (f <= rg->from && t >= rg->to) {  
			del += rg->to - rg->from;
			hugetlb_cgroup_uncharge_file_region(resv, rg,
							    rg->to - rg->from, true);
			list_del(&rg->link);
			kfree(rg);
			continue;
		}

		if (f <= rg->from) {	 
			hugetlb_cgroup_uncharge_file_region(resv, rg,
							    t - rg->from, false);

			del += t - rg->from;
			rg->from = t;
		} else {		 
			hugetlb_cgroup_uncharge_file_region(resv, rg,
							    rg->to - f, false);

			del += rg->to - f;
			rg->to = f;
		}
	}

	spin_unlock(&resv->lock);
	kfree(nrg);
	return del;
}

 
void hugetlb_fix_reserve_counts(struct inode *inode)
{
	struct hugepage_subpool *spool = subpool_inode(inode);
	long rsv_adjust;
	bool reserved = false;

	rsv_adjust = hugepage_subpool_get_pages(spool, 1);
	if (rsv_adjust > 0) {
		struct hstate *h = hstate_inode(inode);

		if (!hugetlb_acct_memory(h, 1))
			reserved = true;
	} else if (!rsv_adjust) {
		reserved = true;
	}

	if (!reserved)
		pr_warn("hugetlb: Huge Page Reserved count may go negative.\n");
}

 
static long region_count(struct resv_map *resv, long f, long t)
{
	struct list_head *head = &resv->regions;
	struct file_region *rg;
	long chg = 0;

	spin_lock(&resv->lock);
	 
	list_for_each_entry(rg, head, link) {
		long seg_from;
		long seg_to;

		if (rg->to <= f)
			continue;
		if (rg->from >= t)
			break;

		seg_from = max(rg->from, f);
		seg_to = min(rg->to, t);

		chg += seg_to - seg_from;
	}
	spin_unlock(&resv->lock);

	return chg;
}

 
static pgoff_t vma_hugecache_offset(struct hstate *h,
			struct vm_area_struct *vma, unsigned long address)
{
	return ((address - vma->vm_start) >> huge_page_shift(h)) +
			(vma->vm_pgoff >> huge_page_order(h));
}

pgoff_t linear_hugepage_index(struct vm_area_struct *vma,
				     unsigned long address)
{
	return vma_hugecache_offset(hstate_vma(vma), vma, address);
}
EXPORT_SYMBOL_GPL(linear_hugepage_index);

 
unsigned long vma_kernel_pagesize(struct vm_area_struct *vma)
{
	if (vma->vm_ops && vma->vm_ops->pagesize)
		return vma->vm_ops->pagesize(vma);
	return PAGE_SIZE;
}
EXPORT_SYMBOL_GPL(vma_kernel_pagesize);

 
__weak unsigned long vma_mmu_pagesize(struct vm_area_struct *vma)
{
	return vma_kernel_pagesize(vma);
}

 
#define HPAGE_RESV_OWNER    (1UL << 0)
#define HPAGE_RESV_UNMAPPED (1UL << 1)
#define HPAGE_RESV_MASK (HPAGE_RESV_OWNER | HPAGE_RESV_UNMAPPED)

 
static unsigned long get_vma_private_data(struct vm_area_struct *vma)
{
	return (unsigned long)vma->vm_private_data;
}

static void set_vma_private_data(struct vm_area_struct *vma,
							unsigned long value)
{
	vma->vm_private_data = (void *)value;
}

static void
resv_map_set_hugetlb_cgroup_uncharge_info(struct resv_map *resv_map,
					  struct hugetlb_cgroup *h_cg,
					  struct hstate *h)
{
#ifdef CONFIG_CGROUP_HUGETLB
	if (!h_cg || !h) {
		resv_map->reservation_counter = NULL;
		resv_map->pages_per_hpage = 0;
		resv_map->css = NULL;
	} else {
		resv_map->reservation_counter =
			&h_cg->rsvd_hugepage[hstate_index(h)];
		resv_map->pages_per_hpage = pages_per_huge_page(h);
		resv_map->css = &h_cg->css;
	}
#endif
}

struct resv_map *resv_map_alloc(void)
{
	struct resv_map *resv_map = kmalloc(sizeof(*resv_map), GFP_KERNEL);
	struct file_region *rg = kmalloc(sizeof(*rg), GFP_KERNEL);

	if (!resv_map || !rg) {
		kfree(resv_map);
		kfree(rg);
		return NULL;
	}

	kref_init(&resv_map->refs);
	spin_lock_init(&resv_map->lock);
	INIT_LIST_HEAD(&resv_map->regions);
	init_rwsem(&resv_map->rw_sema);

	resv_map->adds_in_progress = 0;
	 
	resv_map_set_hugetlb_cgroup_uncharge_info(resv_map, NULL, NULL);

	INIT_LIST_HEAD(&resv_map->region_cache);
	list_add(&rg->link, &resv_map->region_cache);
	resv_map->region_cache_count = 1;

	return resv_map;
}

void resv_map_release(struct kref *ref)
{
	struct resv_map *resv_map = container_of(ref, struct resv_map, refs);
	struct list_head *head = &resv_map->region_cache;
	struct file_region *rg, *trg;

	 
	region_del(resv_map, 0, LONG_MAX);

	 
	list_for_each_entry_safe(rg, trg, head, link) {
		list_del(&rg->link);
		kfree(rg);
	}

	VM_BUG_ON(resv_map->adds_in_progress);

	kfree(resv_map);
}

static inline struct resv_map *inode_resv_map(struct inode *inode)
{
	 
	return (struct resv_map *)(&inode->i_data)->private_data;
}

static struct resv_map *vma_resv_map(struct vm_area_struct *vma)
{
	VM_BUG_ON_VMA(!is_vm_hugetlb_page(vma), vma);
	if (vma->vm_flags & VM_MAYSHARE) {
		struct address_space *mapping = vma->vm_file->f_mapping;
		struct inode *inode = mapping->host;

		return inode_resv_map(inode);

	} else {
		return (struct resv_map *)(get_vma_private_data(vma) &
							~HPAGE_RESV_MASK);
	}
}

static void set_vma_resv_map(struct vm_area_struct *vma, struct resv_map *map)
{
	VM_BUG_ON_VMA(!is_vm_hugetlb_page(vma), vma);
	VM_BUG_ON_VMA(vma->vm_flags & VM_MAYSHARE, vma);

	set_vma_private_data(vma, (unsigned long)map);
}

static void set_vma_resv_flags(struct vm_area_struct *vma, unsigned long flags)
{
	VM_BUG_ON_VMA(!is_vm_hugetlb_page(vma), vma);
	VM_BUG_ON_VMA(vma->vm_flags & VM_MAYSHARE, vma);

	set_vma_private_data(vma, get_vma_private_data(vma) | flags);
}

static int is_vma_resv_set(struct vm_area_struct *vma, unsigned long flag)
{
	VM_BUG_ON_VMA(!is_vm_hugetlb_page(vma), vma);

	return (get_vma_private_data(vma) & flag) != 0;
}

bool __vma_private_lock(struct vm_area_struct *vma)
{
	return !(vma->vm_flags & VM_MAYSHARE) &&
		get_vma_private_data(vma) & ~HPAGE_RESV_MASK &&
		is_vma_resv_set(vma, HPAGE_RESV_OWNER);
}

void hugetlb_dup_vma_private(struct vm_area_struct *vma)
{
	VM_BUG_ON_VMA(!is_vm_hugetlb_page(vma), vma);
	 
	if (vma->vm_flags & VM_MAYSHARE) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		if (vma_lock && vma_lock->vma != vma)
			vma->vm_private_data = NULL;
	} else
		vma->vm_private_data = NULL;
}

 
void clear_vma_resv_huge_pages(struct vm_area_struct *vma)
{
	 
	struct resv_map *reservations = vma_resv_map(vma);

	if (reservations && is_vma_resv_set(vma, HPAGE_RESV_OWNER)) {
		resv_map_put_hugetlb_cgroup_uncharge_info(reservations);
		kref_put(&reservations->refs, resv_map_release);
	}

	hugetlb_dup_vma_private(vma);
}

 
static bool vma_has_reserves(struct vm_area_struct *vma, long chg)
{
	if (vma->vm_flags & VM_NORESERVE) {
		 
		if (vma->vm_flags & VM_MAYSHARE && chg == 0)
			return true;
		else
			return false;
	}

	 
	if (vma->vm_flags & VM_MAYSHARE) {
		 
		if (chg)
			return false;
		else
			return true;
	}

	 
	if (is_vma_resv_set(vma, HPAGE_RESV_OWNER)) {
		 
		if (chg)
			return false;
		else
			return true;
	}

	return false;
}

static void enqueue_hugetlb_folio(struct hstate *h, struct folio *folio)
{
	int nid = folio_nid(folio);

	lockdep_assert_held(&hugetlb_lock);
	VM_BUG_ON_FOLIO(folio_ref_count(folio), folio);

	list_move(&folio->lru, &h->hugepage_freelists[nid]);
	h->free_huge_pages++;
	h->free_huge_pages_node[nid]++;
	folio_set_hugetlb_freed(folio);
}

static struct folio *dequeue_hugetlb_folio_node_exact(struct hstate *h,
								int nid)
{
	struct folio *folio;
	bool pin = !!(current->flags & PF_MEMALLOC_PIN);

	lockdep_assert_held(&hugetlb_lock);
	list_for_each_entry(folio, &h->hugepage_freelists[nid], lru) {
		if (pin && !folio_is_longterm_pinnable(folio))
			continue;

		if (folio_test_hwpoison(folio))
			continue;

		list_move(&folio->lru, &h->hugepage_activelist);
		folio_ref_unfreeze(folio, 1);
		folio_clear_hugetlb_freed(folio);
		h->free_huge_pages--;
		h->free_huge_pages_node[nid]--;
		return folio;
	}

	return NULL;
}

static struct folio *dequeue_hugetlb_folio_nodemask(struct hstate *h, gfp_t gfp_mask,
							int nid, nodemask_t *nmask)
{
	unsigned int cpuset_mems_cookie;
	struct zonelist *zonelist;
	struct zone *zone;
	struct zoneref *z;
	int node = NUMA_NO_NODE;

	zonelist = node_zonelist(nid, gfp_mask);

retry_cpuset:
	cpuset_mems_cookie = read_mems_allowed_begin();
	for_each_zone_zonelist_nodemask(zone, z, zonelist, gfp_zone(gfp_mask), nmask) {
		struct folio *folio;

		if (!cpuset_zone_allowed(zone, gfp_mask))
			continue;
		 
		if (zone_to_nid(zone) == node)
			continue;
		node = zone_to_nid(zone);

		folio = dequeue_hugetlb_folio_node_exact(h, node);
		if (folio)
			return folio;
	}
	if (unlikely(read_mems_allowed_retry(cpuset_mems_cookie)))
		goto retry_cpuset;

	return NULL;
}

static unsigned long available_huge_pages(struct hstate *h)
{
	return h->free_huge_pages - h->resv_huge_pages;
}

static struct folio *dequeue_hugetlb_folio_vma(struct hstate *h,
				struct vm_area_struct *vma,
				unsigned long address, int avoid_reserve,
				long chg)
{
	struct folio *folio = NULL;
	struct mempolicy *mpol;
	gfp_t gfp_mask;
	nodemask_t *nodemask;
	int nid;

	 
	if (!vma_has_reserves(vma, chg) && !available_huge_pages(h))
		goto err;

	 
	if (avoid_reserve && !available_huge_pages(h))
		goto err;

	gfp_mask = htlb_alloc_mask(h);
	nid = huge_node(vma, address, gfp_mask, &mpol, &nodemask);

	if (mpol_is_preferred_many(mpol)) {
		folio = dequeue_hugetlb_folio_nodemask(h, gfp_mask,
							nid, nodemask);

		 
		nodemask = NULL;
	}

	if (!folio)
		folio = dequeue_hugetlb_folio_nodemask(h, gfp_mask,
							nid, nodemask);

	if (folio && !avoid_reserve && vma_has_reserves(vma, chg)) {
		folio_set_hugetlb_restore_reserve(folio);
		h->resv_huge_pages--;
	}

	mpol_cond_put(mpol);
	return folio;

err:
	return NULL;
}

 
static int next_node_allowed(int nid, nodemask_t *nodes_allowed)
{
	nid = next_node_in(nid, *nodes_allowed);
	VM_BUG_ON(nid >= MAX_NUMNODES);

	return nid;
}

static int get_valid_node_allowed(int nid, nodemask_t *nodes_allowed)
{
	if (!node_isset(nid, *nodes_allowed))
		nid = next_node_allowed(nid, nodes_allowed);
	return nid;
}

 
static int hstate_next_node_to_alloc(struct hstate *h,
					nodemask_t *nodes_allowed)
{
	int nid;

	VM_BUG_ON(!nodes_allowed);

	nid = get_valid_node_allowed(h->next_nid_to_alloc, nodes_allowed);
	h->next_nid_to_alloc = next_node_allowed(nid, nodes_allowed);

	return nid;
}

 
static int hstate_next_node_to_free(struct hstate *h, nodemask_t *nodes_allowed)
{
	int nid;

	VM_BUG_ON(!nodes_allowed);

	nid = get_valid_node_allowed(h->next_nid_to_free, nodes_allowed);
	h->next_nid_to_free = next_node_allowed(nid, nodes_allowed);

	return nid;
}

#define for_each_node_mask_to_alloc(hs, nr_nodes, node, mask)		\
	for (nr_nodes = nodes_weight(*mask);				\
		nr_nodes > 0 &&						\
		((node = hstate_next_node_to_alloc(hs, mask)) || 1);	\
		nr_nodes--)

#define for_each_node_mask_to_free(hs, nr_nodes, node, mask)		\
	for (nr_nodes = nodes_weight(*mask);				\
		nr_nodes > 0 &&						\
		((node = hstate_next_node_to_free(hs, mask)) || 1);	\
		nr_nodes--)

 
static void __destroy_compound_gigantic_folio(struct folio *folio,
					unsigned int order, bool demote)
{
	int i;
	int nr_pages = 1 << order;
	struct page *p;

	atomic_set(&folio->_entire_mapcount, 0);
	atomic_set(&folio->_nr_pages_mapped, 0);
	atomic_set(&folio->_pincount, 0);

	for (i = 1; i < nr_pages; i++) {
		p = folio_page(folio, i);
		p->flags &= ~PAGE_FLAGS_CHECK_AT_FREE;
		p->mapping = NULL;
		clear_compound_head(p);
		if (!demote)
			set_page_refcounted(p);
	}

	__folio_clear_head(folio);
}

static void destroy_compound_hugetlb_folio_for_demote(struct folio *folio,
					unsigned int order)
{
	__destroy_compound_gigantic_folio(folio, order, true);
}

#ifdef CONFIG_ARCH_HAS_GIGANTIC_PAGE
static void destroy_compound_gigantic_folio(struct folio *folio,
					unsigned int order)
{
	__destroy_compound_gigantic_folio(folio, order, false);
}

static void free_gigantic_folio(struct folio *folio, unsigned int order)
{
	 
#ifdef CONFIG_CMA
	int nid = folio_nid(folio);

	if (cma_release(hugetlb_cma[nid], &folio->page, 1 << order))
		return;
#endif

	free_contig_range(folio_pfn(folio), 1 << order);
}

#ifdef CONFIG_CONTIG_ALLOC
static struct folio *alloc_gigantic_folio(struct hstate *h, gfp_t gfp_mask,
		int nid, nodemask_t *nodemask)
{
	struct page *page;
	unsigned long nr_pages = pages_per_huge_page(h);
	if (nid == NUMA_NO_NODE)
		nid = numa_mem_id();

#ifdef CONFIG_CMA
	{
		int node;

		if (hugetlb_cma[nid]) {
			page = cma_alloc(hugetlb_cma[nid], nr_pages,
					huge_page_order(h), true);
			if (page)
				return page_folio(page);
		}

		if (!(gfp_mask & __GFP_THISNODE)) {
			for_each_node_mask(node, *nodemask) {
				if (node == nid || !hugetlb_cma[node])
					continue;

				page = cma_alloc(hugetlb_cma[node], nr_pages,
						huge_page_order(h), true);
				if (page)
					return page_folio(page);
			}
		}
	}
#endif

	page = alloc_contig_pages(nr_pages, gfp_mask, nid, nodemask);
	return page ? page_folio(page) : NULL;
}

#else  
static struct folio *alloc_gigantic_folio(struct hstate *h, gfp_t gfp_mask,
					int nid, nodemask_t *nodemask)
{
	return NULL;
}
#endif  

#else  
static struct folio *alloc_gigantic_folio(struct hstate *h, gfp_t gfp_mask,
					int nid, nodemask_t *nodemask)
{
	return NULL;
}
static inline void free_gigantic_folio(struct folio *folio,
						unsigned int order) { }
static inline void destroy_compound_gigantic_folio(struct folio *folio,
						unsigned int order) { }
#endif

static inline void __clear_hugetlb_destructor(struct hstate *h,
						struct folio *folio)
{
	lockdep_assert_held(&hugetlb_lock);

	folio_clear_hugetlb(folio);
}

 
static void __remove_hugetlb_folio(struct hstate *h, struct folio *folio,
							bool adjust_surplus,
							bool demote)
{
	int nid = folio_nid(folio);

	VM_BUG_ON_FOLIO(hugetlb_cgroup_from_folio(folio), folio);
	VM_BUG_ON_FOLIO(hugetlb_cgroup_from_folio_rsvd(folio), folio);

	lockdep_assert_held(&hugetlb_lock);
	if (hstate_is_gigantic(h) && !gigantic_page_runtime_supported())
		return;

	list_del(&folio->lru);

	if (folio_test_hugetlb_freed(folio)) {
		h->free_huge_pages--;
		h->free_huge_pages_node[nid]--;
	}
	if (adjust_surplus) {
		h->surplus_huge_pages--;
		h->surplus_huge_pages_node[nid]--;
	}

	 
	if (!folio_test_hugetlb_vmemmap_optimized(folio))
		__clear_hugetlb_destructor(h, folio);

	  
	if (!demote)
		folio_ref_unfreeze(folio, 1);

	h->nr_huge_pages--;
	h->nr_huge_pages_node[nid]--;
}

static void remove_hugetlb_folio(struct hstate *h, struct folio *folio,
							bool adjust_surplus)
{
	__remove_hugetlb_folio(h, folio, adjust_surplus, false);
}

static void remove_hugetlb_folio_for_demote(struct hstate *h, struct folio *folio,
							bool adjust_surplus)
{
	__remove_hugetlb_folio(h, folio, adjust_surplus, true);
}

static void add_hugetlb_folio(struct hstate *h, struct folio *folio,
			     bool adjust_surplus)
{
	int zeroed;
	int nid = folio_nid(folio);

	VM_BUG_ON_FOLIO(!folio_test_hugetlb_vmemmap_optimized(folio), folio);

	lockdep_assert_held(&hugetlb_lock);

	INIT_LIST_HEAD(&folio->lru);
	h->nr_huge_pages++;
	h->nr_huge_pages_node[nid]++;

	if (adjust_surplus) {
		h->surplus_huge_pages++;
		h->surplus_huge_pages_node[nid]++;
	}

	folio_set_hugetlb(folio);
	folio_change_private(folio, NULL);
	 
	folio_set_hugetlb_vmemmap_optimized(folio);

	 
	zeroed = folio_put_testzero(folio);
	if (unlikely(!zeroed))
		 
		return;

	arch_clear_hugepage_flags(&folio->page);
	enqueue_hugetlb_folio(h, folio);
}

static void __update_and_free_hugetlb_folio(struct hstate *h,
						struct folio *folio)
{
	bool clear_dtor = folio_test_hugetlb_vmemmap_optimized(folio);

	if (hstate_is_gigantic(h) && !gigantic_page_runtime_supported())
		return;

	 
	if (folio_test_hugetlb_raw_hwp_unreliable(folio))
		return;

	if (hugetlb_vmemmap_restore(h, &folio->page)) {
		spin_lock_irq(&hugetlb_lock);
		 
		add_hugetlb_folio(h, folio, true);
		spin_unlock_irq(&hugetlb_lock);
		return;
	}

	 
	if (unlikely(folio_test_hwpoison(folio)))
		folio_clear_hugetlb_hwpoison(folio);

	 
	if (clear_dtor) {
		spin_lock_irq(&hugetlb_lock);
		__clear_hugetlb_destructor(h, folio);
		spin_unlock_irq(&hugetlb_lock);
	}

	 
	if (hstate_is_gigantic(h) ||
	    hugetlb_cma_folio(folio, huge_page_order(h))) {
		destroy_compound_gigantic_folio(folio, huge_page_order(h));
		free_gigantic_folio(folio, huge_page_order(h));
	} else {
		__free_pages(&folio->page, huge_page_order(h));
	}
}

 
static LLIST_HEAD(hpage_freelist);

static void free_hpage_workfn(struct work_struct *work)
{
	struct llist_node *node;

	node = llist_del_all(&hpage_freelist);

	while (node) {
		struct page *page;
		struct hstate *h;

		page = container_of((struct address_space **)node,
				     struct page, mapping);
		node = node->next;
		page->mapping = NULL;
		 
		h = size_to_hstate(page_size(page));

		__update_and_free_hugetlb_folio(h, page_folio(page));

		cond_resched();
	}
}
static DECLARE_WORK(free_hpage_work, free_hpage_workfn);

static inline void flush_free_hpage_work(struct hstate *h)
{
	if (hugetlb_vmemmap_optimizable(h))
		flush_work(&free_hpage_work);
}

static void update_and_free_hugetlb_folio(struct hstate *h, struct folio *folio,
				 bool atomic)
{
	if (!folio_test_hugetlb_vmemmap_optimized(folio) || !atomic) {
		__update_and_free_hugetlb_folio(h, folio);
		return;
	}

	 
	if (llist_add((struct llist_node *)&folio->mapping, &hpage_freelist))
		schedule_work(&free_hpage_work);
}

static void update_and_free_pages_bulk(struct hstate *h, struct list_head *list)
{
	struct page *page, *t_page;
	struct folio *folio;

	list_for_each_entry_safe(page, t_page, list, lru) {
		folio = page_folio(page);
		update_and_free_hugetlb_folio(h, folio, false);
		cond_resched();
	}
}

struct hstate *size_to_hstate(unsigned long size)
{
	struct hstate *h;

	for_each_hstate(h) {
		if (huge_page_size(h) == size)
			return h;
	}
	return NULL;
}

void free_huge_folio(struct folio *folio)
{
	 
	struct hstate *h = folio_hstate(folio);
	int nid = folio_nid(folio);
	struct hugepage_subpool *spool = hugetlb_folio_subpool(folio);
	bool restore_reserve;
	unsigned long flags;

	VM_BUG_ON_FOLIO(folio_ref_count(folio), folio);
	VM_BUG_ON_FOLIO(folio_mapcount(folio), folio);

	hugetlb_set_folio_subpool(folio, NULL);
	if (folio_test_anon(folio))
		__ClearPageAnonExclusive(&folio->page);
	folio->mapping = NULL;
	restore_reserve = folio_test_hugetlb_restore_reserve(folio);
	folio_clear_hugetlb_restore_reserve(folio);

	 
	if (!restore_reserve) {
		 
		if (hugepage_subpool_put_pages(spool, 1) == 0)
			restore_reserve = true;
	}

	spin_lock_irqsave(&hugetlb_lock, flags);
	folio_clear_hugetlb_migratable(folio);
	hugetlb_cgroup_uncharge_folio(hstate_index(h),
				     pages_per_huge_page(h), folio);
	hugetlb_cgroup_uncharge_folio_rsvd(hstate_index(h),
					  pages_per_huge_page(h), folio);
	if (restore_reserve)
		h->resv_huge_pages++;

	if (folio_test_hugetlb_temporary(folio)) {
		remove_hugetlb_folio(h, folio, false);
		spin_unlock_irqrestore(&hugetlb_lock, flags);
		update_and_free_hugetlb_folio(h, folio, true);
	} else if (h->surplus_huge_pages_node[nid]) {
		 
		remove_hugetlb_folio(h, folio, true);
		spin_unlock_irqrestore(&hugetlb_lock, flags);
		update_and_free_hugetlb_folio(h, folio, true);
	} else {
		arch_clear_hugepage_flags(&folio->page);
		enqueue_hugetlb_folio(h, folio);
		spin_unlock_irqrestore(&hugetlb_lock, flags);
	}
}

 
static void __prep_account_new_huge_page(struct hstate *h, int nid)
{
	lockdep_assert_held(&hugetlb_lock);
	h->nr_huge_pages++;
	h->nr_huge_pages_node[nid]++;
}

static void __prep_new_hugetlb_folio(struct hstate *h, struct folio *folio)
{
	hugetlb_vmemmap_optimize(h, &folio->page);
	INIT_LIST_HEAD(&folio->lru);
	folio_set_hugetlb(folio);
	hugetlb_set_folio_subpool(folio, NULL);
	set_hugetlb_cgroup(folio, NULL);
	set_hugetlb_cgroup_rsvd(folio, NULL);
}

static void prep_new_hugetlb_folio(struct hstate *h, struct folio *folio, int nid)
{
	__prep_new_hugetlb_folio(h, folio);
	spin_lock_irq(&hugetlb_lock);
	__prep_account_new_huge_page(h, nid);
	spin_unlock_irq(&hugetlb_lock);
}

static bool __prep_compound_gigantic_folio(struct folio *folio,
					unsigned int order, bool demote)
{
	int i, j;
	int nr_pages = 1 << order;
	struct page *p;

	__folio_clear_reserved(folio);
	for (i = 0; i < nr_pages; i++) {
		p = folio_page(folio, i);

		 
		if (i != 0)	 
			__ClearPageReserved(p);
		 
		if (!demote) {
			if (!page_ref_freeze(p, 1)) {
				pr_warn("HugeTLB page can not be used due to unexpected inflated ref count\n");
				goto out_error;
			}
		} else {
			VM_BUG_ON_PAGE(page_count(p), p);
		}
		if (i != 0)
			set_compound_head(p, &folio->page);
	}
	__folio_set_head(folio);
	 
	folio_set_order(folio, order);
	atomic_set(&folio->_entire_mapcount, -1);
	atomic_set(&folio->_nr_pages_mapped, 0);
	atomic_set(&folio->_pincount, 0);
	return true;

out_error:
	 
	for (j = 0; j < i; j++) {
		p = folio_page(folio, j);
		if (j != 0)
			clear_compound_head(p);
		set_page_refcounted(p);
	}
	 
	for (; j < nr_pages; j++) {
		p = folio_page(folio, j);
		__ClearPageReserved(p);
	}
	return false;
}

static bool prep_compound_gigantic_folio(struct folio *folio,
							unsigned int order)
{
	return __prep_compound_gigantic_folio(folio, order, false);
}

static bool prep_compound_gigantic_folio_for_demote(struct folio *folio,
							unsigned int order)
{
	return __prep_compound_gigantic_folio(folio, order, true);
}

 
int PageHuge(struct page *page)
{
	struct folio *folio;

	if (!PageCompound(page))
		return 0;
	folio = page_folio(page);
	return folio_test_hugetlb(folio);
}
EXPORT_SYMBOL_GPL(PageHuge);

 
struct address_space *hugetlb_page_mapping_lock_write(struct page *hpage)
{
	struct address_space *mapping = page_mapping(hpage);

	if (!mapping)
		return mapping;

	if (i_mmap_trylock_write(mapping))
		return mapping;

	return NULL;
}

pgoff_t hugetlb_basepage_index(struct page *page)
{
	struct page *page_head = compound_head(page);
	pgoff_t index = page_index(page_head);
	unsigned long compound_idx;

	if (compound_order(page_head) > MAX_ORDER)
		compound_idx = page_to_pfn(page) - page_to_pfn(page_head);
	else
		compound_idx = page - page_head;

	return (index << compound_order(page_head)) + compound_idx;
}

static struct folio *alloc_buddy_hugetlb_folio(struct hstate *h,
		gfp_t gfp_mask, int nid, nodemask_t *nmask,
		nodemask_t *node_alloc_noretry)
{
	int order = huge_page_order(h);
	struct page *page;
	bool alloc_try_hard = true;
	bool retry = true;

	 
	if (node_alloc_noretry && node_isset(nid, *node_alloc_noretry))
		alloc_try_hard = false;
	gfp_mask |= __GFP_COMP|__GFP_NOWARN;
	if (alloc_try_hard)
		gfp_mask |= __GFP_RETRY_MAYFAIL;
	if (nid == NUMA_NO_NODE)
		nid = numa_mem_id();
retry:
	page = __alloc_pages(gfp_mask, order, nid, nmask);

	 
	if (page && !page_ref_freeze(page, 1)) {
		__free_pages(page, order);
		if (retry) {	 
			retry = false;
			goto retry;
		}
		 
		pr_warn("HugeTLB head page unexpected inflated ref count\n");
		page = NULL;
	}

	 
	if (node_alloc_noretry && page && !alloc_try_hard)
		node_clear(nid, *node_alloc_noretry);

	 
	if (node_alloc_noretry && !page && alloc_try_hard)
		node_set(nid, *node_alloc_noretry);

	if (!page) {
		__count_vm_event(HTLB_BUDDY_PGALLOC_FAIL);
		return NULL;
	}

	__count_vm_event(HTLB_BUDDY_PGALLOC);
	return page_folio(page);
}

 
static struct folio *alloc_fresh_hugetlb_folio(struct hstate *h,
		gfp_t gfp_mask, int nid, nodemask_t *nmask,
		nodemask_t *node_alloc_noretry)
{
	struct folio *folio;
	bool retry = false;

retry:
	if (hstate_is_gigantic(h))
		folio = alloc_gigantic_folio(h, gfp_mask, nid, nmask);
	else
		folio = alloc_buddy_hugetlb_folio(h, gfp_mask,
				nid, nmask, node_alloc_noretry);
	if (!folio)
		return NULL;
	if (hstate_is_gigantic(h)) {
		if (!prep_compound_gigantic_folio(folio, huge_page_order(h))) {
			 
			free_gigantic_folio(folio, huge_page_order(h));
			if (!retry) {
				retry = true;
				goto retry;
			}
			return NULL;
		}
	}
	prep_new_hugetlb_folio(h, folio, folio_nid(folio));

	return folio;
}

 
static int alloc_pool_huge_page(struct hstate *h, nodemask_t *nodes_allowed,
				nodemask_t *node_alloc_noretry)
{
	struct folio *folio;
	int nr_nodes, node;
	gfp_t gfp_mask = htlb_alloc_mask(h) | __GFP_THISNODE;

	for_each_node_mask_to_alloc(h, nr_nodes, node, nodes_allowed) {
		folio = alloc_fresh_hugetlb_folio(h, gfp_mask, node,
					nodes_allowed, node_alloc_noretry);
		if (folio) {
			free_huge_folio(folio);  
			return 1;
		}
	}

	return 0;
}

 
static struct page *remove_pool_huge_page(struct hstate *h,
						nodemask_t *nodes_allowed,
						 bool acct_surplus)
{
	int nr_nodes, node;
	struct page *page = NULL;
	struct folio *folio;

	lockdep_assert_held(&hugetlb_lock);
	for_each_node_mask_to_free(h, nr_nodes, node, nodes_allowed) {
		 
		if ((!acct_surplus || h->surplus_huge_pages_node[node]) &&
		    !list_empty(&h->hugepage_freelists[node])) {
			page = list_entry(h->hugepage_freelists[node].next,
					  struct page, lru);
			folio = page_folio(page);
			remove_hugetlb_folio(h, folio, acct_surplus);
			break;
		}
	}

	return page;
}

 
int dissolve_free_huge_page(struct page *page)
{
	int rc = -EBUSY;
	struct folio *folio = page_folio(page);

retry:
	 
	if (!folio_test_hugetlb(folio))
		return 0;

	spin_lock_irq(&hugetlb_lock);
	if (!folio_test_hugetlb(folio)) {
		rc = 0;
		goto out;
	}

	if (!folio_ref_count(folio)) {
		struct hstate *h = folio_hstate(folio);
		if (!available_huge_pages(h))
			goto out;

		 
		if (unlikely(!folio_test_hugetlb_freed(folio))) {
			spin_unlock_irq(&hugetlb_lock);
			cond_resched();

			 
			goto retry;
		}

		remove_hugetlb_folio(h, folio, false);
		h->max_huge_pages--;
		spin_unlock_irq(&hugetlb_lock);

		 
		rc = hugetlb_vmemmap_restore(h, &folio->page);
		if (!rc) {
			update_and_free_hugetlb_folio(h, folio, false);
		} else {
			spin_lock_irq(&hugetlb_lock);
			add_hugetlb_folio(h, folio, false);
			h->max_huge_pages++;
			spin_unlock_irq(&hugetlb_lock);
		}

		return rc;
	}
out:
	spin_unlock_irq(&hugetlb_lock);
	return rc;
}

 
int dissolve_free_huge_pages(unsigned long start_pfn, unsigned long end_pfn)
{
	unsigned long pfn;
	struct page *page;
	int rc = 0;
	unsigned int order;
	struct hstate *h;

	if (!hugepages_supported())
		return rc;

	order = huge_page_order(&default_hstate);
	for_each_hstate(h)
		order = min(order, huge_page_order(h));

	for (pfn = start_pfn; pfn < end_pfn; pfn += 1 << order) {
		page = pfn_to_page(pfn);
		rc = dissolve_free_huge_page(page);
		if (rc)
			break;
	}

	return rc;
}

 
static struct folio *alloc_surplus_hugetlb_folio(struct hstate *h,
				gfp_t gfp_mask,	int nid, nodemask_t *nmask)
{
	struct folio *folio = NULL;

	if (hstate_is_gigantic(h))
		return NULL;

	spin_lock_irq(&hugetlb_lock);
	if (h->surplus_huge_pages >= h->nr_overcommit_huge_pages)
		goto out_unlock;
	spin_unlock_irq(&hugetlb_lock);

	folio = alloc_fresh_hugetlb_folio(h, gfp_mask, nid, nmask, NULL);
	if (!folio)
		return NULL;

	spin_lock_irq(&hugetlb_lock);
	 
	if (h->surplus_huge_pages >= h->nr_overcommit_huge_pages) {
		folio_set_hugetlb_temporary(folio);
		spin_unlock_irq(&hugetlb_lock);
		free_huge_folio(folio);
		return NULL;
	}

	h->surplus_huge_pages++;
	h->surplus_huge_pages_node[folio_nid(folio)]++;

out_unlock:
	spin_unlock_irq(&hugetlb_lock);

	return folio;
}

static struct folio *alloc_migrate_hugetlb_folio(struct hstate *h, gfp_t gfp_mask,
				     int nid, nodemask_t *nmask)
{
	struct folio *folio;

	if (hstate_is_gigantic(h))
		return NULL;

	folio = alloc_fresh_hugetlb_folio(h, gfp_mask, nid, nmask, NULL);
	if (!folio)
		return NULL;

	 
	folio_ref_unfreeze(folio, 1);
	 
	folio_set_hugetlb_temporary(folio);

	return folio;
}

 
static
struct folio *alloc_buddy_hugetlb_folio_with_mpol(struct hstate *h,
		struct vm_area_struct *vma, unsigned long addr)
{
	struct folio *folio = NULL;
	struct mempolicy *mpol;
	gfp_t gfp_mask = htlb_alloc_mask(h);
	int nid;
	nodemask_t *nodemask;

	nid = huge_node(vma, addr, gfp_mask, &mpol, &nodemask);
	if (mpol_is_preferred_many(mpol)) {
		gfp_t gfp = gfp_mask | __GFP_NOWARN;

		gfp &=  ~(__GFP_DIRECT_RECLAIM | __GFP_NOFAIL);
		folio = alloc_surplus_hugetlb_folio(h, gfp, nid, nodemask);

		 
		nodemask = NULL;
	}

	if (!folio)
		folio = alloc_surplus_hugetlb_folio(h, gfp_mask, nid, nodemask);
	mpol_cond_put(mpol);
	return folio;
}

 
struct folio *alloc_hugetlb_folio_nodemask(struct hstate *h, int preferred_nid,
		nodemask_t *nmask, gfp_t gfp_mask)
{
	spin_lock_irq(&hugetlb_lock);
	if (available_huge_pages(h)) {
		struct folio *folio;

		folio = dequeue_hugetlb_folio_nodemask(h, gfp_mask,
						preferred_nid, nmask);
		if (folio) {
			spin_unlock_irq(&hugetlb_lock);
			return folio;
		}
	}
	spin_unlock_irq(&hugetlb_lock);

	return alloc_migrate_hugetlb_folio(h, gfp_mask, preferred_nid, nmask);
}

 
struct folio *alloc_hugetlb_folio_vma(struct hstate *h, struct vm_area_struct *vma,
		unsigned long address)
{
	struct mempolicy *mpol;
	nodemask_t *nodemask;
	struct folio *folio;
	gfp_t gfp_mask;
	int node;

	gfp_mask = htlb_alloc_mask(h);
	node = huge_node(vma, address, gfp_mask, &mpol, &nodemask);
	folio = alloc_hugetlb_folio_nodemask(h, node, nodemask, gfp_mask);
	mpol_cond_put(mpol);

	return folio;
}

 
static int gather_surplus_pages(struct hstate *h, long delta)
	__must_hold(&hugetlb_lock)
{
	LIST_HEAD(surplus_list);
	struct folio *folio, *tmp;
	int ret;
	long i;
	long needed, allocated;
	bool alloc_ok = true;

	lockdep_assert_held(&hugetlb_lock);
	needed = (h->resv_huge_pages + delta) - h->free_huge_pages;
	if (needed <= 0) {
		h->resv_huge_pages += delta;
		return 0;
	}

	allocated = 0;

	ret = -ENOMEM;
retry:
	spin_unlock_irq(&hugetlb_lock);
	for (i = 0; i < needed; i++) {
		folio = alloc_surplus_hugetlb_folio(h, htlb_alloc_mask(h),
				NUMA_NO_NODE, NULL);
		if (!folio) {
			alloc_ok = false;
			break;
		}
		list_add(&folio->lru, &surplus_list);
		cond_resched();
	}
	allocated += i;

	 
	spin_lock_irq(&hugetlb_lock);
	needed = (h->resv_huge_pages + delta) -
			(h->free_huge_pages + allocated);
	if (needed > 0) {
		if (alloc_ok)
			goto retry;
		 
		goto free;
	}
	 
	needed += allocated;
	h->resv_huge_pages += delta;
	ret = 0;

	 
	list_for_each_entry_safe(folio, tmp, &surplus_list, lru) {
		if ((--needed) < 0)
			break;
		 
		enqueue_hugetlb_folio(h, folio);
	}
free:
	spin_unlock_irq(&hugetlb_lock);

	 
	list_for_each_entry_safe(folio, tmp, &surplus_list, lru)
		free_huge_folio(folio);
	spin_lock_irq(&hugetlb_lock);

	return ret;
}

 
static void return_unused_surplus_pages(struct hstate *h,
					unsigned long unused_resv_pages)
{
	unsigned long nr_pages;
	struct page *page;
	LIST_HEAD(page_list);

	lockdep_assert_held(&hugetlb_lock);
	 
	h->resv_huge_pages -= unused_resv_pages;

	if (hstate_is_gigantic(h) && !gigantic_page_runtime_supported())
		goto out;

	 
	nr_pages = min(unused_resv_pages, h->surplus_huge_pages);

	 
	while (nr_pages--) {
		page = remove_pool_huge_page(h, &node_states[N_MEMORY], 1);
		if (!page)
			goto out;

		list_add(&page->lru, &page_list);
	}

out:
	spin_unlock_irq(&hugetlb_lock);
	update_and_free_pages_bulk(h, &page_list);
	spin_lock_irq(&hugetlb_lock);
}


 
enum vma_resv_mode {
	VMA_NEEDS_RESV,
	VMA_COMMIT_RESV,
	VMA_END_RESV,
	VMA_ADD_RESV,
	VMA_DEL_RESV,
};
static long __vma_reservation_common(struct hstate *h,
				struct vm_area_struct *vma, unsigned long addr,
				enum vma_resv_mode mode)
{
	struct resv_map *resv;
	pgoff_t idx;
	long ret;
	long dummy_out_regions_needed;

	resv = vma_resv_map(vma);
	if (!resv)
		return 1;

	idx = vma_hugecache_offset(h, vma, addr);
	switch (mode) {
	case VMA_NEEDS_RESV:
		ret = region_chg(resv, idx, idx + 1, &dummy_out_regions_needed);
		 
		VM_BUG_ON(dummy_out_regions_needed != 1);
		break;
	case VMA_COMMIT_RESV:
		ret = region_add(resv, idx, idx + 1, 1, NULL, NULL);
		 
		VM_BUG_ON(ret < 0);
		break;
	case VMA_END_RESV:
		region_abort(resv, idx, idx + 1, 1);
		ret = 0;
		break;
	case VMA_ADD_RESV:
		if (vma->vm_flags & VM_MAYSHARE) {
			ret = region_add(resv, idx, idx + 1, 1, NULL, NULL);
			 
			VM_BUG_ON(ret < 0);
		} else {
			region_abort(resv, idx, idx + 1, 1);
			ret = region_del(resv, idx, idx + 1);
		}
		break;
	case VMA_DEL_RESV:
		if (vma->vm_flags & VM_MAYSHARE) {
			region_abort(resv, idx, idx + 1, 1);
			ret = region_del(resv, idx, idx + 1);
		} else {
			ret = region_add(resv, idx, idx + 1, 1, NULL, NULL);
			 
			VM_BUG_ON(ret < 0);
		}
		break;
	default:
		BUG();
	}

	if (vma->vm_flags & VM_MAYSHARE || mode == VMA_DEL_RESV)
		return ret;
	 
	if (ret > 0)
		return 0;
	if (ret == 0)
		return 1;
	return ret;
}

static long vma_needs_reservation(struct hstate *h,
			struct vm_area_struct *vma, unsigned long addr)
{
	return __vma_reservation_common(h, vma, addr, VMA_NEEDS_RESV);
}

static long vma_commit_reservation(struct hstate *h,
			struct vm_area_struct *vma, unsigned long addr)
{
	return __vma_reservation_common(h, vma, addr, VMA_COMMIT_RESV);
}

static void vma_end_reservation(struct hstate *h,
			struct vm_area_struct *vma, unsigned long addr)
{
	(void)__vma_reservation_common(h, vma, addr, VMA_END_RESV);
}

static long vma_add_reservation(struct hstate *h,
			struct vm_area_struct *vma, unsigned long addr)
{
	return __vma_reservation_common(h, vma, addr, VMA_ADD_RESV);
}

static long vma_del_reservation(struct hstate *h,
			struct vm_area_struct *vma, unsigned long addr)
{
	return __vma_reservation_common(h, vma, addr, VMA_DEL_RESV);
}

 
void restore_reserve_on_error(struct hstate *h, struct vm_area_struct *vma,
			unsigned long address, struct folio *folio)
{
	long rc = vma_needs_reservation(h, vma, address);

	if (folio_test_hugetlb_restore_reserve(folio)) {
		if (unlikely(rc < 0))
			 
			folio_clear_hugetlb_restore_reserve(folio);
		else if (rc)
			(void)vma_add_reservation(h, vma, address);
		else
			vma_end_reservation(h, vma, address);
	} else {
		if (!rc) {
			 
			rc = vma_del_reservation(h, vma, address);
			if (rc < 0)
				 
				folio_set_hugetlb_restore_reserve(folio);
		} else if (rc < 0) {
			 
			if (!(vma->vm_flags & VM_MAYSHARE))
				 
				folio_set_hugetlb_restore_reserve(folio);
		} else
			 
			 vma_end_reservation(h, vma, address);
	}
}

 
static int alloc_and_dissolve_hugetlb_folio(struct hstate *h,
			struct folio *old_folio, struct list_head *list)
{
	gfp_t gfp_mask = htlb_alloc_mask(h) | __GFP_THISNODE;
	int nid = folio_nid(old_folio);
	struct folio *new_folio;
	int ret = 0;

	 
	new_folio = alloc_buddy_hugetlb_folio(h, gfp_mask, nid, NULL, NULL);
	if (!new_folio)
		return -ENOMEM;
	__prep_new_hugetlb_folio(h, new_folio);

retry:
	spin_lock_irq(&hugetlb_lock);
	if (!folio_test_hugetlb(old_folio)) {
		 
		goto free_new;
	} else if (folio_ref_count(old_folio)) {
		bool isolated;

		 
		spin_unlock_irq(&hugetlb_lock);
		isolated = isolate_hugetlb(old_folio, list);
		ret = isolated ? 0 : -EBUSY;
		spin_lock_irq(&hugetlb_lock);
		goto free_new;
	} else if (!folio_test_hugetlb_freed(old_folio)) {
		 
		spin_unlock_irq(&hugetlb_lock);
		cond_resched();
		goto retry;
	} else {
		 
		remove_hugetlb_folio(h, old_folio, false);

		 
		__prep_account_new_huge_page(h, nid);
		enqueue_hugetlb_folio(h, new_folio);

		 
		spin_unlock_irq(&hugetlb_lock);
		update_and_free_hugetlb_folio(h, old_folio, false);
	}

	return ret;

free_new:
	spin_unlock_irq(&hugetlb_lock);
	 
	folio_ref_unfreeze(new_folio, 1);
	update_and_free_hugetlb_folio(h, new_folio, false);

	return ret;
}

int isolate_or_dissolve_huge_page(struct page *page, struct list_head *list)
{
	struct hstate *h;
	struct folio *folio = page_folio(page);
	int ret = -EBUSY;

	 
	spin_lock_irq(&hugetlb_lock);
	if (folio_test_hugetlb(folio)) {
		h = folio_hstate(folio);
	} else {
		spin_unlock_irq(&hugetlb_lock);
		return 0;
	}
	spin_unlock_irq(&hugetlb_lock);

	 
	if (hstate_is_gigantic(h))
		return -ENOMEM;

	if (folio_ref_count(folio) && isolate_hugetlb(folio, list))
		ret = 0;
	else if (!folio_ref_count(folio))
		ret = alloc_and_dissolve_hugetlb_folio(h, folio, list);

	return ret;
}

struct folio *alloc_hugetlb_folio(struct vm_area_struct *vma,
				    unsigned long addr, int avoid_reserve)
{
	struct hugepage_subpool *spool = subpool_vma(vma);
	struct hstate *h = hstate_vma(vma);
	struct folio *folio;
	long map_chg, map_commit;
	long gbl_chg;
	int ret, idx;
	struct hugetlb_cgroup *h_cg = NULL;
	bool deferred_reserve;

	idx = hstate_index(h);
	 
	map_chg = gbl_chg = vma_needs_reservation(h, vma, addr);
	if (map_chg < 0)
		return ERR_PTR(-ENOMEM);

	 
	if (map_chg || avoid_reserve) {
		gbl_chg = hugepage_subpool_get_pages(spool, 1);
		if (gbl_chg < 0) {
			vma_end_reservation(h, vma, addr);
			return ERR_PTR(-ENOSPC);
		}

		 
		if (avoid_reserve)
			gbl_chg = 1;
	}

	 
	deferred_reserve = map_chg || avoid_reserve;
	if (deferred_reserve) {
		ret = hugetlb_cgroup_charge_cgroup_rsvd(
			idx, pages_per_huge_page(h), &h_cg);
		if (ret)
			goto out_subpool_put;
	}

	ret = hugetlb_cgroup_charge_cgroup(idx, pages_per_huge_page(h), &h_cg);
	if (ret)
		goto out_uncharge_cgroup_reservation;

	spin_lock_irq(&hugetlb_lock);
	 
	folio = dequeue_hugetlb_folio_vma(h, vma, addr, avoid_reserve, gbl_chg);
	if (!folio) {
		spin_unlock_irq(&hugetlb_lock);
		folio = alloc_buddy_hugetlb_folio_with_mpol(h, vma, addr);
		if (!folio)
			goto out_uncharge_cgroup;
		spin_lock_irq(&hugetlb_lock);
		if (!avoid_reserve && vma_has_reserves(vma, gbl_chg)) {
			folio_set_hugetlb_restore_reserve(folio);
			h->resv_huge_pages--;
		}
		list_add(&folio->lru, &h->hugepage_activelist);
		folio_ref_unfreeze(folio, 1);
		 
	}

	hugetlb_cgroup_commit_charge(idx, pages_per_huge_page(h), h_cg, folio);
	 
	if (deferred_reserve) {
		hugetlb_cgroup_commit_charge_rsvd(idx, pages_per_huge_page(h),
						  h_cg, folio);
	}

	spin_unlock_irq(&hugetlb_lock);

	hugetlb_set_folio_subpool(folio, spool);

	map_commit = vma_commit_reservation(h, vma, addr);
	if (unlikely(map_chg > map_commit)) {
		 
		long rsv_adjust;

		rsv_adjust = hugepage_subpool_put_pages(spool, 1);
		hugetlb_acct_memory(h, -rsv_adjust);
		if (deferred_reserve)
			hugetlb_cgroup_uncharge_folio_rsvd(hstate_index(h),
					pages_per_huge_page(h), folio);
	}
	return folio;

out_uncharge_cgroup:
	hugetlb_cgroup_uncharge_cgroup(idx, pages_per_huge_page(h), h_cg);
out_uncharge_cgroup_reservation:
	if (deferred_reserve)
		hugetlb_cgroup_uncharge_cgroup_rsvd(idx, pages_per_huge_page(h),
						    h_cg);
out_subpool_put:
	if (map_chg || avoid_reserve)
		hugepage_subpool_put_pages(spool, 1);
	vma_end_reservation(h, vma, addr);
	return ERR_PTR(-ENOSPC);
}

int alloc_bootmem_huge_page(struct hstate *h, int nid)
	__attribute__ ((weak, alias("__alloc_bootmem_huge_page")));
int __alloc_bootmem_huge_page(struct hstate *h, int nid)
{
	struct huge_bootmem_page *m = NULL;  
	int nr_nodes, node;

	 
	if (nid != NUMA_NO_NODE) {
		m = memblock_alloc_try_nid_raw(huge_page_size(h), huge_page_size(h),
				0, MEMBLOCK_ALLOC_ACCESSIBLE, nid);
		if (!m)
			return 0;
		goto found;
	}
	 
	for_each_node_mask_to_alloc(h, nr_nodes, node, &node_states[N_MEMORY]) {
		m = memblock_alloc_try_nid_raw(
				huge_page_size(h), huge_page_size(h),
				0, MEMBLOCK_ALLOC_ACCESSIBLE, node);
		 
		if (!m)
			return 0;
		goto found;
	}

found:
	 
	INIT_LIST_HEAD(&m->list);
	list_add(&m->list, &huge_boot_pages);
	m->hstate = h;
	return 1;
}

 
static void __init gather_bootmem_prealloc(void)
{
	struct huge_bootmem_page *m;

	list_for_each_entry(m, &huge_boot_pages, list) {
		struct page *page = virt_to_page(m);
		struct folio *folio = page_folio(page);
		struct hstate *h = m->hstate;

		VM_BUG_ON(!hstate_is_gigantic(h));
		WARN_ON(folio_ref_count(folio) != 1);
		if (prep_compound_gigantic_folio(folio, huge_page_order(h))) {
			WARN_ON(folio_test_reserved(folio));
			prep_new_hugetlb_folio(h, folio, folio_nid(folio));
			free_huge_folio(folio);  
		} else {
			 
			free_gigantic_folio(folio, huge_page_order(h));
		}

		 
		adjust_managed_page_count(page, pages_per_huge_page(h));
		cond_resched();
	}
}
static void __init hugetlb_hstate_alloc_pages_onenode(struct hstate *h, int nid)
{
	unsigned long i;
	char buf[32];

	for (i = 0; i < h->max_huge_pages_node[nid]; ++i) {
		if (hstate_is_gigantic(h)) {
			if (!alloc_bootmem_huge_page(h, nid))
				break;
		} else {
			struct folio *folio;
			gfp_t gfp_mask = htlb_alloc_mask(h) | __GFP_THISNODE;

			folio = alloc_fresh_hugetlb_folio(h, gfp_mask, nid,
					&node_states[N_MEMORY], NULL);
			if (!folio)
				break;
			free_huge_folio(folio);  
		}
		cond_resched();
	}
	if (i == h->max_huge_pages_node[nid])
		return;

	string_get_size(huge_page_size(h), 1, STRING_UNITS_2, buf, 32);
	pr_warn("HugeTLB: allocating %u of page size %s failed node%d.  Only allocated %lu hugepages.\n",
		h->max_huge_pages_node[nid], buf, nid, i);
	h->max_huge_pages -= (h->max_huge_pages_node[nid] - i);
	h->max_huge_pages_node[nid] = i;
}

static void __init hugetlb_hstate_alloc_pages(struct hstate *h)
{
	unsigned long i;
	nodemask_t *node_alloc_noretry;
	bool node_specific_alloc = false;

	 
	if (hstate_is_gigantic(h) && hugetlb_cma_size) {
		pr_warn_once("HugeTLB: hugetlb_cma is enabled, skip boot time allocation\n");
		return;
	}

	 
	for_each_online_node(i) {
		if (h->max_huge_pages_node[i] > 0) {
			hugetlb_hstate_alloc_pages_onenode(h, i);
			node_specific_alloc = true;
		}
	}

	if (node_specific_alloc)
		return;

	 
	if (!hstate_is_gigantic(h)) {
		 
		node_alloc_noretry = kmalloc(sizeof(*node_alloc_noretry),
						GFP_KERNEL);
	} else {
		 
		node_alloc_noretry = NULL;
	}

	 
	if (node_alloc_noretry)
		nodes_clear(*node_alloc_noretry);

	for (i = 0; i < h->max_huge_pages; ++i) {
		if (hstate_is_gigantic(h)) {
			if (!alloc_bootmem_huge_page(h, NUMA_NO_NODE))
				break;
		} else if (!alloc_pool_huge_page(h,
					 &node_states[N_MEMORY],
					 node_alloc_noretry))
			break;
		cond_resched();
	}
	if (i < h->max_huge_pages) {
		char buf[32];

		string_get_size(huge_page_size(h), 1, STRING_UNITS_2, buf, 32);
		pr_warn("HugeTLB: allocating %lu of page size %s failed.  Only allocated %lu hugepages.\n",
			h->max_huge_pages, buf, i);
		h->max_huge_pages = i;
	}
	kfree(node_alloc_noretry);
}

static void __init hugetlb_init_hstates(void)
{
	struct hstate *h, *h2;

	for_each_hstate(h) {
		 
		if (!hstate_is_gigantic(h))
			hugetlb_hstate_alloc_pages(h);

		 
		if (hstate_is_gigantic(h) && !gigantic_page_runtime_supported())
			continue;
		if (hugetlb_cma_size && h->order <= HUGETLB_PAGE_ORDER)
			continue;
		for_each_hstate(h2) {
			if (h2 == h)
				continue;
			if (h2->order < h->order &&
			    h2->order > h->demote_order)
				h->demote_order = h2->order;
		}
	}
}

static void __init report_hugepages(void)
{
	struct hstate *h;

	for_each_hstate(h) {
		char buf[32];

		string_get_size(huge_page_size(h), 1, STRING_UNITS_2, buf, 32);
		pr_info("HugeTLB: registered %s page size, pre-allocated %ld pages\n",
			buf, h->free_huge_pages);
		pr_info("HugeTLB: %d KiB vmemmap can be freed for a %s page\n",
			hugetlb_vmemmap_optimizable_size(h) / SZ_1K, buf);
	}
}

#ifdef CONFIG_HIGHMEM
static void try_to_free_low(struct hstate *h, unsigned long count,
						nodemask_t *nodes_allowed)
{
	int i;
	LIST_HEAD(page_list);

	lockdep_assert_held(&hugetlb_lock);
	if (hstate_is_gigantic(h))
		return;

	 
	for_each_node_mask(i, *nodes_allowed) {
		struct page *page, *next;
		struct list_head *freel = &h->hugepage_freelists[i];
		list_for_each_entry_safe(page, next, freel, lru) {
			if (count >= h->nr_huge_pages)
				goto out;
			if (PageHighMem(page))
				continue;
			remove_hugetlb_folio(h, page_folio(page), false);
			list_add(&page->lru, &page_list);
		}
	}

out:
	spin_unlock_irq(&hugetlb_lock);
	update_and_free_pages_bulk(h, &page_list);
	spin_lock_irq(&hugetlb_lock);
}
#else
static inline void try_to_free_low(struct hstate *h, unsigned long count,
						nodemask_t *nodes_allowed)
{
}
#endif

 
static int adjust_pool_surplus(struct hstate *h, nodemask_t *nodes_allowed,
				int delta)
{
	int nr_nodes, node;

	lockdep_assert_held(&hugetlb_lock);
	VM_BUG_ON(delta != -1 && delta != 1);

	if (delta < 0) {
		for_each_node_mask_to_alloc(h, nr_nodes, node, nodes_allowed) {
			if (h->surplus_huge_pages_node[node])
				goto found;
		}
	} else {
		for_each_node_mask_to_free(h, nr_nodes, node, nodes_allowed) {
			if (h->surplus_huge_pages_node[node] <
					h->nr_huge_pages_node[node])
				goto found;
		}
	}
	return 0;

found:
	h->surplus_huge_pages += delta;
	h->surplus_huge_pages_node[node] += delta;
	return 1;
}

#define persistent_huge_pages(h) (h->nr_huge_pages - h->surplus_huge_pages)
static int set_max_huge_pages(struct hstate *h, unsigned long count, int nid,
			      nodemask_t *nodes_allowed)
{
	unsigned long min_count, ret;
	struct page *page;
	LIST_HEAD(page_list);
	NODEMASK_ALLOC(nodemask_t, node_alloc_noretry, GFP_KERNEL);

	 
	if (node_alloc_noretry)
		nodes_clear(*node_alloc_noretry);
	else
		return -ENOMEM;

	 
	mutex_lock(&h->resize_lock);
	flush_free_hpage_work(h);
	spin_lock_irq(&hugetlb_lock);

	 
	if (nid != NUMA_NO_NODE) {
		unsigned long old_count = count;

		count += h->nr_huge_pages - h->nr_huge_pages_node[nid];
		 
		if (count < old_count)
			count = ULONG_MAX;
	}

	 
	if (hstate_is_gigantic(h) && !IS_ENABLED(CONFIG_CONTIG_ALLOC)) {
		if (count > persistent_huge_pages(h)) {
			spin_unlock_irq(&hugetlb_lock);
			mutex_unlock(&h->resize_lock);
			NODEMASK_FREE(node_alloc_noretry);
			return -EINVAL;
		}
		 
	}

	 
	while (h->surplus_huge_pages && count > persistent_huge_pages(h)) {
		if (!adjust_pool_surplus(h, nodes_allowed, -1))
			break;
	}

	while (count > persistent_huge_pages(h)) {
		 
		spin_unlock_irq(&hugetlb_lock);

		 
		cond_resched();

		ret = alloc_pool_huge_page(h, nodes_allowed,
						node_alloc_noretry);
		spin_lock_irq(&hugetlb_lock);
		if (!ret)
			goto out;

		 
		if (signal_pending(current))
			goto out;
	}

	 
	min_count = h->resv_huge_pages + h->nr_huge_pages - h->free_huge_pages;
	min_count = max(count, min_count);
	try_to_free_low(h, min_count, nodes_allowed);

	 
	while (min_count < persistent_huge_pages(h)) {
		page = remove_pool_huge_page(h, nodes_allowed, 0);
		if (!page)
			break;

		list_add(&page->lru, &page_list);
	}
	 
	spin_unlock_irq(&hugetlb_lock);
	update_and_free_pages_bulk(h, &page_list);
	flush_free_hpage_work(h);
	spin_lock_irq(&hugetlb_lock);

	while (count < persistent_huge_pages(h)) {
		if (!adjust_pool_surplus(h, nodes_allowed, 1))
			break;
	}
out:
	h->max_huge_pages = persistent_huge_pages(h);
	spin_unlock_irq(&hugetlb_lock);
	mutex_unlock(&h->resize_lock);

	NODEMASK_FREE(node_alloc_noretry);

	return 0;
}

static int demote_free_hugetlb_folio(struct hstate *h, struct folio *folio)
{
	int i, nid = folio_nid(folio);
	struct hstate *target_hstate;
	struct page *subpage;
	struct folio *inner_folio;
	int rc = 0;

	target_hstate = size_to_hstate(PAGE_SIZE << h->demote_order);

	remove_hugetlb_folio_for_demote(h, folio, false);
	spin_unlock_irq(&hugetlb_lock);

	rc = hugetlb_vmemmap_restore(h, &folio->page);
	if (rc) {
		 
		spin_lock_irq(&hugetlb_lock);
		folio_ref_unfreeze(folio, 1);
		add_hugetlb_folio(h, folio, false);
		return rc;
	}

	 
	destroy_compound_hugetlb_folio_for_demote(folio, huge_page_order(h));

	 
	mutex_lock(&target_hstate->resize_lock);
	for (i = 0; i < pages_per_huge_page(h);
				i += pages_per_huge_page(target_hstate)) {
		subpage = folio_page(folio, i);
		inner_folio = page_folio(subpage);
		if (hstate_is_gigantic(target_hstate))
			prep_compound_gigantic_folio_for_demote(inner_folio,
							target_hstate->order);
		else
			prep_compound_page(subpage, target_hstate->order);
		folio_change_private(inner_folio, NULL);
		prep_new_hugetlb_folio(target_hstate, inner_folio, nid);
		free_huge_folio(inner_folio);
	}
	mutex_unlock(&target_hstate->resize_lock);

	spin_lock_irq(&hugetlb_lock);

	 
	h->max_huge_pages--;
	target_hstate->max_huge_pages +=
		pages_per_huge_page(h) / pages_per_huge_page(target_hstate);

	return rc;
}

static int demote_pool_huge_page(struct hstate *h, nodemask_t *nodes_allowed)
	__must_hold(&hugetlb_lock)
{
	int nr_nodes, node;
	struct folio *folio;

	lockdep_assert_held(&hugetlb_lock);

	 
	if (!h->demote_order) {
		pr_warn("HugeTLB: NULL demote order passed to demote_pool_huge_page.\n");
		return -EINVAL;		 
	}

	for_each_node_mask_to_free(h, nr_nodes, node, nodes_allowed) {
		list_for_each_entry(folio, &h->hugepage_freelists[node], lru) {
			if (folio_test_hwpoison(folio))
				continue;
			return demote_free_hugetlb_folio(h, folio);
		}
	}

	 
	return -EBUSY;
}

#define HSTATE_ATTR_RO(_name) \
	static struct kobj_attribute _name##_attr = __ATTR_RO(_name)

#define HSTATE_ATTR_WO(_name) \
	static struct kobj_attribute _name##_attr = __ATTR_WO(_name)

#define HSTATE_ATTR(_name) \
	static struct kobj_attribute _name##_attr = __ATTR_RW(_name)

static struct kobject *hugepages_kobj;
static struct kobject *hstate_kobjs[HUGE_MAX_HSTATE];

static struct hstate *kobj_to_node_hstate(struct kobject *kobj, int *nidp);

static struct hstate *kobj_to_hstate(struct kobject *kobj, int *nidp)
{
	int i;

	for (i = 0; i < HUGE_MAX_HSTATE; i++)
		if (hstate_kobjs[i] == kobj) {
			if (nidp)
				*nidp = NUMA_NO_NODE;
			return &hstates[i];
		}

	return kobj_to_node_hstate(kobj, nidp);
}

static ssize_t nr_hugepages_show_common(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct hstate *h;
	unsigned long nr_huge_pages;
	int nid;

	h = kobj_to_hstate(kobj, &nid);
	if (nid == NUMA_NO_NODE)
		nr_huge_pages = h->nr_huge_pages;
	else
		nr_huge_pages = h->nr_huge_pages_node[nid];

	return sysfs_emit(buf, "%lu\n", nr_huge_pages);
}

static ssize_t __nr_hugepages_store_common(bool obey_mempolicy,
					   struct hstate *h, int nid,
					   unsigned long count, size_t len)
{
	int err;
	nodemask_t nodes_allowed, *n_mask;

	if (hstate_is_gigantic(h) && !gigantic_page_runtime_supported())
		return -EINVAL;

	if (nid == NUMA_NO_NODE) {
		 
		if (!(obey_mempolicy &&
				init_nodemask_of_mempolicy(&nodes_allowed)))
			n_mask = &node_states[N_MEMORY];
		else
			n_mask = &nodes_allowed;
	} else {
		 
		init_nodemask_of_node(&nodes_allowed, nid);
		n_mask = &nodes_allowed;
	}

	err = set_max_huge_pages(h, count, nid, n_mask);

	return err ? err : len;
}

static ssize_t nr_hugepages_store_common(bool obey_mempolicy,
					 struct kobject *kobj, const char *buf,
					 size_t len)
{
	struct hstate *h;
	unsigned long count;
	int nid;
	int err;

	err = kstrtoul(buf, 10, &count);
	if (err)
		return err;

	h = kobj_to_hstate(kobj, &nid);
	return __nr_hugepages_store_common(obey_mempolicy, h, nid, count, len);
}

static ssize_t nr_hugepages_show(struct kobject *kobj,
				       struct kobj_attribute *attr, char *buf)
{
	return nr_hugepages_show_common(kobj, attr, buf);
}

static ssize_t nr_hugepages_store(struct kobject *kobj,
	       struct kobj_attribute *attr, const char *buf, size_t len)
{
	return nr_hugepages_store_common(false, kobj, buf, len);
}
HSTATE_ATTR(nr_hugepages);

#ifdef CONFIG_NUMA

 
static ssize_t nr_hugepages_mempolicy_show(struct kobject *kobj,
					   struct kobj_attribute *attr,
					   char *buf)
{
	return nr_hugepages_show_common(kobj, attr, buf);
}

static ssize_t nr_hugepages_mempolicy_store(struct kobject *kobj,
	       struct kobj_attribute *attr, const char *buf, size_t len)
{
	return nr_hugepages_store_common(true, kobj, buf, len);
}
HSTATE_ATTR(nr_hugepages_mempolicy);
#endif


static ssize_t nr_overcommit_hugepages_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct hstate *h = kobj_to_hstate(kobj, NULL);
	return sysfs_emit(buf, "%lu\n", h->nr_overcommit_huge_pages);
}

static ssize_t nr_overcommit_hugepages_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int err;
	unsigned long input;
	struct hstate *h = kobj_to_hstate(kobj, NULL);

	if (hstate_is_gigantic(h))
		return -EINVAL;

	err = kstrtoul(buf, 10, &input);
	if (err)
		return err;

	spin_lock_irq(&hugetlb_lock);
	h->nr_overcommit_huge_pages = input;
	spin_unlock_irq(&hugetlb_lock);

	return count;
}
HSTATE_ATTR(nr_overcommit_hugepages);

static ssize_t free_hugepages_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct hstate *h;
	unsigned long free_huge_pages;
	int nid;

	h = kobj_to_hstate(kobj, &nid);
	if (nid == NUMA_NO_NODE)
		free_huge_pages = h->free_huge_pages;
	else
		free_huge_pages = h->free_huge_pages_node[nid];

	return sysfs_emit(buf, "%lu\n", free_huge_pages);
}
HSTATE_ATTR_RO(free_hugepages);

static ssize_t resv_hugepages_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct hstate *h = kobj_to_hstate(kobj, NULL);
	return sysfs_emit(buf, "%lu\n", h->resv_huge_pages);
}
HSTATE_ATTR_RO(resv_hugepages);

static ssize_t surplus_hugepages_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct hstate *h;
	unsigned long surplus_huge_pages;
	int nid;

	h = kobj_to_hstate(kobj, &nid);
	if (nid == NUMA_NO_NODE)
		surplus_huge_pages = h->surplus_huge_pages;
	else
		surplus_huge_pages = h->surplus_huge_pages_node[nid];

	return sysfs_emit(buf, "%lu\n", surplus_huge_pages);
}
HSTATE_ATTR_RO(surplus_hugepages);

static ssize_t demote_store(struct kobject *kobj,
	       struct kobj_attribute *attr, const char *buf, size_t len)
{
	unsigned long nr_demote;
	unsigned long nr_available;
	nodemask_t nodes_allowed, *n_mask;
	struct hstate *h;
	int err;
	int nid;

	err = kstrtoul(buf, 10, &nr_demote);
	if (err)
		return err;
	h = kobj_to_hstate(kobj, &nid);

	if (nid != NUMA_NO_NODE) {
		init_nodemask_of_node(&nodes_allowed, nid);
		n_mask = &nodes_allowed;
	} else {
		n_mask = &node_states[N_MEMORY];
	}

	 
	mutex_lock(&h->resize_lock);
	spin_lock_irq(&hugetlb_lock);

	while (nr_demote) {
		 
		if (nid != NUMA_NO_NODE)
			nr_available = h->free_huge_pages_node[nid];
		else
			nr_available = h->free_huge_pages;
		nr_available -= h->resv_huge_pages;
		if (!nr_available)
			break;

		err = demote_pool_huge_page(h, n_mask);
		if (err)
			break;

		nr_demote--;
	}

	spin_unlock_irq(&hugetlb_lock);
	mutex_unlock(&h->resize_lock);

	if (err)
		return err;
	return len;
}
HSTATE_ATTR_WO(demote);

static ssize_t demote_size_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	struct hstate *h = kobj_to_hstate(kobj, NULL);
	unsigned long demote_size = (PAGE_SIZE << h->demote_order) / SZ_1K;

	return sysfs_emit(buf, "%lukB\n", demote_size);
}

static ssize_t demote_size_store(struct kobject *kobj,
					struct kobj_attribute *attr,
					const char *buf, size_t count)
{
	struct hstate *h, *demote_hstate;
	unsigned long demote_size;
	unsigned int demote_order;

	demote_size = (unsigned long)memparse(buf, NULL);

	demote_hstate = size_to_hstate(demote_size);
	if (!demote_hstate)
		return -EINVAL;
	demote_order = demote_hstate->order;
	if (demote_order < HUGETLB_PAGE_ORDER)
		return -EINVAL;

	 
	h = kobj_to_hstate(kobj, NULL);
	if (demote_order >= h->order)
		return -EINVAL;

	 
	mutex_lock(&h->resize_lock);
	h->demote_order = demote_order;
	mutex_unlock(&h->resize_lock);

	return count;
}
HSTATE_ATTR(demote_size);

static struct attribute *hstate_attrs[] = {
	&nr_hugepages_attr.attr,
	&nr_overcommit_hugepages_attr.attr,
	&free_hugepages_attr.attr,
	&resv_hugepages_attr.attr,
	&surplus_hugepages_attr.attr,
#ifdef CONFIG_NUMA
	&nr_hugepages_mempolicy_attr.attr,
#endif
	NULL,
};

static const struct attribute_group hstate_attr_group = {
	.attrs = hstate_attrs,
};

static struct attribute *hstate_demote_attrs[] = {
	&demote_size_attr.attr,
	&demote_attr.attr,
	NULL,
};

static const struct attribute_group hstate_demote_attr_group = {
	.attrs = hstate_demote_attrs,
};

static int hugetlb_sysfs_add_hstate(struct hstate *h, struct kobject *parent,
				    struct kobject **hstate_kobjs,
				    const struct attribute_group *hstate_attr_group)
{
	int retval;
	int hi = hstate_index(h);

	hstate_kobjs[hi] = kobject_create_and_add(h->name, parent);
	if (!hstate_kobjs[hi])
		return -ENOMEM;

	retval = sysfs_create_group(hstate_kobjs[hi], hstate_attr_group);
	if (retval) {
		kobject_put(hstate_kobjs[hi]);
		hstate_kobjs[hi] = NULL;
		return retval;
	}

	if (h->demote_order) {
		retval = sysfs_create_group(hstate_kobjs[hi],
					    &hstate_demote_attr_group);
		if (retval) {
			pr_warn("HugeTLB unable to create demote interfaces for %s\n", h->name);
			sysfs_remove_group(hstate_kobjs[hi], hstate_attr_group);
			kobject_put(hstate_kobjs[hi]);
			hstate_kobjs[hi] = NULL;
			return retval;
		}
	}

	return 0;
}

#ifdef CONFIG_NUMA
static bool hugetlb_sysfs_initialized __ro_after_init;

 
struct node_hstate {
	struct kobject		*hugepages_kobj;
	struct kobject		*hstate_kobjs[HUGE_MAX_HSTATE];
};
static struct node_hstate node_hstates[MAX_NUMNODES];

 
static struct attribute *per_node_hstate_attrs[] = {
	&nr_hugepages_attr.attr,
	&free_hugepages_attr.attr,
	&surplus_hugepages_attr.attr,
	NULL,
};

static const struct attribute_group per_node_hstate_attr_group = {
	.attrs = per_node_hstate_attrs,
};

 
static struct hstate *kobj_to_node_hstate(struct kobject *kobj, int *nidp)
{
	int nid;

	for (nid = 0; nid < nr_node_ids; nid++) {
		struct node_hstate *nhs = &node_hstates[nid];
		int i;
		for (i = 0; i < HUGE_MAX_HSTATE; i++)
			if (nhs->hstate_kobjs[i] == kobj) {
				if (nidp)
					*nidp = nid;
				return &hstates[i];
			}
	}

	BUG();
	return NULL;
}

 
void hugetlb_unregister_node(struct node *node)
{
	struct hstate *h;
	struct node_hstate *nhs = &node_hstates[node->dev.id];

	if (!nhs->hugepages_kobj)
		return;		 

	for_each_hstate(h) {
		int idx = hstate_index(h);
		struct kobject *hstate_kobj = nhs->hstate_kobjs[idx];

		if (!hstate_kobj)
			continue;
		if (h->demote_order)
			sysfs_remove_group(hstate_kobj, &hstate_demote_attr_group);
		sysfs_remove_group(hstate_kobj, &per_node_hstate_attr_group);
		kobject_put(hstate_kobj);
		nhs->hstate_kobjs[idx] = NULL;
	}

	kobject_put(nhs->hugepages_kobj);
	nhs->hugepages_kobj = NULL;
}


 
void hugetlb_register_node(struct node *node)
{
	struct hstate *h;
	struct node_hstate *nhs = &node_hstates[node->dev.id];
	int err;

	if (!hugetlb_sysfs_initialized)
		return;

	if (nhs->hugepages_kobj)
		return;		 

	nhs->hugepages_kobj = kobject_create_and_add("hugepages",
							&node->dev.kobj);
	if (!nhs->hugepages_kobj)
		return;

	for_each_hstate(h) {
		err = hugetlb_sysfs_add_hstate(h, nhs->hugepages_kobj,
						nhs->hstate_kobjs,
						&per_node_hstate_attr_group);
		if (err) {
			pr_err("HugeTLB: Unable to add hstate %s for node %d\n",
				h->name, node->dev.id);
			hugetlb_unregister_node(node);
			break;
		}
	}
}

 
static void __init hugetlb_register_all_nodes(void)
{
	int nid;

	for_each_online_node(nid)
		hugetlb_register_node(node_devices[nid]);
}
#else	 

static struct hstate *kobj_to_node_hstate(struct kobject *kobj, int *nidp)
{
	BUG();
	if (nidp)
		*nidp = -1;
	return NULL;
}

static void hugetlb_register_all_nodes(void) { }

#endif

#ifdef CONFIG_CMA
static void __init hugetlb_cma_check(void);
#else
static inline __init void hugetlb_cma_check(void)
{
}
#endif

static void __init hugetlb_sysfs_init(void)
{
	struct hstate *h;
	int err;

	hugepages_kobj = kobject_create_and_add("hugepages", mm_kobj);
	if (!hugepages_kobj)
		return;

	for_each_hstate(h) {
		err = hugetlb_sysfs_add_hstate(h, hugepages_kobj,
					 hstate_kobjs, &hstate_attr_group);
		if (err)
			pr_err("HugeTLB: Unable to add hstate %s", h->name);
	}

#ifdef CONFIG_NUMA
	hugetlb_sysfs_initialized = true;
#endif
	hugetlb_register_all_nodes();
}

#ifdef CONFIG_SYSCTL
static void hugetlb_sysctl_init(void);
#else
static inline void hugetlb_sysctl_init(void) { }
#endif

static int __init hugetlb_init(void)
{
	int i;

	BUILD_BUG_ON(sizeof_field(struct page, private) * BITS_PER_BYTE <
			__NR_HPAGEFLAGS);

	if (!hugepages_supported()) {
		if (hugetlb_max_hstate || default_hstate_max_huge_pages)
			pr_warn("HugeTLB: huge pages not supported, ignoring associated command-line parameters\n");
		return 0;
	}

	 
	hugetlb_add_hstate(HUGETLB_PAGE_ORDER);
	if (!parsed_default_hugepagesz) {
		 
		default_hstate_idx = hstate_index(size_to_hstate(HPAGE_SIZE));
		if (default_hstate_max_huge_pages) {
			if (default_hstate.max_huge_pages) {
				char buf[32];

				string_get_size(huge_page_size(&default_hstate),
					1, STRING_UNITS_2, buf, 32);
				pr_warn("HugeTLB: Ignoring hugepages=%lu associated with %s page size\n",
					default_hstate.max_huge_pages, buf);
				pr_warn("HugeTLB: Using hugepages=%lu for number of default huge pages\n",
					default_hstate_max_huge_pages);
			}
			default_hstate.max_huge_pages =
				default_hstate_max_huge_pages;

			for_each_online_node(i)
				default_hstate.max_huge_pages_node[i] =
					default_hugepages_in_node[i];
		}
	}

	hugetlb_cma_check();
	hugetlb_init_hstates();
	gather_bootmem_prealloc();
	report_hugepages();

	hugetlb_sysfs_init();
	hugetlb_cgroup_file_init();
	hugetlb_sysctl_init();

#ifdef CONFIG_SMP
	num_fault_mutexes = roundup_pow_of_two(8 * num_possible_cpus());
#else
	num_fault_mutexes = 1;
#endif
	hugetlb_fault_mutex_table =
		kmalloc_array(num_fault_mutexes, sizeof(struct mutex),
			      GFP_KERNEL);
	BUG_ON(!hugetlb_fault_mutex_table);

	for (i = 0; i < num_fault_mutexes; i++)
		mutex_init(&hugetlb_fault_mutex_table[i]);
	return 0;
}
subsys_initcall(hugetlb_init);

 
bool __init __attribute((weak)) arch_hugetlb_valid_size(unsigned long size)
{
	return size == HPAGE_SIZE;
}

void __init hugetlb_add_hstate(unsigned int order)
{
	struct hstate *h;
	unsigned long i;

	if (size_to_hstate(PAGE_SIZE << order)) {
		return;
	}
	BUG_ON(hugetlb_max_hstate >= HUGE_MAX_HSTATE);
	BUG_ON(order == 0);
	h = &hstates[hugetlb_max_hstate++];
	mutex_init(&h->resize_lock);
	h->order = order;
	h->mask = ~(huge_page_size(h) - 1);
	for (i = 0; i < MAX_NUMNODES; ++i)
		INIT_LIST_HEAD(&h->hugepage_freelists[i]);
	INIT_LIST_HEAD(&h->hugepage_activelist);
	h->next_nid_to_alloc = first_memory_node;
	h->next_nid_to_free = first_memory_node;
	snprintf(h->name, HSTATE_NAME_LEN, "hugepages-%lukB",
					huge_page_size(h)/SZ_1K);

	parsed_hstate = h;
}

bool __init __weak hugetlb_node_alloc_supported(void)
{
	return true;
}

static void __init hugepages_clear_pages_in_node(void)
{
	if (!hugetlb_max_hstate) {
		default_hstate_max_huge_pages = 0;
		memset(default_hugepages_in_node, 0,
			sizeof(default_hugepages_in_node));
	} else {
		parsed_hstate->max_huge_pages = 0;
		memset(parsed_hstate->max_huge_pages_node, 0,
			sizeof(parsed_hstate->max_huge_pages_node));
	}
}

 
static int __init hugepages_setup(char *s)
{
	unsigned long *mhp;
	static unsigned long *last_mhp;
	int node = NUMA_NO_NODE;
	int count;
	unsigned long tmp;
	char *p = s;

	if (!parsed_valid_hugepagesz) {
		pr_warn("HugeTLB: hugepages=%s does not follow a valid hugepagesz, ignoring\n", s);
		parsed_valid_hugepagesz = true;
		return 1;
	}

	 
	else if (!hugetlb_max_hstate)
		mhp = &default_hstate_max_huge_pages;
	else
		mhp = &parsed_hstate->max_huge_pages;

	if (mhp == last_mhp) {
		pr_warn("HugeTLB: hugepages= specified twice without interleaving hugepagesz=, ignoring hugepages=%s\n", s);
		return 1;
	}

	while (*p) {
		count = 0;
		if (sscanf(p, "%lu%n", &tmp, &count) != 1)
			goto invalid;
		 
		if (p[count] == ':') {
			if (!hugetlb_node_alloc_supported()) {
				pr_warn("HugeTLB: architecture can't support node specific alloc, ignoring!\n");
				return 1;
			}
			if (tmp >= MAX_NUMNODES || !node_online(tmp))
				goto invalid;
			node = array_index_nospec(tmp, MAX_NUMNODES);
			p += count + 1;
			 
			if (sscanf(p, "%lu%n", &tmp, &count) != 1)
				goto invalid;
			if (!hugetlb_max_hstate)
				default_hugepages_in_node[node] = tmp;
			else
				parsed_hstate->max_huge_pages_node[node] = tmp;
			*mhp += tmp;
			 
			if (p[count] == ',')
				p += count + 1;
			else
				break;
		} else {
			if (p != s)
				goto invalid;
			*mhp = tmp;
			break;
		}
	}

	 
	if (hugetlb_max_hstate && hstate_is_gigantic(parsed_hstate))
		hugetlb_hstate_alloc_pages(parsed_hstate);

	last_mhp = mhp;

	return 1;

invalid:
	pr_warn("HugeTLB: Invalid hugepages parameter %s\n", p);
	hugepages_clear_pages_in_node();
	return 1;
}
__setup("hugepages=", hugepages_setup);

 
static int __init hugepagesz_setup(char *s)
{
	unsigned long size;
	struct hstate *h;

	parsed_valid_hugepagesz = false;
	size = (unsigned long)memparse(s, NULL);

	if (!arch_hugetlb_valid_size(size)) {
		pr_err("HugeTLB: unsupported hugepagesz=%s\n", s);
		return 1;
	}

	h = size_to_hstate(size);
	if (h) {
		 
		if (!parsed_default_hugepagesz ||  h != &default_hstate ||
		    default_hstate.max_huge_pages) {
			pr_warn("HugeTLB: hugepagesz=%s specified twice, ignoring\n", s);
			return 1;
		}

		 
		parsed_hstate = h;
		parsed_valid_hugepagesz = true;
		return 1;
	}

	hugetlb_add_hstate(ilog2(size) - PAGE_SHIFT);
	parsed_valid_hugepagesz = true;
	return 1;
}
__setup("hugepagesz=", hugepagesz_setup);

 
static int __init default_hugepagesz_setup(char *s)
{
	unsigned long size;
	int i;

	parsed_valid_hugepagesz = false;
	if (parsed_default_hugepagesz) {
		pr_err("HugeTLB: default_hugepagesz previously specified, ignoring %s\n", s);
		return 1;
	}

	size = (unsigned long)memparse(s, NULL);

	if (!arch_hugetlb_valid_size(size)) {
		pr_err("HugeTLB: unsupported default_hugepagesz=%s\n", s);
		return 1;
	}

	hugetlb_add_hstate(ilog2(size) - PAGE_SHIFT);
	parsed_valid_hugepagesz = true;
	parsed_default_hugepagesz = true;
	default_hstate_idx = hstate_index(size_to_hstate(size));

	 
	if (default_hstate_max_huge_pages) {
		default_hstate.max_huge_pages = default_hstate_max_huge_pages;
		for_each_online_node(i)
			default_hstate.max_huge_pages_node[i] =
				default_hugepages_in_node[i];
		if (hstate_is_gigantic(&default_hstate))
			hugetlb_hstate_alloc_pages(&default_hstate);
		default_hstate_max_huge_pages = 0;
	}

	return 1;
}
__setup("default_hugepagesz=", default_hugepagesz_setup);

static nodemask_t *policy_mbind_nodemask(gfp_t gfp)
{
#ifdef CONFIG_NUMA
	struct mempolicy *mpol = get_task_policy(current);

	 
	if (mpol->mode == MPOL_BIND &&
		(apply_policy_zone(mpol, gfp_zone(gfp)) &&
		 cpuset_nodemask_valid_mems_allowed(&mpol->nodes)))
		return &mpol->nodes;
#endif
	return NULL;
}

static unsigned int allowed_mems_nr(struct hstate *h)
{
	int node;
	unsigned int nr = 0;
	nodemask_t *mbind_nodemask;
	unsigned int *array = h->free_huge_pages_node;
	gfp_t gfp_mask = htlb_alloc_mask(h);

	mbind_nodemask = policy_mbind_nodemask(gfp_mask);
	for_each_node_mask(node, cpuset_current_mems_allowed) {
		if (!mbind_nodemask || node_isset(node, *mbind_nodemask))
			nr += array[node];
	}

	return nr;
}

#ifdef CONFIG_SYSCTL
static int proc_hugetlb_doulongvec_minmax(struct ctl_table *table, int write,
					  void *buffer, size_t *length,
					  loff_t *ppos, unsigned long *out)
{
	struct ctl_table dup_table;

	 
	dup_table = *table;
	dup_table.data = out;

	return proc_doulongvec_minmax(&dup_table, write, buffer, length, ppos);
}

static int hugetlb_sysctl_handler_common(bool obey_mempolicy,
			 struct ctl_table *table, int write,
			 void *buffer, size_t *length, loff_t *ppos)
{
	struct hstate *h = &default_hstate;
	unsigned long tmp = h->max_huge_pages;
	int ret;

	if (!hugepages_supported())
		return -EOPNOTSUPP;

	ret = proc_hugetlb_doulongvec_minmax(table, write, buffer, length, ppos,
					     &tmp);
	if (ret)
		goto out;

	if (write)
		ret = __nr_hugepages_store_common(obey_mempolicy, h,
						  NUMA_NO_NODE, tmp, *length);
out:
	return ret;
}

static int hugetlb_sysctl_handler(struct ctl_table *table, int write,
			  void *buffer, size_t *length, loff_t *ppos)
{

	return hugetlb_sysctl_handler_common(false, table, write,
							buffer, length, ppos);
}

#ifdef CONFIG_NUMA
static int hugetlb_mempolicy_sysctl_handler(struct ctl_table *table, int write,
			  void *buffer, size_t *length, loff_t *ppos)
{
	return hugetlb_sysctl_handler_common(true, table, write,
							buffer, length, ppos);
}
#endif  

static int hugetlb_overcommit_handler(struct ctl_table *table, int write,
		void *buffer, size_t *length, loff_t *ppos)
{
	struct hstate *h = &default_hstate;
	unsigned long tmp;
	int ret;

	if (!hugepages_supported())
		return -EOPNOTSUPP;

	tmp = h->nr_overcommit_huge_pages;

	if (write && hstate_is_gigantic(h))
		return -EINVAL;

	ret = proc_hugetlb_doulongvec_minmax(table, write, buffer, length, ppos,
					     &tmp);
	if (ret)
		goto out;

	if (write) {
		spin_lock_irq(&hugetlb_lock);
		h->nr_overcommit_huge_pages = tmp;
		spin_unlock_irq(&hugetlb_lock);
	}
out:
	return ret;
}

static struct ctl_table hugetlb_table[] = {
	{
		.procname	= "nr_hugepages",
		.data		= NULL,
		.maxlen		= sizeof(unsigned long),
		.mode		= 0644,
		.proc_handler	= hugetlb_sysctl_handler,
	},
#ifdef CONFIG_NUMA
	{
		.procname       = "nr_hugepages_mempolicy",
		.data           = NULL,
		.maxlen         = sizeof(unsigned long),
		.mode           = 0644,
		.proc_handler   = &hugetlb_mempolicy_sysctl_handler,
	},
#endif
	{
		.procname	= "hugetlb_shm_group",
		.data		= &sysctl_hugetlb_shm_group,
		.maxlen		= sizeof(gid_t),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "nr_overcommit_hugepages",
		.data		= NULL,
		.maxlen		= sizeof(unsigned long),
		.mode		= 0644,
		.proc_handler	= hugetlb_overcommit_handler,
	},
	{ }
};

static void hugetlb_sysctl_init(void)
{
	register_sysctl_init("vm", hugetlb_table);
}
#endif  

void hugetlb_report_meminfo(struct seq_file *m)
{
	struct hstate *h;
	unsigned long total = 0;

	if (!hugepages_supported())
		return;

	for_each_hstate(h) {
		unsigned long count = h->nr_huge_pages;

		total += huge_page_size(h) * count;

		if (h == &default_hstate)
			seq_printf(m,
				   "HugePages_Total:   %5lu\n"
				   "HugePages_Free:    %5lu\n"
				   "HugePages_Rsvd:    %5lu\n"
				   "HugePages_Surp:    %5lu\n"
				   "Hugepagesize:   %8lu kB\n",
				   count,
				   h->free_huge_pages,
				   h->resv_huge_pages,
				   h->surplus_huge_pages,
				   huge_page_size(h) / SZ_1K);
	}

	seq_printf(m, "Hugetlb:        %8lu kB\n", total / SZ_1K);
}

int hugetlb_report_node_meminfo(char *buf, int len, int nid)
{
	struct hstate *h = &default_hstate;

	if (!hugepages_supported())
		return 0;

	return sysfs_emit_at(buf, len,
			     "Node %d HugePages_Total: %5u\n"
			     "Node %d HugePages_Free:  %5u\n"
			     "Node %d HugePages_Surp:  %5u\n",
			     nid, h->nr_huge_pages_node[nid],
			     nid, h->free_huge_pages_node[nid],
			     nid, h->surplus_huge_pages_node[nid]);
}

void hugetlb_show_meminfo_node(int nid)
{
	struct hstate *h;

	if (!hugepages_supported())
		return;

	for_each_hstate(h)
		printk("Node %d hugepages_total=%u hugepages_free=%u hugepages_surp=%u hugepages_size=%lukB\n",
			nid,
			h->nr_huge_pages_node[nid],
			h->free_huge_pages_node[nid],
			h->surplus_huge_pages_node[nid],
			huge_page_size(h) / SZ_1K);
}

void hugetlb_report_usage(struct seq_file *m, struct mm_struct *mm)
{
	seq_printf(m, "HugetlbPages:\t%8lu kB\n",
		   K(atomic_long_read(&mm->hugetlb_usage)));
}

 
unsigned long hugetlb_total_pages(void)
{
	struct hstate *h;
	unsigned long nr_total_pages = 0;

	for_each_hstate(h)
		nr_total_pages += h->nr_huge_pages * pages_per_huge_page(h);
	return nr_total_pages;
}

static int hugetlb_acct_memory(struct hstate *h, long delta)
{
	int ret = -ENOMEM;

	if (!delta)
		return 0;

	spin_lock_irq(&hugetlb_lock);
	 
	if (delta > 0) {
		if (gather_surplus_pages(h, delta) < 0)
			goto out;

		if (delta > allowed_mems_nr(h)) {
			return_unused_surplus_pages(h, delta);
			goto out;
		}
	}

	ret = 0;
	if (delta < 0)
		return_unused_surplus_pages(h, (unsigned long) -delta);

out:
	spin_unlock_irq(&hugetlb_lock);
	return ret;
}

static void hugetlb_vm_op_open(struct vm_area_struct *vma)
{
	struct resv_map *resv = vma_resv_map(vma);

	 
	if (resv && is_vma_resv_set(vma, HPAGE_RESV_OWNER)) {
		resv_map_dup_hugetlb_cgroup_uncharge_info(resv);
		kref_get(&resv->refs);
	}

	 
	if (vma->vm_flags & VM_MAYSHARE) {
		struct hugetlb_vma_lock *vma_lock = vma->vm_private_data;

		if (vma_lock) {
			if (vma_lock->vma != vma) {
				vma->vm_private_data = NULL;
				hugetlb_vma_lock_alloc(vma);
			} else
				pr_warn("HugeTLB: vma_lock already exists in %s.\n", __func__);
		} else
			hugetlb_vma_lock_alloc(vma);
	}
}

static void hugetlb_vm_op_close(struct vm_area_struct *vma)
{
	struct hstate *h = hstate_vma(vma);
	struct resv_map *resv;
	struct hugepage_subpool *spool = subpool_vma(vma);
	unsigned long reserve, start, end;
	long gbl_reserve;

	hugetlb_vma_lock_free(vma);

	resv = vma_resv_map(vma);
	if (!resv || !is_vma_resv_set(vma, HPAGE_RESV_OWNER))
		return;

	start = vma_hugecache_offset(h, vma, vma->vm_start);
	end = vma_hugecache_offset(h, vma, vma->vm_end);

	reserve = (end - start) - region_count(resv, start, end);
	hugetlb_cgroup_uncharge_counter(resv, start, end);
	if (reserve) {
		 
		gbl_reserve = hugepage_subpool_put_pages(spool, reserve);
		hugetlb_acct_memory(h, -gbl_reserve);
	}

	kref_put(&resv->refs, resv_map_release);
}

static int hugetlb_vm_op_split(struct vm_area_struct *vma, unsigned long addr)
{
	if (addr & ~(huge_page_mask(hstate_vma(vma))))
		return -EINVAL;

	 
	if (addr & ~PUD_MASK) {
		 
		unsigned long floor = addr & PUD_MASK;
		unsigned long ceil = floor + PUD_SIZE;

		if (floor >= vma->vm_start && ceil <= vma->vm_end)
			hugetlb_unshare_pmds(vma, floor, ceil);
	}

	return 0;
}

static unsigned long hugetlb_vm_op_pagesize(struct vm_area_struct *vma)
{
	return huge_page_size(hstate_vma(vma));
}

 
static vm_fault_t hugetlb_vm_op_fault(struct vm_fault *vmf)
{
	BUG();
	return 0;
}

 
const struct vm_operations_struct hugetlb_vm_ops = {
	.fault = hugetlb_vm_op_fault,
	.open = hugetlb_vm_op_open,
	.close = hugetlb_vm_op_close,
	.may_split = hugetlb_vm_op_split,
	.pagesize = hugetlb_vm_op_pagesize,
};

static pte_t make_huge_pte(struct vm_area_struct *vma, struct page *page,
				int writable)
{
	pte_t entry;
	unsigned int shift = huge_page_shift(hstate_vma(vma));

	if (writable) {
		entry = huge_pte_mkwrite(huge_pte_mkdirty(mk_huge_pte(page,
					 vma->vm_page_prot)));
	} else {
		entry = huge_pte_wrprotect(mk_huge_pte(page,
					   vma->vm_page_prot));
	}
	entry = pte_mkyoung(entry);
	entry = arch_make_huge_pte(entry, shift, vma->vm_flags);

	return entry;
}

static void set_huge_ptep_writable(struct vm_area_struct *vma,
				   unsigned long address, pte_t *ptep)
{
	pte_t entry;

	entry = huge_pte_mkwrite(huge_pte_mkdirty(huge_ptep_get(ptep)));
	if (huge_ptep_set_access_flags(vma, address, ptep, entry, 1))
		update_mmu_cache(vma, address, ptep);
}

bool is_hugetlb_entry_migration(pte_t pte)
{
	swp_entry_t swp;

	if (huge_pte_none(pte) || pte_present(pte))
		return false;
	swp = pte_to_swp_entry(pte);
	if (is_migration_entry(swp))
		return true;
	else
		return false;
}

static bool is_hugetlb_entry_hwpoisoned(pte_t pte)
{
	swp_entry_t swp;

	if (huge_pte_none(pte) || pte_present(pte))
		return false;
	swp = pte_to_swp_entry(pte);
	if (is_hwpoison_entry(swp))
		return true;
	else
		return false;
}

static void
hugetlb_install_folio(struct vm_area_struct *vma, pte_t *ptep, unsigned long addr,
		      struct folio *new_folio, pte_t old, unsigned long sz)
{
	pte_t newpte = make_huge_pte(vma, &new_folio->page, 1);

	__folio_mark_uptodate(new_folio);
	hugepage_add_new_anon_rmap(new_folio, vma, addr);
	if (userfaultfd_wp(vma) && huge_pte_uffd_wp(old))
		newpte = huge_pte_mkuffd_wp(newpte);
	set_huge_pte_at(vma->vm_mm, addr, ptep, newpte, sz);
	hugetlb_count_add(pages_per_huge_page(hstate_vma(vma)), vma->vm_mm);
	folio_set_hugetlb_migratable(new_folio);
}

int copy_hugetlb_page_range(struct mm_struct *dst, struct mm_struct *src,
			    struct vm_area_struct *dst_vma,
			    struct vm_area_struct *src_vma)
{
	pte_t *src_pte, *dst_pte, entry;
	struct folio *pte_folio;
	unsigned long addr;
	bool cow = is_cow_mapping(src_vma->vm_flags);
	struct hstate *h = hstate_vma(src_vma);
	unsigned long sz = huge_page_size(h);
	unsigned long npages = pages_per_huge_page(h);
	struct mmu_notifier_range range;
	unsigned long last_addr_mask;
	int ret = 0;

	if (cow) {
		mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, src,
					src_vma->vm_start,
					src_vma->vm_end);
		mmu_notifier_invalidate_range_start(&range);
		vma_assert_write_locked(src_vma);
		raw_write_seqcount_begin(&src->write_protect_seq);
	} else {
		 
		hugetlb_vma_lock_read(src_vma);
	}

	last_addr_mask = hugetlb_mask_last_page(h);
	for (addr = src_vma->vm_start; addr < src_vma->vm_end; addr += sz) {
		spinlock_t *src_ptl, *dst_ptl;
		src_pte = hugetlb_walk(src_vma, addr, sz);
		if (!src_pte) {
			addr |= last_addr_mask;
			continue;
		}
		dst_pte = huge_pte_alloc(dst, dst_vma, addr, sz);
		if (!dst_pte) {
			ret = -ENOMEM;
			break;
		}

		 
		if (page_count(virt_to_page(dst_pte)) > 1) {
			addr |= last_addr_mask;
			continue;
		}

		dst_ptl = huge_pte_lock(h, dst, dst_pte);
		src_ptl = huge_pte_lockptr(h, src, src_pte);
		spin_lock_nested(src_ptl, SINGLE_DEPTH_NESTING);
		entry = huge_ptep_get(src_pte);
again:
		if (huge_pte_none(entry)) {
			 
			;
		} else if (unlikely(is_hugetlb_entry_hwpoisoned(entry))) {
			if (!userfaultfd_wp(dst_vma))
				entry = huge_pte_clear_uffd_wp(entry);
			set_huge_pte_at(dst, addr, dst_pte, entry, sz);
		} else if (unlikely(is_hugetlb_entry_migration(entry))) {
			swp_entry_t swp_entry = pte_to_swp_entry(entry);
			bool uffd_wp = pte_swp_uffd_wp(entry);

			if (!is_readable_migration_entry(swp_entry) && cow) {
				 
				swp_entry = make_readable_migration_entry(
							swp_offset(swp_entry));
				entry = swp_entry_to_pte(swp_entry);
				if (userfaultfd_wp(src_vma) && uffd_wp)
					entry = pte_swp_mkuffd_wp(entry);
				set_huge_pte_at(src, addr, src_pte, entry, sz);
			}
			if (!userfaultfd_wp(dst_vma))
				entry = huge_pte_clear_uffd_wp(entry);
			set_huge_pte_at(dst, addr, dst_pte, entry, sz);
		} else if (unlikely(is_pte_marker(entry))) {
			pte_marker marker = copy_pte_marker(
				pte_to_swp_entry(entry), dst_vma);

			if (marker)
				set_huge_pte_at(dst, addr, dst_pte,
						make_pte_marker(marker), sz);
		} else {
			entry = huge_ptep_get(src_pte);
			pte_folio = page_folio(pte_page(entry));
			folio_get(pte_folio);

			 
			if (!folio_test_anon(pte_folio)) {
				page_dup_file_rmap(&pte_folio->page, true);
			} else if (page_try_dup_anon_rmap(&pte_folio->page,
							  true, src_vma)) {
				pte_t src_pte_old = entry;
				struct folio *new_folio;

				spin_unlock(src_ptl);
				spin_unlock(dst_ptl);
				 
				new_folio = alloc_hugetlb_folio(dst_vma, addr, 1);
				if (IS_ERR(new_folio)) {
					folio_put(pte_folio);
					ret = PTR_ERR(new_folio);
					break;
				}
				ret = copy_user_large_folio(new_folio,
							    pte_folio,
							    addr, dst_vma);
				folio_put(pte_folio);
				if (ret) {
					folio_put(new_folio);
					break;
				}

				 
				dst_ptl = huge_pte_lock(h, dst, dst_pte);
				src_ptl = huge_pte_lockptr(h, src, src_pte);
				spin_lock_nested(src_ptl, SINGLE_DEPTH_NESTING);
				entry = huge_ptep_get(src_pte);
				if (!pte_same(src_pte_old, entry)) {
					restore_reserve_on_error(h, dst_vma, addr,
								new_folio);
					folio_put(new_folio);
					 
					goto again;
				}
				hugetlb_install_folio(dst_vma, dst_pte, addr,
						      new_folio, src_pte_old, sz);
				spin_unlock(src_ptl);
				spin_unlock(dst_ptl);
				continue;
			}

			if (cow) {
				 
				huge_ptep_set_wrprotect(src, addr, src_pte);
				entry = huge_pte_wrprotect(entry);
			}

			if (!userfaultfd_wp(dst_vma))
				entry = huge_pte_clear_uffd_wp(entry);

			set_huge_pte_at(dst, addr, dst_pte, entry, sz);
			hugetlb_count_add(npages, dst);
		}
		spin_unlock(src_ptl);
		spin_unlock(dst_ptl);
	}

	if (cow) {
		raw_write_seqcount_end(&src->write_protect_seq);
		mmu_notifier_invalidate_range_end(&range);
	} else {
		hugetlb_vma_unlock_read(src_vma);
	}

	return ret;
}

static void move_huge_pte(struct vm_area_struct *vma, unsigned long old_addr,
			  unsigned long new_addr, pte_t *src_pte, pte_t *dst_pte,
			  unsigned long sz)
{
	struct hstate *h = hstate_vma(vma);
	struct mm_struct *mm = vma->vm_mm;
	spinlock_t *src_ptl, *dst_ptl;
	pte_t pte;

	dst_ptl = huge_pte_lock(h, mm, dst_pte);
	src_ptl = huge_pte_lockptr(h, mm, src_pte);

	 
	if (src_ptl != dst_ptl)
		spin_lock_nested(src_ptl, SINGLE_DEPTH_NESTING);

	pte = huge_ptep_get_and_clear(mm, old_addr, src_pte);
	set_huge_pte_at(mm, new_addr, dst_pte, pte, sz);

	if (src_ptl != dst_ptl)
		spin_unlock(src_ptl);
	spin_unlock(dst_ptl);
}

int move_hugetlb_page_tables(struct vm_area_struct *vma,
			     struct vm_area_struct *new_vma,
			     unsigned long old_addr, unsigned long new_addr,
			     unsigned long len)
{
	struct hstate *h = hstate_vma(vma);
	struct address_space *mapping = vma->vm_file->f_mapping;
	unsigned long sz = huge_page_size(h);
	struct mm_struct *mm = vma->vm_mm;
	unsigned long old_end = old_addr + len;
	unsigned long last_addr_mask;
	pte_t *src_pte, *dst_pte;
	struct mmu_notifier_range range;
	bool shared_pmd = false;

	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, mm, old_addr,
				old_end);
	adjust_range_if_pmd_sharing_possible(vma, &range.start, &range.end);
	 
	flush_cache_range(vma, range.start, range.end);

	mmu_notifier_invalidate_range_start(&range);
	last_addr_mask = hugetlb_mask_last_page(h);
	 
	hugetlb_vma_lock_write(vma);
	i_mmap_lock_write(mapping);
	for (; old_addr < old_end; old_addr += sz, new_addr += sz) {
		src_pte = hugetlb_walk(vma, old_addr, sz);
		if (!src_pte) {
			old_addr |= last_addr_mask;
			new_addr |= last_addr_mask;
			continue;
		}
		if (huge_pte_none(huge_ptep_get(src_pte)))
			continue;

		if (huge_pmd_unshare(mm, vma, old_addr, src_pte)) {
			shared_pmd = true;
			old_addr |= last_addr_mask;
			new_addr |= last_addr_mask;
			continue;
		}

		dst_pte = huge_pte_alloc(mm, new_vma, new_addr, sz);
		if (!dst_pte)
			break;

		move_huge_pte(vma, old_addr, new_addr, src_pte, dst_pte, sz);
	}

	if (shared_pmd)
		flush_hugetlb_tlb_range(vma, range.start, range.end);
	else
		flush_hugetlb_tlb_range(vma, old_end - len, old_end);
	mmu_notifier_invalidate_range_end(&range);
	i_mmap_unlock_write(mapping);
	hugetlb_vma_unlock_write(vma);

	return len + old_addr - old_end;
}

void __unmap_hugepage_range(struct mmu_gather *tlb, struct vm_area_struct *vma,
			    unsigned long start, unsigned long end,
			    struct page *ref_page, zap_flags_t zap_flags)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long address;
	pte_t *ptep;
	pte_t pte;
	spinlock_t *ptl;
	struct page *page;
	struct hstate *h = hstate_vma(vma);
	unsigned long sz = huge_page_size(h);
	unsigned long last_addr_mask;
	bool force_flush = false;

	WARN_ON(!is_vm_hugetlb_page(vma));
	BUG_ON(start & ~huge_page_mask(h));
	BUG_ON(end & ~huge_page_mask(h));

	 
	tlb_change_page_size(tlb, sz);
	tlb_start_vma(tlb, vma);

	last_addr_mask = hugetlb_mask_last_page(h);
	address = start;
	for (; address < end; address += sz) {
		ptep = hugetlb_walk(vma, address, sz);
		if (!ptep) {
			address |= last_addr_mask;
			continue;
		}

		ptl = huge_pte_lock(h, mm, ptep);
		if (huge_pmd_unshare(mm, vma, address, ptep)) {
			spin_unlock(ptl);
			tlb_flush_pmd_range(tlb, address & PUD_MASK, PUD_SIZE);
			force_flush = true;
			address |= last_addr_mask;
			continue;
		}

		pte = huge_ptep_get(ptep);
		if (huge_pte_none(pte)) {
			spin_unlock(ptl);
			continue;
		}

		 
		if (unlikely(!pte_present(pte))) {
			 
			if (pte_swp_uffd_wp_any(pte) &&
			    !(zap_flags & ZAP_FLAG_DROP_MARKER))
				set_huge_pte_at(mm, address, ptep,
						make_pte_marker(PTE_MARKER_UFFD_WP),
						sz);
			else
				huge_pte_clear(mm, address, ptep, sz);
			spin_unlock(ptl);
			continue;
		}

		page = pte_page(pte);
		 
		if (ref_page) {
			if (page != ref_page) {
				spin_unlock(ptl);
				continue;
			}
			 
			set_vma_resv_flags(vma, HPAGE_RESV_UNMAPPED);
		}

		pte = huge_ptep_get_and_clear(mm, address, ptep);
		tlb_remove_huge_tlb_entry(h, tlb, ptep, address);
		if (huge_pte_dirty(pte))
			set_page_dirty(page);
		 
		if (huge_pte_uffd_wp(pte) &&
		    !(zap_flags & ZAP_FLAG_DROP_MARKER))
			set_huge_pte_at(mm, address, ptep,
					make_pte_marker(PTE_MARKER_UFFD_WP),
					sz);
		hugetlb_count_sub(pages_per_huge_page(h), mm);
		page_remove_rmap(page, vma, true);

		spin_unlock(ptl);
		tlb_remove_page_size(tlb, page, huge_page_size(h));
		 
		if (ref_page)
			break;
	}
	tlb_end_vma(tlb, vma);

	 
	if (force_flush)
		tlb_flush_mmu_tlbonly(tlb);
}

void __hugetlb_zap_begin(struct vm_area_struct *vma,
			 unsigned long *start, unsigned long *end)
{
	if (!vma->vm_file)	 
		return;

	adjust_range_if_pmd_sharing_possible(vma, start, end);
	hugetlb_vma_lock_write(vma);
	if (vma->vm_file)
		i_mmap_lock_write(vma->vm_file->f_mapping);
}

void __hugetlb_zap_end(struct vm_area_struct *vma,
		       struct zap_details *details)
{
	zap_flags_t zap_flags = details ? details->zap_flags : 0;

	if (!vma->vm_file)	 
		return;

	if (zap_flags & ZAP_FLAG_UNMAP) {	 
		 
		__hugetlb_vma_unlock_write_free(vma);
	} else {
		hugetlb_vma_unlock_write(vma);
	}

	if (vma->vm_file)
		i_mmap_unlock_write(vma->vm_file->f_mapping);
}

void unmap_hugepage_range(struct vm_area_struct *vma, unsigned long start,
			  unsigned long end, struct page *ref_page,
			  zap_flags_t zap_flags)
{
	struct mmu_notifier_range range;
	struct mmu_gather tlb;

	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, vma->vm_mm,
				start, end);
	adjust_range_if_pmd_sharing_possible(vma, &range.start, &range.end);
	mmu_notifier_invalidate_range_start(&range);
	tlb_gather_mmu(&tlb, vma->vm_mm);

	__unmap_hugepage_range(&tlb, vma, start, end, ref_page, zap_flags);

	mmu_notifier_invalidate_range_end(&range);
	tlb_finish_mmu(&tlb);
}

 
static void unmap_ref_private(struct mm_struct *mm, struct vm_area_struct *vma,
			      struct page *page, unsigned long address)
{
	struct hstate *h = hstate_vma(vma);
	struct vm_area_struct *iter_vma;
	struct address_space *mapping;
	pgoff_t pgoff;

	 
	address = address & huge_page_mask(h);
	pgoff = ((address - vma->vm_start) >> PAGE_SHIFT) +
			vma->vm_pgoff;
	mapping = vma->vm_file->f_mapping;

	 
	i_mmap_lock_write(mapping);
	vma_interval_tree_foreach(iter_vma, &mapping->i_mmap, pgoff, pgoff) {
		 
		if (iter_vma == vma)
			continue;

		 
		if (iter_vma->vm_flags & VM_MAYSHARE)
			continue;

		 
		if (!is_vma_resv_set(iter_vma, HPAGE_RESV_OWNER))
			unmap_hugepage_range(iter_vma, address,
					     address + huge_page_size(h), page, 0);
	}
	i_mmap_unlock_write(mapping);
}

 
static vm_fault_t hugetlb_wp(struct mm_struct *mm, struct vm_area_struct *vma,
		       unsigned long address, pte_t *ptep, unsigned int flags,
		       struct folio *pagecache_folio, spinlock_t *ptl)
{
	const bool unshare = flags & FAULT_FLAG_UNSHARE;
	pte_t pte = huge_ptep_get(ptep);
	struct hstate *h = hstate_vma(vma);
	struct folio *old_folio;
	struct folio *new_folio;
	int outside_reserve = 0;
	vm_fault_t ret = 0;
	unsigned long haddr = address & huge_page_mask(h);
	struct mmu_notifier_range range;

	 
	if (!unshare && huge_pte_uffd_wp(pte))
		return 0;

	 
	if (WARN_ON_ONCE(!unshare && !(vma->vm_flags & VM_WRITE)))
		return VM_FAULT_SIGSEGV;

	 
	if (vma->vm_flags & VM_MAYSHARE) {
		set_huge_ptep_writable(vma, haddr, ptep);
		return 0;
	}

	old_folio = page_folio(pte_page(pte));

	delayacct_wpcopy_start();

retry_avoidcopy:
	 
	if (folio_mapcount(old_folio) == 1 && folio_test_anon(old_folio)) {
		if (!PageAnonExclusive(&old_folio->page))
			page_move_anon_rmap(&old_folio->page, vma);
		if (likely(!unshare))
			set_huge_ptep_writable(vma, haddr, ptep);

		delayacct_wpcopy_end();
		return 0;
	}
	VM_BUG_ON_PAGE(folio_test_anon(old_folio) &&
		       PageAnonExclusive(&old_folio->page), &old_folio->page);

	 
	if (is_vma_resv_set(vma, HPAGE_RESV_OWNER) &&
			old_folio != pagecache_folio)
		outside_reserve = 1;

	folio_get(old_folio);

	 
	spin_unlock(ptl);
	new_folio = alloc_hugetlb_folio(vma, haddr, outside_reserve);

	if (IS_ERR(new_folio)) {
		 
		if (outside_reserve) {
			struct address_space *mapping = vma->vm_file->f_mapping;
			pgoff_t idx;
			u32 hash;

			folio_put(old_folio);
			 
			idx = vma_hugecache_offset(h, vma, haddr);
			hash = hugetlb_fault_mutex_hash(mapping, idx);
			hugetlb_vma_unlock_read(vma);
			mutex_unlock(&hugetlb_fault_mutex_table[hash]);

			unmap_ref_private(mm, vma, &old_folio->page, haddr);

			mutex_lock(&hugetlb_fault_mutex_table[hash]);
			hugetlb_vma_lock_read(vma);
			spin_lock(ptl);
			ptep = hugetlb_walk(vma, haddr, huge_page_size(h));
			if (likely(ptep &&
				   pte_same(huge_ptep_get(ptep), pte)))
				goto retry_avoidcopy;
			 
			delayacct_wpcopy_end();
			return 0;
		}

		ret = vmf_error(PTR_ERR(new_folio));
		goto out_release_old;
	}

	 
	if (unlikely(anon_vma_prepare(vma))) {
		ret = VM_FAULT_OOM;
		goto out_release_all;
	}

	if (copy_user_large_folio(new_folio, old_folio, address, vma)) {
		ret = VM_FAULT_HWPOISON_LARGE;
		goto out_release_all;
	}
	__folio_mark_uptodate(new_folio);

	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, mm, haddr,
				haddr + huge_page_size(h));
	mmu_notifier_invalidate_range_start(&range);

	 
	spin_lock(ptl);
	ptep = hugetlb_walk(vma, haddr, huge_page_size(h));
	if (likely(ptep && pte_same(huge_ptep_get(ptep), pte))) {
		pte_t newpte = make_huge_pte(vma, &new_folio->page, !unshare);

		 
		huge_ptep_clear_flush(vma, haddr, ptep);
		page_remove_rmap(&old_folio->page, vma, true);
		hugepage_add_new_anon_rmap(new_folio, vma, haddr);
		if (huge_pte_uffd_wp(pte))
			newpte = huge_pte_mkuffd_wp(newpte);
		set_huge_pte_at(mm, haddr, ptep, newpte, huge_page_size(h));
		folio_set_hugetlb_migratable(new_folio);
		 
		new_folio = old_folio;
	}
	spin_unlock(ptl);
	mmu_notifier_invalidate_range_end(&range);
out_release_all:
	 
	if (new_folio != old_folio)
		restore_reserve_on_error(h, vma, haddr, new_folio);
	folio_put(new_folio);
out_release_old:
	folio_put(old_folio);

	spin_lock(ptl);  

	delayacct_wpcopy_end();
	return ret;
}

 
static bool hugetlbfs_pagecache_present(struct hstate *h,
			struct vm_area_struct *vma, unsigned long address)
{
	struct address_space *mapping = vma->vm_file->f_mapping;
	pgoff_t idx = vma_hugecache_offset(h, vma, address);
	struct folio *folio;

	folio = filemap_get_folio(mapping, idx);
	if (IS_ERR(folio))
		return false;
	folio_put(folio);
	return true;
}

int hugetlb_add_to_page_cache(struct folio *folio, struct address_space *mapping,
			   pgoff_t idx)
{
	struct inode *inode = mapping->host;
	struct hstate *h = hstate_inode(inode);
	int err;

	__folio_set_locked(folio);
	err = __filemap_add_folio(mapping, folio, idx, GFP_KERNEL, NULL);

	if (unlikely(err)) {
		__folio_clear_locked(folio);
		return err;
	}
	folio_clear_hugetlb_restore_reserve(folio);

	 
	folio_mark_dirty(folio);

	spin_lock(&inode->i_lock);
	inode->i_blocks += blocks_per_huge_page(h);
	spin_unlock(&inode->i_lock);
	return 0;
}

static inline vm_fault_t hugetlb_handle_userfault(struct vm_area_struct *vma,
						  struct address_space *mapping,
						  pgoff_t idx,
						  unsigned int flags,
						  unsigned long haddr,
						  unsigned long addr,
						  unsigned long reason)
{
	u32 hash;
	struct vm_fault vmf = {
		.vma = vma,
		.address = haddr,
		.real_address = addr,
		.flags = flags,

		 
	};

	 
	hugetlb_vma_unlock_read(vma);
	hash = hugetlb_fault_mutex_hash(mapping, idx);
	mutex_unlock(&hugetlb_fault_mutex_table[hash]);
	return handle_userfault(&vmf, reason);
}

 
static bool hugetlb_pte_stable(struct hstate *h, struct mm_struct *mm,
			       pte_t *ptep, pte_t old_pte)
{
	spinlock_t *ptl;
	bool same;

	ptl = huge_pte_lock(h, mm, ptep);
	same = pte_same(huge_ptep_get(ptep), old_pte);
	spin_unlock(ptl);

	return same;
}

static vm_fault_t hugetlb_no_page(struct mm_struct *mm,
			struct vm_area_struct *vma,
			struct address_space *mapping, pgoff_t idx,
			unsigned long address, pte_t *ptep,
			pte_t old_pte, unsigned int flags)
{
	struct hstate *h = hstate_vma(vma);
	vm_fault_t ret = VM_FAULT_SIGBUS;
	int anon_rmap = 0;
	unsigned long size;
	struct folio *folio;
	pte_t new_pte;
	spinlock_t *ptl;
	unsigned long haddr = address & huge_page_mask(h);
	bool new_folio, new_pagecache_folio = false;
	u32 hash = hugetlb_fault_mutex_hash(mapping, idx);

	 
	if (is_vma_resv_set(vma, HPAGE_RESV_UNMAPPED)) {
		pr_warn_ratelimited("PID %d killed due to inadequate hugepage pool\n",
			   current->pid);
		goto out;
	}

	 
	new_folio = false;
	folio = filemap_lock_folio(mapping, idx);
	if (IS_ERR(folio)) {
		size = i_size_read(mapping->host) >> huge_page_shift(h);
		if (idx >= size)
			goto out;
		 
		if (userfaultfd_missing(vma)) {
			 
			if (!hugetlb_pte_stable(h, mm, ptep, old_pte)) {
				ret = 0;
				goto out;
			}

			return hugetlb_handle_userfault(vma, mapping, idx, flags,
							haddr, address,
							VM_UFFD_MISSING);
		}

		folio = alloc_hugetlb_folio(vma, haddr, 0);
		if (IS_ERR(folio)) {
			 
			if (hugetlb_pte_stable(h, mm, ptep, old_pte))
				ret = vmf_error(PTR_ERR(folio));
			else
				ret = 0;
			goto out;
		}
		clear_huge_page(&folio->page, address, pages_per_huge_page(h));
		__folio_mark_uptodate(folio);
		new_folio = true;

		if (vma->vm_flags & VM_MAYSHARE) {
			int err = hugetlb_add_to_page_cache(folio, mapping, idx);
			if (err) {
				 
				restore_reserve_on_error(h, vma, haddr, folio);
				folio_put(folio);
				goto out;
			}
			new_pagecache_folio = true;
		} else {
			folio_lock(folio);
			if (unlikely(anon_vma_prepare(vma))) {
				ret = VM_FAULT_OOM;
				goto backout_unlocked;
			}
			anon_rmap = 1;
		}
	} else {
		 
		if (unlikely(folio_test_hwpoison(folio))) {
			ret = VM_FAULT_HWPOISON_LARGE |
				VM_FAULT_SET_HINDEX(hstate_index(h));
			goto backout_unlocked;
		}

		 
		if (userfaultfd_minor(vma)) {
			folio_unlock(folio);
			folio_put(folio);
			 
			if (!hugetlb_pte_stable(h, mm, ptep, old_pte)) {
				ret = 0;
				goto out;
			}
			return hugetlb_handle_userfault(vma, mapping, idx, flags,
							haddr, address,
							VM_UFFD_MINOR);
		}
	}

	 
	if ((flags & FAULT_FLAG_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		if (vma_needs_reservation(h, vma, haddr) < 0) {
			ret = VM_FAULT_OOM;
			goto backout_unlocked;
		}
		 
		vma_end_reservation(h, vma, haddr);
	}

	ptl = huge_pte_lock(h, mm, ptep);
	ret = 0;
	 
	if (!pte_same(huge_ptep_get(ptep), old_pte))
		goto backout;

	if (anon_rmap)
		hugepage_add_new_anon_rmap(folio, vma, haddr);
	else
		page_dup_file_rmap(&folio->page, true);
	new_pte = make_huge_pte(vma, &folio->page, ((vma->vm_flags & VM_WRITE)
				&& (vma->vm_flags & VM_SHARED)));
	 
	if (unlikely(pte_marker_uffd_wp(old_pte)))
		new_pte = huge_pte_mkuffd_wp(new_pte);
	set_huge_pte_at(mm, haddr, ptep, new_pte, huge_page_size(h));

	hugetlb_count_add(pages_per_huge_page(h), mm);
	if ((flags & FAULT_FLAG_WRITE) && !(vma->vm_flags & VM_SHARED)) {
		 
		ret = hugetlb_wp(mm, vma, address, ptep, flags, folio, ptl);
	}

	spin_unlock(ptl);

	 
	if (new_folio)
		folio_set_hugetlb_migratable(folio);

	folio_unlock(folio);
out:
	hugetlb_vma_unlock_read(vma);
	mutex_unlock(&hugetlb_fault_mutex_table[hash]);
	return ret;

backout:
	spin_unlock(ptl);
backout_unlocked:
	if (new_folio && !new_pagecache_folio)
		restore_reserve_on_error(h, vma, haddr, folio);

	folio_unlock(folio);
	folio_put(folio);
	goto out;
}

#ifdef CONFIG_SMP
u32 hugetlb_fault_mutex_hash(struct address_space *mapping, pgoff_t idx)
{
	unsigned long key[2];
	u32 hash;

	key[0] = (unsigned long) mapping;
	key[1] = idx;

	hash = jhash2((u32 *)&key, sizeof(key)/(sizeof(u32)), 0);

	return hash & (num_fault_mutexes - 1);
}
#else
 
u32 hugetlb_fault_mutex_hash(struct address_space *mapping, pgoff_t idx)
{
	return 0;
}
#endif

vm_fault_t hugetlb_fault(struct mm_struct *mm, struct vm_area_struct *vma,
			unsigned long address, unsigned int flags)
{
	pte_t *ptep, entry;
	spinlock_t *ptl;
	vm_fault_t ret;
	u32 hash;
	pgoff_t idx;
	struct folio *folio = NULL;
	struct folio *pagecache_folio = NULL;
	struct hstate *h = hstate_vma(vma);
	struct address_space *mapping;
	int need_wait_lock = 0;
	unsigned long haddr = address & huge_page_mask(h);

	 
	if (flags & FAULT_FLAG_VMA_LOCK) {
		vma_end_read(vma);
		return VM_FAULT_RETRY;
	}

	 
	mapping = vma->vm_file->f_mapping;
	idx = vma_hugecache_offset(h, vma, haddr);
	hash = hugetlb_fault_mutex_hash(mapping, idx);
	mutex_lock(&hugetlb_fault_mutex_table[hash]);

	 
	hugetlb_vma_lock_read(vma);
	ptep = huge_pte_alloc(mm, vma, haddr, huge_page_size(h));
	if (!ptep) {
		hugetlb_vma_unlock_read(vma);
		mutex_unlock(&hugetlb_fault_mutex_table[hash]);
		return VM_FAULT_OOM;
	}

	entry = huge_ptep_get(ptep);
	if (huge_pte_none_mostly(entry)) {
		if (is_pte_marker(entry)) {
			pte_marker marker =
				pte_marker_get(pte_to_swp_entry(entry));

			if (marker & PTE_MARKER_POISONED) {
				ret = VM_FAULT_HWPOISON_LARGE;
				goto out_mutex;
			}
		}

		 
		return hugetlb_no_page(mm, vma, mapping, idx, address, ptep,
				      entry, flags);
	}

	ret = 0;

	 
	if (!pte_present(entry)) {
		if (unlikely(is_hugetlb_entry_migration(entry))) {
			 
			mutex_unlock(&hugetlb_fault_mutex_table[hash]);
			migration_entry_wait_huge(vma, ptep);
			return 0;
		} else if (unlikely(is_hugetlb_entry_hwpoisoned(entry)))
			ret = VM_FAULT_HWPOISON_LARGE |
			    VM_FAULT_SET_HINDEX(hstate_index(h));
		goto out_mutex;
	}

	 
	if ((flags & (FAULT_FLAG_WRITE|FAULT_FLAG_UNSHARE)) &&
	    !(vma->vm_flags & VM_MAYSHARE) && !huge_pte_write(entry)) {
		if (vma_needs_reservation(h, vma, haddr) < 0) {
			ret = VM_FAULT_OOM;
			goto out_mutex;
		}
		 
		vma_end_reservation(h, vma, haddr);

		pagecache_folio = filemap_lock_folio(mapping, idx);
		if (IS_ERR(pagecache_folio))
			pagecache_folio = NULL;
	}

	ptl = huge_pte_lock(h, mm, ptep);

	 
	if (unlikely(!pte_same(entry, huge_ptep_get(ptep))))
		goto out_ptl;

	 
	if (userfaultfd_wp(vma) && huge_pte_uffd_wp(huge_ptep_get(ptep)) &&
	    (flags & FAULT_FLAG_WRITE) && !huge_pte_write(entry)) {
		struct vm_fault vmf = {
			.vma = vma,
			.address = haddr,
			.real_address = address,
			.flags = flags,
		};

		spin_unlock(ptl);
		if (pagecache_folio) {
			folio_unlock(pagecache_folio);
			folio_put(pagecache_folio);
		}
		hugetlb_vma_unlock_read(vma);
		mutex_unlock(&hugetlb_fault_mutex_table[hash]);
		return handle_userfault(&vmf, VM_UFFD_WP);
	}

	 
	folio = page_folio(pte_page(entry));
	if (folio != pagecache_folio)
		if (!folio_trylock(folio)) {
			need_wait_lock = 1;
			goto out_ptl;
		}

	folio_get(folio);

	if (flags & (FAULT_FLAG_WRITE|FAULT_FLAG_UNSHARE)) {
		if (!huge_pte_write(entry)) {
			ret = hugetlb_wp(mm, vma, address, ptep, flags,
					 pagecache_folio, ptl);
			goto out_put_page;
		} else if (likely(flags & FAULT_FLAG_WRITE)) {
			entry = huge_pte_mkdirty(entry);
		}
	}
	entry = pte_mkyoung(entry);
	if (huge_ptep_set_access_flags(vma, haddr, ptep, entry,
						flags & FAULT_FLAG_WRITE))
		update_mmu_cache(vma, haddr, ptep);
out_put_page:
	if (folio != pagecache_folio)
		folio_unlock(folio);
	folio_put(folio);
out_ptl:
	spin_unlock(ptl);

	if (pagecache_folio) {
		folio_unlock(pagecache_folio);
		folio_put(pagecache_folio);
	}
out_mutex:
	hugetlb_vma_unlock_read(vma);
	mutex_unlock(&hugetlb_fault_mutex_table[hash]);
	 
	if (need_wait_lock)
		folio_wait_locked(folio);
	return ret;
}

#ifdef CONFIG_USERFAULTFD
 
int hugetlb_mfill_atomic_pte(pte_t *dst_pte,
			     struct vm_area_struct *dst_vma,
			     unsigned long dst_addr,
			     unsigned long src_addr,
			     uffd_flags_t flags,
			     struct folio **foliop)
{
	struct mm_struct *dst_mm = dst_vma->vm_mm;
	bool is_continue = uffd_flags_mode_is(flags, MFILL_ATOMIC_CONTINUE);
	bool wp_enabled = (flags & MFILL_ATOMIC_WP);
	struct hstate *h = hstate_vma(dst_vma);
	struct address_space *mapping = dst_vma->vm_file->f_mapping;
	pgoff_t idx = vma_hugecache_offset(h, dst_vma, dst_addr);
	unsigned long size;
	int vm_shared = dst_vma->vm_flags & VM_SHARED;
	pte_t _dst_pte;
	spinlock_t *ptl;
	int ret = -ENOMEM;
	struct folio *folio;
	int writable;
	bool folio_in_pagecache = false;

	if (uffd_flags_mode_is(flags, MFILL_ATOMIC_POISON)) {
		ptl = huge_pte_lock(h, dst_mm, dst_pte);

		 
		if (!huge_pte_none(huge_ptep_get(dst_pte))) {
			spin_unlock(ptl);
			return -EEXIST;
		}

		_dst_pte = make_pte_marker(PTE_MARKER_POISONED);
		set_huge_pte_at(dst_mm, dst_addr, dst_pte, _dst_pte,
				huge_page_size(h));

		 
		update_mmu_cache(dst_vma, dst_addr, dst_pte);

		spin_unlock(ptl);
		return 0;
	}

	if (is_continue) {
		ret = -EFAULT;
		folio = filemap_lock_folio(mapping, idx);
		if (IS_ERR(folio))
			goto out;
		folio_in_pagecache = true;
	} else if (!*foliop) {
		 
		if (vm_shared &&
		    hugetlbfs_pagecache_present(h, dst_vma, dst_addr)) {
			ret = -EEXIST;
			goto out;
		}

		folio = alloc_hugetlb_folio(dst_vma, dst_addr, 0);
		if (IS_ERR(folio)) {
			ret = -ENOMEM;
			goto out;
		}

		ret = copy_folio_from_user(folio, (const void __user *) src_addr,
					   false);

		 
		if (unlikely(ret)) {
			ret = -ENOENT;
			 
			restore_reserve_on_error(h, dst_vma, dst_addr, folio);
			folio_put(folio);

			 
			folio = alloc_hugetlb_folio_vma(h, dst_vma, dst_addr);
			if (!folio) {
				ret = -ENOMEM;
				goto out;
			}
			*foliop = folio;
			 
			goto out;
		}
	} else {
		if (vm_shared &&
		    hugetlbfs_pagecache_present(h, dst_vma, dst_addr)) {
			folio_put(*foliop);
			ret = -EEXIST;
			*foliop = NULL;
			goto out;
		}

		folio = alloc_hugetlb_folio(dst_vma, dst_addr, 0);
		if (IS_ERR(folio)) {
			folio_put(*foliop);
			ret = -ENOMEM;
			*foliop = NULL;
			goto out;
		}
		ret = copy_user_large_folio(folio, *foliop, dst_addr, dst_vma);
		folio_put(*foliop);
		*foliop = NULL;
		if (ret) {
			folio_put(folio);
			goto out;
		}
	}

	 
	__folio_mark_uptodate(folio);

	 
	if (vm_shared && !is_continue) {
		size = i_size_read(mapping->host) >> huge_page_shift(h);
		ret = -EFAULT;
		if (idx >= size)
			goto out_release_nounlock;

		 
		ret = hugetlb_add_to_page_cache(folio, mapping, idx);
		if (ret)
			goto out_release_nounlock;
		folio_in_pagecache = true;
	}

	ptl = huge_pte_lock(h, dst_mm, dst_pte);

	ret = -EIO;
	if (folio_test_hwpoison(folio))
		goto out_release_unlock;

	 
	ret = -EEXIST;
	if (!huge_pte_none_mostly(huge_ptep_get(dst_pte)))
		goto out_release_unlock;

	if (folio_in_pagecache)
		page_dup_file_rmap(&folio->page, true);
	else
		hugepage_add_new_anon_rmap(folio, dst_vma, dst_addr);

	 
	if (wp_enabled || (is_continue && !vm_shared))
		writable = 0;
	else
		writable = dst_vma->vm_flags & VM_WRITE;

	_dst_pte = make_huge_pte(dst_vma, &folio->page, writable);
	 
	_dst_pte = huge_pte_mkdirty(_dst_pte);
	_dst_pte = pte_mkyoung(_dst_pte);

	if (wp_enabled)
		_dst_pte = huge_pte_mkuffd_wp(_dst_pte);

	set_huge_pte_at(dst_mm, dst_addr, dst_pte, _dst_pte, huge_page_size(h));

	hugetlb_count_add(pages_per_huge_page(h), dst_mm);

	 
	update_mmu_cache(dst_vma, dst_addr, dst_pte);

	spin_unlock(ptl);
	if (!is_continue)
		folio_set_hugetlb_migratable(folio);
	if (vm_shared || is_continue)
		folio_unlock(folio);
	ret = 0;
out:
	return ret;
out_release_unlock:
	spin_unlock(ptl);
	if (vm_shared || is_continue)
		folio_unlock(folio);
out_release_nounlock:
	if (!folio_in_pagecache)
		restore_reserve_on_error(h, dst_vma, dst_addr, folio);
	folio_put(folio);
	goto out;
}
#endif  

struct page *hugetlb_follow_page_mask(struct vm_area_struct *vma,
				      unsigned long address, unsigned int flags,
				      unsigned int *page_mask)
{
	struct hstate *h = hstate_vma(vma);
	struct mm_struct *mm = vma->vm_mm;
	unsigned long haddr = address & huge_page_mask(h);
	struct page *page = NULL;
	spinlock_t *ptl;
	pte_t *pte, entry;
	int ret;

	hugetlb_vma_lock_read(vma);
	pte = hugetlb_walk(vma, haddr, huge_page_size(h));
	if (!pte)
		goto out_unlock;

	ptl = huge_pte_lock(h, mm, pte);
	entry = huge_ptep_get(pte);
	if (pte_present(entry)) {
		page = pte_page(entry);

		if (!huge_pte_write(entry)) {
			if (flags & FOLL_WRITE) {
				page = NULL;
				goto out;
			}

			if (gup_must_unshare(vma, flags, page)) {
				 
				page = ERR_PTR(-EMLINK);
				goto out;
			}
		}

		page = nth_page(page, ((address & ~huge_page_mask(h)) >> PAGE_SHIFT));

		 
		ret = try_grab_page(page, flags);

		if (WARN_ON_ONCE(ret)) {
			page = ERR_PTR(ret);
			goto out;
		}

		*page_mask = (1U << huge_page_order(h)) - 1;
	}
out:
	spin_unlock(ptl);
out_unlock:
	hugetlb_vma_unlock_read(vma);

	 
	if (!page && (flags & FOLL_DUMP) &&
	    !hugetlbfs_pagecache_present(h, vma, address))
		page = ERR_PTR(-EFAULT);

	return page;
}

long hugetlb_change_protection(struct vm_area_struct *vma,
		unsigned long address, unsigned long end,
		pgprot_t newprot, unsigned long cp_flags)
{
	struct mm_struct *mm = vma->vm_mm;
	unsigned long start = address;
	pte_t *ptep;
	pte_t pte;
	struct hstate *h = hstate_vma(vma);
	long pages = 0, psize = huge_page_size(h);
	bool shared_pmd = false;
	struct mmu_notifier_range range;
	unsigned long last_addr_mask;
	bool uffd_wp = cp_flags & MM_CP_UFFD_WP;
	bool uffd_wp_resolve = cp_flags & MM_CP_UFFD_WP_RESOLVE;

	 
	mmu_notifier_range_init(&range, MMU_NOTIFY_PROTECTION_VMA,
				0, mm, start, end);
	adjust_range_if_pmd_sharing_possible(vma, &range.start, &range.end);

	BUG_ON(address >= end);
	flush_cache_range(vma, range.start, range.end);

	mmu_notifier_invalidate_range_start(&range);
	hugetlb_vma_lock_write(vma);
	i_mmap_lock_write(vma->vm_file->f_mapping);
	last_addr_mask = hugetlb_mask_last_page(h);
	for (; address < end; address += psize) {
		spinlock_t *ptl;
		ptep = hugetlb_walk(vma, address, psize);
		if (!ptep) {
			if (!uffd_wp) {
				address |= last_addr_mask;
				continue;
			}
			 
			ptep = huge_pte_alloc(mm, vma, address, psize);
			if (!ptep) {
				pages = -ENOMEM;
				break;
			}
		}
		ptl = huge_pte_lock(h, mm, ptep);
		if (huge_pmd_unshare(mm, vma, address, ptep)) {
			 
			WARN_ON_ONCE(uffd_wp || uffd_wp_resolve);
			pages++;
			spin_unlock(ptl);
			shared_pmd = true;
			address |= last_addr_mask;
			continue;
		}
		pte = huge_ptep_get(ptep);
		if (unlikely(is_hugetlb_entry_hwpoisoned(pte))) {
			 
		} else if (unlikely(is_hugetlb_entry_migration(pte))) {
			swp_entry_t entry = pte_to_swp_entry(pte);
			struct page *page = pfn_swap_entry_to_page(entry);
			pte_t newpte = pte;

			if (is_writable_migration_entry(entry)) {
				if (PageAnon(page))
					entry = make_readable_exclusive_migration_entry(
								swp_offset(entry));
				else
					entry = make_readable_migration_entry(
								swp_offset(entry));
				newpte = swp_entry_to_pte(entry);
				pages++;
			}

			if (uffd_wp)
				newpte = pte_swp_mkuffd_wp(newpte);
			else if (uffd_wp_resolve)
				newpte = pte_swp_clear_uffd_wp(newpte);
			if (!pte_same(pte, newpte))
				set_huge_pte_at(mm, address, ptep, newpte, psize);
		} else if (unlikely(is_pte_marker(pte))) {
			 
			WARN_ON_ONCE(!pte_marker_uffd_wp(pte));
			if (uffd_wp_resolve)
				 
				huge_pte_clear(mm, address, ptep, psize);
		} else if (!huge_pte_none(pte)) {
			pte_t old_pte;
			unsigned int shift = huge_page_shift(hstate_vma(vma));

			old_pte = huge_ptep_modify_prot_start(vma, address, ptep);
			pte = huge_pte_modify(old_pte, newprot);
			pte = arch_make_huge_pte(pte, shift, vma->vm_flags);
			if (uffd_wp)
				pte = huge_pte_mkuffd_wp(pte);
			else if (uffd_wp_resolve)
				pte = huge_pte_clear_uffd_wp(pte);
			huge_ptep_modify_prot_commit(vma, address, ptep, old_pte, pte);
			pages++;
		} else {
			 
			if (unlikely(uffd_wp))
				 
				set_huge_pte_at(mm, address, ptep,
						make_pte_marker(PTE_MARKER_UFFD_WP),
						psize);
		}
		spin_unlock(ptl);
	}
	 
	if (shared_pmd)
		flush_hugetlb_tlb_range(vma, range.start, range.end);
	else
		flush_hugetlb_tlb_range(vma, start, end);
	 
	i_mmap_unlock_write(vma->vm_file->f_mapping);
	hugetlb_vma_unlock_write(vma);
	mmu_notifier_invalidate_range_end(&range);

	return pages > 0 ? (pages << h->order) : pages;
}

 
bool hugetlb_reserve_pages(struct inode *inode,
					long from, long to,
					struct vm_area_struct *vma,
					vm_flags_t vm_flags)
{
	long chg = -1, add = -1;
	struct hstate *h = hstate_inode(inode);
	struct hugepage_subpool *spool = subpool_inode(inode);
	struct resv_map *resv_map;
	struct hugetlb_cgroup *h_cg = NULL;
	long gbl_reserve, regions_needed = 0;

	 
	if (from > to) {
		VM_WARN(1, "%s called with a negative range\n", __func__);
		return false;
	}

	 
	hugetlb_vma_lock_alloc(vma);

	 
	if (vm_flags & VM_NORESERVE)
		return true;

	 
	if (!vma || vma->vm_flags & VM_MAYSHARE) {
		 
		resv_map = inode_resv_map(inode);

		chg = region_chg(resv_map, from, to, &regions_needed);
	} else {
		 
		resv_map = resv_map_alloc();
		if (!resv_map)
			goto out_err;

		chg = to - from;

		set_vma_resv_map(vma, resv_map);
		set_vma_resv_flags(vma, HPAGE_RESV_OWNER);
	}

	if (chg < 0)
		goto out_err;

	if (hugetlb_cgroup_charge_cgroup_rsvd(hstate_index(h),
				chg * pages_per_huge_page(h), &h_cg) < 0)
		goto out_err;

	if (vma && !(vma->vm_flags & VM_MAYSHARE) && h_cg) {
		 
		resv_map_set_hugetlb_cgroup_uncharge_info(resv_map, h_cg, h);
	}

	 
	gbl_reserve = hugepage_subpool_get_pages(spool, chg);
	if (gbl_reserve < 0)
		goto out_uncharge_cgroup;

	 
	if (hugetlb_acct_memory(h, gbl_reserve) < 0)
		goto out_put_pages;

	 
	if (!vma || vma->vm_flags & VM_MAYSHARE) {
		add = region_add(resv_map, from, to, regions_needed, h, h_cg);

		if (unlikely(add < 0)) {
			hugetlb_acct_memory(h, -gbl_reserve);
			goto out_put_pages;
		} else if (unlikely(chg > add)) {
			 
			long rsv_adjust;

			 
			hugetlb_cgroup_uncharge_cgroup_rsvd(
				hstate_index(h),
				(chg - add) * pages_per_huge_page(h), h_cg);

			rsv_adjust = hugepage_subpool_put_pages(spool,
								chg - add);
			hugetlb_acct_memory(h, -rsv_adjust);
		} else if (h_cg) {
			 
			hugetlb_cgroup_put_rsvd_cgroup(h_cg);
		}
	}
	return true;

out_put_pages:
	 
	(void)hugepage_subpool_put_pages(spool, chg);
out_uncharge_cgroup:
	hugetlb_cgroup_uncharge_cgroup_rsvd(hstate_index(h),
					    chg * pages_per_huge_page(h), h_cg);
out_err:
	hugetlb_vma_lock_free(vma);
	if (!vma || vma->vm_flags & VM_MAYSHARE)
		 
		if (chg >= 0 && add < 0)
			region_abort(resv_map, from, to, regions_needed);
	if (vma && is_vma_resv_set(vma, HPAGE_RESV_OWNER)) {
		kref_put(&resv_map->refs, resv_map_release);
		set_vma_resv_map(vma, NULL);
	}
	return false;
}

long hugetlb_unreserve_pages(struct inode *inode, long start, long end,
								long freed)
{
	struct hstate *h = hstate_inode(inode);
	struct resv_map *resv_map = inode_resv_map(inode);
	long chg = 0;
	struct hugepage_subpool *spool = subpool_inode(inode);
	long gbl_reserve;

	 
	if (resv_map) {
		chg = region_del(resv_map, start, end);
		 
		if (chg < 0)
			return chg;
	}

	spin_lock(&inode->i_lock);
	inode->i_blocks -= (blocks_per_huge_page(h) * freed);
	spin_unlock(&inode->i_lock);

	 
	gbl_reserve = hugepage_subpool_put_pages(spool, (chg - freed));
	hugetlb_acct_memory(h, -gbl_reserve);

	return 0;
}

#ifdef CONFIG_ARCH_WANT_HUGE_PMD_SHARE
static unsigned long page_table_shareable(struct vm_area_struct *svma,
				struct vm_area_struct *vma,
				unsigned long addr, pgoff_t idx)
{
	unsigned long saddr = ((idx - svma->vm_pgoff) << PAGE_SHIFT) +
				svma->vm_start;
	unsigned long sbase = saddr & PUD_MASK;
	unsigned long s_end = sbase + PUD_SIZE;

	 
	unsigned long vm_flags = vma->vm_flags & ~VM_LOCKED_MASK;
	unsigned long svm_flags = svma->vm_flags & ~VM_LOCKED_MASK;

	 
	if (pmd_index(addr) != pmd_index(saddr) ||
	    vm_flags != svm_flags ||
	    !range_in_vma(svma, sbase, s_end) ||
	    !svma->vm_private_data)
		return 0;

	return saddr;
}

bool want_pmd_share(struct vm_area_struct *vma, unsigned long addr)
{
	unsigned long start = addr & PUD_MASK;
	unsigned long end = start + PUD_SIZE;

#ifdef CONFIG_USERFAULTFD
	if (uffd_disable_huge_pmd_share(vma))
		return false;
#endif
	 
	if (!(vma->vm_flags & VM_MAYSHARE))
		return false;
	if (!vma->vm_private_data)	 
		return false;
	if (!range_in_vma(vma, start, end))
		return false;
	return true;
}

 
void adjust_range_if_pmd_sharing_possible(struct vm_area_struct *vma,
				unsigned long *start, unsigned long *end)
{
	unsigned long v_start = ALIGN(vma->vm_start, PUD_SIZE),
		v_end = ALIGN_DOWN(vma->vm_end, PUD_SIZE);

	 
	if (!(vma->vm_flags & VM_MAYSHARE) || !(v_end > v_start) ||
		(*end <= v_start) || (*start >= v_end))
		return;

	 
	if (*start > v_start)
		*start = ALIGN_DOWN(*start, PUD_SIZE);

	if (*end < v_end)
		*end = ALIGN(*end, PUD_SIZE);
}

 
pte_t *huge_pmd_share(struct mm_struct *mm, struct vm_area_struct *vma,
		      unsigned long addr, pud_t *pud)
{
	struct address_space *mapping = vma->vm_file->f_mapping;
	pgoff_t idx = ((addr - vma->vm_start) >> PAGE_SHIFT) +
			vma->vm_pgoff;
	struct vm_area_struct *svma;
	unsigned long saddr;
	pte_t *spte = NULL;
	pte_t *pte;

	i_mmap_lock_read(mapping);
	vma_interval_tree_foreach(svma, &mapping->i_mmap, idx, idx) {
		if (svma == vma)
			continue;

		saddr = page_table_shareable(svma, vma, addr, idx);
		if (saddr) {
			spte = hugetlb_walk(svma, saddr,
					    vma_mmu_pagesize(svma));
			if (spte) {
				get_page(virt_to_page(spte));
				break;
			}
		}
	}

	if (!spte)
		goto out;

	spin_lock(&mm->page_table_lock);
	if (pud_none(*pud)) {
		pud_populate(mm, pud,
				(pmd_t *)((unsigned long)spte & PAGE_MASK));
		mm_inc_nr_pmds(mm);
	} else {
		put_page(virt_to_page(spte));
	}
	spin_unlock(&mm->page_table_lock);
out:
	pte = (pte_t *)pmd_alloc(mm, pud, addr);
	i_mmap_unlock_read(mapping);
	return pte;
}

 
int huge_pmd_unshare(struct mm_struct *mm, struct vm_area_struct *vma,
					unsigned long addr, pte_t *ptep)
{
	pgd_t *pgd = pgd_offset(mm, addr);
	p4d_t *p4d = p4d_offset(pgd, addr);
	pud_t *pud = pud_offset(p4d, addr);

	i_mmap_assert_write_locked(vma->vm_file->f_mapping);
	hugetlb_vma_assert_locked(vma);
	BUG_ON(page_count(virt_to_page(ptep)) == 0);
	if (page_count(virt_to_page(ptep)) == 1)
		return 0;

	pud_clear(pud);
	put_page(virt_to_page(ptep));
	mm_dec_nr_pmds(mm);
	return 1;
}

#else  

pte_t *huge_pmd_share(struct mm_struct *mm, struct vm_area_struct *vma,
		      unsigned long addr, pud_t *pud)
{
	return NULL;
}

int huge_pmd_unshare(struct mm_struct *mm, struct vm_area_struct *vma,
				unsigned long addr, pte_t *ptep)
{
	return 0;
}

void adjust_range_if_pmd_sharing_possible(struct vm_area_struct *vma,
				unsigned long *start, unsigned long *end)
{
}

bool want_pmd_share(struct vm_area_struct *vma, unsigned long addr)
{
	return false;
}
#endif  

#ifdef CONFIG_ARCH_WANT_GENERAL_HUGETLB
pte_t *huge_pte_alloc(struct mm_struct *mm, struct vm_area_struct *vma,
			unsigned long addr, unsigned long sz)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pte_t *pte = NULL;

	pgd = pgd_offset(mm, addr);
	p4d = p4d_alloc(mm, pgd, addr);
	if (!p4d)
		return NULL;
	pud = pud_alloc(mm, p4d, addr);
	if (pud) {
		if (sz == PUD_SIZE) {
			pte = (pte_t *)pud;
		} else {
			BUG_ON(sz != PMD_SIZE);
			if (want_pmd_share(vma, addr) && pud_none(*pud))
				pte = huge_pmd_share(mm, vma, addr, pud);
			else
				pte = (pte_t *)pmd_alloc(mm, pud, addr);
		}
	}

	if (pte) {
		pte_t pteval = ptep_get_lockless(pte);

		BUG_ON(pte_present(pteval) && !pte_huge(pteval));
	}

	return pte;
}

 
pte_t *huge_pte_offset(struct mm_struct *mm,
		       unsigned long addr, unsigned long sz)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;

	pgd = pgd_offset(mm, addr);
	if (!pgd_present(*pgd))
		return NULL;
	p4d = p4d_offset(pgd, addr);
	if (!p4d_present(*p4d))
		return NULL;

	pud = pud_offset(p4d, addr);
	if (sz == PUD_SIZE)
		 
		return (pte_t *)pud;
	if (!pud_present(*pud))
		return NULL;
	 

	pmd = pmd_offset(pud, addr);
	 
	return (pte_t *)pmd;
}

 
unsigned long hugetlb_mask_last_page(struct hstate *h)
{
	unsigned long hp_size = huge_page_size(h);

	if (hp_size == PUD_SIZE)
		return P4D_SIZE - PUD_SIZE;
	else if (hp_size == PMD_SIZE)
		return PUD_SIZE - PMD_SIZE;
	else
		return 0UL;
}

#else

 
__weak unsigned long hugetlb_mask_last_page(struct hstate *h)
{
#ifdef CONFIG_ARCH_WANT_HUGE_PMD_SHARE
	if (huge_page_size(h) == PMD_SIZE)
		return PUD_SIZE - PMD_SIZE;
#endif
	return 0UL;
}

#endif  

 
bool isolate_hugetlb(struct folio *folio, struct list_head *list)
{
	bool ret = true;

	spin_lock_irq(&hugetlb_lock);
	if (!folio_test_hugetlb(folio) ||
	    !folio_test_hugetlb_migratable(folio) ||
	    !folio_try_get(folio)) {
		ret = false;
		goto unlock;
	}
	folio_clear_hugetlb_migratable(folio);
	list_move_tail(&folio->lru, list);
unlock:
	spin_unlock_irq(&hugetlb_lock);
	return ret;
}

int get_hwpoison_hugetlb_folio(struct folio *folio, bool *hugetlb, bool unpoison)
{
	int ret = 0;

	*hugetlb = false;
	spin_lock_irq(&hugetlb_lock);
	if (folio_test_hugetlb(folio)) {
		*hugetlb = true;
		if (folio_test_hugetlb_freed(folio))
			ret = 0;
		else if (folio_test_hugetlb_migratable(folio) || unpoison)
			ret = folio_try_get(folio);
		else
			ret = -EBUSY;
	}
	spin_unlock_irq(&hugetlb_lock);
	return ret;
}

int get_huge_page_for_hwpoison(unsigned long pfn, int flags,
				bool *migratable_cleared)
{
	int ret;

	spin_lock_irq(&hugetlb_lock);
	ret = __get_huge_page_for_hwpoison(pfn, flags, migratable_cleared);
	spin_unlock_irq(&hugetlb_lock);
	return ret;
}

void folio_putback_active_hugetlb(struct folio *folio)
{
	spin_lock_irq(&hugetlb_lock);
	folio_set_hugetlb_migratable(folio);
	list_move_tail(&folio->lru, &(folio_hstate(folio))->hugepage_activelist);
	spin_unlock_irq(&hugetlb_lock);
	folio_put(folio);
}

void move_hugetlb_state(struct folio *old_folio, struct folio *new_folio, int reason)
{
	struct hstate *h = folio_hstate(old_folio);

	hugetlb_cgroup_migrate(old_folio, new_folio);
	set_page_owner_migrate_reason(&new_folio->page, reason);

	 
	if (folio_test_hugetlb_temporary(new_folio)) {
		int old_nid = folio_nid(old_folio);
		int new_nid = folio_nid(new_folio);

		folio_set_hugetlb_temporary(old_folio);
		folio_clear_hugetlb_temporary(new_folio);


		 
		if (new_nid == old_nid)
			return;
		spin_lock_irq(&hugetlb_lock);
		if (h->surplus_huge_pages_node[old_nid]) {
			h->surplus_huge_pages_node[old_nid]--;
			h->surplus_huge_pages_node[new_nid]++;
		}
		spin_unlock_irq(&hugetlb_lock);
	}
}

static void hugetlb_unshare_pmds(struct vm_area_struct *vma,
				   unsigned long start,
				   unsigned long end)
{
	struct hstate *h = hstate_vma(vma);
	unsigned long sz = huge_page_size(h);
	struct mm_struct *mm = vma->vm_mm;
	struct mmu_notifier_range range;
	unsigned long address;
	spinlock_t *ptl;
	pte_t *ptep;

	if (!(vma->vm_flags & VM_MAYSHARE))
		return;

	if (start >= end)
		return;

	flush_cache_range(vma, start, end);
	 
	mmu_notifier_range_init(&range, MMU_NOTIFY_CLEAR, 0, mm,
				start, end);
	mmu_notifier_invalidate_range_start(&range);
	hugetlb_vma_lock_write(vma);
	i_mmap_lock_write(vma->vm_file->f_mapping);
	for (address = start; address < end; address += PUD_SIZE) {
		ptep = hugetlb_walk(vma, address, sz);
		if (!ptep)
			continue;
		ptl = huge_pte_lock(h, mm, ptep);
		huge_pmd_unshare(mm, vma, address, ptep);
		spin_unlock(ptl);
	}
	flush_hugetlb_tlb_range(vma, start, end);
	i_mmap_unlock_write(vma->vm_file->f_mapping);
	hugetlb_vma_unlock_write(vma);
	 
	mmu_notifier_invalidate_range_end(&range);
}

 
void hugetlb_unshare_all_pmds(struct vm_area_struct *vma)
{
	hugetlb_unshare_pmds(vma, ALIGN(vma->vm_start, PUD_SIZE),
			ALIGN_DOWN(vma->vm_end, PUD_SIZE));
}

#ifdef CONFIG_CMA
static bool cma_reserve_called __initdata;

static int __init cmdline_parse_hugetlb_cma(char *p)
{
	int nid, count = 0;
	unsigned long tmp;
	char *s = p;

	while (*s) {
		if (sscanf(s, "%lu%n", &tmp, &count) != 1)
			break;

		if (s[count] == ':') {
			if (tmp >= MAX_NUMNODES)
				break;
			nid = array_index_nospec(tmp, MAX_NUMNODES);

			s += count + 1;
			tmp = memparse(s, &s);
			hugetlb_cma_size_in_node[nid] = tmp;
			hugetlb_cma_size += tmp;

			 
			if (*s == ',')
				s++;
			else
				break;
		} else {
			hugetlb_cma_size = memparse(p, &p);
			break;
		}
	}

	return 0;
}

early_param("hugetlb_cma", cmdline_parse_hugetlb_cma);

void __init hugetlb_cma_reserve(int order)
{
	unsigned long size, reserved, per_node;
	bool node_specific_cma_alloc = false;
	int nid;

	cma_reserve_called = true;

	if (!hugetlb_cma_size)
		return;

	for (nid = 0; nid < MAX_NUMNODES; nid++) {
		if (hugetlb_cma_size_in_node[nid] == 0)
			continue;

		if (!node_online(nid)) {
			pr_warn("hugetlb_cma: invalid node %d specified\n", nid);
			hugetlb_cma_size -= hugetlb_cma_size_in_node[nid];
			hugetlb_cma_size_in_node[nid] = 0;
			continue;
		}

		if (hugetlb_cma_size_in_node[nid] < (PAGE_SIZE << order)) {
			pr_warn("hugetlb_cma: cma area of node %d should be at least %lu MiB\n",
				nid, (PAGE_SIZE << order) / SZ_1M);
			hugetlb_cma_size -= hugetlb_cma_size_in_node[nid];
			hugetlb_cma_size_in_node[nid] = 0;
		} else {
			node_specific_cma_alloc = true;
		}
	}

	 
	if (!hugetlb_cma_size)
		return;

	if (hugetlb_cma_size < (PAGE_SIZE << order)) {
		pr_warn("hugetlb_cma: cma area should be at least %lu MiB\n",
			(PAGE_SIZE << order) / SZ_1M);
		hugetlb_cma_size = 0;
		return;
	}

	if (!node_specific_cma_alloc) {
		 
		per_node = DIV_ROUND_UP(hugetlb_cma_size, nr_online_nodes);
		pr_info("hugetlb_cma: reserve %lu MiB, up to %lu MiB per node\n",
			hugetlb_cma_size / SZ_1M, per_node / SZ_1M);
	}

	reserved = 0;
	for_each_online_node(nid) {
		int res;
		char name[CMA_MAX_NAME];

		if (node_specific_cma_alloc) {
			if (hugetlb_cma_size_in_node[nid] == 0)
				continue;

			size = hugetlb_cma_size_in_node[nid];
		} else {
			size = min(per_node, hugetlb_cma_size - reserved);
		}

		size = round_up(size, PAGE_SIZE << order);

		snprintf(name, sizeof(name), "hugetlb%d", nid);
		 
		res = cma_declare_contiguous_nid(0, size, 0,
						PAGE_SIZE << HUGETLB_PAGE_ORDER,
						 0, false, name,
						 &hugetlb_cma[nid], nid);
		if (res) {
			pr_warn("hugetlb_cma: reservation failed: err %d, node %d",
				res, nid);
			continue;
		}

		reserved += size;
		pr_info("hugetlb_cma: reserved %lu MiB on node %d\n",
			size / SZ_1M, nid);

		if (reserved >= hugetlb_cma_size)
			break;
	}

	if (!reserved)
		 
		hugetlb_cma_size = 0;
}

static void __init hugetlb_cma_check(void)
{
	if (!hugetlb_cma_size || cma_reserve_called)
		return;

	pr_warn("hugetlb_cma: the option isn't supported by current arch\n");
}

#endif  
