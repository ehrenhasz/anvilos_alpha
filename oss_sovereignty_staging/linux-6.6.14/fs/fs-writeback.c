
 

#include <linux/kernel.h>
#include <linux/export.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/kthread.h>
#include <linux/writeback.h>
#include <linux/blkdev.h>
#include <linux/backing-dev.h>
#include <linux/tracepoint.h>
#include <linux/device.h>
#include <linux/memcontrol.h>
#include "internal.h"

 
#define MIN_WRITEBACK_PAGES	(4096UL >> (PAGE_SHIFT - 10))

 
struct wb_writeback_work {
	long nr_pages;
	struct super_block *sb;
	enum writeback_sync_modes sync_mode;
	unsigned int tagged_writepages:1;
	unsigned int for_kupdate:1;
	unsigned int range_cyclic:1;
	unsigned int for_background:1;
	unsigned int for_sync:1;	 
	unsigned int auto_free:1;	 
	enum wb_reason reason;		 

	struct list_head list;		 
	struct wb_completion *done;	 
};

 
unsigned int dirtytime_expire_interval = 12 * 60 * 60;

static inline struct inode *wb_inode(struct list_head *head)
{
	return list_entry(head, struct inode, i_io_list);
}

 
#define CREATE_TRACE_POINTS
#include <trace/events/writeback.h>

EXPORT_TRACEPOINT_SYMBOL_GPL(wbc_writepage);

static bool wb_io_lists_populated(struct bdi_writeback *wb)
{
	if (wb_has_dirty_io(wb)) {
		return false;
	} else {
		set_bit(WB_has_dirty_io, &wb->state);
		WARN_ON_ONCE(!wb->avg_write_bandwidth);
		atomic_long_add(wb->avg_write_bandwidth,
				&wb->bdi->tot_write_bandwidth);
		return true;
	}
}

static void wb_io_lists_depopulated(struct bdi_writeback *wb)
{
	if (wb_has_dirty_io(wb) && list_empty(&wb->b_dirty) &&
	    list_empty(&wb->b_io) && list_empty(&wb->b_more_io)) {
		clear_bit(WB_has_dirty_io, &wb->state);
		WARN_ON_ONCE(atomic_long_sub_return(wb->avg_write_bandwidth,
					&wb->bdi->tot_write_bandwidth) < 0);
	}
}

 
static bool inode_io_list_move_locked(struct inode *inode,
				      struct bdi_writeback *wb,
				      struct list_head *head)
{
	assert_spin_locked(&wb->list_lock);
	assert_spin_locked(&inode->i_lock);
	WARN_ON_ONCE(inode->i_state & I_FREEING);

	list_move(&inode->i_io_list, head);

	 
	if (head != &wb->b_dirty_time)
		return wb_io_lists_populated(wb);

	wb_io_lists_depopulated(wb);
	return false;
}

static void wb_wakeup(struct bdi_writeback *wb)
{
	spin_lock_irq(&wb->work_lock);
	if (test_bit(WB_registered, &wb->state))
		mod_delayed_work(bdi_wq, &wb->dwork, 0);
	spin_unlock_irq(&wb->work_lock);
}

static void finish_writeback_work(struct bdi_writeback *wb,
				  struct wb_writeback_work *work)
{
	struct wb_completion *done = work->done;

	if (work->auto_free)
		kfree(work);
	if (done) {
		wait_queue_head_t *waitq = done->waitq;

		 
		if (atomic_dec_and_test(&done->cnt))
			wake_up_all(waitq);
	}
}

static void wb_queue_work(struct bdi_writeback *wb,
			  struct wb_writeback_work *work)
{
	trace_writeback_queue(wb, work);

	if (work->done)
		atomic_inc(&work->done->cnt);

	spin_lock_irq(&wb->work_lock);

	if (test_bit(WB_registered, &wb->state)) {
		list_add_tail(&work->list, &wb->work_list);
		mod_delayed_work(bdi_wq, &wb->dwork, 0);
	} else
		finish_writeback_work(wb, work);

	spin_unlock_irq(&wb->work_lock);
}

 
void wb_wait_for_completion(struct wb_completion *done)
{
	atomic_dec(&done->cnt);		 
	wait_event(*done->waitq, !atomic_read(&done->cnt));
}

#ifdef CONFIG_CGROUP_WRITEBACK

 
#define WB_FRN_TIME_SHIFT	13	 
#define WB_FRN_TIME_AVG_SHIFT	3	 
#define WB_FRN_TIME_CUT_DIV	8	 
#define WB_FRN_TIME_PERIOD	(2 * (1 << WB_FRN_TIME_SHIFT))	 

#define WB_FRN_HIST_SLOTS	16	 
#define WB_FRN_HIST_UNIT	(WB_FRN_TIME_PERIOD / WB_FRN_HIST_SLOTS)
					 
#define WB_FRN_HIST_THR_SLOTS	(WB_FRN_HIST_SLOTS / 2)
					 
#define WB_FRN_HIST_MAX_SLOTS	(WB_FRN_HIST_THR_SLOTS / 2 + 1)
					 
#define WB_FRN_MAX_IN_FLIGHT	1024	 

 
#define WB_MAX_INODES_PER_ISW  ((1024UL - sizeof(struct inode_switch_wbs_context)) \
                                / sizeof(struct inode *))

static atomic_t isw_nr_in_flight = ATOMIC_INIT(0);
static struct workqueue_struct *isw_wq;

void __inode_attach_wb(struct inode *inode, struct folio *folio)
{
	struct backing_dev_info *bdi = inode_to_bdi(inode);
	struct bdi_writeback *wb = NULL;

	if (inode_cgwb_enabled(inode)) {
		struct cgroup_subsys_state *memcg_css;

		if (folio) {
			memcg_css = mem_cgroup_css_from_folio(folio);
			wb = wb_get_create(bdi, memcg_css, GFP_ATOMIC);
		} else {
			 
			memcg_css = task_get_css(current, memory_cgrp_id);
			wb = wb_get_create(bdi, memcg_css, GFP_ATOMIC);
			css_put(memcg_css);
		}
	}

	if (!wb)
		wb = &bdi->wb;

	 
	if (unlikely(cmpxchg(&inode->i_wb, NULL, wb)))
		wb_put(wb);
}
EXPORT_SYMBOL_GPL(__inode_attach_wb);

 
static void inode_cgwb_move_to_attached(struct inode *inode,
					struct bdi_writeback *wb)
{
	assert_spin_locked(&wb->list_lock);
	assert_spin_locked(&inode->i_lock);
	WARN_ON_ONCE(inode->i_state & I_FREEING);

	inode->i_state &= ~I_SYNC_QUEUED;
	if (wb != &wb->bdi->wb)
		list_move(&inode->i_io_list, &wb->b_attached);
	else
		list_del_init(&inode->i_io_list);
	wb_io_lists_depopulated(wb);
}

 
static struct bdi_writeback *
locked_inode_to_wb_and_lock_list(struct inode *inode)
	__releases(&inode->i_lock)
	__acquires(&wb->list_lock)
{
	while (true) {
		struct bdi_writeback *wb = inode_to_wb(inode);

		 
		wb_get(wb);
		spin_unlock(&inode->i_lock);
		spin_lock(&wb->list_lock);

		 
		if (likely(wb == inode->i_wb)) {
			wb_put(wb);	 
			return wb;
		}

		spin_unlock(&wb->list_lock);
		wb_put(wb);
		cpu_relax();
		spin_lock(&inode->i_lock);
	}
}

 
static struct bdi_writeback *inode_to_wb_and_lock_list(struct inode *inode)
	__acquires(&wb->list_lock)
{
	spin_lock(&inode->i_lock);
	return locked_inode_to_wb_and_lock_list(inode);
}

struct inode_switch_wbs_context {
	struct rcu_work		work;

	 
	struct bdi_writeback	*new_wb;
	struct inode		*inodes[];
};

static void bdi_down_write_wb_switch_rwsem(struct backing_dev_info *bdi)
{
	down_write(&bdi->wb_switch_rwsem);
}

static void bdi_up_write_wb_switch_rwsem(struct backing_dev_info *bdi)
{
	up_write(&bdi->wb_switch_rwsem);
}

static bool inode_do_switch_wbs(struct inode *inode,
				struct bdi_writeback *old_wb,
				struct bdi_writeback *new_wb)
{
	struct address_space *mapping = inode->i_mapping;
	XA_STATE(xas, &mapping->i_pages, 0);
	struct folio *folio;
	bool switched = false;

	spin_lock(&inode->i_lock);
	xa_lock_irq(&mapping->i_pages);

	 
	if (unlikely(inode->i_state & (I_FREEING | I_WILL_FREE)))
		goto skip_switch;

	trace_inode_switch_wbs(inode, old_wb, new_wb);

	 
	xas_for_each_marked(&xas, folio, ULONG_MAX, PAGECACHE_TAG_DIRTY) {
		if (folio_test_dirty(folio)) {
			long nr = folio_nr_pages(folio);
			wb_stat_mod(old_wb, WB_RECLAIMABLE, -nr);
			wb_stat_mod(new_wb, WB_RECLAIMABLE, nr);
		}
	}

	xas_set(&xas, 0);
	xas_for_each_marked(&xas, folio, ULONG_MAX, PAGECACHE_TAG_WRITEBACK) {
		long nr = folio_nr_pages(folio);
		WARN_ON_ONCE(!folio_test_writeback(folio));
		wb_stat_mod(old_wb, WB_WRITEBACK, -nr);
		wb_stat_mod(new_wb, WB_WRITEBACK, nr);
	}

	if (mapping_tagged(mapping, PAGECACHE_TAG_WRITEBACK)) {
		atomic_dec(&old_wb->writeback_inodes);
		atomic_inc(&new_wb->writeback_inodes);
	}

	wb_get(new_wb);

	 
	if (!list_empty(&inode->i_io_list)) {
		inode->i_wb = new_wb;

		if (inode->i_state & I_DIRTY_ALL) {
			struct inode *pos;

			list_for_each_entry(pos, &new_wb->b_dirty, i_io_list)
				if (time_after_eq(inode->dirtied_when,
						  pos->dirtied_when))
					break;
			inode_io_list_move_locked(inode, new_wb,
						  pos->i_io_list.prev);
		} else {
			inode_cgwb_move_to_attached(inode, new_wb);
		}
	} else {
		inode->i_wb = new_wb;
	}

	 
	inode->i_wb_frn_winner = 0;
	inode->i_wb_frn_avg_time = 0;
	inode->i_wb_frn_history = 0;
	switched = true;
skip_switch:
	 
	smp_store_release(&inode->i_state, inode->i_state & ~I_WB_SWITCH);

	xa_unlock_irq(&mapping->i_pages);
	spin_unlock(&inode->i_lock);

	return switched;
}

static void inode_switch_wbs_work_fn(struct work_struct *work)
{
	struct inode_switch_wbs_context *isw =
		container_of(to_rcu_work(work), struct inode_switch_wbs_context, work);
	struct backing_dev_info *bdi = inode_to_bdi(isw->inodes[0]);
	struct bdi_writeback *old_wb = isw->inodes[0]->i_wb;
	struct bdi_writeback *new_wb = isw->new_wb;
	unsigned long nr_switched = 0;
	struct inode **inodep;

	 
	down_read(&bdi->wb_switch_rwsem);

	 
	if (old_wb < new_wb) {
		spin_lock(&old_wb->list_lock);
		spin_lock_nested(&new_wb->list_lock, SINGLE_DEPTH_NESTING);
	} else {
		spin_lock(&new_wb->list_lock);
		spin_lock_nested(&old_wb->list_lock, SINGLE_DEPTH_NESTING);
	}

	for (inodep = isw->inodes; *inodep; inodep++) {
		WARN_ON_ONCE((*inodep)->i_wb != old_wb);
		if (inode_do_switch_wbs(*inodep, old_wb, new_wb))
			nr_switched++;
	}

	spin_unlock(&new_wb->list_lock);
	spin_unlock(&old_wb->list_lock);

	up_read(&bdi->wb_switch_rwsem);

	if (nr_switched) {
		wb_wakeup(new_wb);
		wb_put_many(old_wb, nr_switched);
	}

	for (inodep = isw->inodes; *inodep; inodep++)
		iput(*inodep);
	wb_put(new_wb);
	kfree(isw);
	atomic_dec(&isw_nr_in_flight);
}

static bool inode_prepare_wbs_switch(struct inode *inode,
				     struct bdi_writeback *new_wb)
{
	 
	smp_mb();

	if (IS_DAX(inode))
		return false;

	 
	spin_lock(&inode->i_lock);
	if (!(inode->i_sb->s_flags & SB_ACTIVE) ||
	    inode->i_state & (I_WB_SWITCH | I_FREEING | I_WILL_FREE) ||
	    inode_to_wb(inode) == new_wb) {
		spin_unlock(&inode->i_lock);
		return false;
	}
	inode->i_state |= I_WB_SWITCH;
	__iget(inode);
	spin_unlock(&inode->i_lock);

	return true;
}

 
static void inode_switch_wbs(struct inode *inode, int new_wb_id)
{
	struct backing_dev_info *bdi = inode_to_bdi(inode);
	struct cgroup_subsys_state *memcg_css;
	struct inode_switch_wbs_context *isw;

	 
	if (inode->i_state & I_WB_SWITCH)
		return;

	 
	if (atomic_read(&isw_nr_in_flight) > WB_FRN_MAX_IN_FLIGHT)
		return;

	isw = kzalloc(struct_size(isw, inodes, 2), GFP_ATOMIC);
	if (!isw)
		return;

	atomic_inc(&isw_nr_in_flight);

	 
	rcu_read_lock();
	memcg_css = css_from_id(new_wb_id, &memory_cgrp_subsys);
	if (memcg_css && !css_tryget(memcg_css))
		memcg_css = NULL;
	rcu_read_unlock();
	if (!memcg_css)
		goto out_free;

	isw->new_wb = wb_get_create(bdi, memcg_css, GFP_ATOMIC);
	css_put(memcg_css);
	if (!isw->new_wb)
		goto out_free;

	if (!inode_prepare_wbs_switch(inode, isw->new_wb))
		goto out_free;

	isw->inodes[0] = inode;

	 
	INIT_RCU_WORK(&isw->work, inode_switch_wbs_work_fn);
	queue_rcu_work(isw_wq, &isw->work);
	return;

out_free:
	atomic_dec(&isw_nr_in_flight);
	if (isw->new_wb)
		wb_put(isw->new_wb);
	kfree(isw);
}

static bool isw_prepare_wbs_switch(struct inode_switch_wbs_context *isw,
				   struct list_head *list, int *nr)
{
	struct inode *inode;

	list_for_each_entry(inode, list, i_io_list) {
		if (!inode_prepare_wbs_switch(inode, isw->new_wb))
			continue;

		isw->inodes[*nr] = inode;
		(*nr)++;

		if (*nr >= WB_MAX_INODES_PER_ISW - 1)
			return true;
	}
	return false;
}

 
bool cleanup_offline_cgwb(struct bdi_writeback *wb)
{
	struct cgroup_subsys_state *memcg_css;
	struct inode_switch_wbs_context *isw;
	int nr;
	bool restart = false;

	isw = kzalloc(struct_size(isw, inodes, WB_MAX_INODES_PER_ISW),
		      GFP_KERNEL);
	if (!isw)
		return restart;

	atomic_inc(&isw_nr_in_flight);

	for (memcg_css = wb->memcg_css->parent; memcg_css;
	     memcg_css = memcg_css->parent) {
		isw->new_wb = wb_get_create(wb->bdi, memcg_css, GFP_KERNEL);
		if (isw->new_wb)
			break;
	}
	if (unlikely(!isw->new_wb))
		isw->new_wb = &wb->bdi->wb;  

	nr = 0;
	spin_lock(&wb->list_lock);
	 
	restart = isw_prepare_wbs_switch(isw, &wb->b_attached, &nr);
	if (!restart)
		restart = isw_prepare_wbs_switch(isw, &wb->b_dirty_time, &nr);
	spin_unlock(&wb->list_lock);

	 
	if (nr == 0) {
		atomic_dec(&isw_nr_in_flight);
		wb_put(isw->new_wb);
		kfree(isw);
		return restart;
	}

	 
	INIT_RCU_WORK(&isw->work, inode_switch_wbs_work_fn);
	queue_rcu_work(isw_wq, &isw->work);

	return restart;
}

 
void wbc_attach_and_unlock_inode(struct writeback_control *wbc,
				 struct inode *inode)
{
	if (!inode_cgwb_enabled(inode)) {
		spin_unlock(&inode->i_lock);
		return;
	}

	wbc->wb = inode_to_wb(inode);
	wbc->inode = inode;

	wbc->wb_id = wbc->wb->memcg_css->id;
	wbc->wb_lcand_id = inode->i_wb_frn_winner;
	wbc->wb_tcand_id = 0;
	wbc->wb_bytes = 0;
	wbc->wb_lcand_bytes = 0;
	wbc->wb_tcand_bytes = 0;

	wb_get(wbc->wb);
	spin_unlock(&inode->i_lock);

	 
	if (unlikely(wb_dying(wbc->wb) && !css_is_dying(wbc->wb->memcg_css)))
		inode_switch_wbs(inode, wbc->wb_id);
}
EXPORT_SYMBOL_GPL(wbc_attach_and_unlock_inode);

 
void wbc_detach_inode(struct writeback_control *wbc)
{
	struct bdi_writeback *wb = wbc->wb;
	struct inode *inode = wbc->inode;
	unsigned long avg_time, max_bytes, max_time;
	u16 history;
	int max_id;

	if (!wb)
		return;

	history = inode->i_wb_frn_history;
	avg_time = inode->i_wb_frn_avg_time;

	 
	if (wbc->wb_bytes >= wbc->wb_lcand_bytes &&
	    wbc->wb_bytes >= wbc->wb_tcand_bytes) {
		max_id = wbc->wb_id;
		max_bytes = wbc->wb_bytes;
	} else if (wbc->wb_lcand_bytes >= wbc->wb_tcand_bytes) {
		max_id = wbc->wb_lcand_id;
		max_bytes = wbc->wb_lcand_bytes;
	} else {
		max_id = wbc->wb_tcand_id;
		max_bytes = wbc->wb_tcand_bytes;
	}

	 
	max_time = DIV_ROUND_UP((max_bytes >> PAGE_SHIFT) << WB_FRN_TIME_SHIFT,
				wb->avg_write_bandwidth);
	if (avg_time)
		avg_time += (max_time >> WB_FRN_TIME_AVG_SHIFT) -
			    (avg_time >> WB_FRN_TIME_AVG_SHIFT);
	else
		avg_time = max_time;	 

	if (max_time >= avg_time / WB_FRN_TIME_CUT_DIV) {
		int slots;

		 
		slots = min(DIV_ROUND_UP(max_time, WB_FRN_HIST_UNIT),
			    (unsigned long)WB_FRN_HIST_MAX_SLOTS);
		history <<= slots;
		if (wbc->wb_id != max_id)
			history |= (1U << slots) - 1;

		if (history)
			trace_inode_foreign_history(inode, wbc, history);

		 
		if (hweight16(history) > WB_FRN_HIST_THR_SLOTS)
			inode_switch_wbs(inode, max_id);
	}

	 
	inode->i_wb_frn_winner = max_id;
	inode->i_wb_frn_avg_time = min(avg_time, (unsigned long)U16_MAX);
	inode->i_wb_frn_history = history;

	wb_put(wbc->wb);
	wbc->wb = NULL;
}
EXPORT_SYMBOL_GPL(wbc_detach_inode);

 
void wbc_account_cgroup_owner(struct writeback_control *wbc, struct page *page,
			      size_t bytes)
{
	struct folio *folio;
	struct cgroup_subsys_state *css;
	int id;

	 
	if (!wbc->wb || wbc->no_cgroup_owner)
		return;

	folio = page_folio(page);
	css = mem_cgroup_css_from_folio(folio);
	 
	if (!(css->flags & CSS_ONLINE))
		return;

	id = css->id;

	if (id == wbc->wb_id) {
		wbc->wb_bytes += bytes;
		return;
	}

	if (id == wbc->wb_lcand_id)
		wbc->wb_lcand_bytes += bytes;

	 
	if (!wbc->wb_tcand_bytes)
		wbc->wb_tcand_id = id;
	if (id == wbc->wb_tcand_id)
		wbc->wb_tcand_bytes += bytes;
	else
		wbc->wb_tcand_bytes -= min(bytes, wbc->wb_tcand_bytes);
}
EXPORT_SYMBOL_GPL(wbc_account_cgroup_owner);

 
static long wb_split_bdi_pages(struct bdi_writeback *wb, long nr_pages)
{
	unsigned long this_bw = wb->avg_write_bandwidth;
	unsigned long tot_bw = atomic_long_read(&wb->bdi->tot_write_bandwidth);

	if (nr_pages == LONG_MAX)
		return LONG_MAX;

	 
	if (!tot_bw || this_bw >= tot_bw)
		return nr_pages;
	else
		return DIV_ROUND_UP_ULL((u64)nr_pages * this_bw, tot_bw);
}

 
static void bdi_split_work_to_wbs(struct backing_dev_info *bdi,
				  struct wb_writeback_work *base_work,
				  bool skip_if_busy)
{
	struct bdi_writeback *last_wb = NULL;
	struct bdi_writeback *wb = list_entry(&bdi->wb_list,
					      struct bdi_writeback, bdi_node);

	might_sleep();
restart:
	rcu_read_lock();
	list_for_each_entry_continue_rcu(wb, &bdi->wb_list, bdi_node) {
		DEFINE_WB_COMPLETION(fallback_work_done, bdi);
		struct wb_writeback_work fallback_work;
		struct wb_writeback_work *work;
		long nr_pages;

		if (last_wb) {
			wb_put(last_wb);
			last_wb = NULL;
		}

		 
		if (!wb_has_dirty_io(wb) &&
		    (base_work->sync_mode == WB_SYNC_NONE ||
		     list_empty(&wb->b_dirty_time)))
			continue;
		if (skip_if_busy && writeback_in_progress(wb))
			continue;

		nr_pages = wb_split_bdi_pages(wb, base_work->nr_pages);

		work = kmalloc(sizeof(*work), GFP_ATOMIC);
		if (work) {
			*work = *base_work;
			work->nr_pages = nr_pages;
			work->auto_free = 1;
			wb_queue_work(wb, work);
			continue;
		}

		 
		if (!wb_tryget(wb))
			continue;

		 
		work = &fallback_work;
		*work = *base_work;
		work->nr_pages = nr_pages;
		work->auto_free = 0;
		work->done = &fallback_work_done;

		wb_queue_work(wb, work);
		last_wb = wb;

		rcu_read_unlock();
		wb_wait_for_completion(&fallback_work_done);
		goto restart;
	}
	rcu_read_unlock();

	if (last_wb)
		wb_put(last_wb);
}

 
int cgroup_writeback_by_id(u64 bdi_id, int memcg_id,
			   enum wb_reason reason, struct wb_completion *done)
{
	struct backing_dev_info *bdi;
	struct cgroup_subsys_state *memcg_css;
	struct bdi_writeback *wb;
	struct wb_writeback_work *work;
	unsigned long dirty;
	int ret;

	 
	bdi = bdi_get_by_id(bdi_id);
	if (!bdi)
		return -ENOENT;

	rcu_read_lock();
	memcg_css = css_from_id(memcg_id, &memory_cgrp_subsys);
	if (memcg_css && !css_tryget(memcg_css))
		memcg_css = NULL;
	rcu_read_unlock();
	if (!memcg_css) {
		ret = -ENOENT;
		goto out_bdi_put;
	}

	 
	wb = wb_get_lookup(bdi, memcg_css);
	if (!wb) {
		ret = -ENOENT;
		goto out_css_put;
	}

	 
	dirty = memcg_page_state(mem_cgroup_from_css(memcg_css), NR_FILE_DIRTY);
	dirty = dirty * 10 / 8;

	 
	work = kzalloc(sizeof(*work), GFP_NOWAIT | __GFP_NOWARN);
	if (work) {
		work->nr_pages = dirty;
		work->sync_mode = WB_SYNC_NONE;
		work->range_cyclic = 1;
		work->reason = reason;
		work->done = done;
		work->auto_free = 1;
		wb_queue_work(wb, work);
		ret = 0;
	} else {
		ret = -ENOMEM;
	}

	wb_put(wb);
out_css_put:
	css_put(memcg_css);
out_bdi_put:
	bdi_put(bdi);
	return ret;
}

 
void cgroup_writeback_umount(void)
{
	 
	smp_mb();

	if (atomic_read(&isw_nr_in_flight)) {
		 
		rcu_barrier();
		flush_workqueue(isw_wq);
	}
}

static int __init cgroup_writeback_init(void)
{
	isw_wq = alloc_workqueue("inode_switch_wbs", 0, 0);
	if (!isw_wq)
		return -ENOMEM;
	return 0;
}
fs_initcall(cgroup_writeback_init);

#else	 

static void bdi_down_write_wb_switch_rwsem(struct backing_dev_info *bdi) { }
static void bdi_up_write_wb_switch_rwsem(struct backing_dev_info *bdi) { }

static void inode_cgwb_move_to_attached(struct inode *inode,
					struct bdi_writeback *wb)
{
	assert_spin_locked(&wb->list_lock);
	assert_spin_locked(&inode->i_lock);
	WARN_ON_ONCE(inode->i_state & I_FREEING);

	inode->i_state &= ~I_SYNC_QUEUED;
	list_del_init(&inode->i_io_list);
	wb_io_lists_depopulated(wb);
}

static struct bdi_writeback *
locked_inode_to_wb_and_lock_list(struct inode *inode)
	__releases(&inode->i_lock)
	__acquires(&wb->list_lock)
{
	struct bdi_writeback *wb = inode_to_wb(inode);

	spin_unlock(&inode->i_lock);
	spin_lock(&wb->list_lock);
	return wb;
}

static struct bdi_writeback *inode_to_wb_and_lock_list(struct inode *inode)
	__acquires(&wb->list_lock)
{
	struct bdi_writeback *wb = inode_to_wb(inode);

	spin_lock(&wb->list_lock);
	return wb;
}

static long wb_split_bdi_pages(struct bdi_writeback *wb, long nr_pages)
{
	return nr_pages;
}

static void bdi_split_work_to_wbs(struct backing_dev_info *bdi,
				  struct wb_writeback_work *base_work,
				  bool skip_if_busy)
{
	might_sleep();

	if (!skip_if_busy || !writeback_in_progress(&bdi->wb)) {
		base_work->auto_free = 0;
		wb_queue_work(&bdi->wb, base_work);
	}
}

#endif	 

 
static unsigned long get_nr_dirty_pages(void)
{
	return global_node_page_state(NR_FILE_DIRTY) +
		get_nr_dirty_inodes();
}

static void wb_start_writeback(struct bdi_writeback *wb, enum wb_reason reason)
{
	if (!wb_has_dirty_io(wb))
		return;

	 
	if (test_bit(WB_start_all, &wb->state) ||
	    test_and_set_bit(WB_start_all, &wb->state))
		return;

	wb->start_all_reason = reason;
	wb_wakeup(wb);
}

 
void wb_start_background_writeback(struct bdi_writeback *wb)
{
	 
	trace_writeback_wake_background(wb);
	wb_wakeup(wb);
}

 
void inode_io_list_del(struct inode *inode)
{
	struct bdi_writeback *wb;

	wb = inode_to_wb_and_lock_list(inode);
	spin_lock(&inode->i_lock);

	inode->i_state &= ~I_SYNC_QUEUED;
	list_del_init(&inode->i_io_list);
	wb_io_lists_depopulated(wb);

	spin_unlock(&inode->i_lock);
	spin_unlock(&wb->list_lock);
}
EXPORT_SYMBOL(inode_io_list_del);

 
void sb_mark_inode_writeback(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	unsigned long flags;

	if (list_empty(&inode->i_wb_list)) {
		spin_lock_irqsave(&sb->s_inode_wblist_lock, flags);
		if (list_empty(&inode->i_wb_list)) {
			list_add_tail(&inode->i_wb_list, &sb->s_inodes_wb);
			trace_sb_mark_inode_writeback(inode);
		}
		spin_unlock_irqrestore(&sb->s_inode_wblist_lock, flags);
	}
}

 
void sb_clear_inode_writeback(struct inode *inode)
{
	struct super_block *sb = inode->i_sb;
	unsigned long flags;

	if (!list_empty(&inode->i_wb_list)) {
		spin_lock_irqsave(&sb->s_inode_wblist_lock, flags);
		if (!list_empty(&inode->i_wb_list)) {
			list_del_init(&inode->i_wb_list);
			trace_sb_clear_inode_writeback(inode);
		}
		spin_unlock_irqrestore(&sb->s_inode_wblist_lock, flags);
	}
}

 
static void redirty_tail_locked(struct inode *inode, struct bdi_writeback *wb)
{
	assert_spin_locked(&inode->i_lock);

	inode->i_state &= ~I_SYNC_QUEUED;
	 
	if (inode->i_state & I_FREEING) {
		list_del_init(&inode->i_io_list);
		wb_io_lists_depopulated(wb);
		return;
	}
	if (!list_empty(&wb->b_dirty)) {
		struct inode *tail;

		tail = wb_inode(wb->b_dirty.next);
		if (time_before(inode->dirtied_when, tail->dirtied_when))
			inode->dirtied_when = jiffies;
	}
	inode_io_list_move_locked(inode, wb, &wb->b_dirty);
}

static void redirty_tail(struct inode *inode, struct bdi_writeback *wb)
{
	spin_lock(&inode->i_lock);
	redirty_tail_locked(inode, wb);
	spin_unlock(&inode->i_lock);
}

 
static void requeue_io(struct inode *inode, struct bdi_writeback *wb)
{
	inode_io_list_move_locked(inode, wb, &wb->b_more_io);
}

static void inode_sync_complete(struct inode *inode)
{
	inode->i_state &= ~I_SYNC;
	 
	inode_add_lru(inode);
	 
	smp_mb();
	wake_up_bit(&inode->i_state, __I_SYNC);
}

static bool inode_dirtied_after(struct inode *inode, unsigned long t)
{
	bool ret = time_after(inode->dirtied_when, t);
#ifndef CONFIG_64BIT
	 
	ret = ret && time_before_eq(inode->dirtied_when, jiffies);
#endif
	return ret;
}

 
static int move_expired_inodes(struct list_head *delaying_queue,
			       struct list_head *dispatch_queue,
			       unsigned long dirtied_before)
{
	LIST_HEAD(tmp);
	struct list_head *pos, *node;
	struct super_block *sb = NULL;
	struct inode *inode;
	int do_sb_sort = 0;
	int moved = 0;

	while (!list_empty(delaying_queue)) {
		inode = wb_inode(delaying_queue->prev);
		if (inode_dirtied_after(inode, dirtied_before))
			break;
		spin_lock(&inode->i_lock);
		list_move(&inode->i_io_list, &tmp);
		moved++;
		inode->i_state |= I_SYNC_QUEUED;
		spin_unlock(&inode->i_lock);
		if (sb_is_blkdev_sb(inode->i_sb))
			continue;
		if (sb && sb != inode->i_sb)
			do_sb_sort = 1;
		sb = inode->i_sb;
	}

	 
	if (!do_sb_sort) {
		list_splice(&tmp, dispatch_queue);
		goto out;
	}

	 
	while (!list_empty(&tmp)) {
		sb = wb_inode(tmp.prev)->i_sb;
		list_for_each_prev_safe(pos, node, &tmp) {
			inode = wb_inode(pos);
			if (inode->i_sb == sb)
				list_move(&inode->i_io_list, dispatch_queue);
		}
	}
out:
	return moved;
}

 
static void queue_io(struct bdi_writeback *wb, struct wb_writeback_work *work,
		     unsigned long dirtied_before)
{
	int moved;
	unsigned long time_expire_jif = dirtied_before;

	assert_spin_locked(&wb->list_lock);
	list_splice_init(&wb->b_more_io, &wb->b_io);
	moved = move_expired_inodes(&wb->b_dirty, &wb->b_io, dirtied_before);
	if (!work->for_sync)
		time_expire_jif = jiffies - dirtytime_expire_interval * HZ;
	moved += move_expired_inodes(&wb->b_dirty_time, &wb->b_io,
				     time_expire_jif);
	if (moved)
		wb_io_lists_populated(wb);
	trace_writeback_queue_io(wb, work, dirtied_before, moved);
}

static int write_inode(struct inode *inode, struct writeback_control *wbc)
{
	int ret;

	if (inode->i_sb->s_op->write_inode && !is_bad_inode(inode)) {
		trace_writeback_write_inode_start(inode, wbc);
		ret = inode->i_sb->s_op->write_inode(inode, wbc);
		trace_writeback_write_inode(inode, wbc);
		return ret;
	}
	return 0;
}

 
static void __inode_wait_for_writeback(struct inode *inode)
	__releases(inode->i_lock)
	__acquires(inode->i_lock)
{
	DEFINE_WAIT_BIT(wq, &inode->i_state, __I_SYNC);
	wait_queue_head_t *wqh;

	wqh = bit_waitqueue(&inode->i_state, __I_SYNC);
	while (inode->i_state & I_SYNC) {
		spin_unlock(&inode->i_lock);
		__wait_on_bit(wqh, &wq, bit_wait,
			      TASK_UNINTERRUPTIBLE);
		spin_lock(&inode->i_lock);
	}
}

 
void inode_wait_for_writeback(struct inode *inode)
{
	spin_lock(&inode->i_lock);
	__inode_wait_for_writeback(inode);
	spin_unlock(&inode->i_lock);
}

 
static void inode_sleep_on_writeback(struct inode *inode)
	__releases(inode->i_lock)
{
	DEFINE_WAIT(wait);
	wait_queue_head_t *wqh = bit_waitqueue(&inode->i_state, __I_SYNC);
	int sleep;

	prepare_to_wait(wqh, &wait, TASK_UNINTERRUPTIBLE);
	sleep = inode->i_state & I_SYNC;
	spin_unlock(&inode->i_lock);
	if (sleep)
		schedule();
	finish_wait(wqh, &wait);
}

 
static void requeue_inode(struct inode *inode, struct bdi_writeback *wb,
			  struct writeback_control *wbc)
{
	if (inode->i_state & I_FREEING)
		return;

	 
	if ((inode->i_state & I_DIRTY) &&
	    (wbc->sync_mode == WB_SYNC_ALL || wbc->tagged_writepages))
		inode->dirtied_when = jiffies;

	if (wbc->pages_skipped) {
		 
		if (inode->i_state & I_DIRTY_ALL)
			redirty_tail_locked(inode, wb);
		else
			inode_cgwb_move_to_attached(inode, wb);
		return;
	}

	if (mapping_tagged(inode->i_mapping, PAGECACHE_TAG_DIRTY)) {
		 
		if (wbc->nr_to_write <= 0) {
			 
			requeue_io(inode, wb);
		} else {
			 
			redirty_tail_locked(inode, wb);
		}
	} else if (inode->i_state & I_DIRTY) {
		 
		redirty_tail_locked(inode, wb);
	} else if (inode->i_state & I_DIRTY_TIME) {
		inode->dirtied_when = jiffies;
		inode_io_list_move_locked(inode, wb, &wb->b_dirty_time);
		inode->i_state &= ~I_SYNC_QUEUED;
	} else {
		 
		inode_cgwb_move_to_attached(inode, wb);
	}
}

 
static int
__writeback_single_inode(struct inode *inode, struct writeback_control *wbc)
{
	struct address_space *mapping = inode->i_mapping;
	long nr_to_write = wbc->nr_to_write;
	unsigned dirty;
	int ret;

	WARN_ON(!(inode->i_state & I_SYNC));

	trace_writeback_single_inode_start(inode, wbc, nr_to_write);

	ret = do_writepages(mapping, wbc);

	 
	if (wbc->sync_mode == WB_SYNC_ALL && !wbc->for_sync) {
		int err = filemap_fdatawait(mapping);
		if (ret == 0)
			ret = err;
	}

	 
	if ((inode->i_state & I_DIRTY_TIME) &&
	    (wbc->sync_mode == WB_SYNC_ALL ||
	     time_after(jiffies, inode->dirtied_time_when +
			dirtytime_expire_interval * HZ))) {
		trace_writeback_lazytime(inode);
		mark_inode_dirty_sync(inode);
	}

	 
	spin_lock(&inode->i_lock);
	dirty = inode->i_state & I_DIRTY;
	inode->i_state &= ~dirty;

	 
	smp_mb();

	if (mapping_tagged(mapping, PAGECACHE_TAG_DIRTY))
		inode->i_state |= I_DIRTY_PAGES;
	else if (unlikely(inode->i_state & I_PINNING_FSCACHE_WB)) {
		if (!(inode->i_state & I_DIRTY_PAGES)) {
			inode->i_state &= ~I_PINNING_FSCACHE_WB;
			wbc->unpinned_fscache_wb = true;
			dirty |= I_PINNING_FSCACHE_WB;  
		}
	}

	spin_unlock(&inode->i_lock);

	 
	if (dirty & ~I_DIRTY_PAGES) {
		int err = write_inode(inode, wbc);
		if (ret == 0)
			ret = err;
	}
	wbc->unpinned_fscache_wb = false;
	trace_writeback_single_inode(inode, wbc, nr_to_write);
	return ret;
}

 
static int writeback_single_inode(struct inode *inode,
				  struct writeback_control *wbc)
{
	struct bdi_writeback *wb;
	int ret = 0;

	spin_lock(&inode->i_lock);
	if (!atomic_read(&inode->i_count))
		WARN_ON(!(inode->i_state & (I_WILL_FREE|I_FREEING)));
	else
		WARN_ON(inode->i_state & I_WILL_FREE);

	if (inode->i_state & I_SYNC) {
		 
		if (wbc->sync_mode != WB_SYNC_ALL)
			goto out;
		__inode_wait_for_writeback(inode);
	}
	WARN_ON(inode->i_state & I_SYNC);
	 
	if (!(inode->i_state & I_DIRTY_ALL) &&
	    (wbc->sync_mode != WB_SYNC_ALL ||
	     !mapping_tagged(inode->i_mapping, PAGECACHE_TAG_WRITEBACK)))
		goto out;
	inode->i_state |= I_SYNC;
	wbc_attach_and_unlock_inode(wbc, inode);

	ret = __writeback_single_inode(inode, wbc);

	wbc_detach_inode(wbc);

	wb = inode_to_wb_and_lock_list(inode);
	spin_lock(&inode->i_lock);
	 
	if (!(inode->i_state & I_FREEING)) {
		 
		if (!(inode->i_state & I_DIRTY_ALL))
			inode_cgwb_move_to_attached(inode, wb);
		else if (!(inode->i_state & I_SYNC_QUEUED)) {
			if ((inode->i_state & I_DIRTY))
				redirty_tail_locked(inode, wb);
			else if (inode->i_state & I_DIRTY_TIME) {
				inode->dirtied_when = jiffies;
				inode_io_list_move_locked(inode,
							  wb,
							  &wb->b_dirty_time);
			}
		}
	}

	spin_unlock(&wb->list_lock);
	inode_sync_complete(inode);
out:
	spin_unlock(&inode->i_lock);
	return ret;
}

static long writeback_chunk_size(struct bdi_writeback *wb,
				 struct wb_writeback_work *work)
{
	long pages;

	 
	if (work->sync_mode == WB_SYNC_ALL || work->tagged_writepages)
		pages = LONG_MAX;
	else {
		pages = min(wb->avg_write_bandwidth / 2,
			    global_wb_domain.dirty_limit / DIRTY_SCOPE);
		pages = min(pages, work->nr_pages);
		pages = round_down(pages + MIN_WRITEBACK_PAGES,
				   MIN_WRITEBACK_PAGES);
	}

	return pages;
}

 
static long writeback_sb_inodes(struct super_block *sb,
				struct bdi_writeback *wb,
				struct wb_writeback_work *work)
{
	struct writeback_control wbc = {
		.sync_mode		= work->sync_mode,
		.tagged_writepages	= work->tagged_writepages,
		.for_kupdate		= work->for_kupdate,
		.for_background		= work->for_background,
		.for_sync		= work->for_sync,
		.range_cyclic		= work->range_cyclic,
		.range_start		= 0,
		.range_end		= LLONG_MAX,
	};
	unsigned long start_time = jiffies;
	long write_chunk;
	long total_wrote = 0;   

	while (!list_empty(&wb->b_io)) {
		struct inode *inode = wb_inode(wb->b_io.prev);
		struct bdi_writeback *tmp_wb;
		long wrote;

		if (inode->i_sb != sb) {
			if (work->sb) {
				 
				redirty_tail(inode, wb);
				continue;
			}

			 
			break;
		}

		 
		spin_lock(&inode->i_lock);
		if (inode->i_state & (I_NEW | I_FREEING | I_WILL_FREE)) {
			redirty_tail_locked(inode, wb);
			spin_unlock(&inode->i_lock);
			continue;
		}
		if ((inode->i_state & I_SYNC) && wbc.sync_mode != WB_SYNC_ALL) {
			 
			requeue_io(inode, wb);
			spin_unlock(&inode->i_lock);
			trace_writeback_sb_inodes_requeue(inode);
			continue;
		}
		spin_unlock(&wb->list_lock);

		 
		if (inode->i_state & I_SYNC) {
			 
			inode_sleep_on_writeback(inode);
			 
			spin_lock(&wb->list_lock);
			continue;
		}
		inode->i_state |= I_SYNC;
		wbc_attach_and_unlock_inode(&wbc, inode);

		write_chunk = writeback_chunk_size(wb, work);
		wbc.nr_to_write = write_chunk;
		wbc.pages_skipped = 0;

		 
		__writeback_single_inode(inode, &wbc);

		wbc_detach_inode(&wbc);
		work->nr_pages -= write_chunk - wbc.nr_to_write;
		wrote = write_chunk - wbc.nr_to_write - wbc.pages_skipped;
		wrote = wrote < 0 ? 0 : wrote;
		total_wrote += wrote;

		if (need_resched()) {
			 
			blk_flush_plug(current->plug, false);
			cond_resched();
		}

		 
		tmp_wb = inode_to_wb_and_lock_list(inode);
		spin_lock(&inode->i_lock);
		if (!(inode->i_state & I_DIRTY_ALL))
			total_wrote++;
		requeue_inode(inode, tmp_wb, &wbc);
		inode_sync_complete(inode);
		spin_unlock(&inode->i_lock);

		if (unlikely(tmp_wb != wb)) {
			spin_unlock(&tmp_wb->list_lock);
			spin_lock(&wb->list_lock);
		}

		 
		if (total_wrote) {
			if (time_is_before_jiffies(start_time + HZ / 10UL))
				break;
			if (work->nr_pages <= 0)
				break;
		}
	}
	return total_wrote;
}

static long __writeback_inodes_wb(struct bdi_writeback *wb,
				  struct wb_writeback_work *work)
{
	unsigned long start_time = jiffies;
	long wrote = 0;

	while (!list_empty(&wb->b_io)) {
		struct inode *inode = wb_inode(wb->b_io.prev);
		struct super_block *sb = inode->i_sb;

		if (!super_trylock_shared(sb)) {
			 
			redirty_tail(inode, wb);
			continue;
		}
		wrote += writeback_sb_inodes(sb, wb, work);
		up_read(&sb->s_umount);

		 
		if (wrote) {
			if (time_is_before_jiffies(start_time + HZ / 10UL))
				break;
			if (work->nr_pages <= 0)
				break;
		}
	}
	 
	return wrote;
}

static long writeback_inodes_wb(struct bdi_writeback *wb, long nr_pages,
				enum wb_reason reason)
{
	struct wb_writeback_work work = {
		.nr_pages	= nr_pages,
		.sync_mode	= WB_SYNC_NONE,
		.range_cyclic	= 1,
		.reason		= reason,
	};
	struct blk_plug plug;

	blk_start_plug(&plug);
	spin_lock(&wb->list_lock);
	if (list_empty(&wb->b_io))
		queue_io(wb, &work, jiffies);
	__writeback_inodes_wb(wb, &work);
	spin_unlock(&wb->list_lock);
	blk_finish_plug(&plug);

	return nr_pages - work.nr_pages;
}

 
static long wb_writeback(struct bdi_writeback *wb,
			 struct wb_writeback_work *work)
{
	long nr_pages = work->nr_pages;
	unsigned long dirtied_before = jiffies;
	struct inode *inode;
	long progress;
	struct blk_plug plug;

	blk_start_plug(&plug);
	for (;;) {
		 
		if (work->nr_pages <= 0)
			break;

		 
		if ((work->for_background || work->for_kupdate) &&
		    !list_empty(&wb->work_list))
			break;

		 
		if (work->for_background && !wb_over_bg_thresh(wb))
			break;


		spin_lock(&wb->list_lock);

		 
		if (work->for_kupdate) {
			dirtied_before = jiffies -
				msecs_to_jiffies(dirty_expire_interval * 10);
		} else if (work->for_background)
			dirtied_before = jiffies;

		trace_writeback_start(wb, work);
		if (list_empty(&wb->b_io))
			queue_io(wb, work, dirtied_before);
		if (work->sb)
			progress = writeback_sb_inodes(work->sb, wb, work);
		else
			progress = __writeback_inodes_wb(wb, work);
		trace_writeback_written(wb, work);

		 
		if (progress) {
			spin_unlock(&wb->list_lock);
			continue;
		}

		 
		if (list_empty(&wb->b_more_io)) {
			spin_unlock(&wb->list_lock);
			break;
		}

		 
		trace_writeback_wait(wb, work);
		inode = wb_inode(wb->b_more_io.prev);
		spin_lock(&inode->i_lock);
		spin_unlock(&wb->list_lock);
		 
		inode_sleep_on_writeback(inode);
	}
	blk_finish_plug(&plug);

	return nr_pages - work->nr_pages;
}

 
static struct wb_writeback_work *get_next_work_item(struct bdi_writeback *wb)
{
	struct wb_writeback_work *work = NULL;

	spin_lock_irq(&wb->work_lock);
	if (!list_empty(&wb->work_list)) {
		work = list_entry(wb->work_list.next,
				  struct wb_writeback_work, list);
		list_del_init(&work->list);
	}
	spin_unlock_irq(&wb->work_lock);
	return work;
}

static long wb_check_background_flush(struct bdi_writeback *wb)
{
	if (wb_over_bg_thresh(wb)) {

		struct wb_writeback_work work = {
			.nr_pages	= LONG_MAX,
			.sync_mode	= WB_SYNC_NONE,
			.for_background	= 1,
			.range_cyclic	= 1,
			.reason		= WB_REASON_BACKGROUND,
		};

		return wb_writeback(wb, &work);
	}

	return 0;
}

static long wb_check_old_data_flush(struct bdi_writeback *wb)
{
	unsigned long expired;
	long nr_pages;

	 
	if (!dirty_writeback_interval)
		return 0;

	expired = wb->last_old_flush +
			msecs_to_jiffies(dirty_writeback_interval * 10);
	if (time_before(jiffies, expired))
		return 0;

	wb->last_old_flush = jiffies;
	nr_pages = get_nr_dirty_pages();

	if (nr_pages) {
		struct wb_writeback_work work = {
			.nr_pages	= nr_pages,
			.sync_mode	= WB_SYNC_NONE,
			.for_kupdate	= 1,
			.range_cyclic	= 1,
			.reason		= WB_REASON_PERIODIC,
		};

		return wb_writeback(wb, &work);
	}

	return 0;
}

static long wb_check_start_all(struct bdi_writeback *wb)
{
	long nr_pages;

	if (!test_bit(WB_start_all, &wb->state))
		return 0;

	nr_pages = get_nr_dirty_pages();
	if (nr_pages) {
		struct wb_writeback_work work = {
			.nr_pages	= wb_split_bdi_pages(wb, nr_pages),
			.sync_mode	= WB_SYNC_NONE,
			.range_cyclic	= 1,
			.reason		= wb->start_all_reason,
		};

		nr_pages = wb_writeback(wb, &work);
	}

	clear_bit(WB_start_all, &wb->state);
	return nr_pages;
}


 
static long wb_do_writeback(struct bdi_writeback *wb)
{
	struct wb_writeback_work *work;
	long wrote = 0;

	set_bit(WB_writeback_running, &wb->state);
	while ((work = get_next_work_item(wb)) != NULL) {
		trace_writeback_exec(wb, work);
		wrote += wb_writeback(wb, work);
		finish_writeback_work(wb, work);
	}

	 
	wrote += wb_check_start_all(wb);

	 
	wrote += wb_check_old_data_flush(wb);
	wrote += wb_check_background_flush(wb);
	clear_bit(WB_writeback_running, &wb->state);

	return wrote;
}

 
void wb_workfn(struct work_struct *work)
{
	struct bdi_writeback *wb = container_of(to_delayed_work(work),
						struct bdi_writeback, dwork);
	long pages_written;

	set_worker_desc("flush-%s", bdi_dev_name(wb->bdi));

	if (likely(!current_is_workqueue_rescuer() ||
		   !test_bit(WB_registered, &wb->state))) {
		 
		do {
			pages_written = wb_do_writeback(wb);
			trace_writeback_pages_written(pages_written);
		} while (!list_empty(&wb->work_list));
	} else {
		 
		pages_written = writeback_inodes_wb(wb, 1024,
						    WB_REASON_FORKER_THREAD);
		trace_writeback_pages_written(pages_written);
	}

	if (!list_empty(&wb->work_list))
		wb_wakeup(wb);
	else if (wb_has_dirty_io(wb) && dirty_writeback_interval)
		wb_wakeup_delayed(wb);
}

 
static void __wakeup_flusher_threads_bdi(struct backing_dev_info *bdi,
					 enum wb_reason reason)
{
	struct bdi_writeback *wb;

	if (!bdi_has_dirty_io(bdi))
		return;

	list_for_each_entry_rcu(wb, &bdi->wb_list, bdi_node)
		wb_start_writeback(wb, reason);
}

void wakeup_flusher_threads_bdi(struct backing_dev_info *bdi,
				enum wb_reason reason)
{
	rcu_read_lock();
	__wakeup_flusher_threads_bdi(bdi, reason);
	rcu_read_unlock();
}

 
void wakeup_flusher_threads(enum wb_reason reason)
{
	struct backing_dev_info *bdi;

	 
	blk_flush_plug(current->plug, true);

	rcu_read_lock();
	list_for_each_entry_rcu(bdi, &bdi_list, bdi_list)
		__wakeup_flusher_threads_bdi(bdi, reason);
	rcu_read_unlock();
}

 
static void wakeup_dirtytime_writeback(struct work_struct *w);
static DECLARE_DELAYED_WORK(dirtytime_work, wakeup_dirtytime_writeback);

static void wakeup_dirtytime_writeback(struct work_struct *w)
{
	struct backing_dev_info *bdi;

	rcu_read_lock();
	list_for_each_entry_rcu(bdi, &bdi_list, bdi_list) {
		struct bdi_writeback *wb;

		list_for_each_entry_rcu(wb, &bdi->wb_list, bdi_node)
			if (!list_empty(&wb->b_dirty_time))
				wb_wakeup(wb);
	}
	rcu_read_unlock();
	schedule_delayed_work(&dirtytime_work, dirtytime_expire_interval * HZ);
}

static int __init start_dirtytime_writeback(void)
{
	schedule_delayed_work(&dirtytime_work, dirtytime_expire_interval * HZ);
	return 0;
}
__initcall(start_dirtytime_writeback);

int dirtytime_interval_handler(struct ctl_table *table, int write,
			       void *buffer, size_t *lenp, loff_t *ppos)
{
	int ret;

	ret = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	if (ret == 0 && write)
		mod_delayed_work(system_wq, &dirtytime_work, 0);
	return ret;
}

 
void __mark_inode_dirty(struct inode *inode, int flags)
{
	struct super_block *sb = inode->i_sb;
	int dirtytime = 0;
	struct bdi_writeback *wb = NULL;

	trace_writeback_mark_inode_dirty(inode, flags);

	if (flags & I_DIRTY_INODE) {
		 
		if (inode->i_state & I_DIRTY_TIME) {
			spin_lock(&inode->i_lock);
			if (inode->i_state & I_DIRTY_TIME) {
				inode->i_state &= ~I_DIRTY_TIME;
				flags |= I_DIRTY_TIME;
			}
			spin_unlock(&inode->i_lock);
		}

		 
		trace_writeback_dirty_inode_start(inode, flags);
		if (sb->s_op->dirty_inode)
			sb->s_op->dirty_inode(inode,
				flags & (I_DIRTY_INODE | I_DIRTY_TIME));
		trace_writeback_dirty_inode(inode, flags);

		 
		flags &= ~I_DIRTY_TIME;
	} else {
		 
		dirtytime = flags & I_DIRTY_TIME;
		WARN_ON_ONCE(dirtytime && flags != I_DIRTY_TIME);
	}

	 
	smp_mb();

	if ((inode->i_state & flags) == flags)
		return;

	spin_lock(&inode->i_lock);
	if ((inode->i_state & flags) != flags) {
		const int was_dirty = inode->i_state & I_DIRTY;

		inode_attach_wb(inode, NULL);

		inode->i_state |= flags;

		 
		if (!was_dirty) {
			wb = locked_inode_to_wb_and_lock_list(inode);
			spin_lock(&inode->i_lock);
		}

		 
		if (inode->i_state & I_SYNC_QUEUED)
			goto out_unlock;

		 
		if (!S_ISBLK(inode->i_mode)) {
			if (inode_unhashed(inode))
				goto out_unlock;
		}
		if (inode->i_state & I_FREEING)
			goto out_unlock;

		 
		if (!was_dirty) {
			struct list_head *dirty_list;
			bool wakeup_bdi = false;

			inode->dirtied_when = jiffies;
			if (dirtytime)
				inode->dirtied_time_when = jiffies;

			if (inode->i_state & I_DIRTY)
				dirty_list = &wb->b_dirty;
			else
				dirty_list = &wb->b_dirty_time;

			wakeup_bdi = inode_io_list_move_locked(inode, wb,
							       dirty_list);

			spin_unlock(&wb->list_lock);
			spin_unlock(&inode->i_lock);
			trace_writeback_dirty_inode_enqueue(inode);

			 
			if (wakeup_bdi &&
			    (wb->bdi->capabilities & BDI_CAP_WRITEBACK))
				wb_wakeup_delayed(wb);
			return;
		}
	}
out_unlock:
	if (wb)
		spin_unlock(&wb->list_lock);
	spin_unlock(&inode->i_lock);
}
EXPORT_SYMBOL(__mark_inode_dirty);

 
static void wait_sb_inodes(struct super_block *sb)
{
	LIST_HEAD(sync_list);

	 
	WARN_ON(!rwsem_is_locked(&sb->s_umount));

	mutex_lock(&sb->s_sync_lock);

	 
	rcu_read_lock();
	spin_lock_irq(&sb->s_inode_wblist_lock);
	list_splice_init(&sb->s_inodes_wb, &sync_list);

	 
	while (!list_empty(&sync_list)) {
		struct inode *inode = list_first_entry(&sync_list, struct inode,
						       i_wb_list);
		struct address_space *mapping = inode->i_mapping;

		 
		list_move_tail(&inode->i_wb_list, &sb->s_inodes_wb);

		 
		if (!mapping_tagged(mapping, PAGECACHE_TAG_WRITEBACK))
			continue;

		spin_unlock_irq(&sb->s_inode_wblist_lock);

		spin_lock(&inode->i_lock);
		if (inode->i_state & (I_FREEING|I_WILL_FREE|I_NEW)) {
			spin_unlock(&inode->i_lock);

			spin_lock_irq(&sb->s_inode_wblist_lock);
			continue;
		}
		__iget(inode);
		spin_unlock(&inode->i_lock);
		rcu_read_unlock();

		 
		filemap_fdatawait_keep_errors(mapping);

		cond_resched();

		iput(inode);

		rcu_read_lock();
		spin_lock_irq(&sb->s_inode_wblist_lock);
	}
	spin_unlock_irq(&sb->s_inode_wblist_lock);
	rcu_read_unlock();
	mutex_unlock(&sb->s_sync_lock);
}

static void __writeback_inodes_sb_nr(struct super_block *sb, unsigned long nr,
				     enum wb_reason reason, bool skip_if_busy)
{
	struct backing_dev_info *bdi = sb->s_bdi;
	DEFINE_WB_COMPLETION(done, bdi);
	struct wb_writeback_work work = {
		.sb			= sb,
		.sync_mode		= WB_SYNC_NONE,
		.tagged_writepages	= 1,
		.done			= &done,
		.nr_pages		= nr,
		.reason			= reason,
	};

	if (!bdi_has_dirty_io(bdi) || bdi == &noop_backing_dev_info)
		return;
	WARN_ON(!rwsem_is_locked(&sb->s_umount));

	bdi_split_work_to_wbs(sb->s_bdi, &work, skip_if_busy);
	wb_wait_for_completion(&done);
}

 
void writeback_inodes_sb_nr(struct super_block *sb,
			    unsigned long nr,
			    enum wb_reason reason)
{
	__writeback_inodes_sb_nr(sb, nr, reason, false);
}
EXPORT_SYMBOL(writeback_inodes_sb_nr);

 
void writeback_inodes_sb(struct super_block *sb, enum wb_reason reason)
{
	return writeback_inodes_sb_nr(sb, get_nr_dirty_pages(), reason);
}
EXPORT_SYMBOL(writeback_inodes_sb);

 
void try_to_writeback_inodes_sb(struct super_block *sb, enum wb_reason reason)
{
	if (!down_read_trylock(&sb->s_umount))
		return;

	__writeback_inodes_sb_nr(sb, get_nr_dirty_pages(), reason, true);
	up_read(&sb->s_umount);
}
EXPORT_SYMBOL(try_to_writeback_inodes_sb);

 
void sync_inodes_sb(struct super_block *sb)
{
	struct backing_dev_info *bdi = sb->s_bdi;
	DEFINE_WB_COMPLETION(done, bdi);
	struct wb_writeback_work work = {
		.sb		= sb,
		.sync_mode	= WB_SYNC_ALL,
		.nr_pages	= LONG_MAX,
		.range_cyclic	= 0,
		.done		= &done,
		.reason		= WB_REASON_SYNC,
		.for_sync	= 1,
	};

	 
	if (bdi == &noop_backing_dev_info)
		return;
	WARN_ON(!rwsem_is_locked(&sb->s_umount));

	 
	bdi_down_write_wb_switch_rwsem(bdi);
	bdi_split_work_to_wbs(bdi, &work, false);
	wb_wait_for_completion(&done);
	bdi_up_write_wb_switch_rwsem(bdi);

	wait_sb_inodes(sb);
}
EXPORT_SYMBOL(sync_inodes_sb);

 
int write_inode_now(struct inode *inode, int sync)
{
	struct writeback_control wbc = {
		.nr_to_write = LONG_MAX,
		.sync_mode = sync ? WB_SYNC_ALL : WB_SYNC_NONE,
		.range_start = 0,
		.range_end = LLONG_MAX,
	};

	if (!mapping_can_writeback(inode->i_mapping))
		wbc.nr_to_write = 0;

	might_sleep();
	return writeback_single_inode(inode, &wbc);
}
EXPORT_SYMBOL(write_inode_now);

 
int sync_inode_metadata(struct inode *inode, int wait)
{
	struct writeback_control wbc = {
		.sync_mode = wait ? WB_SYNC_ALL : WB_SYNC_NONE,
		.nr_to_write = 0,  
	};

	return writeback_single_inode(inode, &wbc);
}
EXPORT_SYMBOL(sync_inode_metadata);
