
 

#include <linux/blkdev.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/hugetlb.h>
#include <linux/mman.h>
#include <linux/slab.h>
#include <linux/kernel_stat.h>
#include <linux/swap.h>
#include <linux/vmalloc.h>
#include <linux/pagemap.h>
#include <linux/namei.h>
#include <linux/shmem_fs.h>
#include <linux/blk-cgroup.h>
#include <linux/random.h>
#include <linux/writeback.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/init.h>
#include <linux/ksm.h>
#include <linux/rmap.h>
#include <linux/security.h>
#include <linux/backing-dev.h>
#include <linux/mutex.h>
#include <linux/capability.h>
#include <linux/syscalls.h>
#include <linux/memcontrol.h>
#include <linux/poll.h>
#include <linux/oom.h>
#include <linux/swapfile.h>
#include <linux/export.h>
#include <linux/swap_slots.h>
#include <linux/sort.h>
#include <linux/completion.h>
#include <linux/suspend.h>
#include <linux/zswap.h>

#include <asm/tlbflush.h>
#include <linux/swapops.h>
#include <linux/swap_cgroup.h>
#include "internal.h"
#include "swap.h"

static bool swap_count_continued(struct swap_info_struct *, pgoff_t,
				 unsigned char);
static void free_swap_count_continuations(struct swap_info_struct *);

static DEFINE_SPINLOCK(swap_lock);
static unsigned int nr_swapfiles;
atomic_long_t nr_swap_pages;
 
EXPORT_SYMBOL_GPL(nr_swap_pages);
 
long total_swap_pages;
static int least_priority = -1;
unsigned long swapfile_maximum_size;
#ifdef CONFIG_MIGRATION
bool swap_migration_ad_supported;
#endif	 

static const char Bad_file[] = "Bad swap file entry ";
static const char Unused_file[] = "Unused swap file entry ";
static const char Bad_offset[] = "Bad swap offset entry ";
static const char Unused_offset[] = "Unused swap offset entry ";

 
static PLIST_HEAD(swap_active_head);

 
static struct plist_head *swap_avail_heads;
static DEFINE_SPINLOCK(swap_avail_lock);

static struct swap_info_struct *swap_info[MAX_SWAPFILES];

static DEFINE_MUTEX(swapon_mutex);

static DECLARE_WAIT_QUEUE_HEAD(proc_poll_wait);
 
static atomic_t proc_poll_event = ATOMIC_INIT(0);

atomic_t nr_rotate_swap = ATOMIC_INIT(0);

static struct swap_info_struct *swap_type_to_swap_info(int type)
{
	if (type >= MAX_SWAPFILES)
		return NULL;

	return READ_ONCE(swap_info[type]);  
}

static inline unsigned char swap_count(unsigned char ent)
{
	return ent & ~SWAP_HAS_CACHE;	 
}

 
#define TTRS_ANYWAY		0x1
 
#define TTRS_UNMAPPED		0x2
 
#define TTRS_FULL		0x4

 
static int __try_to_reclaim_swap(struct swap_info_struct *si,
				 unsigned long offset, unsigned long flags)
{
	swp_entry_t entry = swp_entry(si->type, offset);
	struct folio *folio;
	int ret = 0;

	folio = filemap_get_folio(swap_address_space(entry), offset);
	if (IS_ERR(folio))
		return 0;
	 
	if (folio_trylock(folio)) {
		if ((flags & TTRS_ANYWAY) ||
		    ((flags & TTRS_UNMAPPED) && !folio_mapped(folio)) ||
		    ((flags & TTRS_FULL) && mem_cgroup_swap_full(folio)))
			ret = folio_free_swap(folio);
		folio_unlock(folio);
	}
	folio_put(folio);
	return ret;
}

static inline struct swap_extent *first_se(struct swap_info_struct *sis)
{
	struct rb_node *rb = rb_first(&sis->swap_extent_root);
	return rb_entry(rb, struct swap_extent, rb_node);
}

static inline struct swap_extent *next_se(struct swap_extent *se)
{
	struct rb_node *rb = rb_next(&se->rb_node);
	return rb ? rb_entry(rb, struct swap_extent, rb_node) : NULL;
}

 
static int discard_swap(struct swap_info_struct *si)
{
	struct swap_extent *se;
	sector_t start_block;
	sector_t nr_blocks;
	int err = 0;

	 
	se = first_se(si);
	start_block = (se->start_block + 1) << (PAGE_SHIFT - 9);
	nr_blocks = ((sector_t)se->nr_pages - 1) << (PAGE_SHIFT - 9);
	if (nr_blocks) {
		err = blkdev_issue_discard(si->bdev, start_block,
				nr_blocks, GFP_KERNEL);
		if (err)
			return err;
		cond_resched();
	}

	for (se = next_se(se); se; se = next_se(se)) {
		start_block = se->start_block << (PAGE_SHIFT - 9);
		nr_blocks = (sector_t)se->nr_pages << (PAGE_SHIFT - 9);

		err = blkdev_issue_discard(si->bdev, start_block,
				nr_blocks, GFP_KERNEL);
		if (err)
			break;

		cond_resched();
	}
	return err;		 
}

static struct swap_extent *
offset_to_swap_extent(struct swap_info_struct *sis, unsigned long offset)
{
	struct swap_extent *se;
	struct rb_node *rb;

	rb = sis->swap_extent_root.rb_node;
	while (rb) {
		se = rb_entry(rb, struct swap_extent, rb_node);
		if (offset < se->start_page)
			rb = rb->rb_left;
		else if (offset >= se->start_page + se->nr_pages)
			rb = rb->rb_right;
		else
			return se;
	}
	 
	BUG();
}

sector_t swap_page_sector(struct page *page)
{
	struct swap_info_struct *sis = page_swap_info(page);
	struct swap_extent *se;
	sector_t sector;
	pgoff_t offset;

	offset = __page_file_index(page);
	se = offset_to_swap_extent(sis, offset);
	sector = se->start_block + (offset - se->start_page);
	return sector << (PAGE_SHIFT - 9);
}

 
static void discard_swap_cluster(struct swap_info_struct *si,
				 pgoff_t start_page, pgoff_t nr_pages)
{
	struct swap_extent *se = offset_to_swap_extent(si, start_page);

	while (nr_pages) {
		pgoff_t offset = start_page - se->start_page;
		sector_t start_block = se->start_block + offset;
		sector_t nr_blocks = se->nr_pages - offset;

		if (nr_blocks > nr_pages)
			nr_blocks = nr_pages;
		start_page += nr_blocks;
		nr_pages -= nr_blocks;

		start_block <<= PAGE_SHIFT - 9;
		nr_blocks <<= PAGE_SHIFT - 9;
		if (blkdev_issue_discard(si->bdev, start_block,
					nr_blocks, GFP_NOIO))
			break;

		se = next_se(se);
	}
}

#ifdef CONFIG_THP_SWAP
#define SWAPFILE_CLUSTER	HPAGE_PMD_NR

#define swap_entry_size(size)	(size)
#else
#define SWAPFILE_CLUSTER	256

 
#define swap_entry_size(size)	1
#endif
#define LATENCY_LIMIT		256

static inline void cluster_set_flag(struct swap_cluster_info *info,
	unsigned int flag)
{
	info->flags = flag;
}

static inline unsigned int cluster_count(struct swap_cluster_info *info)
{
	return info->data;
}

static inline void cluster_set_count(struct swap_cluster_info *info,
				     unsigned int c)
{
	info->data = c;
}

static inline void cluster_set_count_flag(struct swap_cluster_info *info,
					 unsigned int c, unsigned int f)
{
	info->flags = f;
	info->data = c;
}

static inline unsigned int cluster_next(struct swap_cluster_info *info)
{
	return info->data;
}

static inline void cluster_set_next(struct swap_cluster_info *info,
				    unsigned int n)
{
	info->data = n;
}

static inline void cluster_set_next_flag(struct swap_cluster_info *info,
					 unsigned int n, unsigned int f)
{
	info->flags = f;
	info->data = n;
}

static inline bool cluster_is_free(struct swap_cluster_info *info)
{
	return info->flags & CLUSTER_FLAG_FREE;
}

static inline bool cluster_is_null(struct swap_cluster_info *info)
{
	return info->flags & CLUSTER_FLAG_NEXT_NULL;
}

static inline void cluster_set_null(struct swap_cluster_info *info)
{
	info->flags = CLUSTER_FLAG_NEXT_NULL;
	info->data = 0;
}

static inline bool cluster_is_huge(struct swap_cluster_info *info)
{
	if (IS_ENABLED(CONFIG_THP_SWAP))
		return info->flags & CLUSTER_FLAG_HUGE;
	return false;
}

static inline void cluster_clear_huge(struct swap_cluster_info *info)
{
	info->flags &= ~CLUSTER_FLAG_HUGE;
}

static inline struct swap_cluster_info *lock_cluster(struct swap_info_struct *si,
						     unsigned long offset)
{
	struct swap_cluster_info *ci;

	ci = si->cluster_info;
	if (ci) {
		ci += offset / SWAPFILE_CLUSTER;
		spin_lock(&ci->lock);
	}
	return ci;
}

static inline void unlock_cluster(struct swap_cluster_info *ci)
{
	if (ci)
		spin_unlock(&ci->lock);
}

 
static inline struct swap_cluster_info *lock_cluster_or_swap_info(
		struct swap_info_struct *si, unsigned long offset)
{
	struct swap_cluster_info *ci;

	 
	ci = lock_cluster(si, offset);
	 
	if (!ci)
		spin_lock(&si->lock);

	return ci;
}

static inline void unlock_cluster_or_swap_info(struct swap_info_struct *si,
					       struct swap_cluster_info *ci)
{
	if (ci)
		unlock_cluster(ci);
	else
		spin_unlock(&si->lock);
}

static inline bool cluster_list_empty(struct swap_cluster_list *list)
{
	return cluster_is_null(&list->head);
}

static inline unsigned int cluster_list_first(struct swap_cluster_list *list)
{
	return cluster_next(&list->head);
}

static void cluster_list_init(struct swap_cluster_list *list)
{
	cluster_set_null(&list->head);
	cluster_set_null(&list->tail);
}

static void cluster_list_add_tail(struct swap_cluster_list *list,
				  struct swap_cluster_info *ci,
				  unsigned int idx)
{
	if (cluster_list_empty(list)) {
		cluster_set_next_flag(&list->head, idx, 0);
		cluster_set_next_flag(&list->tail, idx, 0);
	} else {
		struct swap_cluster_info *ci_tail;
		unsigned int tail = cluster_next(&list->tail);

		 
		ci_tail = ci + tail;
		spin_lock_nested(&ci_tail->lock, SINGLE_DEPTH_NESTING);
		cluster_set_next(ci_tail, idx);
		spin_unlock(&ci_tail->lock);
		cluster_set_next_flag(&list->tail, idx, 0);
	}
}

static unsigned int cluster_list_del_first(struct swap_cluster_list *list,
					   struct swap_cluster_info *ci)
{
	unsigned int idx;

	idx = cluster_next(&list->head);
	if (cluster_next(&list->tail) == idx) {
		cluster_set_null(&list->head);
		cluster_set_null(&list->tail);
	} else
		cluster_set_next_flag(&list->head,
				      cluster_next(&ci[idx]), 0);

	return idx;
}

 
static void swap_cluster_schedule_discard(struct swap_info_struct *si,
		unsigned int idx)
{
	 
	memset(si->swap_map + idx * SWAPFILE_CLUSTER,
			SWAP_MAP_BAD, SWAPFILE_CLUSTER);

	cluster_list_add_tail(&si->discard_clusters, si->cluster_info, idx);

	schedule_work(&si->discard_work);
}

static void __free_cluster(struct swap_info_struct *si, unsigned long idx)
{
	struct swap_cluster_info *ci = si->cluster_info;

	cluster_set_flag(ci + idx, CLUSTER_FLAG_FREE);
	cluster_list_add_tail(&si->free_clusters, ci, idx);
}

 
static void swap_do_scheduled_discard(struct swap_info_struct *si)
{
	struct swap_cluster_info *info, *ci;
	unsigned int idx;

	info = si->cluster_info;

	while (!cluster_list_empty(&si->discard_clusters)) {
		idx = cluster_list_del_first(&si->discard_clusters, info);
		spin_unlock(&si->lock);

		discard_swap_cluster(si, idx * SWAPFILE_CLUSTER,
				SWAPFILE_CLUSTER);

		spin_lock(&si->lock);
		ci = lock_cluster(si, idx * SWAPFILE_CLUSTER);
		__free_cluster(si, idx);
		memset(si->swap_map + idx * SWAPFILE_CLUSTER,
				0, SWAPFILE_CLUSTER);
		unlock_cluster(ci);
	}
}

static void swap_discard_work(struct work_struct *work)
{
	struct swap_info_struct *si;

	si = container_of(work, struct swap_info_struct, discard_work);

	spin_lock(&si->lock);
	swap_do_scheduled_discard(si);
	spin_unlock(&si->lock);
}

static void swap_users_ref_free(struct percpu_ref *ref)
{
	struct swap_info_struct *si;

	si = container_of(ref, struct swap_info_struct, users);
	complete(&si->comp);
}

static void alloc_cluster(struct swap_info_struct *si, unsigned long idx)
{
	struct swap_cluster_info *ci = si->cluster_info;

	VM_BUG_ON(cluster_list_first(&si->free_clusters) != idx);
	cluster_list_del_first(&si->free_clusters, ci);
	cluster_set_count_flag(ci + idx, 0, 0);
}

static void free_cluster(struct swap_info_struct *si, unsigned long idx)
{
	struct swap_cluster_info *ci = si->cluster_info + idx;

	VM_BUG_ON(cluster_count(ci) != 0);
	 
	if ((si->flags & (SWP_WRITEOK | SWP_PAGE_DISCARD)) ==
	    (SWP_WRITEOK | SWP_PAGE_DISCARD)) {
		swap_cluster_schedule_discard(si, idx);
		return;
	}

	__free_cluster(si, idx);
}

 
static void inc_cluster_info_page(struct swap_info_struct *p,
	struct swap_cluster_info *cluster_info, unsigned long page_nr)
{
	unsigned long idx = page_nr / SWAPFILE_CLUSTER;

	if (!cluster_info)
		return;
	if (cluster_is_free(&cluster_info[idx]))
		alloc_cluster(p, idx);

	VM_BUG_ON(cluster_count(&cluster_info[idx]) >= SWAPFILE_CLUSTER);
	cluster_set_count(&cluster_info[idx],
		cluster_count(&cluster_info[idx]) + 1);
}

 
static void dec_cluster_info_page(struct swap_info_struct *p,
	struct swap_cluster_info *cluster_info, unsigned long page_nr)
{
	unsigned long idx = page_nr / SWAPFILE_CLUSTER;

	if (!cluster_info)
		return;

	VM_BUG_ON(cluster_count(&cluster_info[idx]) == 0);
	cluster_set_count(&cluster_info[idx],
		cluster_count(&cluster_info[idx]) - 1);

	if (cluster_count(&cluster_info[idx]) == 0)
		free_cluster(p, idx);
}

 
static bool
scan_swap_map_ssd_cluster_conflict(struct swap_info_struct *si,
	unsigned long offset)
{
	struct percpu_cluster *percpu_cluster;
	bool conflict;

	offset /= SWAPFILE_CLUSTER;
	conflict = !cluster_list_empty(&si->free_clusters) &&
		offset != cluster_list_first(&si->free_clusters) &&
		cluster_is_free(&si->cluster_info[offset]);

	if (!conflict)
		return false;

	percpu_cluster = this_cpu_ptr(si->percpu_cluster);
	cluster_set_null(&percpu_cluster->index);
	return true;
}

 
static bool scan_swap_map_try_ssd_cluster(struct swap_info_struct *si,
	unsigned long *offset, unsigned long *scan_base)
{
	struct percpu_cluster *cluster;
	struct swap_cluster_info *ci;
	unsigned long tmp, max;

new_cluster:
	cluster = this_cpu_ptr(si->percpu_cluster);
	if (cluster_is_null(&cluster->index)) {
		if (!cluster_list_empty(&si->free_clusters)) {
			cluster->index = si->free_clusters.head;
			cluster->next = cluster_next(&cluster->index) *
					SWAPFILE_CLUSTER;
		} else if (!cluster_list_empty(&si->discard_clusters)) {
			 
			swap_do_scheduled_discard(si);
			*scan_base = this_cpu_read(*si->cluster_next_cpu);
			*offset = *scan_base;
			goto new_cluster;
		} else
			return false;
	}

	 
	tmp = cluster->next;
	max = min_t(unsigned long, si->max,
		    (cluster_next(&cluster->index) + 1) * SWAPFILE_CLUSTER);
	if (tmp < max) {
		ci = lock_cluster(si, tmp);
		while (tmp < max) {
			if (!si->swap_map[tmp])
				break;
			tmp++;
		}
		unlock_cluster(ci);
	}
	if (tmp >= max) {
		cluster_set_null(&cluster->index);
		goto new_cluster;
	}
	cluster->next = tmp + 1;
	*offset = tmp;
	*scan_base = tmp;
	return true;
}

static void __del_from_avail_list(struct swap_info_struct *p)
{
	int nid;

	assert_spin_locked(&p->lock);
	for_each_node(nid)
		plist_del(&p->avail_lists[nid], &swap_avail_heads[nid]);
}

static void del_from_avail_list(struct swap_info_struct *p)
{
	spin_lock(&swap_avail_lock);
	__del_from_avail_list(p);
	spin_unlock(&swap_avail_lock);
}

static void swap_range_alloc(struct swap_info_struct *si, unsigned long offset,
			     unsigned int nr_entries)
{
	unsigned int end = offset + nr_entries - 1;

	if (offset == si->lowest_bit)
		si->lowest_bit += nr_entries;
	if (end == si->highest_bit)
		WRITE_ONCE(si->highest_bit, si->highest_bit - nr_entries);
	WRITE_ONCE(si->inuse_pages, si->inuse_pages + nr_entries);
	if (si->inuse_pages == si->pages) {
		si->lowest_bit = si->max;
		si->highest_bit = 0;
		del_from_avail_list(si);
	}
}

static void add_to_avail_list(struct swap_info_struct *p)
{
	int nid;

	spin_lock(&swap_avail_lock);
	for_each_node(nid)
		plist_add(&p->avail_lists[nid], &swap_avail_heads[nid]);
	spin_unlock(&swap_avail_lock);
}

static void swap_range_free(struct swap_info_struct *si, unsigned long offset,
			    unsigned int nr_entries)
{
	unsigned long begin = offset;
	unsigned long end = offset + nr_entries - 1;
	void (*swap_slot_free_notify)(struct block_device *, unsigned long);

	if (offset < si->lowest_bit)
		si->lowest_bit = offset;
	if (end > si->highest_bit) {
		bool was_full = !si->highest_bit;

		WRITE_ONCE(si->highest_bit, end);
		if (was_full && (si->flags & SWP_WRITEOK))
			add_to_avail_list(si);
	}
	atomic_long_add(nr_entries, &nr_swap_pages);
	WRITE_ONCE(si->inuse_pages, si->inuse_pages - nr_entries);
	if (si->flags & SWP_BLKDEV)
		swap_slot_free_notify =
			si->bdev->bd_disk->fops->swap_slot_free_notify;
	else
		swap_slot_free_notify = NULL;
	while (offset <= end) {
		arch_swap_invalidate_page(si->type, offset);
		zswap_invalidate(si->type, offset);
		if (swap_slot_free_notify)
			swap_slot_free_notify(si->bdev, offset);
		offset++;
	}
	clear_shadow_from_swap_cache(si->type, begin, end);
}

static void set_cluster_next(struct swap_info_struct *si, unsigned long next)
{
	unsigned long prev;

	if (!(si->flags & SWP_SOLIDSTATE)) {
		si->cluster_next = next;
		return;
	}

	prev = this_cpu_read(*si->cluster_next_cpu);
	 
	if ((prev >> SWAP_ADDRESS_SPACE_SHIFT) !=
	    (next >> SWAP_ADDRESS_SPACE_SHIFT)) {
		 
		if (si->highest_bit <= si->lowest_bit)
			return;
		next = get_random_u32_inclusive(si->lowest_bit, si->highest_bit);
		next = ALIGN_DOWN(next, SWAP_ADDRESS_SPACE_PAGES);
		next = max_t(unsigned int, next, si->lowest_bit);
	}
	this_cpu_write(*si->cluster_next_cpu, next);
}

static bool swap_offset_available_and_locked(struct swap_info_struct *si,
					     unsigned long offset)
{
	if (data_race(!si->swap_map[offset])) {
		spin_lock(&si->lock);
		return true;
	}

	if (vm_swap_full() && READ_ONCE(si->swap_map[offset]) == SWAP_HAS_CACHE) {
		spin_lock(&si->lock);
		return true;
	}

	return false;
}

static int scan_swap_map_slots(struct swap_info_struct *si,
			       unsigned char usage, int nr,
			       swp_entry_t slots[])
{
	struct swap_cluster_info *ci;
	unsigned long offset;
	unsigned long scan_base;
	unsigned long last_in_cluster = 0;
	int latency_ration = LATENCY_LIMIT;
	int n_ret = 0;
	bool scanned_many = false;

	 

	si->flags += SWP_SCANNING;
	 
	if (si->flags & SWP_SOLIDSTATE)
		scan_base = this_cpu_read(*si->cluster_next_cpu);
	else
		scan_base = si->cluster_next;
	offset = scan_base;

	 
	if (si->cluster_info) {
		if (!scan_swap_map_try_ssd_cluster(si, &offset, &scan_base))
			goto scan;
	} else if (unlikely(!si->cluster_nr--)) {
		if (si->pages - si->inuse_pages < SWAPFILE_CLUSTER) {
			si->cluster_nr = SWAPFILE_CLUSTER - 1;
			goto checks;
		}

		spin_unlock(&si->lock);

		 
		scan_base = offset = si->lowest_bit;
		last_in_cluster = offset + SWAPFILE_CLUSTER - 1;

		 
		for (; last_in_cluster <= si->highest_bit; offset++) {
			if (si->swap_map[offset])
				last_in_cluster = offset + SWAPFILE_CLUSTER;
			else if (offset == last_in_cluster) {
				spin_lock(&si->lock);
				offset -= SWAPFILE_CLUSTER - 1;
				si->cluster_next = offset;
				si->cluster_nr = SWAPFILE_CLUSTER - 1;
				goto checks;
			}
			if (unlikely(--latency_ration < 0)) {
				cond_resched();
				latency_ration = LATENCY_LIMIT;
			}
		}

		offset = scan_base;
		spin_lock(&si->lock);
		si->cluster_nr = SWAPFILE_CLUSTER - 1;
	}

checks:
	if (si->cluster_info) {
		while (scan_swap_map_ssd_cluster_conflict(si, offset)) {
		 
			if (n_ret)
				goto done;
			if (!scan_swap_map_try_ssd_cluster(si, &offset,
							&scan_base))
				goto scan;
		}
	}
	if (!(si->flags & SWP_WRITEOK))
		goto no_page;
	if (!si->highest_bit)
		goto no_page;
	if (offset > si->highest_bit)
		scan_base = offset = si->lowest_bit;

	ci = lock_cluster(si, offset);
	 
	if (vm_swap_full() && si->swap_map[offset] == SWAP_HAS_CACHE) {
		int swap_was_freed;
		unlock_cluster(ci);
		spin_unlock(&si->lock);
		swap_was_freed = __try_to_reclaim_swap(si, offset, TTRS_ANYWAY);
		spin_lock(&si->lock);
		 
		if (swap_was_freed)
			goto checks;
		goto scan;  
	}

	if (si->swap_map[offset]) {
		unlock_cluster(ci);
		if (!n_ret)
			goto scan;
		else
			goto done;
	}
	WRITE_ONCE(si->swap_map[offset], usage);
	inc_cluster_info_page(si, si->cluster_info, offset);
	unlock_cluster(ci);

	swap_range_alloc(si, offset, 1);
	slots[n_ret++] = swp_entry(si->type, offset);

	 
	if ((n_ret == nr) || (offset >= si->highest_bit))
		goto done;

	 

	 
	if (unlikely(--latency_ration < 0)) {
		if (n_ret)
			goto done;
		spin_unlock(&si->lock);
		cond_resched();
		spin_lock(&si->lock);
		latency_ration = LATENCY_LIMIT;
	}

	 
	if (si->cluster_info) {
		if (scan_swap_map_try_ssd_cluster(si, &offset, &scan_base))
			goto checks;
	} else if (si->cluster_nr && !si->swap_map[++offset]) {
		 
		--si->cluster_nr;
		goto checks;
	}

	 
	if (!scanned_many) {
		unsigned long scan_limit;

		if (offset < scan_base)
			scan_limit = scan_base;
		else
			scan_limit = si->highest_bit;
		for (; offset <= scan_limit && --latency_ration > 0;
		     offset++) {
			if (!si->swap_map[offset])
				goto checks;
		}
	}

done:
	set_cluster_next(si, offset + 1);
	si->flags -= SWP_SCANNING;
	return n_ret;

scan:
	spin_unlock(&si->lock);
	while (++offset <= READ_ONCE(si->highest_bit)) {
		if (unlikely(--latency_ration < 0)) {
			cond_resched();
			latency_ration = LATENCY_LIMIT;
			scanned_many = true;
		}
		if (swap_offset_available_and_locked(si, offset))
			goto checks;
	}
	offset = si->lowest_bit;
	while (offset < scan_base) {
		if (unlikely(--latency_ration < 0)) {
			cond_resched();
			latency_ration = LATENCY_LIMIT;
			scanned_many = true;
		}
		if (swap_offset_available_and_locked(si, offset))
			goto checks;
		offset++;
	}
	spin_lock(&si->lock);

no_page:
	si->flags -= SWP_SCANNING;
	return n_ret;
}

static int swap_alloc_cluster(struct swap_info_struct *si, swp_entry_t *slot)
{
	unsigned long idx;
	struct swap_cluster_info *ci;
	unsigned long offset;

	 
	if (!IS_ENABLED(CONFIG_THP_SWAP)) {
		VM_WARN_ON_ONCE(1);
		return 0;
	}

	if (cluster_list_empty(&si->free_clusters))
		return 0;

	idx = cluster_list_first(&si->free_clusters);
	offset = idx * SWAPFILE_CLUSTER;
	ci = lock_cluster(si, offset);
	alloc_cluster(si, idx);
	cluster_set_count_flag(ci, SWAPFILE_CLUSTER, CLUSTER_FLAG_HUGE);

	memset(si->swap_map + offset, SWAP_HAS_CACHE, SWAPFILE_CLUSTER);
	unlock_cluster(ci);
	swap_range_alloc(si, offset, SWAPFILE_CLUSTER);
	*slot = swp_entry(si->type, offset);

	return 1;
}

static void swap_free_cluster(struct swap_info_struct *si, unsigned long idx)
{
	unsigned long offset = idx * SWAPFILE_CLUSTER;
	struct swap_cluster_info *ci;

	ci = lock_cluster(si, offset);
	memset(si->swap_map + offset, 0, SWAPFILE_CLUSTER);
	cluster_set_count_flag(ci, 0, 0);
	free_cluster(si, idx);
	unlock_cluster(ci);
	swap_range_free(si, offset, SWAPFILE_CLUSTER);
}

int get_swap_pages(int n_goal, swp_entry_t swp_entries[], int entry_size)
{
	unsigned long size = swap_entry_size(entry_size);
	struct swap_info_struct *si, *next;
	long avail_pgs;
	int n_ret = 0;
	int node;

	 
	WARN_ON_ONCE(n_goal > 1 && size == SWAPFILE_CLUSTER);

	spin_lock(&swap_avail_lock);

	avail_pgs = atomic_long_read(&nr_swap_pages) / size;
	if (avail_pgs <= 0) {
		spin_unlock(&swap_avail_lock);
		goto noswap;
	}

	n_goal = min3((long)n_goal, (long)SWAP_BATCH, avail_pgs);

	atomic_long_sub(n_goal * size, &nr_swap_pages);

start_over:
	node = numa_node_id();
	plist_for_each_entry_safe(si, next, &swap_avail_heads[node], avail_lists[node]) {
		 
		plist_requeue(&si->avail_lists[node], &swap_avail_heads[node]);
		spin_unlock(&swap_avail_lock);
		spin_lock(&si->lock);
		if (!si->highest_bit || !(si->flags & SWP_WRITEOK)) {
			spin_lock(&swap_avail_lock);
			if (plist_node_empty(&si->avail_lists[node])) {
				spin_unlock(&si->lock);
				goto nextsi;
			}
			WARN(!si->highest_bit,
			     "swap_info %d in list but !highest_bit\n",
			     si->type);
			WARN(!(si->flags & SWP_WRITEOK),
			     "swap_info %d in list but !SWP_WRITEOK\n",
			     si->type);
			__del_from_avail_list(si);
			spin_unlock(&si->lock);
			goto nextsi;
		}
		if (size == SWAPFILE_CLUSTER) {
			if (si->flags & SWP_BLKDEV)
				n_ret = swap_alloc_cluster(si, swp_entries);
		} else
			n_ret = scan_swap_map_slots(si, SWAP_HAS_CACHE,
						    n_goal, swp_entries);
		spin_unlock(&si->lock);
		if (n_ret || size == SWAPFILE_CLUSTER)
			goto check_out;
		cond_resched();

		spin_lock(&swap_avail_lock);
nextsi:
		 
		if (plist_node_empty(&next->avail_lists[node]))
			goto start_over;
	}

	spin_unlock(&swap_avail_lock);

check_out:
	if (n_ret < n_goal)
		atomic_long_add((long)(n_goal - n_ret) * size,
				&nr_swap_pages);
noswap:
	return n_ret;
}

static struct swap_info_struct *_swap_info_get(swp_entry_t entry)
{
	struct swap_info_struct *p;
	unsigned long offset;

	if (!entry.val)
		goto out;
	p = swp_swap_info(entry);
	if (!p)
		goto bad_nofile;
	if (data_race(!(p->flags & SWP_USED)))
		goto bad_device;
	offset = swp_offset(entry);
	if (offset >= p->max)
		goto bad_offset;
	if (data_race(!p->swap_map[swp_offset(entry)]))
		goto bad_free;
	return p;

bad_free:
	pr_err("%s: %s%08lx\n", __func__, Unused_offset, entry.val);
	goto out;
bad_offset:
	pr_err("%s: %s%08lx\n", __func__, Bad_offset, entry.val);
	goto out;
bad_device:
	pr_err("%s: %s%08lx\n", __func__, Unused_file, entry.val);
	goto out;
bad_nofile:
	pr_err("%s: %s%08lx\n", __func__, Bad_file, entry.val);
out:
	return NULL;
}

static struct swap_info_struct *swap_info_get_cont(swp_entry_t entry,
					struct swap_info_struct *q)
{
	struct swap_info_struct *p;

	p = _swap_info_get(entry);

	if (p != q) {
		if (q != NULL)
			spin_unlock(&q->lock);
		if (p != NULL)
			spin_lock(&p->lock);
	}
	return p;
}

static unsigned char __swap_entry_free_locked(struct swap_info_struct *p,
					      unsigned long offset,
					      unsigned char usage)
{
	unsigned char count;
	unsigned char has_cache;

	count = p->swap_map[offset];

	has_cache = count & SWAP_HAS_CACHE;
	count &= ~SWAP_HAS_CACHE;

	if (usage == SWAP_HAS_CACHE) {
		VM_BUG_ON(!has_cache);
		has_cache = 0;
	} else if (count == SWAP_MAP_SHMEM) {
		 
		count = 0;
	} else if ((count & ~COUNT_CONTINUED) <= SWAP_MAP_MAX) {
		if (count == COUNT_CONTINUED) {
			if (swap_count_continued(p, offset, count))
				count = SWAP_MAP_MAX | COUNT_CONTINUED;
			else
				count = SWAP_MAP_MAX;
		} else
			count--;
	}

	usage = count | has_cache;
	if (usage)
		WRITE_ONCE(p->swap_map[offset], usage);
	else
		WRITE_ONCE(p->swap_map[offset], SWAP_HAS_CACHE);

	return usage;
}

 
struct swap_info_struct *get_swap_device(swp_entry_t entry)
{
	struct swap_info_struct *si;
	unsigned long offset;

	if (!entry.val)
		goto out;
	si = swp_swap_info(entry);
	if (!si)
		goto bad_nofile;
	if (!percpu_ref_tryget_live(&si->users))
		goto out;
	 
	smp_rmb();
	offset = swp_offset(entry);
	if (offset >= si->max)
		goto put_out;

	return si;
bad_nofile:
	pr_err("%s: %s%08lx\n", __func__, Bad_file, entry.val);
out:
	return NULL;
put_out:
	pr_err("%s: %s%08lx\n", __func__, Bad_offset, entry.val);
	percpu_ref_put(&si->users);
	return NULL;
}

static unsigned char __swap_entry_free(struct swap_info_struct *p,
				       swp_entry_t entry)
{
	struct swap_cluster_info *ci;
	unsigned long offset = swp_offset(entry);
	unsigned char usage;

	ci = lock_cluster_or_swap_info(p, offset);
	usage = __swap_entry_free_locked(p, offset, 1);
	unlock_cluster_or_swap_info(p, ci);
	if (!usage)
		free_swap_slot(entry);

	return usage;
}

static void swap_entry_free(struct swap_info_struct *p, swp_entry_t entry)
{
	struct swap_cluster_info *ci;
	unsigned long offset = swp_offset(entry);
	unsigned char count;

	ci = lock_cluster(p, offset);
	count = p->swap_map[offset];
	VM_BUG_ON(count != SWAP_HAS_CACHE);
	p->swap_map[offset] = 0;
	dec_cluster_info_page(p, p->cluster_info, offset);
	unlock_cluster(ci);

	mem_cgroup_uncharge_swap(entry, 1);
	swap_range_free(p, offset, 1);
}

 
void swap_free(swp_entry_t entry)
{
	struct swap_info_struct *p;

	p = _swap_info_get(entry);
	if (p)
		__swap_entry_free(p, entry);
}

 
void put_swap_folio(struct folio *folio, swp_entry_t entry)
{
	unsigned long offset = swp_offset(entry);
	unsigned long idx = offset / SWAPFILE_CLUSTER;
	struct swap_cluster_info *ci;
	struct swap_info_struct *si;
	unsigned char *map;
	unsigned int i, free_entries = 0;
	unsigned char val;
	int size = swap_entry_size(folio_nr_pages(folio));

	si = _swap_info_get(entry);
	if (!si)
		return;

	ci = lock_cluster_or_swap_info(si, offset);
	if (size == SWAPFILE_CLUSTER) {
		VM_BUG_ON(!cluster_is_huge(ci));
		map = si->swap_map + offset;
		for (i = 0; i < SWAPFILE_CLUSTER; i++) {
			val = map[i];
			VM_BUG_ON(!(val & SWAP_HAS_CACHE));
			if (val == SWAP_HAS_CACHE)
				free_entries++;
		}
		cluster_clear_huge(ci);
		if (free_entries == SWAPFILE_CLUSTER) {
			unlock_cluster_or_swap_info(si, ci);
			spin_lock(&si->lock);
			mem_cgroup_uncharge_swap(entry, SWAPFILE_CLUSTER);
			swap_free_cluster(si, idx);
			spin_unlock(&si->lock);
			return;
		}
	}
	for (i = 0; i < size; i++, entry.val++) {
		if (!__swap_entry_free_locked(si, offset + i, SWAP_HAS_CACHE)) {
			unlock_cluster_or_swap_info(si, ci);
			free_swap_slot(entry);
			if (i == size - 1)
				return;
			lock_cluster_or_swap_info(si, offset);
		}
	}
	unlock_cluster_or_swap_info(si, ci);
}

#ifdef CONFIG_THP_SWAP
int split_swap_cluster(swp_entry_t entry)
{
	struct swap_info_struct *si;
	struct swap_cluster_info *ci;
	unsigned long offset = swp_offset(entry);

	si = _swap_info_get(entry);
	if (!si)
		return -EBUSY;
	ci = lock_cluster(si, offset);
	cluster_clear_huge(ci);
	unlock_cluster(ci);
	return 0;
}
#endif

static int swp_entry_cmp(const void *ent1, const void *ent2)
{
	const swp_entry_t *e1 = ent1, *e2 = ent2;

	return (int)swp_type(*e1) - (int)swp_type(*e2);
}

void swapcache_free_entries(swp_entry_t *entries, int n)
{
	struct swap_info_struct *p, *prev;
	int i;

	if (n <= 0)
		return;

	prev = NULL;
	p = NULL;

	 
	if (nr_swapfiles > 1)
		sort(entries, n, sizeof(entries[0]), swp_entry_cmp, NULL);
	for (i = 0; i < n; ++i) {
		p = swap_info_get_cont(entries[i], prev);
		if (p)
			swap_entry_free(p, entries[i]);
		prev = p;
	}
	if (p)
		spin_unlock(&p->lock);
}

int __swap_count(swp_entry_t entry)
{
	struct swap_info_struct *si = swp_swap_info(entry);
	pgoff_t offset = swp_offset(entry);

	return swap_count(si->swap_map[offset]);
}

 
int swap_swapcount(struct swap_info_struct *si, swp_entry_t entry)
{
	pgoff_t offset = swp_offset(entry);
	struct swap_cluster_info *ci;
	int count;

	ci = lock_cluster_or_swap_info(si, offset);
	count = swap_count(si->swap_map[offset]);
	unlock_cluster_or_swap_info(si, ci);
	return count;
}

 
int swp_swapcount(swp_entry_t entry)
{
	int count, tmp_count, n;
	struct swap_info_struct *p;
	struct swap_cluster_info *ci;
	struct page *page;
	pgoff_t offset;
	unsigned char *map;

	p = _swap_info_get(entry);
	if (!p)
		return 0;

	offset = swp_offset(entry);

	ci = lock_cluster_or_swap_info(p, offset);

	count = swap_count(p->swap_map[offset]);
	if (!(count & COUNT_CONTINUED))
		goto out;

	count &= ~COUNT_CONTINUED;
	n = SWAP_MAP_MAX + 1;

	page = vmalloc_to_page(p->swap_map + offset);
	offset &= ~PAGE_MASK;
	VM_BUG_ON(page_private(page) != SWP_CONTINUED);

	do {
		page = list_next_entry(page, lru);
		map = kmap_atomic(page);
		tmp_count = map[offset];
		kunmap_atomic(map);

		count += (tmp_count & ~COUNT_CONTINUED) * n;
		n *= (SWAP_CONT_MAX + 1);
	} while (tmp_count & COUNT_CONTINUED);
out:
	unlock_cluster_or_swap_info(p, ci);
	return count;
}

static bool swap_page_trans_huge_swapped(struct swap_info_struct *si,
					 swp_entry_t entry)
{
	struct swap_cluster_info *ci;
	unsigned char *map = si->swap_map;
	unsigned long roffset = swp_offset(entry);
	unsigned long offset = round_down(roffset, SWAPFILE_CLUSTER);
	int i;
	bool ret = false;

	ci = lock_cluster_or_swap_info(si, offset);
	if (!ci || !cluster_is_huge(ci)) {
		if (swap_count(map[roffset]))
			ret = true;
		goto unlock_out;
	}
	for (i = 0; i < SWAPFILE_CLUSTER; i++) {
		if (swap_count(map[offset + i])) {
			ret = true;
			break;
		}
	}
unlock_out:
	unlock_cluster_or_swap_info(si, ci);
	return ret;
}

static bool folio_swapped(struct folio *folio)
{
	swp_entry_t entry = folio->swap;
	struct swap_info_struct *si = _swap_info_get(entry);

	if (!si)
		return false;

	if (!IS_ENABLED(CONFIG_THP_SWAP) || likely(!folio_test_large(folio)))
		return swap_swapcount(si, entry) != 0;

	return swap_page_trans_huge_swapped(si, entry);
}

 
bool folio_free_swap(struct folio *folio)
{
	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	if (!folio_test_swapcache(folio))
		return false;
	if (folio_test_writeback(folio))
		return false;
	if (folio_swapped(folio))
		return false;

	 
	if (pm_suspended_storage())
		return false;

	delete_from_swap_cache(folio);
	folio_set_dirty(folio);
	return true;
}

 
int free_swap_and_cache(swp_entry_t entry)
{
	struct swap_info_struct *p;
	unsigned char count;

	if (non_swap_entry(entry))
		return 1;

	p = _swap_info_get(entry);
	if (p) {
		count = __swap_entry_free(p, entry);
		if (count == SWAP_HAS_CACHE &&
		    !swap_page_trans_huge_swapped(p, entry))
			__try_to_reclaim_swap(p, swp_offset(entry),
					      TTRS_UNMAPPED | TTRS_FULL);
	}
	return p != NULL;
}

#ifdef CONFIG_HIBERNATION

swp_entry_t get_swap_page_of_type(int type)
{
	struct swap_info_struct *si = swap_type_to_swap_info(type);
	swp_entry_t entry = {0};

	if (!si)
		goto fail;

	 
	spin_lock(&si->lock);
	if ((si->flags & SWP_WRITEOK) && scan_swap_map_slots(si, 1, 1, &entry))
		atomic_long_dec(&nr_swap_pages);
	spin_unlock(&si->lock);
fail:
	return entry;
}

 
int swap_type_of(dev_t device, sector_t offset)
{
	int type;

	if (!device)
		return -1;

	spin_lock(&swap_lock);
	for (type = 0; type < nr_swapfiles; type++) {
		struct swap_info_struct *sis = swap_info[type];

		if (!(sis->flags & SWP_WRITEOK))
			continue;

		if (device == sis->bdev->bd_dev) {
			struct swap_extent *se = first_se(sis);

			if (se->start_block == offset) {
				spin_unlock(&swap_lock);
				return type;
			}
		}
	}
	spin_unlock(&swap_lock);
	return -ENODEV;
}

int find_first_swap(dev_t *device)
{
	int type;

	spin_lock(&swap_lock);
	for (type = 0; type < nr_swapfiles; type++) {
		struct swap_info_struct *sis = swap_info[type];

		if (!(sis->flags & SWP_WRITEOK))
			continue;
		*device = sis->bdev->bd_dev;
		spin_unlock(&swap_lock);
		return type;
	}
	spin_unlock(&swap_lock);
	return -ENODEV;
}

 
sector_t swapdev_block(int type, pgoff_t offset)
{
	struct swap_info_struct *si = swap_type_to_swap_info(type);
	struct swap_extent *se;

	if (!si || !(si->flags & SWP_WRITEOK))
		return 0;
	se = offset_to_swap_extent(si, offset);
	return se->start_block + (offset - se->start_page);
}

 
unsigned int count_swap_pages(int type, int free)
{
	unsigned int n = 0;

	spin_lock(&swap_lock);
	if ((unsigned int)type < nr_swapfiles) {
		struct swap_info_struct *sis = swap_info[type];

		spin_lock(&sis->lock);
		if (sis->flags & SWP_WRITEOK) {
			n = sis->pages;
			if (free)
				n -= sis->inuse_pages;
		}
		spin_unlock(&sis->lock);
	}
	spin_unlock(&swap_lock);
	return n;
}
#endif  

static inline int pte_same_as_swp(pte_t pte, pte_t swp_pte)
{
	return pte_same(pte_swp_clear_flags(pte), swp_pte);
}

 
static int unuse_pte(struct vm_area_struct *vma, pmd_t *pmd,
		unsigned long addr, swp_entry_t entry, struct folio *folio)
{
	struct page *page = folio_file_page(folio, swp_offset(entry));
	struct page *swapcache;
	spinlock_t *ptl;
	pte_t *pte, new_pte, old_pte;
	bool hwpoisoned = PageHWPoison(page);
	int ret = 1;

	swapcache = page;
	page = ksm_might_need_to_copy(page, vma, addr);
	if (unlikely(!page))
		return -ENOMEM;
	else if (unlikely(PTR_ERR(page) == -EHWPOISON))
		hwpoisoned = true;

	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	if (unlikely(!pte || !pte_same_as_swp(ptep_get(pte),
						swp_entry_to_pte(entry)))) {
		ret = 0;
		goto out;
	}

	old_pte = ptep_get(pte);

	if (unlikely(hwpoisoned || !PageUptodate(page))) {
		swp_entry_t swp_entry;

		dec_mm_counter(vma->vm_mm, MM_SWAPENTS);
		if (hwpoisoned) {
			swp_entry = make_hwpoison_entry(swapcache);
			page = swapcache;
		} else {
			swp_entry = make_poisoned_swp_entry();
		}
		new_pte = swp_entry_to_pte(swp_entry);
		ret = 0;
		goto setpte;
	}

	 
	arch_swap_restore(entry, page_folio(page));

	 
	BUG_ON(!PageAnon(page) && PageMappedToDisk(page));
	BUG_ON(PageAnon(page) && PageAnonExclusive(page));

	dec_mm_counter(vma->vm_mm, MM_SWAPENTS);
	inc_mm_counter(vma->vm_mm, MM_ANONPAGES);
	get_page(page);
	if (page == swapcache) {
		rmap_t rmap_flags = RMAP_NONE;

		 
		VM_BUG_ON_PAGE(PageWriteback(page), page);
		if (pte_swp_exclusive(old_pte))
			rmap_flags |= RMAP_EXCLUSIVE;

		page_add_anon_rmap(page, vma, addr, rmap_flags);
	} else {  
		page_add_new_anon_rmap(page, vma, addr);
		lru_cache_add_inactive_or_unevictable(page, vma);
	}
	new_pte = pte_mkold(mk_pte(page, vma->vm_page_prot));
	if (pte_swp_soft_dirty(old_pte))
		new_pte = pte_mksoft_dirty(new_pte);
	if (pte_swp_uffd_wp(old_pte))
		new_pte = pte_mkuffd_wp(new_pte);
setpte:
	set_pte_at(vma->vm_mm, addr, pte, new_pte);
	swap_free(entry);
out:
	if (pte)
		pte_unmap_unlock(pte, ptl);
	if (page != swapcache) {
		unlock_page(page);
		put_page(page);
	}
	return ret;
}

static int unuse_pte_range(struct vm_area_struct *vma, pmd_t *pmd,
			unsigned long addr, unsigned long end,
			unsigned int type)
{
	pte_t *pte = NULL;
	struct swap_info_struct *si;

	si = swap_info[type];
	do {
		struct folio *folio;
		unsigned long offset;
		unsigned char swp_count;
		swp_entry_t entry;
		int ret;
		pte_t ptent;

		if (!pte++) {
			pte = pte_offset_map(pmd, addr);
			if (!pte)
				break;
		}

		ptent = ptep_get_lockless(pte);

		if (!is_swap_pte(ptent))
			continue;

		entry = pte_to_swp_entry(ptent);
		if (swp_type(entry) != type)
			continue;

		offset = swp_offset(entry);
		pte_unmap(pte);
		pte = NULL;

		folio = swap_cache_get_folio(entry, vma, addr);
		if (!folio) {
			struct page *page;
			struct vm_fault vmf = {
				.vma = vma,
				.address = addr,
				.real_address = addr,
				.pmd = pmd,
			};

			page = swapin_readahead(entry, GFP_HIGHUSER_MOVABLE,
						&vmf);
			if (page)
				folio = page_folio(page);
		}
		if (!folio) {
			swp_count = READ_ONCE(si->swap_map[offset]);
			if (swp_count == 0 || swp_count == SWAP_MAP_BAD)
				continue;
			return -ENOMEM;
		}

		folio_lock(folio);
		folio_wait_writeback(folio);
		ret = unuse_pte(vma, pmd, addr, entry, folio);
		if (ret < 0) {
			folio_unlock(folio);
			folio_put(folio);
			return ret;
		}

		folio_free_swap(folio);
		folio_unlock(folio);
		folio_put(folio);
	} while (addr += PAGE_SIZE, addr != end);

	if (pte)
		pte_unmap(pte);
	return 0;
}

static inline int unuse_pmd_range(struct vm_area_struct *vma, pud_t *pud,
				unsigned long addr, unsigned long end,
				unsigned int type)
{
	pmd_t *pmd;
	unsigned long next;
	int ret;

	pmd = pmd_offset(pud, addr);
	do {
		cond_resched();
		next = pmd_addr_end(addr, end);
		ret = unuse_pte_range(vma, pmd, addr, next, type);
		if (ret)
			return ret;
	} while (pmd++, addr = next, addr != end);
	return 0;
}

static inline int unuse_pud_range(struct vm_area_struct *vma, p4d_t *p4d,
				unsigned long addr, unsigned long end,
				unsigned int type)
{
	pud_t *pud;
	unsigned long next;
	int ret;

	pud = pud_offset(p4d, addr);
	do {
		next = pud_addr_end(addr, end);
		if (pud_none_or_clear_bad(pud))
			continue;
		ret = unuse_pmd_range(vma, pud, addr, next, type);
		if (ret)
			return ret;
	} while (pud++, addr = next, addr != end);
	return 0;
}

static inline int unuse_p4d_range(struct vm_area_struct *vma, pgd_t *pgd,
				unsigned long addr, unsigned long end,
				unsigned int type)
{
	p4d_t *p4d;
	unsigned long next;
	int ret;

	p4d = p4d_offset(pgd, addr);
	do {
		next = p4d_addr_end(addr, end);
		if (p4d_none_or_clear_bad(p4d))
			continue;
		ret = unuse_pud_range(vma, p4d, addr, next, type);
		if (ret)
			return ret;
	} while (p4d++, addr = next, addr != end);
	return 0;
}

static int unuse_vma(struct vm_area_struct *vma, unsigned int type)
{
	pgd_t *pgd;
	unsigned long addr, end, next;
	int ret;

	addr = vma->vm_start;
	end = vma->vm_end;

	pgd = pgd_offset(vma->vm_mm, addr);
	do {
		next = pgd_addr_end(addr, end);
		if (pgd_none_or_clear_bad(pgd))
			continue;
		ret = unuse_p4d_range(vma, pgd, addr, next, type);
		if (ret)
			return ret;
	} while (pgd++, addr = next, addr != end);
	return 0;
}

static int unuse_mm(struct mm_struct *mm, unsigned int type)
{
	struct vm_area_struct *vma;
	int ret = 0;
	VMA_ITERATOR(vmi, mm, 0);

	mmap_read_lock(mm);
	for_each_vma(vmi, vma) {
		if (vma->anon_vma) {
			ret = unuse_vma(vma, type);
			if (ret)
				break;
		}

		cond_resched();
	}
	mmap_read_unlock(mm);
	return ret;
}

 
static unsigned int find_next_to_unuse(struct swap_info_struct *si,
					unsigned int prev)
{
	unsigned int i;
	unsigned char count;

	 
	for (i = prev + 1; i < si->max; i++) {
		count = READ_ONCE(si->swap_map[i]);
		if (count && swap_count(count) != SWAP_MAP_BAD)
			break;
		if ((i % LATENCY_LIMIT) == 0)
			cond_resched();
	}

	if (i == si->max)
		i = 0;

	return i;
}

static int try_to_unuse(unsigned int type)
{
	struct mm_struct *prev_mm;
	struct mm_struct *mm;
	struct list_head *p;
	int retval = 0;
	struct swap_info_struct *si = swap_info[type];
	struct folio *folio;
	swp_entry_t entry;
	unsigned int i;

	if (!READ_ONCE(si->inuse_pages))
		return 0;

retry:
	retval = shmem_unuse(type);
	if (retval)
		return retval;

	prev_mm = &init_mm;
	mmget(prev_mm);

	spin_lock(&mmlist_lock);
	p = &init_mm.mmlist;
	while (READ_ONCE(si->inuse_pages) &&
	       !signal_pending(current) &&
	       (p = p->next) != &init_mm.mmlist) {

		mm = list_entry(p, struct mm_struct, mmlist);
		if (!mmget_not_zero(mm))
			continue;
		spin_unlock(&mmlist_lock);
		mmput(prev_mm);
		prev_mm = mm;
		retval = unuse_mm(mm, type);
		if (retval) {
			mmput(prev_mm);
			return retval;
		}

		 
		cond_resched();
		spin_lock(&mmlist_lock);
	}
	spin_unlock(&mmlist_lock);

	mmput(prev_mm);

	i = 0;
	while (READ_ONCE(si->inuse_pages) &&
	       !signal_pending(current) &&
	       (i = find_next_to_unuse(si, i)) != 0) {

		entry = swp_entry(type, i);
		folio = filemap_get_folio(swap_address_space(entry), i);
		if (IS_ERR(folio))
			continue;

		 
		folio_lock(folio);
		folio_wait_writeback(folio);
		folio_free_swap(folio);
		folio_unlock(folio);
		folio_put(folio);
	}

	 
	if (READ_ONCE(si->inuse_pages)) {
		if (!signal_pending(current))
			goto retry;
		return -EINTR;
	}

	return 0;
}

 
static void drain_mmlist(void)
{
	struct list_head *p, *next;
	unsigned int type;

	for (type = 0; type < nr_swapfiles; type++)
		if (swap_info[type]->inuse_pages)
			return;
	spin_lock(&mmlist_lock);
	list_for_each_safe(p, next, &init_mm.mmlist)
		list_del_init(p);
	spin_unlock(&mmlist_lock);
}

 
static void destroy_swap_extents(struct swap_info_struct *sis)
{
	while (!RB_EMPTY_ROOT(&sis->swap_extent_root)) {
		struct rb_node *rb = sis->swap_extent_root.rb_node;
		struct swap_extent *se = rb_entry(rb, struct swap_extent, rb_node);

		rb_erase(rb, &sis->swap_extent_root);
		kfree(se);
	}

	if (sis->flags & SWP_ACTIVATED) {
		struct file *swap_file = sis->swap_file;
		struct address_space *mapping = swap_file->f_mapping;

		sis->flags &= ~SWP_ACTIVATED;
		if (mapping->a_ops->swap_deactivate)
			mapping->a_ops->swap_deactivate(swap_file);
	}
}

 
int
add_swap_extent(struct swap_info_struct *sis, unsigned long start_page,
		unsigned long nr_pages, sector_t start_block)
{
	struct rb_node **link = &sis->swap_extent_root.rb_node, *parent = NULL;
	struct swap_extent *se;
	struct swap_extent *new_se;

	 
	while (*link) {
		parent = *link;
		link = &parent->rb_right;
	}

	if (parent) {
		se = rb_entry(parent, struct swap_extent, rb_node);
		BUG_ON(se->start_page + se->nr_pages != start_page);
		if (se->start_block + se->nr_pages == start_block) {
			 
			se->nr_pages += nr_pages;
			return 0;
		}
	}

	 
	new_se = kmalloc(sizeof(*se), GFP_KERNEL);
	if (new_se == NULL)
		return -ENOMEM;
	new_se->start_page = start_page;
	new_se->nr_pages = nr_pages;
	new_se->start_block = start_block;

	rb_link_node(&new_se->rb_node, parent, link);
	rb_insert_color(&new_se->rb_node, &sis->swap_extent_root);
	return 1;
}
EXPORT_SYMBOL_GPL(add_swap_extent);

 
static int setup_swap_extents(struct swap_info_struct *sis, sector_t *span)
{
	struct file *swap_file = sis->swap_file;
	struct address_space *mapping = swap_file->f_mapping;
	struct inode *inode = mapping->host;
	int ret;

	if (S_ISBLK(inode->i_mode)) {
		ret = add_swap_extent(sis, 0, sis->max, 0);
		*span = sis->pages;
		return ret;
	}

	if (mapping->a_ops->swap_activate) {
		ret = mapping->a_ops->swap_activate(sis, swap_file, span);
		if (ret < 0)
			return ret;
		sis->flags |= SWP_ACTIVATED;
		if ((sis->flags & SWP_FS_OPS) &&
		    sio_pool_init() != 0) {
			destroy_swap_extents(sis);
			return -ENOMEM;
		}
		return ret;
	}

	return generic_swapfile_activate(sis, swap_file, span);
}

static int swap_node(struct swap_info_struct *p)
{
	struct block_device *bdev;

	if (p->bdev)
		bdev = p->bdev;
	else
		bdev = p->swap_file->f_inode->i_sb->s_bdev;

	return bdev ? bdev->bd_disk->node_id : NUMA_NO_NODE;
}

static void setup_swap_info(struct swap_info_struct *p, int prio,
			    unsigned char *swap_map,
			    struct swap_cluster_info *cluster_info)
{
	int i;

	if (prio >= 0)
		p->prio = prio;
	else
		p->prio = --least_priority;
	 
	p->list.prio = -p->prio;
	for_each_node(i) {
		if (p->prio >= 0)
			p->avail_lists[i].prio = -p->prio;
		else {
			if (swap_node(p) == i)
				p->avail_lists[i].prio = 1;
			else
				p->avail_lists[i].prio = -p->prio;
		}
	}
	p->swap_map = swap_map;
	p->cluster_info = cluster_info;
}

static void _enable_swap_info(struct swap_info_struct *p)
{
	p->flags |= SWP_WRITEOK;
	atomic_long_add(p->pages, &nr_swap_pages);
	total_swap_pages += p->pages;

	assert_spin_locked(&swap_lock);
	 
	plist_add(&p->list, &swap_active_head);

	 
	if (p->highest_bit)
		add_to_avail_list(p);
}

static void enable_swap_info(struct swap_info_struct *p, int prio,
				unsigned char *swap_map,
				struct swap_cluster_info *cluster_info)
{
	zswap_swapon(p->type);

	spin_lock(&swap_lock);
	spin_lock(&p->lock);
	setup_swap_info(p, prio, swap_map, cluster_info);
	spin_unlock(&p->lock);
	spin_unlock(&swap_lock);
	 
	percpu_ref_resurrect(&p->users);
	spin_lock(&swap_lock);
	spin_lock(&p->lock);
	_enable_swap_info(p);
	spin_unlock(&p->lock);
	spin_unlock(&swap_lock);
}

static void reinsert_swap_info(struct swap_info_struct *p)
{
	spin_lock(&swap_lock);
	spin_lock(&p->lock);
	setup_swap_info(p, p->prio, p->swap_map, p->cluster_info);
	_enable_swap_info(p);
	spin_unlock(&p->lock);
	spin_unlock(&swap_lock);
}

bool has_usable_swap(void)
{
	bool ret = true;

	spin_lock(&swap_lock);
	if (plist_head_empty(&swap_active_head))
		ret = false;
	spin_unlock(&swap_lock);
	return ret;
}

SYSCALL_DEFINE1(swapoff, const char __user *, specialfile)
{
	struct swap_info_struct *p = NULL;
	unsigned char *swap_map;
	struct swap_cluster_info *cluster_info;
	struct file *swap_file, *victim;
	struct address_space *mapping;
	struct inode *inode;
	struct filename *pathname;
	int err, found = 0;
	unsigned int old_block_size;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	BUG_ON(!current->mm);

	pathname = getname(specialfile);
	if (IS_ERR(pathname))
		return PTR_ERR(pathname);

	victim = file_open_name(pathname, O_RDWR|O_LARGEFILE, 0);
	err = PTR_ERR(victim);
	if (IS_ERR(victim))
		goto out;

	mapping = victim->f_mapping;
	spin_lock(&swap_lock);
	plist_for_each_entry(p, &swap_active_head, list) {
		if (p->flags & SWP_WRITEOK) {
			if (p->swap_file->f_mapping == mapping) {
				found = 1;
				break;
			}
		}
	}
	if (!found) {
		err = -EINVAL;
		spin_unlock(&swap_lock);
		goto out_dput;
	}
	if (!security_vm_enough_memory_mm(current->mm, p->pages))
		vm_unacct_memory(p->pages);
	else {
		err = -ENOMEM;
		spin_unlock(&swap_lock);
		goto out_dput;
	}
	spin_lock(&p->lock);
	del_from_avail_list(p);
	if (p->prio < 0) {
		struct swap_info_struct *si = p;
		int nid;

		plist_for_each_entry_continue(si, &swap_active_head, list) {
			si->prio++;
			si->list.prio--;
			for_each_node(nid) {
				if (si->avail_lists[nid].prio != 1)
					si->avail_lists[nid].prio--;
			}
		}
		least_priority++;
	}
	plist_del(&p->list, &swap_active_head);
	atomic_long_sub(p->pages, &nr_swap_pages);
	total_swap_pages -= p->pages;
	p->flags &= ~SWP_WRITEOK;
	spin_unlock(&p->lock);
	spin_unlock(&swap_lock);

	disable_swap_slots_cache_lock();

	set_current_oom_origin();
	err = try_to_unuse(p->type);
	clear_current_oom_origin();

	if (err) {
		 
		reinsert_swap_info(p);
		reenable_swap_slots_cache_unlock();
		goto out_dput;
	}

	reenable_swap_slots_cache_unlock();

	 
	percpu_ref_kill(&p->users);
	synchronize_rcu();
	wait_for_completion(&p->comp);

	flush_work(&p->discard_work);

	destroy_swap_extents(p);
	if (p->flags & SWP_CONTINUED)
		free_swap_count_continuations(p);

	if (!p->bdev || !bdev_nonrot(p->bdev))
		atomic_dec(&nr_rotate_swap);

	mutex_lock(&swapon_mutex);
	spin_lock(&swap_lock);
	spin_lock(&p->lock);
	drain_mmlist();

	 
	p->highest_bit = 0;		 
	while (p->flags >= SWP_SCANNING) {
		spin_unlock(&p->lock);
		spin_unlock(&swap_lock);
		schedule_timeout_uninterruptible(1);
		spin_lock(&swap_lock);
		spin_lock(&p->lock);
	}

	swap_file = p->swap_file;
	old_block_size = p->old_block_size;
	p->swap_file = NULL;
	p->max = 0;
	swap_map = p->swap_map;
	p->swap_map = NULL;
	cluster_info = p->cluster_info;
	p->cluster_info = NULL;
	spin_unlock(&p->lock);
	spin_unlock(&swap_lock);
	arch_swap_invalidate_area(p->type);
	zswap_swapoff(p->type);
	mutex_unlock(&swapon_mutex);
	free_percpu(p->percpu_cluster);
	p->percpu_cluster = NULL;
	free_percpu(p->cluster_next_cpu);
	p->cluster_next_cpu = NULL;
	vfree(swap_map);
	kvfree(cluster_info);
	 
	swap_cgroup_swapoff(p->type);
	exit_swap_address_space(p->type);

	inode = mapping->host;
	if (S_ISBLK(inode->i_mode)) {
		struct block_device *bdev = I_BDEV(inode);

		set_blocksize(bdev, old_block_size);
		blkdev_put(bdev, p);
	}

	inode_lock(inode);
	inode->i_flags &= ~S_SWAPFILE;
	inode_unlock(inode);
	filp_close(swap_file, NULL);

	 
	spin_lock(&swap_lock);
	p->flags = 0;
	spin_unlock(&swap_lock);

	err = 0;
	atomic_inc(&proc_poll_event);
	wake_up_interruptible(&proc_poll_wait);

out_dput:
	filp_close(victim, NULL);
out:
	putname(pathname);
	return err;
}

#ifdef CONFIG_PROC_FS
static __poll_t swaps_poll(struct file *file, poll_table *wait)
{
	struct seq_file *seq = file->private_data;

	poll_wait(file, &proc_poll_wait, wait);

	if (seq->poll_event != atomic_read(&proc_poll_event)) {
		seq->poll_event = atomic_read(&proc_poll_event);
		return EPOLLIN | EPOLLRDNORM | EPOLLERR | EPOLLPRI;
	}

	return EPOLLIN | EPOLLRDNORM;
}

 
static void *swap_start(struct seq_file *swap, loff_t *pos)
{
	struct swap_info_struct *si;
	int type;
	loff_t l = *pos;

	mutex_lock(&swapon_mutex);

	if (!l)
		return SEQ_START_TOKEN;

	for (type = 0; (si = swap_type_to_swap_info(type)); type++) {
		if (!(si->flags & SWP_USED) || !si->swap_map)
			continue;
		if (!--l)
			return si;
	}

	return NULL;
}

static void *swap_next(struct seq_file *swap, void *v, loff_t *pos)
{
	struct swap_info_struct *si = v;
	int type;

	if (v == SEQ_START_TOKEN)
		type = 0;
	else
		type = si->type + 1;

	++(*pos);
	for (; (si = swap_type_to_swap_info(type)); type++) {
		if (!(si->flags & SWP_USED) || !si->swap_map)
			continue;
		return si;
	}

	return NULL;
}

static void swap_stop(struct seq_file *swap, void *v)
{
	mutex_unlock(&swapon_mutex);
}

static int swap_show(struct seq_file *swap, void *v)
{
	struct swap_info_struct *si = v;
	struct file *file;
	int len;
	unsigned long bytes, inuse;

	if (si == SEQ_START_TOKEN) {
		seq_puts(swap, "Filename\t\t\t\tType\t\tSize\t\tUsed\t\tPriority\n");
		return 0;
	}

	bytes = K(si->pages);
	inuse = K(READ_ONCE(si->inuse_pages));

	file = si->swap_file;
	len = seq_file_path(swap, file, " \t\n\\");
	seq_printf(swap, "%*s%s\t%lu\t%s%lu\t%s%d\n",
			len < 40 ? 40 - len : 1, " ",
			S_ISBLK(file_inode(file)->i_mode) ?
				"partition" : "file\t",
			bytes, bytes < 10000000 ? "\t" : "",
			inuse, inuse < 10000000 ? "\t" : "",
			si->prio);
	return 0;
}

static const struct seq_operations swaps_op = {
	.start =	swap_start,
	.next =		swap_next,
	.stop =		swap_stop,
	.show =		swap_show
};

static int swaps_open(struct inode *inode, struct file *file)
{
	struct seq_file *seq;
	int ret;

	ret = seq_open(file, &swaps_op);
	if (ret)
		return ret;

	seq = file->private_data;
	seq->poll_event = atomic_read(&proc_poll_event);
	return 0;
}

static const struct proc_ops swaps_proc_ops = {
	.proc_flags	= PROC_ENTRY_PERMANENT,
	.proc_open	= swaps_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_release	= seq_release,
	.proc_poll	= swaps_poll,
};

static int __init procswaps_init(void)
{
	proc_create("swaps", 0, NULL, &swaps_proc_ops);
	return 0;
}
__initcall(procswaps_init);
#endif  

#ifdef MAX_SWAPFILES_CHECK
static int __init max_swapfiles_check(void)
{
	MAX_SWAPFILES_CHECK();
	return 0;
}
late_initcall(max_swapfiles_check);
#endif

static struct swap_info_struct *alloc_swap_info(void)
{
	struct swap_info_struct *p;
	struct swap_info_struct *defer = NULL;
	unsigned int type;
	int i;

	p = kvzalloc(struct_size(p, avail_lists, nr_node_ids), GFP_KERNEL);
	if (!p)
		return ERR_PTR(-ENOMEM);

	if (percpu_ref_init(&p->users, swap_users_ref_free,
			    PERCPU_REF_INIT_DEAD, GFP_KERNEL)) {
		kvfree(p);
		return ERR_PTR(-ENOMEM);
	}

	spin_lock(&swap_lock);
	for (type = 0; type < nr_swapfiles; type++) {
		if (!(swap_info[type]->flags & SWP_USED))
			break;
	}
	if (type >= MAX_SWAPFILES) {
		spin_unlock(&swap_lock);
		percpu_ref_exit(&p->users);
		kvfree(p);
		return ERR_PTR(-EPERM);
	}
	if (type >= nr_swapfiles) {
		p->type = type;
		 
		smp_store_release(&swap_info[type], p);  
		nr_swapfiles++;
	} else {
		defer = p;
		p = swap_info[type];
		 
	}
	p->swap_extent_root = RB_ROOT;
	plist_node_init(&p->list, 0);
	for_each_node(i)
		plist_node_init(&p->avail_lists[i], 0);
	p->flags = SWP_USED;
	spin_unlock(&swap_lock);
	if (defer) {
		percpu_ref_exit(&defer->users);
		kvfree(defer);
	}
	spin_lock_init(&p->lock);
	spin_lock_init(&p->cont_lock);
	init_completion(&p->comp);

	return p;
}

static int claim_swapfile(struct swap_info_struct *p, struct inode *inode)
{
	int error;

	if (S_ISBLK(inode->i_mode)) {
		p->bdev = blkdev_get_by_dev(inode->i_rdev,
				BLK_OPEN_READ | BLK_OPEN_WRITE, p, NULL);
		if (IS_ERR(p->bdev)) {
			error = PTR_ERR(p->bdev);
			p->bdev = NULL;
			return error;
		}
		p->old_block_size = block_size(p->bdev);
		error = set_blocksize(p->bdev, PAGE_SIZE);
		if (error < 0)
			return error;
		 
		if (bdev_is_zoned(p->bdev))
			return -EINVAL;
		p->flags |= SWP_BLKDEV;
	} else if (S_ISREG(inode->i_mode)) {
		p->bdev = inode->i_sb->s_bdev;
	}

	return 0;
}


 
unsigned long generic_max_swapfile_size(void)
{
	return swp_offset(pte_to_swp_entry(
			swp_entry_to_pte(swp_entry(0, ~0UL)))) + 1;
}

 
__weak unsigned long arch_max_swapfile_size(void)
{
	return generic_max_swapfile_size();
}

static unsigned long read_swap_header(struct swap_info_struct *p,
					union swap_header *swap_header,
					struct inode *inode)
{
	int i;
	unsigned long maxpages;
	unsigned long swapfilepages;
	unsigned long last_page;

	if (memcmp("SWAPSPACE2", swap_header->magic.magic, 10)) {
		pr_err("Unable to find swap-space signature\n");
		return 0;
	}

	 
	if (swab32(swap_header->info.version) == 1) {
		swab32s(&swap_header->info.version);
		swab32s(&swap_header->info.last_page);
		swab32s(&swap_header->info.nr_badpages);
		if (swap_header->info.nr_badpages > MAX_SWAP_BADPAGES)
			return 0;
		for (i = 0; i < swap_header->info.nr_badpages; i++)
			swab32s(&swap_header->info.badpages[i]);
	}
	 
	if (swap_header->info.version != 1) {
		pr_warn("Unable to handle swap header version %d\n",
			swap_header->info.version);
		return 0;
	}

	p->lowest_bit  = 1;
	p->cluster_next = 1;
	p->cluster_nr = 0;

	maxpages = swapfile_maximum_size;
	last_page = swap_header->info.last_page;
	if (!last_page) {
		pr_warn("Empty swap-file\n");
		return 0;
	}
	if (last_page > maxpages) {
		pr_warn("Truncating oversized swap area, only using %luk out of %luk\n",
			K(maxpages), K(last_page));
	}
	if (maxpages > last_page) {
		maxpages = last_page + 1;
		 
		if ((unsigned int)maxpages == 0)
			maxpages = UINT_MAX;
	}
	p->highest_bit = maxpages - 1;

	if (!maxpages)
		return 0;
	swapfilepages = i_size_read(inode) >> PAGE_SHIFT;
	if (swapfilepages && maxpages > swapfilepages) {
		pr_warn("Swap area shorter than signature indicates\n");
		return 0;
	}
	if (swap_header->info.nr_badpages && S_ISREG(inode->i_mode))
		return 0;
	if (swap_header->info.nr_badpages > MAX_SWAP_BADPAGES)
		return 0;

	return maxpages;
}

#define SWAP_CLUSTER_INFO_COLS						\
	DIV_ROUND_UP(L1_CACHE_BYTES, sizeof(struct swap_cluster_info))
#define SWAP_CLUSTER_SPACE_COLS						\
	DIV_ROUND_UP(SWAP_ADDRESS_SPACE_PAGES, SWAPFILE_CLUSTER)
#define SWAP_CLUSTER_COLS						\
	max_t(unsigned int, SWAP_CLUSTER_INFO_COLS, SWAP_CLUSTER_SPACE_COLS)

static int setup_swap_map_and_extents(struct swap_info_struct *p,
					union swap_header *swap_header,
					unsigned char *swap_map,
					struct swap_cluster_info *cluster_info,
					unsigned long maxpages,
					sector_t *span)
{
	unsigned int j, k;
	unsigned int nr_good_pages;
	int nr_extents;
	unsigned long nr_clusters = DIV_ROUND_UP(maxpages, SWAPFILE_CLUSTER);
	unsigned long col = p->cluster_next / SWAPFILE_CLUSTER % SWAP_CLUSTER_COLS;
	unsigned long i, idx;

	nr_good_pages = maxpages - 1;	 

	cluster_list_init(&p->free_clusters);
	cluster_list_init(&p->discard_clusters);

	for (i = 0; i < swap_header->info.nr_badpages; i++) {
		unsigned int page_nr = swap_header->info.badpages[i];
		if (page_nr == 0 || page_nr > swap_header->info.last_page)
			return -EINVAL;
		if (page_nr < maxpages) {
			swap_map[page_nr] = SWAP_MAP_BAD;
			nr_good_pages--;
			 
			inc_cluster_info_page(p, cluster_info, page_nr);
		}
	}

	 
	for (i = maxpages; i < round_up(maxpages, SWAPFILE_CLUSTER); i++)
		inc_cluster_info_page(p, cluster_info, i);

	if (nr_good_pages) {
		swap_map[0] = SWAP_MAP_BAD;
		 
		inc_cluster_info_page(p, cluster_info, 0);
		p->max = maxpages;
		p->pages = nr_good_pages;
		nr_extents = setup_swap_extents(p, span);
		if (nr_extents < 0)
			return nr_extents;
		nr_good_pages = p->pages;
	}
	if (!nr_good_pages) {
		pr_warn("Empty swap-file\n");
		return -EINVAL;
	}

	if (!cluster_info)
		return nr_extents;


	 
	for (k = 0; k < SWAP_CLUSTER_COLS; k++) {
		j = (k + col) % SWAP_CLUSTER_COLS;
		for (i = 0; i < DIV_ROUND_UP(nr_clusters, SWAP_CLUSTER_COLS); i++) {
			idx = i * SWAP_CLUSTER_COLS + j;
			if (idx >= nr_clusters)
				continue;
			if (cluster_count(&cluster_info[idx]))
				continue;
			cluster_set_flag(&cluster_info[idx], CLUSTER_FLAG_FREE);
			cluster_list_add_tail(&p->free_clusters, cluster_info,
					      idx);
		}
	}
	return nr_extents;
}

SYSCALL_DEFINE2(swapon, const char __user *, specialfile, int, swap_flags)
{
	struct swap_info_struct *p;
	struct filename *name;
	struct file *swap_file = NULL;
	struct address_space *mapping;
	struct dentry *dentry;
	int prio;
	int error;
	union swap_header *swap_header;
	int nr_extents;
	sector_t span;
	unsigned long maxpages;
	unsigned char *swap_map = NULL;
	struct swap_cluster_info *cluster_info = NULL;
	struct page *page = NULL;
	struct inode *inode = NULL;
	bool inced_nr_rotate_swap = false;

	if (swap_flags & ~SWAP_FLAGS_VALID)
		return -EINVAL;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	if (!swap_avail_heads)
		return -ENOMEM;

	p = alloc_swap_info();
	if (IS_ERR(p))
		return PTR_ERR(p);

	INIT_WORK(&p->discard_work, swap_discard_work);

	name = getname(specialfile);
	if (IS_ERR(name)) {
		error = PTR_ERR(name);
		name = NULL;
		goto bad_swap;
	}
	swap_file = file_open_name(name, O_RDWR|O_LARGEFILE, 0);
	if (IS_ERR(swap_file)) {
		error = PTR_ERR(swap_file);
		swap_file = NULL;
		goto bad_swap;
	}

	p->swap_file = swap_file;
	mapping = swap_file->f_mapping;
	dentry = swap_file->f_path.dentry;
	inode = mapping->host;

	error = claim_swapfile(p, inode);
	if (unlikely(error))
		goto bad_swap;

	inode_lock(inode);
	if (d_unlinked(dentry) || cant_mount(dentry)) {
		error = -ENOENT;
		goto bad_swap_unlock_inode;
	}
	if (IS_SWAPFILE(inode)) {
		error = -EBUSY;
		goto bad_swap_unlock_inode;
	}

	 
	if (!mapping->a_ops->read_folio) {
		error = -EINVAL;
		goto bad_swap_unlock_inode;
	}
	page = read_mapping_page(mapping, 0, swap_file);
	if (IS_ERR(page)) {
		error = PTR_ERR(page);
		goto bad_swap_unlock_inode;
	}
	swap_header = kmap(page);

	maxpages = read_swap_header(p, swap_header, inode);
	if (unlikely(!maxpages)) {
		error = -EINVAL;
		goto bad_swap_unlock_inode;
	}

	 
	swap_map = vzalloc(maxpages);
	if (!swap_map) {
		error = -ENOMEM;
		goto bad_swap_unlock_inode;
	}

	if (p->bdev && bdev_stable_writes(p->bdev))
		p->flags |= SWP_STABLE_WRITES;

	if (p->bdev && bdev_synchronous(p->bdev))
		p->flags |= SWP_SYNCHRONOUS_IO;

	if (p->bdev && bdev_nonrot(p->bdev)) {
		int cpu;
		unsigned long ci, nr_cluster;

		p->flags |= SWP_SOLIDSTATE;
		p->cluster_next_cpu = alloc_percpu(unsigned int);
		if (!p->cluster_next_cpu) {
			error = -ENOMEM;
			goto bad_swap_unlock_inode;
		}
		 
		for_each_possible_cpu(cpu) {
			per_cpu(*p->cluster_next_cpu, cpu) =
				get_random_u32_inclusive(1, p->highest_bit);
		}
		nr_cluster = DIV_ROUND_UP(maxpages, SWAPFILE_CLUSTER);

		cluster_info = kvcalloc(nr_cluster, sizeof(*cluster_info),
					GFP_KERNEL);
		if (!cluster_info) {
			error = -ENOMEM;
			goto bad_swap_unlock_inode;
		}

		for (ci = 0; ci < nr_cluster; ci++)
			spin_lock_init(&((cluster_info + ci)->lock));

		p->percpu_cluster = alloc_percpu(struct percpu_cluster);
		if (!p->percpu_cluster) {
			error = -ENOMEM;
			goto bad_swap_unlock_inode;
		}
		for_each_possible_cpu(cpu) {
			struct percpu_cluster *cluster;
			cluster = per_cpu_ptr(p->percpu_cluster, cpu);
			cluster_set_null(&cluster->index);
		}
	} else {
		atomic_inc(&nr_rotate_swap);
		inced_nr_rotate_swap = true;
	}

	error = swap_cgroup_swapon(p->type, maxpages);
	if (error)
		goto bad_swap_unlock_inode;

	nr_extents = setup_swap_map_and_extents(p, swap_header, swap_map,
		cluster_info, maxpages, &span);
	if (unlikely(nr_extents < 0)) {
		error = nr_extents;
		goto bad_swap_unlock_inode;
	}

	if ((swap_flags & SWAP_FLAG_DISCARD) &&
	    p->bdev && bdev_max_discard_sectors(p->bdev)) {
		 
		p->flags |= (SWP_DISCARDABLE | SWP_AREA_DISCARD |
			     SWP_PAGE_DISCARD);

		 
		if (swap_flags & SWAP_FLAG_DISCARD_ONCE)
			p->flags &= ~SWP_PAGE_DISCARD;
		else if (swap_flags & SWAP_FLAG_DISCARD_PAGES)
			p->flags &= ~SWP_AREA_DISCARD;

		 
		if (p->flags & SWP_AREA_DISCARD) {
			int err = discard_swap(p);
			if (unlikely(err))
				pr_err("swapon: discard_swap(%p): %d\n",
					p, err);
		}
	}

	error = init_swap_address_space(p->type, maxpages);
	if (error)
		goto bad_swap_unlock_inode;

	 
	inode->i_flags |= S_SWAPFILE;
	error = inode_drain_writes(inode);
	if (error) {
		inode->i_flags &= ~S_SWAPFILE;
		goto free_swap_address_space;
	}

	mutex_lock(&swapon_mutex);
	prio = -1;
	if (swap_flags & SWAP_FLAG_PREFER)
		prio =
		  (swap_flags & SWAP_FLAG_PRIO_MASK) >> SWAP_FLAG_PRIO_SHIFT;
	enable_swap_info(p, prio, swap_map, cluster_info);

	pr_info("Adding %uk swap on %s.  Priority:%d extents:%d across:%lluk %s%s%s%s\n",
		K(p->pages), name->name, p->prio, nr_extents,
		K((unsigned long long)span),
		(p->flags & SWP_SOLIDSTATE) ? "SS" : "",
		(p->flags & SWP_DISCARDABLE) ? "D" : "",
		(p->flags & SWP_AREA_DISCARD) ? "s" : "",
		(p->flags & SWP_PAGE_DISCARD) ? "c" : "");

	mutex_unlock(&swapon_mutex);
	atomic_inc(&proc_poll_event);
	wake_up_interruptible(&proc_poll_wait);

	error = 0;
	goto out;
free_swap_address_space:
	exit_swap_address_space(p->type);
bad_swap_unlock_inode:
	inode_unlock(inode);
bad_swap:
	free_percpu(p->percpu_cluster);
	p->percpu_cluster = NULL;
	free_percpu(p->cluster_next_cpu);
	p->cluster_next_cpu = NULL;
	if (inode && S_ISBLK(inode->i_mode) && p->bdev) {
		set_blocksize(p->bdev, p->old_block_size);
		blkdev_put(p->bdev, p);
	}
	inode = NULL;
	destroy_swap_extents(p);
	swap_cgroup_swapoff(p->type);
	spin_lock(&swap_lock);
	p->swap_file = NULL;
	p->flags = 0;
	spin_unlock(&swap_lock);
	vfree(swap_map);
	kvfree(cluster_info);
	if (inced_nr_rotate_swap)
		atomic_dec(&nr_rotate_swap);
	if (swap_file)
		filp_close(swap_file, NULL);
out:
	if (page && !IS_ERR(page)) {
		kunmap(page);
		put_page(page);
	}
	if (name)
		putname(name);
	if (inode)
		inode_unlock(inode);
	if (!error)
		enable_swap_slots_cache();
	return error;
}

void si_swapinfo(struct sysinfo *val)
{
	unsigned int type;
	unsigned long nr_to_be_unused = 0;

	spin_lock(&swap_lock);
	for (type = 0; type < nr_swapfiles; type++) {
		struct swap_info_struct *si = swap_info[type];

		if ((si->flags & SWP_USED) && !(si->flags & SWP_WRITEOK))
			nr_to_be_unused += READ_ONCE(si->inuse_pages);
	}
	val->freeswap = atomic_long_read(&nr_swap_pages) + nr_to_be_unused;
	val->totalswap = total_swap_pages + nr_to_be_unused;
	spin_unlock(&swap_lock);
}

 
static int __swap_duplicate(swp_entry_t entry, unsigned char usage)
{
	struct swap_info_struct *p;
	struct swap_cluster_info *ci;
	unsigned long offset;
	unsigned char count;
	unsigned char has_cache;
	int err;

	p = swp_swap_info(entry);

	offset = swp_offset(entry);
	ci = lock_cluster_or_swap_info(p, offset);

	count = p->swap_map[offset];

	 
	if (unlikely(swap_count(count) == SWAP_MAP_BAD)) {
		err = -ENOENT;
		goto unlock_out;
	}

	has_cache = count & SWAP_HAS_CACHE;
	count &= ~SWAP_HAS_CACHE;
	err = 0;

	if (usage == SWAP_HAS_CACHE) {

		 
		if (!has_cache && count)
			has_cache = SWAP_HAS_CACHE;
		else if (has_cache)		 
			err = -EEXIST;
		else				 
			err = -ENOENT;

	} else if (count || has_cache) {

		if ((count & ~COUNT_CONTINUED) < SWAP_MAP_MAX)
			count += usage;
		else if ((count & ~COUNT_CONTINUED) > SWAP_MAP_MAX)
			err = -EINVAL;
		else if (swap_count_continued(p, offset, count))
			count = COUNT_CONTINUED;
		else
			err = -ENOMEM;
	} else
		err = -ENOENT;			 

	WRITE_ONCE(p->swap_map[offset], count | has_cache);

unlock_out:
	unlock_cluster_or_swap_info(p, ci);
	return err;
}

 
void swap_shmem_alloc(swp_entry_t entry)
{
	__swap_duplicate(entry, SWAP_MAP_SHMEM);
}

 
int swap_duplicate(swp_entry_t entry)
{
	int err = 0;

	while (!err && __swap_duplicate(entry, 1) == -ENOMEM)
		err = add_swap_count_continuation(entry, GFP_ATOMIC);
	return err;
}

 
int swapcache_prepare(swp_entry_t entry)
{
	return __swap_duplicate(entry, SWAP_HAS_CACHE);
}

struct swap_info_struct *swp_swap_info(swp_entry_t entry)
{
	return swap_type_to_swap_info(swp_type(entry));
}

struct swap_info_struct *page_swap_info(struct page *page)
{
	swp_entry_t entry = page_swap_entry(page);
	return swp_swap_info(entry);
}

 
struct address_space *swapcache_mapping(struct folio *folio)
{
	return page_swap_info(&folio->page)->swap_file->f_mapping;
}
EXPORT_SYMBOL_GPL(swapcache_mapping);

pgoff_t __page_file_index(struct page *page)
{
	swp_entry_t swap = page_swap_entry(page);
	return swp_offset(swap);
}
EXPORT_SYMBOL_GPL(__page_file_index);

 
int add_swap_count_continuation(swp_entry_t entry, gfp_t gfp_mask)
{
	struct swap_info_struct *si;
	struct swap_cluster_info *ci;
	struct page *head;
	struct page *page;
	struct page *list_page;
	pgoff_t offset;
	unsigned char count;
	int ret = 0;

	 
	page = alloc_page(gfp_mask | __GFP_HIGHMEM);

	si = get_swap_device(entry);
	if (!si) {
		 
		goto outer;
	}
	spin_lock(&si->lock);

	offset = swp_offset(entry);

	ci = lock_cluster(si, offset);

	count = swap_count(si->swap_map[offset]);

	if ((count & ~COUNT_CONTINUED) != SWAP_MAP_MAX) {
		 
		goto out;
	}

	if (!page) {
		ret = -ENOMEM;
		goto out;
	}

	head = vmalloc_to_page(si->swap_map + offset);
	offset &= ~PAGE_MASK;

	spin_lock(&si->cont_lock);
	 
	if (!page_private(head)) {
		BUG_ON(count & COUNT_CONTINUED);
		INIT_LIST_HEAD(&head->lru);
		set_page_private(head, SWP_CONTINUED);
		si->flags |= SWP_CONTINUED;
	}

	list_for_each_entry(list_page, &head->lru, lru) {
		unsigned char *map;

		 
		if (!(count & COUNT_CONTINUED))
			goto out_unlock_cont;

		map = kmap_atomic(list_page) + offset;
		count = *map;
		kunmap_atomic(map);

		 
		if ((count & ~COUNT_CONTINUED) != SWAP_CONT_MAX)
			goto out_unlock_cont;
	}

	list_add_tail(&page->lru, &head->lru);
	page = NULL;			 
out_unlock_cont:
	spin_unlock(&si->cont_lock);
out:
	unlock_cluster(ci);
	spin_unlock(&si->lock);
	put_swap_device(si);
outer:
	if (page)
		__free_page(page);
	return ret;
}

 
static bool swap_count_continued(struct swap_info_struct *si,
				 pgoff_t offset, unsigned char count)
{
	struct page *head;
	struct page *page;
	unsigned char *map;
	bool ret;

	head = vmalloc_to_page(si->swap_map + offset);
	if (page_private(head) != SWP_CONTINUED) {
		BUG_ON(count & COUNT_CONTINUED);
		return false;		 
	}

	spin_lock(&si->cont_lock);
	offset &= ~PAGE_MASK;
	page = list_next_entry(head, lru);
	map = kmap_atomic(page) + offset;

	if (count == SWAP_MAP_MAX)	 
		goto init_map;		 

	if (count == (SWAP_MAP_MAX | COUNT_CONTINUED)) {  
		 
		while (*map == (SWAP_CONT_MAX | COUNT_CONTINUED)) {
			kunmap_atomic(map);
			page = list_next_entry(page, lru);
			BUG_ON(page == head);
			map = kmap_atomic(page) + offset;
		}
		if (*map == SWAP_CONT_MAX) {
			kunmap_atomic(map);
			page = list_next_entry(page, lru);
			if (page == head) {
				ret = false;	 
				goto out;
			}
			map = kmap_atomic(page) + offset;
init_map:		*map = 0;		 
		}
		*map += 1;
		kunmap_atomic(map);
		while ((page = list_prev_entry(page, lru)) != head) {
			map = kmap_atomic(page) + offset;
			*map = COUNT_CONTINUED;
			kunmap_atomic(map);
		}
		ret = true;			 

	} else {				 
		 
		BUG_ON(count != COUNT_CONTINUED);
		while (*map == COUNT_CONTINUED) {
			kunmap_atomic(map);
			page = list_next_entry(page, lru);
			BUG_ON(page == head);
			map = kmap_atomic(page) + offset;
		}
		BUG_ON(*map == 0);
		*map -= 1;
		if (*map == 0)
			count = 0;
		kunmap_atomic(map);
		while ((page = list_prev_entry(page, lru)) != head) {
			map = kmap_atomic(page) + offset;
			*map = SWAP_CONT_MAX | count;
			count = COUNT_CONTINUED;
			kunmap_atomic(map);
		}
		ret = count == COUNT_CONTINUED;
	}
out:
	spin_unlock(&si->cont_lock);
	return ret;
}

 
static void free_swap_count_continuations(struct swap_info_struct *si)
{
	pgoff_t offset;

	for (offset = 0; offset < si->max; offset += PAGE_SIZE) {
		struct page *head;
		head = vmalloc_to_page(si->swap_map + offset);
		if (page_private(head)) {
			struct page *page, *next;

			list_for_each_entry_safe(page, next, &head->lru, lru) {
				list_del(&page->lru);
				__free_page(page);
			}
		}
	}
}

#if defined(CONFIG_MEMCG) && defined(CONFIG_BLK_CGROUP)
void __folio_throttle_swaprate(struct folio *folio, gfp_t gfp)
{
	struct swap_info_struct *si, *next;
	int nid = folio_nid(folio);

	if (!(gfp & __GFP_IO))
		return;

	if (!blk_cgroup_congested())
		return;

	 
	if (current->throttle_disk)
		return;

	spin_lock(&swap_avail_lock);
	plist_for_each_entry_safe(si, next, &swap_avail_heads[nid],
				  avail_lists[nid]) {
		if (si->bdev) {
			blkcg_schedule_throttle(si->bdev->bd_disk, true);
			break;
		}
	}
	spin_unlock(&swap_avail_lock);
}
#endif

static int __init swapfile_init(void)
{
	int nid;

	swap_avail_heads = kmalloc_array(nr_node_ids, sizeof(struct plist_head),
					 GFP_KERNEL);
	if (!swap_avail_heads) {
		pr_emerg("Not enough memory for swap heads, swap is disabled\n");
		return -ENOMEM;
	}

	for_each_node(nid)
		plist_head_init(&swap_avail_heads[nid]);

	swapfile_maximum_size = arch_max_swapfile_size();

#ifdef CONFIG_MIGRATION
	if (swapfile_maximum_size >= (1UL << SWP_MIG_TOTAL_BITS))
		swap_migration_ad_supported = true;
#endif	 

	return 0;
}
subsys_initcall(swapfile_init);
