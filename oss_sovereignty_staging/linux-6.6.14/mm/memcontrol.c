
 

#include <linux/page_counter.h>
#include <linux/memcontrol.h>
#include <linux/cgroup.h>
#include <linux/pagewalk.h>
#include <linux/sched/mm.h>
#include <linux/shmem_fs.h>
#include <linux/hugetlb.h>
#include <linux/pagemap.h>
#include <linux/vm_event_item.h>
#include <linux/smp.h>
#include <linux/page-flags.h>
#include <linux/backing-dev.h>
#include <linux/bit_spinlock.h>
#include <linux/rcupdate.h>
#include <linux/limits.h>
#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/rbtree.h>
#include <linux/slab.h>
#include <linux/swap.h>
#include <linux/swapops.h>
#include <linux/spinlock.h>
#include <linux/eventfd.h>
#include <linux/poll.h>
#include <linux/sort.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/vmpressure.h>
#include <linux/memremap.h>
#include <linux/mm_inline.h>
#include <linux/swap_cgroup.h>
#include <linux/cpu.h>
#include <linux/oom.h>
#include <linux/lockdep.h>
#include <linux/file.h>
#include <linux/resume_user_mode.h>
#include <linux/psi.h>
#include <linux/seq_buf.h>
#include <linux/sched/isolation.h>
#include "internal.h"
#include <net/sock.h>
#include <net/ip.h>
#include "slab.h"
#include "swap.h"

#include <linux/uaccess.h>

#include <trace/events/vmscan.h>

struct cgroup_subsys memory_cgrp_subsys __read_mostly;
EXPORT_SYMBOL(memory_cgrp_subsys);

struct mem_cgroup *root_mem_cgroup __read_mostly;

 
DEFINE_PER_CPU(struct mem_cgroup *, int_active_memcg);
EXPORT_PER_CPU_SYMBOL_GPL(int_active_memcg);

 
static bool cgroup_memory_nosocket __ro_after_init;

 
static bool cgroup_memory_nokmem __ro_after_init;

 
static bool cgroup_memory_nobpf __ro_after_init;

#ifdef CONFIG_CGROUP_WRITEBACK
static DECLARE_WAIT_QUEUE_HEAD(memcg_cgwb_frn_waitq);
#endif

 
static bool do_memsw_account(void)
{
	return !cgroup_subsys_on_dfl(memory_cgrp_subsys);
}

#define THRESHOLDS_EVENTS_TARGET 128
#define SOFTLIMIT_EVENTS_TARGET 1024

 

struct mem_cgroup_tree_per_node {
	struct rb_root rb_root;
	struct rb_node *rb_rightmost;
	spinlock_t lock;
};

struct mem_cgroup_tree {
	struct mem_cgroup_tree_per_node *rb_tree_per_node[MAX_NUMNODES];
};

static struct mem_cgroup_tree soft_limit_tree __read_mostly;

 
struct mem_cgroup_eventfd_list {
	struct list_head list;
	struct eventfd_ctx *eventfd;
};

 
struct mem_cgroup_event {
	 
	struct mem_cgroup *memcg;
	 
	struct eventfd_ctx *eventfd;
	 
	struct list_head list;
	 
	int (*register_event)(struct mem_cgroup *memcg,
			      struct eventfd_ctx *eventfd, const char *args);
	 
	void (*unregister_event)(struct mem_cgroup *memcg,
				 struct eventfd_ctx *eventfd);
	 
	poll_table pt;
	wait_queue_head_t *wqh;
	wait_queue_entry_t wait;
	struct work_struct remove;
};

static void mem_cgroup_threshold(struct mem_cgroup *memcg);
static void mem_cgroup_oom_notify(struct mem_cgroup *memcg);

 
 
#define MOVE_ANON	0x1U
#define MOVE_FILE	0x2U
#define MOVE_MASK	(MOVE_ANON | MOVE_FILE)

 
static struct move_charge_struct {
	spinlock_t	  lock;  
	struct mm_struct  *mm;
	struct mem_cgroup *from;
	struct mem_cgroup *to;
	unsigned long flags;
	unsigned long precharge;
	unsigned long moved_charge;
	unsigned long moved_swap;
	struct task_struct *moving_task;	 
	wait_queue_head_t waitq;		 
} mc = {
	.lock = __SPIN_LOCK_UNLOCKED(mc.lock),
	.waitq = __WAIT_QUEUE_HEAD_INITIALIZER(mc.waitq),
};

 
#define	MEM_CGROUP_MAX_RECLAIM_LOOPS		100
#define	MEM_CGROUP_MAX_SOFT_LIMIT_RECLAIM_LOOPS	2

 
enum res_type {
	_MEM,
	_MEMSWAP,
	_KMEM,
	_TCP,
};

#define MEMFILE_PRIVATE(x, val)	((x) << 16 | (val))
#define MEMFILE_TYPE(val)	((val) >> 16 & 0xffff)
#define MEMFILE_ATTR(val)	((val) & 0xffff)

 
#define for_each_mem_cgroup_tree(iter, root)		\
	for (iter = mem_cgroup_iter(root, NULL, NULL);	\
	     iter != NULL;				\
	     iter = mem_cgroup_iter(root, iter, NULL))

#define for_each_mem_cgroup(iter)			\
	for (iter = mem_cgroup_iter(NULL, NULL, NULL);	\
	     iter != NULL;				\
	     iter = mem_cgroup_iter(NULL, iter, NULL))

static inline bool task_is_dying(void)
{
	return tsk_is_oom_victim(current) || fatal_signal_pending(current) ||
		(current->flags & PF_EXITING);
}

 
struct vmpressure *memcg_to_vmpressure(struct mem_cgroup *memcg)
{
	if (!memcg)
		memcg = root_mem_cgroup;
	return &memcg->vmpressure;
}

struct mem_cgroup *vmpressure_to_memcg(struct vmpressure *vmpr)
{
	return container_of(vmpr, struct mem_cgroup, vmpressure);
}

#ifdef CONFIG_MEMCG_KMEM
static DEFINE_SPINLOCK(objcg_lock);

bool mem_cgroup_kmem_disabled(void)
{
	return cgroup_memory_nokmem;
}

static void obj_cgroup_uncharge_pages(struct obj_cgroup *objcg,
				      unsigned int nr_pages);

static void obj_cgroup_release(struct percpu_ref *ref)
{
	struct obj_cgroup *objcg = container_of(ref, struct obj_cgroup, refcnt);
	unsigned int nr_bytes;
	unsigned int nr_pages;
	unsigned long flags;

	 
	nr_bytes = atomic_read(&objcg->nr_charged_bytes);
	WARN_ON_ONCE(nr_bytes & (PAGE_SIZE - 1));
	nr_pages = nr_bytes >> PAGE_SHIFT;

	if (nr_pages)
		obj_cgroup_uncharge_pages(objcg, nr_pages);

	spin_lock_irqsave(&objcg_lock, flags);
	list_del(&objcg->list);
	spin_unlock_irqrestore(&objcg_lock, flags);

	percpu_ref_exit(ref);
	kfree_rcu(objcg, rcu);
}

static struct obj_cgroup *obj_cgroup_alloc(void)
{
	struct obj_cgroup *objcg;
	int ret;

	objcg = kzalloc(sizeof(struct obj_cgroup), GFP_KERNEL);
	if (!objcg)
		return NULL;

	ret = percpu_ref_init(&objcg->refcnt, obj_cgroup_release, 0,
			      GFP_KERNEL);
	if (ret) {
		kfree(objcg);
		return NULL;
	}
	INIT_LIST_HEAD(&objcg->list);
	return objcg;
}

static void memcg_reparent_objcgs(struct mem_cgroup *memcg,
				  struct mem_cgroup *parent)
{
	struct obj_cgroup *objcg, *iter;

	objcg = rcu_replace_pointer(memcg->objcg, NULL, true);

	spin_lock_irq(&objcg_lock);

	 
	list_add(&objcg->list, &memcg->objcg_list);
	 
	list_for_each_entry(iter, &memcg->objcg_list, list)
		WRITE_ONCE(iter->memcg, parent);
	 
	list_splice(&memcg->objcg_list, &parent->objcg_list);

	spin_unlock_irq(&objcg_lock);

	percpu_ref_kill(&objcg->refcnt);
}

 
DEFINE_STATIC_KEY_FALSE(memcg_kmem_online_key);
EXPORT_SYMBOL(memcg_kmem_online_key);

DEFINE_STATIC_KEY_FALSE(memcg_bpf_enabled_key);
EXPORT_SYMBOL(memcg_bpf_enabled_key);
#endif

 
struct cgroup_subsys_state *mem_cgroup_css_from_folio(struct folio *folio)
{
	struct mem_cgroup *memcg = folio_memcg(folio);

	if (!memcg || !cgroup_subsys_on_dfl(memory_cgrp_subsys))
		memcg = root_mem_cgroup;

	return &memcg->css;
}

 
ino_t page_cgroup_ino(struct page *page)
{
	struct mem_cgroup *memcg;
	unsigned long ino = 0;

	rcu_read_lock();
	 
	memcg = folio_memcg_check(page_folio(page));

	while (memcg && !(memcg->css.flags & CSS_ONLINE))
		memcg = parent_mem_cgroup(memcg);
	if (memcg)
		ino = cgroup_ino(memcg->css.cgroup);
	rcu_read_unlock();
	return ino;
}

static void __mem_cgroup_insert_exceeded(struct mem_cgroup_per_node *mz,
					 struct mem_cgroup_tree_per_node *mctz,
					 unsigned long new_usage_in_excess)
{
	struct rb_node **p = &mctz->rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct mem_cgroup_per_node *mz_node;
	bool rightmost = true;

	if (mz->on_tree)
		return;

	mz->usage_in_excess = new_usage_in_excess;
	if (!mz->usage_in_excess)
		return;
	while (*p) {
		parent = *p;
		mz_node = rb_entry(parent, struct mem_cgroup_per_node,
					tree_node);
		if (mz->usage_in_excess < mz_node->usage_in_excess) {
			p = &(*p)->rb_left;
			rightmost = false;
		} else {
			p = &(*p)->rb_right;
		}
	}

	if (rightmost)
		mctz->rb_rightmost = &mz->tree_node;

	rb_link_node(&mz->tree_node, parent, p);
	rb_insert_color(&mz->tree_node, &mctz->rb_root);
	mz->on_tree = true;
}

static void __mem_cgroup_remove_exceeded(struct mem_cgroup_per_node *mz,
					 struct mem_cgroup_tree_per_node *mctz)
{
	if (!mz->on_tree)
		return;

	if (&mz->tree_node == mctz->rb_rightmost)
		mctz->rb_rightmost = rb_prev(&mz->tree_node);

	rb_erase(&mz->tree_node, &mctz->rb_root);
	mz->on_tree = false;
}

static void mem_cgroup_remove_exceeded(struct mem_cgroup_per_node *mz,
				       struct mem_cgroup_tree_per_node *mctz)
{
	unsigned long flags;

	spin_lock_irqsave(&mctz->lock, flags);
	__mem_cgroup_remove_exceeded(mz, mctz);
	spin_unlock_irqrestore(&mctz->lock, flags);
}

static unsigned long soft_limit_excess(struct mem_cgroup *memcg)
{
	unsigned long nr_pages = page_counter_read(&memcg->memory);
	unsigned long soft_limit = READ_ONCE(memcg->soft_limit);
	unsigned long excess = 0;

	if (nr_pages > soft_limit)
		excess = nr_pages - soft_limit;

	return excess;
}

static void mem_cgroup_update_tree(struct mem_cgroup *memcg, int nid)
{
	unsigned long excess;
	struct mem_cgroup_per_node *mz;
	struct mem_cgroup_tree_per_node *mctz;

	if (lru_gen_enabled()) {
		if (soft_limit_excess(memcg))
			lru_gen_soft_reclaim(memcg, nid);
		return;
	}

	mctz = soft_limit_tree.rb_tree_per_node[nid];
	if (!mctz)
		return;
	 
	for (; memcg; memcg = parent_mem_cgroup(memcg)) {
		mz = memcg->nodeinfo[nid];
		excess = soft_limit_excess(memcg);
		 
		if (excess || mz->on_tree) {
			unsigned long flags;

			spin_lock_irqsave(&mctz->lock, flags);
			 
			if (mz->on_tree)
				__mem_cgroup_remove_exceeded(mz, mctz);
			 
			__mem_cgroup_insert_exceeded(mz, mctz, excess);
			spin_unlock_irqrestore(&mctz->lock, flags);
		}
	}
}

static void mem_cgroup_remove_from_trees(struct mem_cgroup *memcg)
{
	struct mem_cgroup_tree_per_node *mctz;
	struct mem_cgroup_per_node *mz;
	int nid;

	for_each_node(nid) {
		mz = memcg->nodeinfo[nid];
		mctz = soft_limit_tree.rb_tree_per_node[nid];
		if (mctz)
			mem_cgroup_remove_exceeded(mz, mctz);
	}
}

static struct mem_cgroup_per_node *
__mem_cgroup_largest_soft_limit_node(struct mem_cgroup_tree_per_node *mctz)
{
	struct mem_cgroup_per_node *mz;

retry:
	mz = NULL;
	if (!mctz->rb_rightmost)
		goto done;		 

	mz = rb_entry(mctz->rb_rightmost,
		      struct mem_cgroup_per_node, tree_node);
	 
	__mem_cgroup_remove_exceeded(mz, mctz);
	if (!soft_limit_excess(mz->memcg) ||
	    !css_tryget(&mz->memcg->css))
		goto retry;
done:
	return mz;
}

static struct mem_cgroup_per_node *
mem_cgroup_largest_soft_limit_node(struct mem_cgroup_tree_per_node *mctz)
{
	struct mem_cgroup_per_node *mz;

	spin_lock_irq(&mctz->lock);
	mz = __mem_cgroup_largest_soft_limit_node(mctz);
	spin_unlock_irq(&mctz->lock);
	return mz;
}

 
static void flush_memcg_stats_dwork(struct work_struct *w);
static DECLARE_DEFERRABLE_WORK(stats_flush_dwork, flush_memcg_stats_dwork);
static DEFINE_PER_CPU(unsigned int, stats_updates);
static atomic_t stats_flush_ongoing = ATOMIC_INIT(0);
static atomic_t stats_flush_threshold = ATOMIC_INIT(0);
static u64 flush_next_time;

#define FLUSH_TIME (2UL*HZ)

 
static void memcg_stats_lock(void)
{
	preempt_disable_nested();
	VM_WARN_ON_IRQS_ENABLED();
}

static void __memcg_stats_lock(void)
{
	preempt_disable_nested();
}

static void memcg_stats_unlock(void)
{
	preempt_enable_nested();
}

static inline void memcg_rstat_updated(struct mem_cgroup *memcg, int val)
{
	unsigned int x;

	if (!val)
		return;

	cgroup_rstat_updated(memcg->css.cgroup, smp_processor_id());

	x = __this_cpu_add_return(stats_updates, abs(val));
	if (x > MEMCG_CHARGE_BATCH) {
		 
		if (atomic_read(&stats_flush_threshold) <= num_online_cpus())
			atomic_add(x / MEMCG_CHARGE_BATCH, &stats_flush_threshold);
		__this_cpu_write(stats_updates, 0);
	}
}

static void do_flush_stats(void)
{
	 
	if (atomic_read(&stats_flush_ongoing) ||
	    atomic_xchg(&stats_flush_ongoing, 1))
		return;

	WRITE_ONCE(flush_next_time, jiffies_64 + 2*FLUSH_TIME);

	cgroup_rstat_flush(root_mem_cgroup->css.cgroup);

	atomic_set(&stats_flush_threshold, 0);
	atomic_set(&stats_flush_ongoing, 0);
}

void mem_cgroup_flush_stats(void)
{
	if (atomic_read(&stats_flush_threshold) > num_online_cpus())
		do_flush_stats();
}

void mem_cgroup_flush_stats_ratelimited(void)
{
	if (time_after64(jiffies_64, READ_ONCE(flush_next_time)))
		mem_cgroup_flush_stats();
}

static void flush_memcg_stats_dwork(struct work_struct *w)
{
	 
	do_flush_stats();
	queue_delayed_work(system_unbound_wq, &stats_flush_dwork, FLUSH_TIME);
}

 
static const unsigned int memcg_vm_event_stat[] = {
	PGPGIN,
	PGPGOUT,
	PGSCAN_KSWAPD,
	PGSCAN_DIRECT,
	PGSCAN_KHUGEPAGED,
	PGSTEAL_KSWAPD,
	PGSTEAL_DIRECT,
	PGSTEAL_KHUGEPAGED,
	PGFAULT,
	PGMAJFAULT,
	PGREFILL,
	PGACTIVATE,
	PGDEACTIVATE,
	PGLAZYFREE,
	PGLAZYFREED,
#if defined(CONFIG_MEMCG_KMEM) && defined(CONFIG_ZSWAP)
	ZSWPIN,
	ZSWPOUT,
#endif
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	THP_FAULT_ALLOC,
	THP_COLLAPSE_ALLOC,
#endif
};

#define NR_MEMCG_EVENTS ARRAY_SIZE(memcg_vm_event_stat)
static int mem_cgroup_events_index[NR_VM_EVENT_ITEMS] __read_mostly;

static void init_memcg_events(void)
{
	int i;

	for (i = 0; i < NR_MEMCG_EVENTS; ++i)
		mem_cgroup_events_index[memcg_vm_event_stat[i]] = i + 1;
}

static inline int memcg_events_index(enum vm_event_item idx)
{
	return mem_cgroup_events_index[idx] - 1;
}

struct memcg_vmstats_percpu {
	 
	long			state[MEMCG_NR_STAT];
	unsigned long		events[NR_MEMCG_EVENTS];

	 
	long			state_prev[MEMCG_NR_STAT];
	unsigned long		events_prev[NR_MEMCG_EVENTS];

	 
	unsigned long		nr_page_events;
	unsigned long		targets[MEM_CGROUP_NTARGETS];
};

struct memcg_vmstats {
	 
	long			state[MEMCG_NR_STAT];
	unsigned long		events[NR_MEMCG_EVENTS];

	 
	long			state_local[MEMCG_NR_STAT];
	unsigned long		events_local[NR_MEMCG_EVENTS];

	 
	long			state_pending[MEMCG_NR_STAT];
	unsigned long		events_pending[NR_MEMCG_EVENTS];
};

unsigned long memcg_page_state(struct mem_cgroup *memcg, int idx)
{
	long x = READ_ONCE(memcg->vmstats->state[idx]);
#ifdef CONFIG_SMP
	if (x < 0)
		x = 0;
#endif
	return x;
}

 
void __mod_memcg_state(struct mem_cgroup *memcg, int idx, int val)
{
	if (mem_cgroup_disabled())
		return;

	__this_cpu_add(memcg->vmstats_percpu->state[idx], val);
	memcg_rstat_updated(memcg, val);
}

 
static unsigned long memcg_page_state_local(struct mem_cgroup *memcg, int idx)
{
	long x = READ_ONCE(memcg->vmstats->state_local[idx]);

#ifdef CONFIG_SMP
	if (x < 0)
		x = 0;
#endif
	return x;
}

void __mod_memcg_lruvec_state(struct lruvec *lruvec, enum node_stat_item idx,
			      int val)
{
	struct mem_cgroup_per_node *pn;
	struct mem_cgroup *memcg;

	pn = container_of(lruvec, struct mem_cgroup_per_node, lruvec);
	memcg = pn->memcg;

	 
	__memcg_stats_lock();
	if (IS_ENABLED(CONFIG_DEBUG_VM)) {
		switch (idx) {
		case NR_ANON_MAPPED:
		case NR_FILE_MAPPED:
		case NR_ANON_THPS:
		case NR_SHMEM_PMDMAPPED:
		case NR_FILE_PMDMAPPED:
			WARN_ON_ONCE(!in_task());
			break;
		default:
			VM_WARN_ON_IRQS_ENABLED();
		}
	}

	 
	__this_cpu_add(memcg->vmstats_percpu->state[idx], val);

	 
	__this_cpu_add(pn->lruvec_stats_percpu->state[idx], val);

	memcg_rstat_updated(memcg, val);
	memcg_stats_unlock();
}

 
void __mod_lruvec_state(struct lruvec *lruvec, enum node_stat_item idx,
			int val)
{
	 
	__mod_node_page_state(lruvec_pgdat(lruvec), idx, val);

	 
	if (!mem_cgroup_disabled())
		__mod_memcg_lruvec_state(lruvec, idx, val);
}

void __mod_lruvec_page_state(struct page *page, enum node_stat_item idx,
			     int val)
{
	struct page *head = compound_head(page);  
	struct mem_cgroup *memcg;
	pg_data_t *pgdat = page_pgdat(page);
	struct lruvec *lruvec;

	rcu_read_lock();
	memcg = page_memcg(head);
	 
	if (!memcg) {
		rcu_read_unlock();
		__mod_node_page_state(pgdat, idx, val);
		return;
	}

	lruvec = mem_cgroup_lruvec(memcg, pgdat);
	__mod_lruvec_state(lruvec, idx, val);
	rcu_read_unlock();
}
EXPORT_SYMBOL(__mod_lruvec_page_state);

void __mod_lruvec_kmem_state(void *p, enum node_stat_item idx, int val)
{
	pg_data_t *pgdat = page_pgdat(virt_to_page(p));
	struct mem_cgroup *memcg;
	struct lruvec *lruvec;

	rcu_read_lock();
	memcg = mem_cgroup_from_slab_obj(p);

	 
	if (!memcg) {
		__mod_node_page_state(pgdat, idx, val);
	} else {
		lruvec = mem_cgroup_lruvec(memcg, pgdat);
		__mod_lruvec_state(lruvec, idx, val);
	}
	rcu_read_unlock();
}

 
void __count_memcg_events(struct mem_cgroup *memcg, enum vm_event_item idx,
			  unsigned long count)
{
	int index = memcg_events_index(idx);

	if (mem_cgroup_disabled() || index < 0)
		return;

	memcg_stats_lock();
	__this_cpu_add(memcg->vmstats_percpu->events[index], count);
	memcg_rstat_updated(memcg, count);
	memcg_stats_unlock();
}

static unsigned long memcg_events(struct mem_cgroup *memcg, int event)
{
	int index = memcg_events_index(event);

	if (index < 0)
		return 0;
	return READ_ONCE(memcg->vmstats->events[index]);
}

static unsigned long memcg_events_local(struct mem_cgroup *memcg, int event)
{
	int index = memcg_events_index(event);

	if (index < 0)
		return 0;

	return READ_ONCE(memcg->vmstats->events_local[index]);
}

static void mem_cgroup_charge_statistics(struct mem_cgroup *memcg,
					 int nr_pages)
{
	 
	if (nr_pages > 0)
		__count_memcg_events(memcg, PGPGIN, 1);
	else {
		__count_memcg_events(memcg, PGPGOUT, 1);
		nr_pages = -nr_pages;  
	}

	__this_cpu_add(memcg->vmstats_percpu->nr_page_events, nr_pages);
}

static bool mem_cgroup_event_ratelimit(struct mem_cgroup *memcg,
				       enum mem_cgroup_events_target target)
{
	unsigned long val, next;

	val = __this_cpu_read(memcg->vmstats_percpu->nr_page_events);
	next = __this_cpu_read(memcg->vmstats_percpu->targets[target]);
	 
	if ((long)(next - val) < 0) {
		switch (target) {
		case MEM_CGROUP_TARGET_THRESH:
			next = val + THRESHOLDS_EVENTS_TARGET;
			break;
		case MEM_CGROUP_TARGET_SOFTLIMIT:
			next = val + SOFTLIMIT_EVENTS_TARGET;
			break;
		default:
			break;
		}
		__this_cpu_write(memcg->vmstats_percpu->targets[target], next);
		return true;
	}
	return false;
}

 
static void memcg_check_events(struct mem_cgroup *memcg, int nid)
{
	if (IS_ENABLED(CONFIG_PREEMPT_RT))
		return;

	 
	if (unlikely(mem_cgroup_event_ratelimit(memcg,
						MEM_CGROUP_TARGET_THRESH))) {
		bool do_softlimit;

		do_softlimit = mem_cgroup_event_ratelimit(memcg,
						MEM_CGROUP_TARGET_SOFTLIMIT);
		mem_cgroup_threshold(memcg);
		if (unlikely(do_softlimit))
			mem_cgroup_update_tree(memcg, nid);
	}
}

struct mem_cgroup *mem_cgroup_from_task(struct task_struct *p)
{
	 
	if (unlikely(!p))
		return NULL;

	return mem_cgroup_from_css(task_css(p, memory_cgrp_id));
}
EXPORT_SYMBOL(mem_cgroup_from_task);

static __always_inline struct mem_cgroup *active_memcg(void)
{
	if (!in_task())
		return this_cpu_read(int_active_memcg);
	else
		return current->active_memcg;
}

 
struct mem_cgroup *get_mem_cgroup_from_mm(struct mm_struct *mm)
{
	struct mem_cgroup *memcg;

	if (mem_cgroup_disabled())
		return NULL;

	 
	if (unlikely(!mm)) {
		memcg = active_memcg();
		if (unlikely(memcg)) {
			 
			css_get(&memcg->css);
			return memcg;
		}
		mm = current->mm;
		if (unlikely(!mm))
			return root_mem_cgroup;
	}

	rcu_read_lock();
	do {
		memcg = mem_cgroup_from_task(rcu_dereference(mm->owner));
		if (unlikely(!memcg))
			memcg = root_mem_cgroup;
	} while (!css_tryget(&memcg->css));
	rcu_read_unlock();
	return memcg;
}
EXPORT_SYMBOL(get_mem_cgroup_from_mm);

static __always_inline bool memcg_kmem_bypass(void)
{
	 
	if (unlikely(active_memcg()))
		return false;

	 
	if (!in_task() || !current->mm || (current->flags & PF_KTHREAD))
		return true;

	return false;
}

 
struct mem_cgroup *mem_cgroup_iter(struct mem_cgroup *root,
				   struct mem_cgroup *prev,
				   struct mem_cgroup_reclaim_cookie *reclaim)
{
	struct mem_cgroup_reclaim_iter *iter;
	struct cgroup_subsys_state *css = NULL;
	struct mem_cgroup *memcg = NULL;
	struct mem_cgroup *pos = NULL;

	if (mem_cgroup_disabled())
		return NULL;

	if (!root)
		root = root_mem_cgroup;

	rcu_read_lock();

	if (reclaim) {
		struct mem_cgroup_per_node *mz;

		mz = root->nodeinfo[reclaim->pgdat->node_id];
		iter = &mz->iter;

		 
		if (!prev)
			reclaim->generation = iter->generation;
		else if (reclaim->generation != iter->generation)
			goto out_unlock;

		while (1) {
			pos = READ_ONCE(iter->position);
			if (!pos || css_tryget(&pos->css))
				break;
			 
			(void)cmpxchg(&iter->position, pos, NULL);
		}
	} else if (prev) {
		pos = prev;
	}

	if (pos)
		css = &pos->css;

	for (;;) {
		css = css_next_descendant_pre(css, &root->css);
		if (!css) {
			 
			if (!prev)
				continue;
			break;
		}

		 
		if (css == &root->css || css_tryget(css)) {
			memcg = mem_cgroup_from_css(css);
			break;
		}
	}

	if (reclaim) {
		 
		(void)cmpxchg(&iter->position, pos, memcg);

		if (pos)
			css_put(&pos->css);

		if (!memcg)
			iter->generation++;
	}

out_unlock:
	rcu_read_unlock();
	if (prev && prev != root)
		css_put(&prev->css);

	return memcg;
}

 
void mem_cgroup_iter_break(struct mem_cgroup *root,
			   struct mem_cgroup *prev)
{
	if (!root)
		root = root_mem_cgroup;
	if (prev && prev != root)
		css_put(&prev->css);
}

static void __invalidate_reclaim_iterators(struct mem_cgroup *from,
					struct mem_cgroup *dead_memcg)
{
	struct mem_cgroup_reclaim_iter *iter;
	struct mem_cgroup_per_node *mz;
	int nid;

	for_each_node(nid) {
		mz = from->nodeinfo[nid];
		iter = &mz->iter;
		cmpxchg(&iter->position, dead_memcg, NULL);
	}
}

static void invalidate_reclaim_iterators(struct mem_cgroup *dead_memcg)
{
	struct mem_cgroup *memcg = dead_memcg;
	struct mem_cgroup *last;

	do {
		__invalidate_reclaim_iterators(memcg, dead_memcg);
		last = memcg;
	} while ((memcg = parent_mem_cgroup(memcg)));

	 
	if (!mem_cgroup_is_root(last))
		__invalidate_reclaim_iterators(root_mem_cgroup,
						dead_memcg);
}

 
void mem_cgroup_scan_tasks(struct mem_cgroup *memcg,
			   int (*fn)(struct task_struct *, void *), void *arg)
{
	struct mem_cgroup *iter;
	int ret = 0;

	BUG_ON(mem_cgroup_is_root(memcg));

	for_each_mem_cgroup_tree(iter, memcg) {
		struct css_task_iter it;
		struct task_struct *task;

		css_task_iter_start(&iter->css, CSS_TASK_ITER_PROCS, &it);
		while (!ret && (task = css_task_iter_next(&it)))
			ret = fn(task, arg);
		css_task_iter_end(&it);
		if (ret) {
			mem_cgroup_iter_break(memcg, iter);
			break;
		}
	}
}

#ifdef CONFIG_DEBUG_VM
void lruvec_memcg_debug(struct lruvec *lruvec, struct folio *folio)
{
	struct mem_cgroup *memcg;

	if (mem_cgroup_disabled())
		return;

	memcg = folio_memcg(folio);

	if (!memcg)
		VM_BUG_ON_FOLIO(!mem_cgroup_is_root(lruvec_memcg(lruvec)), folio);
	else
		VM_BUG_ON_FOLIO(lruvec_memcg(lruvec) != memcg, folio);
}
#endif

 
struct lruvec *folio_lruvec_lock(struct folio *folio)
{
	struct lruvec *lruvec = folio_lruvec(folio);

	spin_lock(&lruvec->lru_lock);
	lruvec_memcg_debug(lruvec, folio);

	return lruvec;
}

 
struct lruvec *folio_lruvec_lock_irq(struct folio *folio)
{
	struct lruvec *lruvec = folio_lruvec(folio);

	spin_lock_irq(&lruvec->lru_lock);
	lruvec_memcg_debug(lruvec, folio);

	return lruvec;
}

 
struct lruvec *folio_lruvec_lock_irqsave(struct folio *folio,
		unsigned long *flags)
{
	struct lruvec *lruvec = folio_lruvec(folio);

	spin_lock_irqsave(&lruvec->lru_lock, *flags);
	lruvec_memcg_debug(lruvec, folio);

	return lruvec;
}

 
void mem_cgroup_update_lru_size(struct lruvec *lruvec, enum lru_list lru,
				int zid, int nr_pages)
{
	struct mem_cgroup_per_node *mz;
	unsigned long *lru_size;
	long size;

	if (mem_cgroup_disabled())
		return;

	mz = container_of(lruvec, struct mem_cgroup_per_node, lruvec);
	lru_size = &mz->lru_zone_size[zid][lru];

	if (nr_pages < 0)
		*lru_size += nr_pages;

	size = *lru_size;
	if (WARN_ONCE(size < 0,
		"%s(%p, %d, %d): lru_size %ld\n",
		__func__, lruvec, lru, nr_pages, size)) {
		VM_BUG_ON(1);
		*lru_size = 0;
	}

	if (nr_pages > 0)
		*lru_size += nr_pages;
}

 
static unsigned long mem_cgroup_margin(struct mem_cgroup *memcg)
{
	unsigned long margin = 0;
	unsigned long count;
	unsigned long limit;

	count = page_counter_read(&memcg->memory);
	limit = READ_ONCE(memcg->memory.max);
	if (count < limit)
		margin = limit - count;

	if (do_memsw_account()) {
		count = page_counter_read(&memcg->memsw);
		limit = READ_ONCE(memcg->memsw.max);
		if (count < limit)
			margin = min(margin, limit - count);
		else
			margin = 0;
	}

	return margin;
}

 
static bool mem_cgroup_under_move(struct mem_cgroup *memcg)
{
	struct mem_cgroup *from;
	struct mem_cgroup *to;
	bool ret = false;
	 
	spin_lock(&mc.lock);
	from = mc.from;
	to = mc.to;
	if (!from)
		goto unlock;

	ret = mem_cgroup_is_descendant(from, memcg) ||
		mem_cgroup_is_descendant(to, memcg);
unlock:
	spin_unlock(&mc.lock);
	return ret;
}

static bool mem_cgroup_wait_acct_move(struct mem_cgroup *memcg)
{
	if (mc.moving_task && current != mc.moving_task) {
		if (mem_cgroup_under_move(memcg)) {
			DEFINE_WAIT(wait);
			prepare_to_wait(&mc.waitq, &wait, TASK_INTERRUPTIBLE);
			 
			if (mc.moving_task)
				schedule();
			finish_wait(&mc.waitq, &wait);
			return true;
		}
	}
	return false;
}

struct memory_stat {
	const char *name;
	unsigned int idx;
};

static const struct memory_stat memory_stats[] = {
	{ "anon",			NR_ANON_MAPPED			},
	{ "file",			NR_FILE_PAGES			},
	{ "kernel",			MEMCG_KMEM			},
	{ "kernel_stack",		NR_KERNEL_STACK_KB		},
	{ "pagetables",			NR_PAGETABLE			},
	{ "sec_pagetables",		NR_SECONDARY_PAGETABLE		},
	{ "percpu",			MEMCG_PERCPU_B			},
	{ "sock",			MEMCG_SOCK			},
	{ "vmalloc",			MEMCG_VMALLOC			},
	{ "shmem",			NR_SHMEM			},
#if defined(CONFIG_MEMCG_KMEM) && defined(CONFIG_ZSWAP)
	{ "zswap",			MEMCG_ZSWAP_B			},
	{ "zswapped",			MEMCG_ZSWAPPED			},
#endif
	{ "file_mapped",		NR_FILE_MAPPED			},
	{ "file_dirty",			NR_FILE_DIRTY			},
	{ "file_writeback",		NR_WRITEBACK			},
#ifdef CONFIG_SWAP
	{ "swapcached",			NR_SWAPCACHE			},
#endif
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	{ "anon_thp",			NR_ANON_THPS			},
	{ "file_thp",			NR_FILE_THPS			},
	{ "shmem_thp",			NR_SHMEM_THPS			},
#endif
	{ "inactive_anon",		NR_INACTIVE_ANON		},
	{ "active_anon",		NR_ACTIVE_ANON			},
	{ "inactive_file",		NR_INACTIVE_FILE		},
	{ "active_file",		NR_ACTIVE_FILE			},
	{ "unevictable",		NR_UNEVICTABLE			},
	{ "slab_reclaimable",		NR_SLAB_RECLAIMABLE_B		},
	{ "slab_unreclaimable",		NR_SLAB_UNRECLAIMABLE_B		},

	 
	{ "workingset_refault_anon",	WORKINGSET_REFAULT_ANON		},
	{ "workingset_refault_file",	WORKINGSET_REFAULT_FILE		},
	{ "workingset_activate_anon",	WORKINGSET_ACTIVATE_ANON	},
	{ "workingset_activate_file",	WORKINGSET_ACTIVATE_FILE	},
	{ "workingset_restore_anon",	WORKINGSET_RESTORE_ANON		},
	{ "workingset_restore_file",	WORKINGSET_RESTORE_FILE		},
	{ "workingset_nodereclaim",	WORKINGSET_NODERECLAIM		},
};

 
static int memcg_page_state_unit(int item)
{
	switch (item) {
	case MEMCG_PERCPU_B:
	case MEMCG_ZSWAP_B:
	case NR_SLAB_RECLAIMABLE_B:
	case NR_SLAB_UNRECLAIMABLE_B:
	case WORKINGSET_REFAULT_ANON:
	case WORKINGSET_REFAULT_FILE:
	case WORKINGSET_ACTIVATE_ANON:
	case WORKINGSET_ACTIVATE_FILE:
	case WORKINGSET_RESTORE_ANON:
	case WORKINGSET_RESTORE_FILE:
	case WORKINGSET_NODERECLAIM:
		return 1;
	case NR_KERNEL_STACK_KB:
		return SZ_1K;
	default:
		return PAGE_SIZE;
	}
}

static inline unsigned long memcg_page_state_output(struct mem_cgroup *memcg,
						    int item)
{
	return memcg_page_state(memcg, item) * memcg_page_state_unit(item);
}

static void memcg_stat_format(struct mem_cgroup *memcg, struct seq_buf *s)
{
	int i;

	 
	mem_cgroup_flush_stats();

	for (i = 0; i < ARRAY_SIZE(memory_stats); i++) {
		u64 size;

		size = memcg_page_state_output(memcg, memory_stats[i].idx);
		seq_buf_printf(s, "%s %llu\n", memory_stats[i].name, size);

		if (unlikely(memory_stats[i].idx == NR_SLAB_UNRECLAIMABLE_B)) {
			size += memcg_page_state_output(memcg,
							NR_SLAB_RECLAIMABLE_B);
			seq_buf_printf(s, "slab %llu\n", size);
		}
	}

	 
	seq_buf_printf(s, "pgscan %lu\n",
		       memcg_events(memcg, PGSCAN_KSWAPD) +
		       memcg_events(memcg, PGSCAN_DIRECT) +
		       memcg_events(memcg, PGSCAN_KHUGEPAGED));
	seq_buf_printf(s, "pgsteal %lu\n",
		       memcg_events(memcg, PGSTEAL_KSWAPD) +
		       memcg_events(memcg, PGSTEAL_DIRECT) +
		       memcg_events(memcg, PGSTEAL_KHUGEPAGED));

	for (i = 0; i < ARRAY_SIZE(memcg_vm_event_stat); i++) {
		if (memcg_vm_event_stat[i] == PGPGIN ||
		    memcg_vm_event_stat[i] == PGPGOUT)
			continue;

		seq_buf_printf(s, "%s %lu\n",
			       vm_event_name(memcg_vm_event_stat[i]),
			       memcg_events(memcg, memcg_vm_event_stat[i]));
	}

	 
	WARN_ON_ONCE(seq_buf_has_overflowed(s));
}

static void memcg1_stat_format(struct mem_cgroup *memcg, struct seq_buf *s);

static void memory_stat_format(struct mem_cgroup *memcg, struct seq_buf *s)
{
	if (cgroup_subsys_on_dfl(memory_cgrp_subsys))
		memcg_stat_format(memcg, s);
	else
		memcg1_stat_format(memcg, s);
	WARN_ON_ONCE(seq_buf_has_overflowed(s));
}

 
void mem_cgroup_print_oom_context(struct mem_cgroup *memcg, struct task_struct *p)
{
	rcu_read_lock();

	if (memcg) {
		pr_cont(",oom_memcg=");
		pr_cont_cgroup_path(memcg->css.cgroup);
	} else
		pr_cont(",global_oom");
	if (p) {
		pr_cont(",task_memcg=");
		pr_cont_cgroup_path(task_cgroup(p, memory_cgrp_id));
	}
	rcu_read_unlock();
}

 
void mem_cgroup_print_oom_meminfo(struct mem_cgroup *memcg)
{
	 
	static char buf[PAGE_SIZE];
	struct seq_buf s;

	lockdep_assert_held(&oom_lock);

	pr_info("memory: usage %llukB, limit %llukB, failcnt %lu\n",
		K((u64)page_counter_read(&memcg->memory)),
		K((u64)READ_ONCE(memcg->memory.max)), memcg->memory.failcnt);
	if (cgroup_subsys_on_dfl(memory_cgrp_subsys))
		pr_info("swap: usage %llukB, limit %llukB, failcnt %lu\n",
			K((u64)page_counter_read(&memcg->swap)),
			K((u64)READ_ONCE(memcg->swap.max)), memcg->swap.failcnt);
	else {
		pr_info("memory+swap: usage %llukB, limit %llukB, failcnt %lu\n",
			K((u64)page_counter_read(&memcg->memsw)),
			K((u64)memcg->memsw.max), memcg->memsw.failcnt);
		pr_info("kmem: usage %llukB, limit %llukB, failcnt %lu\n",
			K((u64)page_counter_read(&memcg->kmem)),
			K((u64)memcg->kmem.max), memcg->kmem.failcnt);
	}

	pr_info("Memory cgroup stats for ");
	pr_cont_cgroup_path(memcg->css.cgroup);
	pr_cont(":");
	seq_buf_init(&s, buf, sizeof(buf));
	memory_stat_format(memcg, &s);
	seq_buf_do_printk(&s, KERN_INFO);
}

 
unsigned long mem_cgroup_get_max(struct mem_cgroup *memcg)
{
	unsigned long max = READ_ONCE(memcg->memory.max);

	if (do_memsw_account()) {
		if (mem_cgroup_swappiness(memcg)) {
			 
			unsigned long swap = READ_ONCE(memcg->memsw.max) - max;

			max += min(swap, (unsigned long)total_swap_pages);
		}
	} else {
		if (mem_cgroup_swappiness(memcg))
			max += min(READ_ONCE(memcg->swap.max),
				   (unsigned long)total_swap_pages);
	}
	return max;
}

unsigned long mem_cgroup_size(struct mem_cgroup *memcg)
{
	return page_counter_read(&memcg->memory);
}

static bool mem_cgroup_out_of_memory(struct mem_cgroup *memcg, gfp_t gfp_mask,
				     int order)
{
	struct oom_control oc = {
		.zonelist = NULL,
		.nodemask = NULL,
		.memcg = memcg,
		.gfp_mask = gfp_mask,
		.order = order,
	};
	bool ret = true;

	if (mutex_lock_killable(&oom_lock))
		return true;

	if (mem_cgroup_margin(memcg) >= (1 << order))
		goto unlock;

	 
	ret = task_is_dying() || out_of_memory(&oc);

unlock:
	mutex_unlock(&oom_lock);
	return ret;
}

static int mem_cgroup_soft_reclaim(struct mem_cgroup *root_memcg,
				   pg_data_t *pgdat,
				   gfp_t gfp_mask,
				   unsigned long *total_scanned)
{
	struct mem_cgroup *victim = NULL;
	int total = 0;
	int loop = 0;
	unsigned long excess;
	unsigned long nr_scanned;
	struct mem_cgroup_reclaim_cookie reclaim = {
		.pgdat = pgdat,
	};

	excess = soft_limit_excess(root_memcg);

	while (1) {
		victim = mem_cgroup_iter(root_memcg, victim, &reclaim);
		if (!victim) {
			loop++;
			if (loop >= 2) {
				 
				if (!total)
					break;
				 
				if (total >= (excess >> 2) ||
					(loop > MEM_CGROUP_MAX_RECLAIM_LOOPS))
					break;
			}
			continue;
		}
		total += mem_cgroup_shrink_node(victim, gfp_mask, false,
					pgdat, &nr_scanned);
		*total_scanned += nr_scanned;
		if (!soft_limit_excess(root_memcg))
			break;
	}
	mem_cgroup_iter_break(root_memcg, victim);
	return total;
}

#ifdef CONFIG_LOCKDEP
static struct lockdep_map memcg_oom_lock_dep_map = {
	.name = "memcg_oom_lock",
};
#endif

static DEFINE_SPINLOCK(memcg_oom_lock);

 
static bool mem_cgroup_oom_trylock(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter, *failed = NULL;

	spin_lock(&memcg_oom_lock);

	for_each_mem_cgroup_tree(iter, memcg) {
		if (iter->oom_lock) {
			 
			failed = iter;
			mem_cgroup_iter_break(memcg, iter);
			break;
		} else
			iter->oom_lock = true;
	}

	if (failed) {
		 
		for_each_mem_cgroup_tree(iter, memcg) {
			if (iter == failed) {
				mem_cgroup_iter_break(memcg, iter);
				break;
			}
			iter->oom_lock = false;
		}
	} else
		mutex_acquire(&memcg_oom_lock_dep_map, 0, 1, _RET_IP_);

	spin_unlock(&memcg_oom_lock);

	return !failed;
}

static void mem_cgroup_oom_unlock(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter;

	spin_lock(&memcg_oom_lock);
	mutex_release(&memcg_oom_lock_dep_map, _RET_IP_);
	for_each_mem_cgroup_tree(iter, memcg)
		iter->oom_lock = false;
	spin_unlock(&memcg_oom_lock);
}

static void mem_cgroup_mark_under_oom(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter;

	spin_lock(&memcg_oom_lock);
	for_each_mem_cgroup_tree(iter, memcg)
		iter->under_oom++;
	spin_unlock(&memcg_oom_lock);
}

static void mem_cgroup_unmark_under_oom(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter;

	 
	spin_lock(&memcg_oom_lock);
	for_each_mem_cgroup_tree(iter, memcg)
		if (iter->under_oom > 0)
			iter->under_oom--;
	spin_unlock(&memcg_oom_lock);
}

static DECLARE_WAIT_QUEUE_HEAD(memcg_oom_waitq);

struct oom_wait_info {
	struct mem_cgroup *memcg;
	wait_queue_entry_t	wait;
};

static int memcg_oom_wake_function(wait_queue_entry_t *wait,
	unsigned mode, int sync, void *arg)
{
	struct mem_cgroup *wake_memcg = (struct mem_cgroup *)arg;
	struct mem_cgroup *oom_wait_memcg;
	struct oom_wait_info *oom_wait_info;

	oom_wait_info = container_of(wait, struct oom_wait_info, wait);
	oom_wait_memcg = oom_wait_info->memcg;

	if (!mem_cgroup_is_descendant(wake_memcg, oom_wait_memcg) &&
	    !mem_cgroup_is_descendant(oom_wait_memcg, wake_memcg))
		return 0;
	return autoremove_wake_function(wait, mode, sync, arg);
}

static void memcg_oom_recover(struct mem_cgroup *memcg)
{
	 
	if (memcg && memcg->under_oom)
		__wake_up(&memcg_oom_waitq, TASK_NORMAL, 0, memcg);
}

 
static bool mem_cgroup_oom(struct mem_cgroup *memcg, gfp_t mask, int order)
{
	bool locked, ret;

	if (order > PAGE_ALLOC_COSTLY_ORDER)
		return false;

	memcg_memory_event(memcg, MEMCG_OOM);

	 
	if (READ_ONCE(memcg->oom_kill_disable)) {
		if (current->in_user_fault) {
			css_get(&memcg->css);
			current->memcg_in_oom = memcg;
			current->memcg_oom_gfp_mask = mask;
			current->memcg_oom_order = order;
		}
		return false;
	}

	mem_cgroup_mark_under_oom(memcg);

	locked = mem_cgroup_oom_trylock(memcg);

	if (locked)
		mem_cgroup_oom_notify(memcg);

	mem_cgroup_unmark_under_oom(memcg);
	ret = mem_cgroup_out_of_memory(memcg, mask, order);

	if (locked)
		mem_cgroup_oom_unlock(memcg);

	return ret;
}

 
bool mem_cgroup_oom_synchronize(bool handle)
{
	struct mem_cgroup *memcg = current->memcg_in_oom;
	struct oom_wait_info owait;
	bool locked;

	 
	if (!memcg)
		return false;

	if (!handle)
		goto cleanup;

	owait.memcg = memcg;
	owait.wait.flags = 0;
	owait.wait.func = memcg_oom_wake_function;
	owait.wait.private = current;
	INIT_LIST_HEAD(&owait.wait.entry);

	prepare_to_wait(&memcg_oom_waitq, &owait.wait, TASK_KILLABLE);
	mem_cgroup_mark_under_oom(memcg);

	locked = mem_cgroup_oom_trylock(memcg);

	if (locked)
		mem_cgroup_oom_notify(memcg);

	schedule();
	mem_cgroup_unmark_under_oom(memcg);
	finish_wait(&memcg_oom_waitq, &owait.wait);

	if (locked)
		mem_cgroup_oom_unlock(memcg);
cleanup:
	current->memcg_in_oom = NULL;
	css_put(&memcg->css);
	return true;
}

 
struct mem_cgroup *mem_cgroup_get_oom_group(struct task_struct *victim,
					    struct mem_cgroup *oom_domain)
{
	struct mem_cgroup *oom_group = NULL;
	struct mem_cgroup *memcg;

	if (!cgroup_subsys_on_dfl(memory_cgrp_subsys))
		return NULL;

	if (!oom_domain)
		oom_domain = root_mem_cgroup;

	rcu_read_lock();

	memcg = mem_cgroup_from_task(victim);
	if (mem_cgroup_is_root(memcg))
		goto out;

	 
	if (unlikely(!mem_cgroup_is_descendant(memcg, oom_domain)))
		goto out;

	 
	for (; memcg; memcg = parent_mem_cgroup(memcg)) {
		if (READ_ONCE(memcg->oom_group))
			oom_group = memcg;

		if (memcg == oom_domain)
			break;
	}

	if (oom_group)
		css_get(&oom_group->css);
out:
	rcu_read_unlock();

	return oom_group;
}

void mem_cgroup_print_oom_group(struct mem_cgroup *memcg)
{
	pr_info("Tasks in ");
	pr_cont_cgroup_path(memcg->css.cgroup);
	pr_cont(" are going to be killed due to memory.oom.group set\n");
}

 
void folio_memcg_lock(struct folio *folio)
{
	struct mem_cgroup *memcg;
	unsigned long flags;

	 
	rcu_read_lock();

	if (mem_cgroup_disabled())
		return;
again:
	memcg = folio_memcg(folio);
	if (unlikely(!memcg))
		return;

#ifdef CONFIG_PROVE_LOCKING
	local_irq_save(flags);
	might_lock(&memcg->move_lock);
	local_irq_restore(flags);
#endif

	if (atomic_read(&memcg->moving_account) <= 0)
		return;

	spin_lock_irqsave(&memcg->move_lock, flags);
	if (memcg != folio_memcg(folio)) {
		spin_unlock_irqrestore(&memcg->move_lock, flags);
		goto again;
	}

	 
	memcg->move_lock_task = current;
	memcg->move_lock_flags = flags;
}

static void __folio_memcg_unlock(struct mem_cgroup *memcg)
{
	if (memcg && memcg->move_lock_task == current) {
		unsigned long flags = memcg->move_lock_flags;

		memcg->move_lock_task = NULL;
		memcg->move_lock_flags = 0;

		spin_unlock_irqrestore(&memcg->move_lock, flags);
	}

	rcu_read_unlock();
}

 
void folio_memcg_unlock(struct folio *folio)
{
	__folio_memcg_unlock(folio_memcg(folio));
}

struct memcg_stock_pcp {
	local_lock_t stock_lock;
	struct mem_cgroup *cached;  
	unsigned int nr_pages;

#ifdef CONFIG_MEMCG_KMEM
	struct obj_cgroup *cached_objcg;
	struct pglist_data *cached_pgdat;
	unsigned int nr_bytes;
	int nr_slab_reclaimable_b;
	int nr_slab_unreclaimable_b;
#endif

	struct work_struct work;
	unsigned long flags;
#define FLUSHING_CACHED_CHARGE	0
};
static DEFINE_PER_CPU(struct memcg_stock_pcp, memcg_stock) = {
	.stock_lock = INIT_LOCAL_LOCK(stock_lock),
};
static DEFINE_MUTEX(percpu_charge_mutex);

#ifdef CONFIG_MEMCG_KMEM
static struct obj_cgroup *drain_obj_stock(struct memcg_stock_pcp *stock);
static bool obj_stock_flush_required(struct memcg_stock_pcp *stock,
				     struct mem_cgroup *root_memcg);
static void memcg_account_kmem(struct mem_cgroup *memcg, int nr_pages);

#else
static inline struct obj_cgroup *drain_obj_stock(struct memcg_stock_pcp *stock)
{
	return NULL;
}
static bool obj_stock_flush_required(struct memcg_stock_pcp *stock,
				     struct mem_cgroup *root_memcg)
{
	return false;
}
static void memcg_account_kmem(struct mem_cgroup *memcg, int nr_pages)
{
}
#endif

 
static bool consume_stock(struct mem_cgroup *memcg, unsigned int nr_pages)
{
	struct memcg_stock_pcp *stock;
	unsigned long flags;
	bool ret = false;

	if (nr_pages > MEMCG_CHARGE_BATCH)
		return ret;

	local_lock_irqsave(&memcg_stock.stock_lock, flags);

	stock = this_cpu_ptr(&memcg_stock);
	if (memcg == READ_ONCE(stock->cached) && stock->nr_pages >= nr_pages) {
		stock->nr_pages -= nr_pages;
		ret = true;
	}

	local_unlock_irqrestore(&memcg_stock.stock_lock, flags);

	return ret;
}

 
static void drain_stock(struct memcg_stock_pcp *stock)
{
	struct mem_cgroup *old = READ_ONCE(stock->cached);

	if (!old)
		return;

	if (stock->nr_pages) {
		page_counter_uncharge(&old->memory, stock->nr_pages);
		if (do_memsw_account())
			page_counter_uncharge(&old->memsw, stock->nr_pages);
		stock->nr_pages = 0;
	}

	css_put(&old->css);
	WRITE_ONCE(stock->cached, NULL);
}

static void drain_local_stock(struct work_struct *dummy)
{
	struct memcg_stock_pcp *stock;
	struct obj_cgroup *old = NULL;
	unsigned long flags;

	 
	local_lock_irqsave(&memcg_stock.stock_lock, flags);

	stock = this_cpu_ptr(&memcg_stock);
	old = drain_obj_stock(stock);
	drain_stock(stock);
	clear_bit(FLUSHING_CACHED_CHARGE, &stock->flags);

	local_unlock_irqrestore(&memcg_stock.stock_lock, flags);
	if (old)
		obj_cgroup_put(old);
}

 
static void __refill_stock(struct mem_cgroup *memcg, unsigned int nr_pages)
{
	struct memcg_stock_pcp *stock;

	stock = this_cpu_ptr(&memcg_stock);
	if (READ_ONCE(stock->cached) != memcg) {  
		drain_stock(stock);
		css_get(&memcg->css);
		WRITE_ONCE(stock->cached, memcg);
	}
	stock->nr_pages += nr_pages;

	if (stock->nr_pages > MEMCG_CHARGE_BATCH)
		drain_stock(stock);
}

static void refill_stock(struct mem_cgroup *memcg, unsigned int nr_pages)
{
	unsigned long flags;

	local_lock_irqsave(&memcg_stock.stock_lock, flags);
	__refill_stock(memcg, nr_pages);
	local_unlock_irqrestore(&memcg_stock.stock_lock, flags);
}

 
static void drain_all_stock(struct mem_cgroup *root_memcg)
{
	int cpu, curcpu;

	 
	if (!mutex_trylock(&percpu_charge_mutex))
		return;
	 
	migrate_disable();
	curcpu = smp_processor_id();
	for_each_online_cpu(cpu) {
		struct memcg_stock_pcp *stock = &per_cpu(memcg_stock, cpu);
		struct mem_cgroup *memcg;
		bool flush = false;

		rcu_read_lock();
		memcg = READ_ONCE(stock->cached);
		if (memcg && stock->nr_pages &&
		    mem_cgroup_is_descendant(memcg, root_memcg))
			flush = true;
		else if (obj_stock_flush_required(stock, root_memcg))
			flush = true;
		rcu_read_unlock();

		if (flush &&
		    !test_and_set_bit(FLUSHING_CACHED_CHARGE, &stock->flags)) {
			if (cpu == curcpu)
				drain_local_stock(&stock->work);
			else if (!cpu_is_isolated(cpu))
				schedule_work_on(cpu, &stock->work);
		}
	}
	migrate_enable();
	mutex_unlock(&percpu_charge_mutex);
}

static int memcg_hotplug_cpu_dead(unsigned int cpu)
{
	struct memcg_stock_pcp *stock;

	stock = &per_cpu(memcg_stock, cpu);
	drain_stock(stock);

	return 0;
}

static unsigned long reclaim_high(struct mem_cgroup *memcg,
				  unsigned int nr_pages,
				  gfp_t gfp_mask)
{
	unsigned long nr_reclaimed = 0;

	do {
		unsigned long pflags;

		if (page_counter_read(&memcg->memory) <=
		    READ_ONCE(memcg->memory.high))
			continue;

		memcg_memory_event(memcg, MEMCG_HIGH);

		psi_memstall_enter(&pflags);
		nr_reclaimed += try_to_free_mem_cgroup_pages(memcg, nr_pages,
							gfp_mask,
							MEMCG_RECLAIM_MAY_SWAP);
		psi_memstall_leave(&pflags);
	} while ((memcg = parent_mem_cgroup(memcg)) &&
		 !mem_cgroup_is_root(memcg));

	return nr_reclaimed;
}

static void high_work_func(struct work_struct *work)
{
	struct mem_cgroup *memcg;

	memcg = container_of(work, struct mem_cgroup, high_work);
	reclaim_high(memcg, MEMCG_CHARGE_BATCH, GFP_KERNEL);
}

 
#define MEMCG_MAX_HIGH_DELAY_JIFFIES (2UL*HZ)

 
 #define MEMCG_DELAY_PRECISION_SHIFT 20
 #define MEMCG_DELAY_SCALING_SHIFT 14

static u64 calculate_overage(unsigned long usage, unsigned long high)
{
	u64 overage;

	if (usage <= high)
		return 0;

	 
	high = max(high, 1UL);

	overage = usage - high;
	overage <<= MEMCG_DELAY_PRECISION_SHIFT;
	return div64_u64(overage, high);
}

static u64 mem_find_max_overage(struct mem_cgroup *memcg)
{
	u64 overage, max_overage = 0;

	do {
		overage = calculate_overage(page_counter_read(&memcg->memory),
					    READ_ONCE(memcg->memory.high));
		max_overage = max(overage, max_overage);
	} while ((memcg = parent_mem_cgroup(memcg)) &&
		 !mem_cgroup_is_root(memcg));

	return max_overage;
}

static u64 swap_find_max_overage(struct mem_cgroup *memcg)
{
	u64 overage, max_overage = 0;

	do {
		overage = calculate_overage(page_counter_read(&memcg->swap),
					    READ_ONCE(memcg->swap.high));
		if (overage)
			memcg_memory_event(memcg, MEMCG_SWAP_HIGH);
		max_overage = max(overage, max_overage);
	} while ((memcg = parent_mem_cgroup(memcg)) &&
		 !mem_cgroup_is_root(memcg));

	return max_overage;
}

 
static unsigned long calculate_high_delay(struct mem_cgroup *memcg,
					  unsigned int nr_pages,
					  u64 max_overage)
{
	unsigned long penalty_jiffies;

	if (!max_overage)
		return 0;

	 
	penalty_jiffies = max_overage * max_overage * HZ;
	penalty_jiffies >>= MEMCG_DELAY_PRECISION_SHIFT;
	penalty_jiffies >>= MEMCG_DELAY_SCALING_SHIFT;

	 
	return penalty_jiffies * nr_pages / MEMCG_CHARGE_BATCH;
}

 
void mem_cgroup_handle_over_high(gfp_t gfp_mask)
{
	unsigned long penalty_jiffies;
	unsigned long pflags;
	unsigned long nr_reclaimed;
	unsigned int nr_pages = current->memcg_nr_pages_over_high;
	int nr_retries = MAX_RECLAIM_RETRIES;
	struct mem_cgroup *memcg;
	bool in_retry = false;

	if (likely(!nr_pages))
		return;

	memcg = get_mem_cgroup_from_mm(current->mm);
	current->memcg_nr_pages_over_high = 0;

retry_reclaim:
	 
	nr_reclaimed = reclaim_high(memcg,
				    in_retry ? SWAP_CLUSTER_MAX : nr_pages,
				    gfp_mask);

	 
	penalty_jiffies = calculate_high_delay(memcg, nr_pages,
					       mem_find_max_overage(memcg));

	penalty_jiffies += calculate_high_delay(memcg, nr_pages,
						swap_find_max_overage(memcg));

	 
	penalty_jiffies = min(penalty_jiffies, MEMCG_MAX_HIGH_DELAY_JIFFIES);

	 
	if (penalty_jiffies <= HZ / 100)
		goto out;

	 
	if (nr_reclaimed || nr_retries--) {
		in_retry = true;
		goto retry_reclaim;
	}

	 
	psi_memstall_enter(&pflags);
	schedule_timeout_killable(penalty_jiffies);
	psi_memstall_leave(&pflags);

out:
	css_put(&memcg->css);
}

static int try_charge_memcg(struct mem_cgroup *memcg, gfp_t gfp_mask,
			unsigned int nr_pages)
{
	unsigned int batch = max(MEMCG_CHARGE_BATCH, nr_pages);
	int nr_retries = MAX_RECLAIM_RETRIES;
	struct mem_cgroup *mem_over_limit;
	struct page_counter *counter;
	unsigned long nr_reclaimed;
	bool passed_oom = false;
	unsigned int reclaim_options = MEMCG_RECLAIM_MAY_SWAP;
	bool drained = false;
	bool raised_max_event = false;
	unsigned long pflags;

retry:
	if (consume_stock(memcg, nr_pages))
		return 0;

	if (!do_memsw_account() ||
	    page_counter_try_charge(&memcg->memsw, batch, &counter)) {
		if (page_counter_try_charge(&memcg->memory, batch, &counter))
			goto done_restock;
		if (do_memsw_account())
			page_counter_uncharge(&memcg->memsw, batch);
		mem_over_limit = mem_cgroup_from_counter(counter, memory);
	} else {
		mem_over_limit = mem_cgroup_from_counter(counter, memsw);
		reclaim_options &= ~MEMCG_RECLAIM_MAY_SWAP;
	}

	if (batch > nr_pages) {
		batch = nr_pages;
		goto retry;
	}

	 
	if (unlikely(current->flags & PF_MEMALLOC))
		goto force;

	if (unlikely(task_in_memcg_oom(current)))
		goto nomem;

	if (!gfpflags_allow_blocking(gfp_mask))
		goto nomem;

	memcg_memory_event(mem_over_limit, MEMCG_MAX);
	raised_max_event = true;

	psi_memstall_enter(&pflags);
	nr_reclaimed = try_to_free_mem_cgroup_pages(mem_over_limit, nr_pages,
						    gfp_mask, reclaim_options);
	psi_memstall_leave(&pflags);

	if (mem_cgroup_margin(mem_over_limit) >= nr_pages)
		goto retry;

	if (!drained) {
		drain_all_stock(mem_over_limit);
		drained = true;
		goto retry;
	}

	if (gfp_mask & __GFP_NORETRY)
		goto nomem;
	 
	if (nr_reclaimed && nr_pages <= (1 << PAGE_ALLOC_COSTLY_ORDER))
		goto retry;
	 
	if (mem_cgroup_wait_acct_move(mem_over_limit))
		goto retry;

	if (nr_retries--)
		goto retry;

	if (gfp_mask & __GFP_RETRY_MAYFAIL)
		goto nomem;

	 
	if (passed_oom && task_is_dying())
		goto nomem;

	 
	if (mem_cgroup_oom(mem_over_limit, gfp_mask,
			   get_order(nr_pages * PAGE_SIZE))) {
		passed_oom = true;
		nr_retries = MAX_RECLAIM_RETRIES;
		goto retry;
	}
nomem:
	 
	if (!(gfp_mask & (__GFP_NOFAIL | __GFP_HIGH)))
		return -ENOMEM;
force:
	 
	if (!raised_max_event)
		memcg_memory_event(mem_over_limit, MEMCG_MAX);

	 
	page_counter_charge(&memcg->memory, nr_pages);
	if (do_memsw_account())
		page_counter_charge(&memcg->memsw, nr_pages);

	return 0;

done_restock:
	if (batch > nr_pages)
		refill_stock(memcg, batch - nr_pages);

	 
	do {
		bool mem_high, swap_high;

		mem_high = page_counter_read(&memcg->memory) >
			READ_ONCE(memcg->memory.high);
		swap_high = page_counter_read(&memcg->swap) >
			READ_ONCE(memcg->swap.high);

		 
		if (!in_task()) {
			if (mem_high) {
				schedule_work(&memcg->high_work);
				break;
			}
			continue;
		}

		if (mem_high || swap_high) {
			 
			current->memcg_nr_pages_over_high += batch;
			set_notify_resume(current);
			break;
		}
	} while ((memcg = parent_mem_cgroup(memcg)));

	if (current->memcg_nr_pages_over_high > MEMCG_CHARGE_BATCH &&
	    !(current->flags & PF_MEMALLOC) &&
	    gfpflags_allow_blocking(gfp_mask)) {
		mem_cgroup_handle_over_high(gfp_mask);
	}
	return 0;
}

static inline int try_charge(struct mem_cgroup *memcg, gfp_t gfp_mask,
			     unsigned int nr_pages)
{
	if (mem_cgroup_is_root(memcg))
		return 0;

	return try_charge_memcg(memcg, gfp_mask, nr_pages);
}

static inline void cancel_charge(struct mem_cgroup *memcg, unsigned int nr_pages)
{
	if (mem_cgroup_is_root(memcg))
		return;

	page_counter_uncharge(&memcg->memory, nr_pages);
	if (do_memsw_account())
		page_counter_uncharge(&memcg->memsw, nr_pages);
}

static void commit_charge(struct folio *folio, struct mem_cgroup *memcg)
{
	VM_BUG_ON_FOLIO(folio_memcg(folio), folio);
	 
	folio->memcg_data = (unsigned long)memcg;
}

#ifdef CONFIG_MEMCG_KMEM
 
#define OBJCGS_CLEAR_MASK	(__GFP_DMA | __GFP_RECLAIMABLE | \
				 __GFP_ACCOUNT | __GFP_NOFAIL)

 
static inline void mod_objcg_mlstate(struct obj_cgroup *objcg,
				     struct pglist_data *pgdat,
				     enum node_stat_item idx, int nr)
{
	struct mem_cgroup *memcg;
	struct lruvec *lruvec;

	rcu_read_lock();
	memcg = obj_cgroup_memcg(objcg);
	lruvec = mem_cgroup_lruvec(memcg, pgdat);
	mod_memcg_lruvec_state(lruvec, idx, nr);
	rcu_read_unlock();
}

int memcg_alloc_slab_cgroups(struct slab *slab, struct kmem_cache *s,
				 gfp_t gfp, bool new_slab)
{
	unsigned int objects = objs_per_slab(s, slab);
	unsigned long memcg_data;
	void *vec;

	gfp &= ~OBJCGS_CLEAR_MASK;
	vec = kcalloc_node(objects, sizeof(struct obj_cgroup *), gfp,
			   slab_nid(slab));
	if (!vec)
		return -ENOMEM;

	memcg_data = (unsigned long) vec | MEMCG_DATA_OBJCGS;
	if (new_slab) {
		 
		slab->memcg_data = memcg_data;
	} else if (cmpxchg(&slab->memcg_data, 0, memcg_data)) {
		 
		kfree(vec);
		return 0;
	}

	kmemleak_not_leak(vec);
	return 0;
}

static __always_inline
struct mem_cgroup *mem_cgroup_from_obj_folio(struct folio *folio, void *p)
{
	 
	if (folio_test_slab(folio)) {
		struct obj_cgroup **objcgs;
		struct slab *slab;
		unsigned int off;

		slab = folio_slab(folio);
		objcgs = slab_objcgs(slab);
		if (!objcgs)
			return NULL;

		off = obj_to_index(slab->slab_cache, slab, p);
		if (objcgs[off])
			return obj_cgroup_memcg(objcgs[off]);

		return NULL;
	}

	 
	return folio_memcg_check(folio);
}

 
struct mem_cgroup *mem_cgroup_from_obj(void *p)
{
	struct folio *folio;

	if (mem_cgroup_disabled())
		return NULL;

	if (unlikely(is_vmalloc_addr(p)))
		folio = page_folio(vmalloc_to_page(p));
	else
		folio = virt_to_folio(p);

	return mem_cgroup_from_obj_folio(folio, p);
}

 
struct mem_cgroup *mem_cgroup_from_slab_obj(void *p)
{
	if (mem_cgroup_disabled())
		return NULL;

	return mem_cgroup_from_obj_folio(virt_to_folio(p), p);
}

static struct obj_cgroup *__get_obj_cgroup_from_memcg(struct mem_cgroup *memcg)
{
	struct obj_cgroup *objcg = NULL;

	for (; !mem_cgroup_is_root(memcg); memcg = parent_mem_cgroup(memcg)) {
		objcg = rcu_dereference(memcg->objcg);
		if (objcg && obj_cgroup_tryget(objcg))
			break;
		objcg = NULL;
	}
	return objcg;
}

__always_inline struct obj_cgroup *get_obj_cgroup_from_current(void)
{
	struct obj_cgroup *objcg = NULL;
	struct mem_cgroup *memcg;

	if (memcg_kmem_bypass())
		return NULL;

	rcu_read_lock();
	if (unlikely(active_memcg()))
		memcg = active_memcg();
	else
		memcg = mem_cgroup_from_task(current);
	objcg = __get_obj_cgroup_from_memcg(memcg);
	rcu_read_unlock();
	return objcg;
}

struct obj_cgroup *get_obj_cgroup_from_folio(struct folio *folio)
{
	struct obj_cgroup *objcg;

	if (!memcg_kmem_online())
		return NULL;

	if (folio_memcg_kmem(folio)) {
		objcg = __folio_objcg(folio);
		obj_cgroup_get(objcg);
	} else {
		struct mem_cgroup *memcg;

		rcu_read_lock();
		memcg = __folio_memcg(folio);
		if (memcg)
			objcg = __get_obj_cgroup_from_memcg(memcg);
		else
			objcg = NULL;
		rcu_read_unlock();
	}
	return objcg;
}

static void memcg_account_kmem(struct mem_cgroup *memcg, int nr_pages)
{
	mod_memcg_state(memcg, MEMCG_KMEM, nr_pages);
	if (!cgroup_subsys_on_dfl(memory_cgrp_subsys)) {
		if (nr_pages > 0)
			page_counter_charge(&memcg->kmem, nr_pages);
		else
			page_counter_uncharge(&memcg->kmem, -nr_pages);
	}
}


 
static void obj_cgroup_uncharge_pages(struct obj_cgroup *objcg,
				      unsigned int nr_pages)
{
	struct mem_cgroup *memcg;

	memcg = get_mem_cgroup_from_objcg(objcg);

	memcg_account_kmem(memcg, -nr_pages);
	refill_stock(memcg, nr_pages);

	css_put(&memcg->css);
}

 
static int obj_cgroup_charge_pages(struct obj_cgroup *objcg, gfp_t gfp,
				   unsigned int nr_pages)
{
	struct mem_cgroup *memcg;
	int ret;

	memcg = get_mem_cgroup_from_objcg(objcg);

	ret = try_charge_memcg(memcg, gfp, nr_pages);
	if (ret)
		goto out;

	memcg_account_kmem(memcg, nr_pages);
out:
	css_put(&memcg->css);

	return ret;
}

 
int __memcg_kmem_charge_page(struct page *page, gfp_t gfp, int order)
{
	struct obj_cgroup *objcg;
	int ret = 0;

	objcg = get_obj_cgroup_from_current();
	if (objcg) {
		ret = obj_cgroup_charge_pages(objcg, gfp, 1 << order);
		if (!ret) {
			page->memcg_data = (unsigned long)objcg |
				MEMCG_DATA_KMEM;
			return 0;
		}
		obj_cgroup_put(objcg);
	}
	return ret;
}

 
void __memcg_kmem_uncharge_page(struct page *page, int order)
{
	struct folio *folio = page_folio(page);
	struct obj_cgroup *objcg;
	unsigned int nr_pages = 1 << order;

	if (!folio_memcg_kmem(folio))
		return;

	objcg = __folio_objcg(folio);
	obj_cgroup_uncharge_pages(objcg, nr_pages);
	folio->memcg_data = 0;
	obj_cgroup_put(objcg);
}

void mod_objcg_state(struct obj_cgroup *objcg, struct pglist_data *pgdat,
		     enum node_stat_item idx, int nr)
{
	struct memcg_stock_pcp *stock;
	struct obj_cgroup *old = NULL;
	unsigned long flags;
	int *bytes;

	local_lock_irqsave(&memcg_stock.stock_lock, flags);
	stock = this_cpu_ptr(&memcg_stock);

	 
	if (READ_ONCE(stock->cached_objcg) != objcg) {
		old = drain_obj_stock(stock);
		obj_cgroup_get(objcg);
		stock->nr_bytes = atomic_read(&objcg->nr_charged_bytes)
				? atomic_xchg(&objcg->nr_charged_bytes, 0) : 0;
		WRITE_ONCE(stock->cached_objcg, objcg);
		stock->cached_pgdat = pgdat;
	} else if (stock->cached_pgdat != pgdat) {
		 
		struct pglist_data *oldpg = stock->cached_pgdat;

		if (stock->nr_slab_reclaimable_b) {
			mod_objcg_mlstate(objcg, oldpg, NR_SLAB_RECLAIMABLE_B,
					  stock->nr_slab_reclaimable_b);
			stock->nr_slab_reclaimable_b = 0;
		}
		if (stock->nr_slab_unreclaimable_b) {
			mod_objcg_mlstate(objcg, oldpg, NR_SLAB_UNRECLAIMABLE_B,
					  stock->nr_slab_unreclaimable_b);
			stock->nr_slab_unreclaimable_b = 0;
		}
		stock->cached_pgdat = pgdat;
	}

	bytes = (idx == NR_SLAB_RECLAIMABLE_B) ? &stock->nr_slab_reclaimable_b
					       : &stock->nr_slab_unreclaimable_b;
	 
	if (!*bytes) {
		*bytes = nr;
		nr = 0;
	} else {
		*bytes += nr;
		if (abs(*bytes) > PAGE_SIZE) {
			nr = *bytes;
			*bytes = 0;
		} else {
			nr = 0;
		}
	}
	if (nr)
		mod_objcg_mlstate(objcg, pgdat, idx, nr);

	local_unlock_irqrestore(&memcg_stock.stock_lock, flags);
	if (old)
		obj_cgroup_put(old);
}

static bool consume_obj_stock(struct obj_cgroup *objcg, unsigned int nr_bytes)
{
	struct memcg_stock_pcp *stock;
	unsigned long flags;
	bool ret = false;

	local_lock_irqsave(&memcg_stock.stock_lock, flags);

	stock = this_cpu_ptr(&memcg_stock);
	if (objcg == READ_ONCE(stock->cached_objcg) && stock->nr_bytes >= nr_bytes) {
		stock->nr_bytes -= nr_bytes;
		ret = true;
	}

	local_unlock_irqrestore(&memcg_stock.stock_lock, flags);

	return ret;
}

static struct obj_cgroup *drain_obj_stock(struct memcg_stock_pcp *stock)
{
	struct obj_cgroup *old = READ_ONCE(stock->cached_objcg);

	if (!old)
		return NULL;

	if (stock->nr_bytes) {
		unsigned int nr_pages = stock->nr_bytes >> PAGE_SHIFT;
		unsigned int nr_bytes = stock->nr_bytes & (PAGE_SIZE - 1);

		if (nr_pages) {
			struct mem_cgroup *memcg;

			memcg = get_mem_cgroup_from_objcg(old);

			memcg_account_kmem(memcg, -nr_pages);
			__refill_stock(memcg, nr_pages);

			css_put(&memcg->css);
		}

		 
		atomic_add(nr_bytes, &old->nr_charged_bytes);
		stock->nr_bytes = 0;
	}

	 
	if (stock->nr_slab_reclaimable_b || stock->nr_slab_unreclaimable_b) {
		if (stock->nr_slab_reclaimable_b) {
			mod_objcg_mlstate(old, stock->cached_pgdat,
					  NR_SLAB_RECLAIMABLE_B,
					  stock->nr_slab_reclaimable_b);
			stock->nr_slab_reclaimable_b = 0;
		}
		if (stock->nr_slab_unreclaimable_b) {
			mod_objcg_mlstate(old, stock->cached_pgdat,
					  NR_SLAB_UNRECLAIMABLE_B,
					  stock->nr_slab_unreclaimable_b);
			stock->nr_slab_unreclaimable_b = 0;
		}
		stock->cached_pgdat = NULL;
	}

	WRITE_ONCE(stock->cached_objcg, NULL);
	 
	return old;
}

static bool obj_stock_flush_required(struct memcg_stock_pcp *stock,
				     struct mem_cgroup *root_memcg)
{
	struct obj_cgroup *objcg = READ_ONCE(stock->cached_objcg);
	struct mem_cgroup *memcg;

	if (objcg) {
		memcg = obj_cgroup_memcg(objcg);
		if (memcg && mem_cgroup_is_descendant(memcg, root_memcg))
			return true;
	}

	return false;
}

static void refill_obj_stock(struct obj_cgroup *objcg, unsigned int nr_bytes,
			     bool allow_uncharge)
{
	struct memcg_stock_pcp *stock;
	struct obj_cgroup *old = NULL;
	unsigned long flags;
	unsigned int nr_pages = 0;

	local_lock_irqsave(&memcg_stock.stock_lock, flags);

	stock = this_cpu_ptr(&memcg_stock);
	if (READ_ONCE(stock->cached_objcg) != objcg) {  
		old = drain_obj_stock(stock);
		obj_cgroup_get(objcg);
		WRITE_ONCE(stock->cached_objcg, objcg);
		stock->nr_bytes = atomic_read(&objcg->nr_charged_bytes)
				? atomic_xchg(&objcg->nr_charged_bytes, 0) : 0;
		allow_uncharge = true;	 
	}
	stock->nr_bytes += nr_bytes;

	if (allow_uncharge && (stock->nr_bytes > PAGE_SIZE)) {
		nr_pages = stock->nr_bytes >> PAGE_SHIFT;
		stock->nr_bytes &= (PAGE_SIZE - 1);
	}

	local_unlock_irqrestore(&memcg_stock.stock_lock, flags);
	if (old)
		obj_cgroup_put(old);

	if (nr_pages)
		obj_cgroup_uncharge_pages(objcg, nr_pages);
}

int obj_cgroup_charge(struct obj_cgroup *objcg, gfp_t gfp, size_t size)
{
	unsigned int nr_pages, nr_bytes;
	int ret;

	if (consume_obj_stock(objcg, size))
		return 0;

	 
	nr_pages = size >> PAGE_SHIFT;
	nr_bytes = size & (PAGE_SIZE - 1);

	if (nr_bytes)
		nr_pages += 1;

	ret = obj_cgroup_charge_pages(objcg, gfp, nr_pages);
	if (!ret && nr_bytes)
		refill_obj_stock(objcg, PAGE_SIZE - nr_bytes, false);

	return ret;
}

void obj_cgroup_uncharge(struct obj_cgroup *objcg, size_t size)
{
	refill_obj_stock(objcg, size, true);
}

#endif  

 
void split_page_memcg(struct page *head, unsigned int nr)
{
	struct folio *folio = page_folio(head);
	struct mem_cgroup *memcg = folio_memcg(folio);
	int i;

	if (mem_cgroup_disabled() || !memcg)
		return;

	for (i = 1; i < nr; i++)
		folio_page(folio, i)->memcg_data = folio->memcg_data;

	if (folio_memcg_kmem(folio))
		obj_cgroup_get_many(__folio_objcg(folio), nr - 1);
	else
		css_get_many(&memcg->css, nr - 1);
}

#ifdef CONFIG_SWAP
 
static int mem_cgroup_move_swap_account(swp_entry_t entry,
				struct mem_cgroup *from, struct mem_cgroup *to)
{
	unsigned short old_id, new_id;

	old_id = mem_cgroup_id(from);
	new_id = mem_cgroup_id(to);

	if (swap_cgroup_cmpxchg(entry, old_id, new_id) == old_id) {
		mod_memcg_state(from, MEMCG_SWAP, -1);
		mod_memcg_state(to, MEMCG_SWAP, 1);
		return 0;
	}
	return -EINVAL;
}
#else
static inline int mem_cgroup_move_swap_account(swp_entry_t entry,
				struct mem_cgroup *from, struct mem_cgroup *to)
{
	return -EINVAL;
}
#endif

static DEFINE_MUTEX(memcg_max_mutex);

static int mem_cgroup_resize_max(struct mem_cgroup *memcg,
				 unsigned long max, bool memsw)
{
	bool enlarge = false;
	bool drained = false;
	int ret;
	bool limits_invariant;
	struct page_counter *counter = memsw ? &memcg->memsw : &memcg->memory;

	do {
		if (signal_pending(current)) {
			ret = -EINTR;
			break;
		}

		mutex_lock(&memcg_max_mutex);
		 
		limits_invariant = memsw ? max >= READ_ONCE(memcg->memory.max) :
					   max <= memcg->memsw.max;
		if (!limits_invariant) {
			mutex_unlock(&memcg_max_mutex);
			ret = -EINVAL;
			break;
		}
		if (max > counter->max)
			enlarge = true;
		ret = page_counter_set_max(counter, max);
		mutex_unlock(&memcg_max_mutex);

		if (!ret)
			break;

		if (!drained) {
			drain_all_stock(memcg);
			drained = true;
			continue;
		}

		if (!try_to_free_mem_cgroup_pages(memcg, 1, GFP_KERNEL,
					memsw ? 0 : MEMCG_RECLAIM_MAY_SWAP)) {
			ret = -EBUSY;
			break;
		}
	} while (true);

	if (!ret && enlarge)
		memcg_oom_recover(memcg);

	return ret;
}

unsigned long mem_cgroup_soft_limit_reclaim(pg_data_t *pgdat, int order,
					    gfp_t gfp_mask,
					    unsigned long *total_scanned)
{
	unsigned long nr_reclaimed = 0;
	struct mem_cgroup_per_node *mz, *next_mz = NULL;
	unsigned long reclaimed;
	int loop = 0;
	struct mem_cgroup_tree_per_node *mctz;
	unsigned long excess;

	if (lru_gen_enabled())
		return 0;

	if (order > 0)
		return 0;

	mctz = soft_limit_tree.rb_tree_per_node[pgdat->node_id];

	 
	if (!mctz || RB_EMPTY_ROOT(&mctz->rb_root))
		return 0;

	 
	do {
		if (next_mz)
			mz = next_mz;
		else
			mz = mem_cgroup_largest_soft_limit_node(mctz);
		if (!mz)
			break;

		reclaimed = mem_cgroup_soft_reclaim(mz->memcg, pgdat,
						    gfp_mask, total_scanned);
		nr_reclaimed += reclaimed;
		spin_lock_irq(&mctz->lock);

		 
		next_mz = NULL;
		if (!reclaimed)
			next_mz = __mem_cgroup_largest_soft_limit_node(mctz);

		excess = soft_limit_excess(mz->memcg);
		 
		 
		__mem_cgroup_insert_exceeded(mz, mctz, excess);
		spin_unlock_irq(&mctz->lock);
		css_put(&mz->memcg->css);
		loop++;
		 
		if (!nr_reclaimed &&
			(next_mz == NULL ||
			loop > MEM_CGROUP_MAX_SOFT_LIMIT_RECLAIM_LOOPS))
			break;
	} while (!nr_reclaimed);
	if (next_mz)
		css_put(&next_mz->memcg->css);
	return nr_reclaimed;
}

 
static int mem_cgroup_force_empty(struct mem_cgroup *memcg)
{
	int nr_retries = MAX_RECLAIM_RETRIES;

	 
	lru_add_drain_all();

	drain_all_stock(memcg);

	 
	while (nr_retries && page_counter_read(&memcg->memory)) {
		if (signal_pending(current))
			return -EINTR;

		if (!try_to_free_mem_cgroup_pages(memcg, 1, GFP_KERNEL,
						  MEMCG_RECLAIM_MAY_SWAP))
			nr_retries--;
	}

	return 0;
}

static ssize_t mem_cgroup_force_empty_write(struct kernfs_open_file *of,
					    char *buf, size_t nbytes,
					    loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));

	if (mem_cgroup_is_root(memcg))
		return -EINVAL;
	return mem_cgroup_force_empty(memcg) ?: nbytes;
}

static u64 mem_cgroup_hierarchy_read(struct cgroup_subsys_state *css,
				     struct cftype *cft)
{
	return 1;
}

static int mem_cgroup_hierarchy_write(struct cgroup_subsys_state *css,
				      struct cftype *cft, u64 val)
{
	if (val == 1)
		return 0;

	pr_warn_once("Non-hierarchical mode is deprecated. "
		     "Please report your usecase to linux-mm@kvack.org if you "
		     "depend on this functionality.\n");

	return -EINVAL;
}

static unsigned long mem_cgroup_usage(struct mem_cgroup *memcg, bool swap)
{
	unsigned long val;

	if (mem_cgroup_is_root(memcg)) {
		 
		val = global_node_page_state(NR_FILE_PAGES) +
			global_node_page_state(NR_ANON_MAPPED);
		if (swap)
			val += total_swap_pages - get_nr_swap_pages();
	} else {
		if (!swap)
			val = page_counter_read(&memcg->memory);
		else
			val = page_counter_read(&memcg->memsw);
	}
	return val;
}

enum {
	RES_USAGE,
	RES_LIMIT,
	RES_MAX_USAGE,
	RES_FAILCNT,
	RES_SOFT_LIMIT,
};

static u64 mem_cgroup_read_u64(struct cgroup_subsys_state *css,
			       struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct page_counter *counter;

	switch (MEMFILE_TYPE(cft->private)) {
	case _MEM:
		counter = &memcg->memory;
		break;
	case _MEMSWAP:
		counter = &memcg->memsw;
		break;
	case _KMEM:
		counter = &memcg->kmem;
		break;
	case _TCP:
		counter = &memcg->tcpmem;
		break;
	default:
		BUG();
	}

	switch (MEMFILE_ATTR(cft->private)) {
	case RES_USAGE:
		if (counter == &memcg->memory)
			return (u64)mem_cgroup_usage(memcg, false) * PAGE_SIZE;
		if (counter == &memcg->memsw)
			return (u64)mem_cgroup_usage(memcg, true) * PAGE_SIZE;
		return (u64)page_counter_read(counter) * PAGE_SIZE;
	case RES_LIMIT:
		return (u64)counter->max * PAGE_SIZE;
	case RES_MAX_USAGE:
		return (u64)counter->watermark * PAGE_SIZE;
	case RES_FAILCNT:
		return counter->failcnt;
	case RES_SOFT_LIMIT:
		return (u64)READ_ONCE(memcg->soft_limit) * PAGE_SIZE;
	default:
		BUG();
	}
}

 
static int mem_cgroup_dummy_seq_show(__always_unused struct seq_file *m,
				     __always_unused void *v)
{
	return -EINVAL;
}

#ifdef CONFIG_MEMCG_KMEM
static int memcg_online_kmem(struct mem_cgroup *memcg)
{
	struct obj_cgroup *objcg;

	if (mem_cgroup_kmem_disabled())
		return 0;

	if (unlikely(mem_cgroup_is_root(memcg)))
		return 0;

	objcg = obj_cgroup_alloc();
	if (!objcg)
		return -ENOMEM;

	objcg->memcg = memcg;
	rcu_assign_pointer(memcg->objcg, objcg);

	static_branch_enable(&memcg_kmem_online_key);

	memcg->kmemcg_id = memcg->id.id;

	return 0;
}

static void memcg_offline_kmem(struct mem_cgroup *memcg)
{
	struct mem_cgroup *parent;

	if (mem_cgroup_kmem_disabled())
		return;

	if (unlikely(mem_cgroup_is_root(memcg)))
		return;

	parent = parent_mem_cgroup(memcg);
	if (!parent)
		parent = root_mem_cgroup;

	memcg_reparent_objcgs(memcg, parent);

	 
	memcg_reparent_list_lrus(memcg, parent);
}
#else
static int memcg_online_kmem(struct mem_cgroup *memcg)
{
	return 0;
}
static void memcg_offline_kmem(struct mem_cgroup *memcg)
{
}
#endif  

static int memcg_update_tcp_max(struct mem_cgroup *memcg, unsigned long max)
{
	int ret;

	mutex_lock(&memcg_max_mutex);

	ret = page_counter_set_max(&memcg->tcpmem, max);
	if (ret)
		goto out;

	if (!memcg->tcpmem_active) {
		 
		static_branch_inc(&memcg_sockets_enabled_key);
		memcg->tcpmem_active = true;
	}
out:
	mutex_unlock(&memcg_max_mutex);
	return ret;
}

 
static ssize_t mem_cgroup_write(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned long nr_pages;
	int ret;

	buf = strstrip(buf);
	ret = page_counter_memparse(buf, "-1", &nr_pages);
	if (ret)
		return ret;

	switch (MEMFILE_ATTR(of_cft(of)->private)) {
	case RES_LIMIT:
		if (mem_cgroup_is_root(memcg)) {  
			ret = -EINVAL;
			break;
		}
		switch (MEMFILE_TYPE(of_cft(of)->private)) {
		case _MEM:
			ret = mem_cgroup_resize_max(memcg, nr_pages, false);
			break;
		case _MEMSWAP:
			ret = mem_cgroup_resize_max(memcg, nr_pages, true);
			break;
		case _KMEM:
			pr_warn_once("kmem.limit_in_bytes is deprecated and will be removed. "
				     "Writing any value to this file has no effect. "
				     "Please report your usecase to linux-mm@kvack.org if you "
				     "depend on this functionality.\n");
			ret = 0;
			break;
		case _TCP:
			ret = memcg_update_tcp_max(memcg, nr_pages);
			break;
		}
		break;
	case RES_SOFT_LIMIT:
		if (IS_ENABLED(CONFIG_PREEMPT_RT)) {
			ret = -EOPNOTSUPP;
		} else {
			WRITE_ONCE(memcg->soft_limit, nr_pages);
			ret = 0;
		}
		break;
	}
	return ret ?: nbytes;
}

static ssize_t mem_cgroup_reset(struct kernfs_open_file *of, char *buf,
				size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	struct page_counter *counter;

	switch (MEMFILE_TYPE(of_cft(of)->private)) {
	case _MEM:
		counter = &memcg->memory;
		break;
	case _MEMSWAP:
		counter = &memcg->memsw;
		break;
	case _KMEM:
		counter = &memcg->kmem;
		break;
	case _TCP:
		counter = &memcg->tcpmem;
		break;
	default:
		BUG();
	}

	switch (MEMFILE_ATTR(of_cft(of)->private)) {
	case RES_MAX_USAGE:
		page_counter_reset_watermark(counter);
		break;
	case RES_FAILCNT:
		counter->failcnt = 0;
		break;
	default:
		BUG();
	}

	return nbytes;
}

static u64 mem_cgroup_move_charge_read(struct cgroup_subsys_state *css,
					struct cftype *cft)
{
	return mem_cgroup_from_css(css)->move_charge_at_immigrate;
}

#ifdef CONFIG_MMU
static int mem_cgroup_move_charge_write(struct cgroup_subsys_state *css,
					struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	pr_warn_once("Cgroup memory moving (move_charge_at_immigrate) is deprecated. "
		     "Please report your usecase to linux-mm@kvack.org if you "
		     "depend on this functionality.\n");

	if (val & ~MOVE_MASK)
		return -EINVAL;

	 
	memcg->move_charge_at_immigrate = val;
	return 0;
}
#else
static int mem_cgroup_move_charge_write(struct cgroup_subsys_state *css,
					struct cftype *cft, u64 val)
{
	return -ENOSYS;
}
#endif

#ifdef CONFIG_NUMA

#define LRU_ALL_FILE (BIT(LRU_INACTIVE_FILE) | BIT(LRU_ACTIVE_FILE))
#define LRU_ALL_ANON (BIT(LRU_INACTIVE_ANON) | BIT(LRU_ACTIVE_ANON))
#define LRU_ALL	     ((1 << NR_LRU_LISTS) - 1)

static unsigned long mem_cgroup_node_nr_lru_pages(struct mem_cgroup *memcg,
				int nid, unsigned int lru_mask, bool tree)
{
	struct lruvec *lruvec = mem_cgroup_lruvec(memcg, NODE_DATA(nid));
	unsigned long nr = 0;
	enum lru_list lru;

	VM_BUG_ON((unsigned)nid >= nr_node_ids);

	for_each_lru(lru) {
		if (!(BIT(lru) & lru_mask))
			continue;
		if (tree)
			nr += lruvec_page_state(lruvec, NR_LRU_BASE + lru);
		else
			nr += lruvec_page_state_local(lruvec, NR_LRU_BASE + lru);
	}
	return nr;
}

static unsigned long mem_cgroup_nr_lru_pages(struct mem_cgroup *memcg,
					     unsigned int lru_mask,
					     bool tree)
{
	unsigned long nr = 0;
	enum lru_list lru;

	for_each_lru(lru) {
		if (!(BIT(lru) & lru_mask))
			continue;
		if (tree)
			nr += memcg_page_state(memcg, NR_LRU_BASE + lru);
		else
			nr += memcg_page_state_local(memcg, NR_LRU_BASE + lru);
	}
	return nr;
}

static int memcg_numa_stat_show(struct seq_file *m, void *v)
{
	struct numa_stat {
		const char *name;
		unsigned int lru_mask;
	};

	static const struct numa_stat stats[] = {
		{ "total", LRU_ALL },
		{ "file", LRU_ALL_FILE },
		{ "anon", LRU_ALL_ANON },
		{ "unevictable", BIT(LRU_UNEVICTABLE) },
	};
	const struct numa_stat *stat;
	int nid;
	struct mem_cgroup *memcg = mem_cgroup_from_seq(m);

	mem_cgroup_flush_stats();

	for (stat = stats; stat < stats + ARRAY_SIZE(stats); stat++) {
		seq_printf(m, "%s=%lu", stat->name,
			   mem_cgroup_nr_lru_pages(memcg, stat->lru_mask,
						   false));
		for_each_node_state(nid, N_MEMORY)
			seq_printf(m, " N%d=%lu", nid,
				   mem_cgroup_node_nr_lru_pages(memcg, nid,
							stat->lru_mask, false));
		seq_putc(m, '\n');
	}

	for (stat = stats; stat < stats + ARRAY_SIZE(stats); stat++) {

		seq_printf(m, "hierarchical_%s=%lu", stat->name,
			   mem_cgroup_nr_lru_pages(memcg, stat->lru_mask,
						   true));
		for_each_node_state(nid, N_MEMORY)
			seq_printf(m, " N%d=%lu", nid,
				   mem_cgroup_node_nr_lru_pages(memcg, nid,
							stat->lru_mask, true));
		seq_putc(m, '\n');
	}

	return 0;
}
#endif  

static const unsigned int memcg1_stats[] = {
	NR_FILE_PAGES,
	NR_ANON_MAPPED,
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	NR_ANON_THPS,
#endif
	NR_SHMEM,
	NR_FILE_MAPPED,
	NR_FILE_DIRTY,
	NR_WRITEBACK,
	WORKINGSET_REFAULT_ANON,
	WORKINGSET_REFAULT_FILE,
	MEMCG_SWAP,
};

static const char *const memcg1_stat_names[] = {
	"cache",
	"rss",
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	"rss_huge",
#endif
	"shmem",
	"mapped_file",
	"dirty",
	"writeback",
	"workingset_refault_anon",
	"workingset_refault_file",
	"swap",
};

 
static const unsigned int memcg1_events[] = {
	PGPGIN,
	PGPGOUT,
	PGFAULT,
	PGMAJFAULT,
};

static void memcg1_stat_format(struct mem_cgroup *memcg, struct seq_buf *s)
{
	unsigned long memory, memsw;
	struct mem_cgroup *mi;
	unsigned int i;

	BUILD_BUG_ON(ARRAY_SIZE(memcg1_stat_names) != ARRAY_SIZE(memcg1_stats));

	mem_cgroup_flush_stats();

	for (i = 0; i < ARRAY_SIZE(memcg1_stats); i++) {
		unsigned long nr;

		if (memcg1_stats[i] == MEMCG_SWAP && !do_memsw_account())
			continue;
		nr = memcg_page_state_local(memcg, memcg1_stats[i]);
		seq_buf_printf(s, "%s %lu\n", memcg1_stat_names[i],
			   nr * memcg_page_state_unit(memcg1_stats[i]));
	}

	for (i = 0; i < ARRAY_SIZE(memcg1_events); i++)
		seq_buf_printf(s, "%s %lu\n", vm_event_name(memcg1_events[i]),
			       memcg_events_local(memcg, memcg1_events[i]));

	for (i = 0; i < NR_LRU_LISTS; i++)
		seq_buf_printf(s, "%s %lu\n", lru_list_name(i),
			       memcg_page_state_local(memcg, NR_LRU_BASE + i) *
			       PAGE_SIZE);

	 
	memory = memsw = PAGE_COUNTER_MAX;
	for (mi = memcg; mi; mi = parent_mem_cgroup(mi)) {
		memory = min(memory, READ_ONCE(mi->memory.max));
		memsw = min(memsw, READ_ONCE(mi->memsw.max));
	}
	seq_buf_printf(s, "hierarchical_memory_limit %llu\n",
		       (u64)memory * PAGE_SIZE);
	if (do_memsw_account())
		seq_buf_printf(s, "hierarchical_memsw_limit %llu\n",
			       (u64)memsw * PAGE_SIZE);

	for (i = 0; i < ARRAY_SIZE(memcg1_stats); i++) {
		unsigned long nr;

		if (memcg1_stats[i] == MEMCG_SWAP && !do_memsw_account())
			continue;
		nr = memcg_page_state(memcg, memcg1_stats[i]);
		seq_buf_printf(s, "total_%s %llu\n", memcg1_stat_names[i],
			   (u64)nr * memcg_page_state_unit(memcg1_stats[i]));
	}

	for (i = 0; i < ARRAY_SIZE(memcg1_events); i++)
		seq_buf_printf(s, "total_%s %llu\n",
			       vm_event_name(memcg1_events[i]),
			       (u64)memcg_events(memcg, memcg1_events[i]));

	for (i = 0; i < NR_LRU_LISTS; i++)
		seq_buf_printf(s, "total_%s %llu\n", lru_list_name(i),
			       (u64)memcg_page_state(memcg, NR_LRU_BASE + i) *
			       PAGE_SIZE);

#ifdef CONFIG_DEBUG_VM
	{
		pg_data_t *pgdat;
		struct mem_cgroup_per_node *mz;
		unsigned long anon_cost = 0;
		unsigned long file_cost = 0;

		for_each_online_pgdat(pgdat) {
			mz = memcg->nodeinfo[pgdat->node_id];

			anon_cost += mz->lruvec.anon_cost;
			file_cost += mz->lruvec.file_cost;
		}
		seq_buf_printf(s, "anon_cost %lu\n", anon_cost);
		seq_buf_printf(s, "file_cost %lu\n", file_cost);
	}
#endif
}

static u64 mem_cgroup_swappiness_read(struct cgroup_subsys_state *css,
				      struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	return mem_cgroup_swappiness(memcg);
}

static int mem_cgroup_swappiness_write(struct cgroup_subsys_state *css,
				       struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	if (val > 200)
		return -EINVAL;

	if (!mem_cgroup_is_root(memcg))
		WRITE_ONCE(memcg->swappiness, val);
	else
		WRITE_ONCE(vm_swappiness, val);

	return 0;
}

static void __mem_cgroup_threshold(struct mem_cgroup *memcg, bool swap)
{
	struct mem_cgroup_threshold_ary *t;
	unsigned long usage;
	int i;

	rcu_read_lock();
	if (!swap)
		t = rcu_dereference(memcg->thresholds.primary);
	else
		t = rcu_dereference(memcg->memsw_thresholds.primary);

	if (!t)
		goto unlock;

	usage = mem_cgroup_usage(memcg, swap);

	 
	i = t->current_threshold;

	 
	for (; i >= 0 && unlikely(t->entries[i].threshold > usage); i--)
		eventfd_signal(t->entries[i].eventfd, 1);

	 
	i++;

	 
	for (; i < t->size && unlikely(t->entries[i].threshold <= usage); i++)
		eventfd_signal(t->entries[i].eventfd, 1);

	 
	t->current_threshold = i - 1;
unlock:
	rcu_read_unlock();
}

static void mem_cgroup_threshold(struct mem_cgroup *memcg)
{
	while (memcg) {
		__mem_cgroup_threshold(memcg, false);
		if (do_memsw_account())
			__mem_cgroup_threshold(memcg, true);

		memcg = parent_mem_cgroup(memcg);
	}
}

static int compare_thresholds(const void *a, const void *b)
{
	const struct mem_cgroup_threshold *_a = a;
	const struct mem_cgroup_threshold *_b = b;

	if (_a->threshold > _b->threshold)
		return 1;

	if (_a->threshold < _b->threshold)
		return -1;

	return 0;
}

static int mem_cgroup_oom_notify_cb(struct mem_cgroup *memcg)
{
	struct mem_cgroup_eventfd_list *ev;

	spin_lock(&memcg_oom_lock);

	list_for_each_entry(ev, &memcg->oom_notify, list)
		eventfd_signal(ev->eventfd, 1);

	spin_unlock(&memcg_oom_lock);
	return 0;
}

static void mem_cgroup_oom_notify(struct mem_cgroup *memcg)
{
	struct mem_cgroup *iter;

	for_each_mem_cgroup_tree(iter, memcg)
		mem_cgroup_oom_notify_cb(iter);
}

static int __mem_cgroup_usage_register_event(struct mem_cgroup *memcg,
	struct eventfd_ctx *eventfd, const char *args, enum res_type type)
{
	struct mem_cgroup_thresholds *thresholds;
	struct mem_cgroup_threshold_ary *new;
	unsigned long threshold;
	unsigned long usage;
	int i, size, ret;

	ret = page_counter_memparse(args, "-1", &threshold);
	if (ret)
		return ret;

	mutex_lock(&memcg->thresholds_lock);

	if (type == _MEM) {
		thresholds = &memcg->thresholds;
		usage = mem_cgroup_usage(memcg, false);
	} else if (type == _MEMSWAP) {
		thresholds = &memcg->memsw_thresholds;
		usage = mem_cgroup_usage(memcg, true);
	} else
		BUG();

	 
	if (thresholds->primary)
		__mem_cgroup_threshold(memcg, type == _MEMSWAP);

	size = thresholds->primary ? thresholds->primary->size + 1 : 1;

	 
	new = kmalloc(struct_size(new, entries, size), GFP_KERNEL);
	if (!new) {
		ret = -ENOMEM;
		goto unlock;
	}
	new->size = size;

	 
	if (thresholds->primary)
		memcpy(new->entries, thresholds->primary->entries,
		       flex_array_size(new, entries, size - 1));

	 
	new->entries[size - 1].eventfd = eventfd;
	new->entries[size - 1].threshold = threshold;

	 
	sort(new->entries, size, sizeof(*new->entries),
			compare_thresholds, NULL);

	 
	new->current_threshold = -1;
	for (i = 0; i < size; i++) {
		if (new->entries[i].threshold <= usage) {
			 
			++new->current_threshold;
		} else
			break;
	}

	 
	kfree(thresholds->spare);
	thresholds->spare = thresholds->primary;

	rcu_assign_pointer(thresholds->primary, new);

	 
	synchronize_rcu();

unlock:
	mutex_unlock(&memcg->thresholds_lock);

	return ret;
}

static int mem_cgroup_usage_register_event(struct mem_cgroup *memcg,
	struct eventfd_ctx *eventfd, const char *args)
{
	return __mem_cgroup_usage_register_event(memcg, eventfd, args, _MEM);
}

static int memsw_cgroup_usage_register_event(struct mem_cgroup *memcg,
	struct eventfd_ctx *eventfd, const char *args)
{
	return __mem_cgroup_usage_register_event(memcg, eventfd, args, _MEMSWAP);
}

static void __mem_cgroup_usage_unregister_event(struct mem_cgroup *memcg,
	struct eventfd_ctx *eventfd, enum res_type type)
{
	struct mem_cgroup_thresholds *thresholds;
	struct mem_cgroup_threshold_ary *new;
	unsigned long usage;
	int i, j, size, entries;

	mutex_lock(&memcg->thresholds_lock);

	if (type == _MEM) {
		thresholds = &memcg->thresholds;
		usage = mem_cgroup_usage(memcg, false);
	} else if (type == _MEMSWAP) {
		thresholds = &memcg->memsw_thresholds;
		usage = mem_cgroup_usage(memcg, true);
	} else
		BUG();

	if (!thresholds->primary)
		goto unlock;

	 
	__mem_cgroup_threshold(memcg, type == _MEMSWAP);

	 
	size = entries = 0;
	for (i = 0; i < thresholds->primary->size; i++) {
		if (thresholds->primary->entries[i].eventfd != eventfd)
			size++;
		else
			entries++;
	}

	new = thresholds->spare;

	 
	if (!entries)
		goto unlock;

	 
	if (!size) {
		kfree(new);
		new = NULL;
		goto swap_buffers;
	}

	new->size = size;

	 
	new->current_threshold = -1;
	for (i = 0, j = 0; i < thresholds->primary->size; i++) {
		if (thresholds->primary->entries[i].eventfd == eventfd)
			continue;

		new->entries[j] = thresholds->primary->entries[i];
		if (new->entries[j].threshold <= usage) {
			 
			++new->current_threshold;
		}
		j++;
	}

swap_buffers:
	 
	thresholds->spare = thresholds->primary;

	rcu_assign_pointer(thresholds->primary, new);

	 
	synchronize_rcu();

	 
	if (!new) {
		kfree(thresholds->spare);
		thresholds->spare = NULL;
	}
unlock:
	mutex_unlock(&memcg->thresholds_lock);
}

static void mem_cgroup_usage_unregister_event(struct mem_cgroup *memcg,
	struct eventfd_ctx *eventfd)
{
	return __mem_cgroup_usage_unregister_event(memcg, eventfd, _MEM);
}

static void memsw_cgroup_usage_unregister_event(struct mem_cgroup *memcg,
	struct eventfd_ctx *eventfd)
{
	return __mem_cgroup_usage_unregister_event(memcg, eventfd, _MEMSWAP);
}

static int mem_cgroup_oom_register_event(struct mem_cgroup *memcg,
	struct eventfd_ctx *eventfd, const char *args)
{
	struct mem_cgroup_eventfd_list *event;

	event = kmalloc(sizeof(*event),	GFP_KERNEL);
	if (!event)
		return -ENOMEM;

	spin_lock(&memcg_oom_lock);

	event->eventfd = eventfd;
	list_add(&event->list, &memcg->oom_notify);

	 
	if (memcg->under_oom)
		eventfd_signal(eventfd, 1);
	spin_unlock(&memcg_oom_lock);

	return 0;
}

static void mem_cgroup_oom_unregister_event(struct mem_cgroup *memcg,
	struct eventfd_ctx *eventfd)
{
	struct mem_cgroup_eventfd_list *ev, *tmp;

	spin_lock(&memcg_oom_lock);

	list_for_each_entry_safe(ev, tmp, &memcg->oom_notify, list) {
		if (ev->eventfd == eventfd) {
			list_del(&ev->list);
			kfree(ev);
		}
	}

	spin_unlock(&memcg_oom_lock);
}

static int mem_cgroup_oom_control_read(struct seq_file *sf, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_seq(sf);

	seq_printf(sf, "oom_kill_disable %d\n", READ_ONCE(memcg->oom_kill_disable));
	seq_printf(sf, "under_oom %d\n", (bool)memcg->under_oom);
	seq_printf(sf, "oom_kill %lu\n",
		   atomic_long_read(&memcg->memory_events[MEMCG_OOM_KILL]));
	return 0;
}

static int mem_cgroup_oom_control_write(struct cgroup_subsys_state *css,
	struct cftype *cft, u64 val)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	 
	if (mem_cgroup_is_root(memcg) || !((val == 0) || (val == 1)))
		return -EINVAL;

	WRITE_ONCE(memcg->oom_kill_disable, val);
	if (!val)
		memcg_oom_recover(memcg);

	return 0;
}

#ifdef CONFIG_CGROUP_WRITEBACK

#include <trace/events/writeback.h>

static int memcg_wb_domain_init(struct mem_cgroup *memcg, gfp_t gfp)
{
	return wb_domain_init(&memcg->cgwb_domain, gfp);
}

static void memcg_wb_domain_exit(struct mem_cgroup *memcg)
{
	wb_domain_exit(&memcg->cgwb_domain);
}

static void memcg_wb_domain_size_changed(struct mem_cgroup *memcg)
{
	wb_domain_size_changed(&memcg->cgwb_domain);
}

struct wb_domain *mem_cgroup_wb_domain(struct bdi_writeback *wb)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(wb->memcg_css);

	if (!memcg->css.parent)
		return NULL;

	return &memcg->cgwb_domain;
}

 
void mem_cgroup_wb_stats(struct bdi_writeback *wb, unsigned long *pfilepages,
			 unsigned long *pheadroom, unsigned long *pdirty,
			 unsigned long *pwriteback)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(wb->memcg_css);
	struct mem_cgroup *parent;

	mem_cgroup_flush_stats();

	*pdirty = memcg_page_state(memcg, NR_FILE_DIRTY);
	*pwriteback = memcg_page_state(memcg, NR_WRITEBACK);
	*pfilepages = memcg_page_state(memcg, NR_INACTIVE_FILE) +
			memcg_page_state(memcg, NR_ACTIVE_FILE);

	*pheadroom = PAGE_COUNTER_MAX;
	while ((parent = parent_mem_cgroup(memcg))) {
		unsigned long ceiling = min(READ_ONCE(memcg->memory.max),
					    READ_ONCE(memcg->memory.high));
		unsigned long used = page_counter_read(&memcg->memory);

		*pheadroom = min(*pheadroom, ceiling - min(ceiling, used));
		memcg = parent;
	}
}

 
void mem_cgroup_track_foreign_dirty_slowpath(struct folio *folio,
					     struct bdi_writeback *wb)
{
	struct mem_cgroup *memcg = folio_memcg(folio);
	struct memcg_cgwb_frn *frn;
	u64 now = get_jiffies_64();
	u64 oldest_at = now;
	int oldest = -1;
	int i;

	trace_track_foreign_dirty(folio, wb);

	 
	for (i = 0; i < MEMCG_CGWB_FRN_CNT; i++) {
		frn = &memcg->cgwb_frn[i];
		if (frn->bdi_id == wb->bdi->id &&
		    frn->memcg_id == wb->memcg_css->id)
			break;
		if (time_before64(frn->at, oldest_at) &&
		    atomic_read(&frn->done.cnt) == 1) {
			oldest = i;
			oldest_at = frn->at;
		}
	}

	if (i < MEMCG_CGWB_FRN_CNT) {
		 
		unsigned long update_intv =
			min_t(unsigned long, HZ,
			      msecs_to_jiffies(dirty_expire_interval * 10) / 8);

		if (time_before64(frn->at, now - update_intv))
			frn->at = now;
	} else if (oldest >= 0) {
		 
		frn = &memcg->cgwb_frn[oldest];
		frn->bdi_id = wb->bdi->id;
		frn->memcg_id = wb->memcg_css->id;
		frn->at = now;
	}
}

 
void mem_cgroup_flush_foreign(struct bdi_writeback *wb)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(wb->memcg_css);
	unsigned long intv = msecs_to_jiffies(dirty_expire_interval * 10);
	u64 now = jiffies_64;
	int i;

	for (i = 0; i < MEMCG_CGWB_FRN_CNT; i++) {
		struct memcg_cgwb_frn *frn = &memcg->cgwb_frn[i];

		 
		if (time_after64(frn->at, now - intv) &&
		    atomic_read(&frn->done.cnt) == 1) {
			frn->at = 0;
			trace_flush_foreign(wb, frn->bdi_id, frn->memcg_id);
			cgroup_writeback_by_id(frn->bdi_id, frn->memcg_id,
					       WB_REASON_FOREIGN_FLUSH,
					       &frn->done);
		}
	}
}

#else	 

static int memcg_wb_domain_init(struct mem_cgroup *memcg, gfp_t gfp)
{
	return 0;
}

static void memcg_wb_domain_exit(struct mem_cgroup *memcg)
{
}

static void memcg_wb_domain_size_changed(struct mem_cgroup *memcg)
{
}

#endif	 

 

 
static void memcg_event_remove(struct work_struct *work)
{
	struct mem_cgroup_event *event =
		container_of(work, struct mem_cgroup_event, remove);
	struct mem_cgroup *memcg = event->memcg;

	remove_wait_queue(event->wqh, &event->wait);

	event->unregister_event(memcg, event->eventfd);

	 
	eventfd_signal(event->eventfd, 1);

	eventfd_ctx_put(event->eventfd);
	kfree(event);
	css_put(&memcg->css);
}

 
static int memcg_event_wake(wait_queue_entry_t *wait, unsigned mode,
			    int sync, void *key)
{
	struct mem_cgroup_event *event =
		container_of(wait, struct mem_cgroup_event, wait);
	struct mem_cgroup *memcg = event->memcg;
	__poll_t flags = key_to_poll(key);

	if (flags & EPOLLHUP) {
		 
		spin_lock(&memcg->event_list_lock);
		if (!list_empty(&event->list)) {
			list_del_init(&event->list);
			 
			schedule_work(&event->remove);
		}
		spin_unlock(&memcg->event_list_lock);
	}

	return 0;
}

static void memcg_event_ptable_queue_proc(struct file *file,
		wait_queue_head_t *wqh, poll_table *pt)
{
	struct mem_cgroup_event *event =
		container_of(pt, struct mem_cgroup_event, pt);

	event->wqh = wqh;
	add_wait_queue(wqh, &event->wait);
}

 
static ssize_t memcg_write_event_control(struct kernfs_open_file *of,
					 char *buf, size_t nbytes, loff_t off)
{
	struct cgroup_subsys_state *css = of_css(of);
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct mem_cgroup_event *event;
	struct cgroup_subsys_state *cfile_css;
	unsigned int efd, cfd;
	struct fd efile;
	struct fd cfile;
	struct dentry *cdentry;
	const char *name;
	char *endp;
	int ret;

	if (IS_ENABLED(CONFIG_PREEMPT_RT))
		return -EOPNOTSUPP;

	buf = strstrip(buf);

	efd = simple_strtoul(buf, &endp, 10);
	if (*endp != ' ')
		return -EINVAL;
	buf = endp + 1;

	cfd = simple_strtoul(buf, &endp, 10);
	if ((*endp != ' ') && (*endp != '\0'))
		return -EINVAL;
	buf = endp + 1;

	event = kzalloc(sizeof(*event), GFP_KERNEL);
	if (!event)
		return -ENOMEM;

	event->memcg = memcg;
	INIT_LIST_HEAD(&event->list);
	init_poll_funcptr(&event->pt, memcg_event_ptable_queue_proc);
	init_waitqueue_func_entry(&event->wait, memcg_event_wake);
	INIT_WORK(&event->remove, memcg_event_remove);

	efile = fdget(efd);
	if (!efile.file) {
		ret = -EBADF;
		goto out_kfree;
	}

	event->eventfd = eventfd_ctx_fileget(efile.file);
	if (IS_ERR(event->eventfd)) {
		ret = PTR_ERR(event->eventfd);
		goto out_put_efile;
	}

	cfile = fdget(cfd);
	if (!cfile.file) {
		ret = -EBADF;
		goto out_put_eventfd;
	}

	 
	 
	ret = file_permission(cfile.file, MAY_READ);
	if (ret < 0)
		goto out_put_cfile;

	 
	cdentry = cfile.file->f_path.dentry;
	if (cdentry->d_sb->s_type != &cgroup_fs_type || !d_is_reg(cdentry)) {
		ret = -EINVAL;
		goto out_put_cfile;
	}

	 
	name = cdentry->d_name.name;

	if (!strcmp(name, "memory.usage_in_bytes")) {
		event->register_event = mem_cgroup_usage_register_event;
		event->unregister_event = mem_cgroup_usage_unregister_event;
	} else if (!strcmp(name, "memory.oom_control")) {
		event->register_event = mem_cgroup_oom_register_event;
		event->unregister_event = mem_cgroup_oom_unregister_event;
	} else if (!strcmp(name, "memory.pressure_level")) {
		event->register_event = vmpressure_register_event;
		event->unregister_event = vmpressure_unregister_event;
	} else if (!strcmp(name, "memory.memsw.usage_in_bytes")) {
		event->register_event = memsw_cgroup_usage_register_event;
		event->unregister_event = memsw_cgroup_usage_unregister_event;
	} else {
		ret = -EINVAL;
		goto out_put_cfile;
	}

	 
	cfile_css = css_tryget_online_from_dir(cdentry->d_parent,
					       &memory_cgrp_subsys);
	ret = -EINVAL;
	if (IS_ERR(cfile_css))
		goto out_put_cfile;
	if (cfile_css != css) {
		css_put(cfile_css);
		goto out_put_cfile;
	}

	ret = event->register_event(memcg, event->eventfd, buf);
	if (ret)
		goto out_put_css;

	vfs_poll(efile.file, &event->pt);

	spin_lock_irq(&memcg->event_list_lock);
	list_add(&event->list, &memcg->event_list);
	spin_unlock_irq(&memcg->event_list_lock);

	fdput(cfile);
	fdput(efile);

	return nbytes;

out_put_css:
	css_put(css);
out_put_cfile:
	fdput(cfile);
out_put_eventfd:
	eventfd_ctx_put(event->eventfd);
out_put_efile:
	fdput(efile);
out_kfree:
	kfree(event);

	return ret;
}

#if defined(CONFIG_MEMCG_KMEM) && (defined(CONFIG_SLAB) || defined(CONFIG_SLUB_DEBUG))
static int mem_cgroup_slab_show(struct seq_file *m, void *p)
{
	 
	return 0;
}
#endif

static int memory_stat_show(struct seq_file *m, void *v);

static struct cftype mem_cgroup_legacy_files[] = {
	{
		.name = "usage_in_bytes",
		.private = MEMFILE_PRIVATE(_MEM, RES_USAGE),
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "max_usage_in_bytes",
		.private = MEMFILE_PRIVATE(_MEM, RES_MAX_USAGE),
		.write = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "limit_in_bytes",
		.private = MEMFILE_PRIVATE(_MEM, RES_LIMIT),
		.write = mem_cgroup_write,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "soft_limit_in_bytes",
		.private = MEMFILE_PRIVATE(_MEM, RES_SOFT_LIMIT),
		.write = mem_cgroup_write,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "failcnt",
		.private = MEMFILE_PRIVATE(_MEM, RES_FAILCNT),
		.write = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "stat",
		.seq_show = memory_stat_show,
	},
	{
		.name = "force_empty",
		.write = mem_cgroup_force_empty_write,
	},
	{
		.name = "use_hierarchy",
		.write_u64 = mem_cgroup_hierarchy_write,
		.read_u64 = mem_cgroup_hierarchy_read,
	},
	{
		.name = "cgroup.event_control",		 
		.write = memcg_write_event_control,
		.flags = CFTYPE_NO_PREFIX | CFTYPE_WORLD_WRITABLE,
	},
	{
		.name = "swappiness",
		.read_u64 = mem_cgroup_swappiness_read,
		.write_u64 = mem_cgroup_swappiness_write,
	},
	{
		.name = "move_charge_at_immigrate",
		.read_u64 = mem_cgroup_move_charge_read,
		.write_u64 = mem_cgroup_move_charge_write,
	},
	{
		.name = "oom_control",
		.seq_show = mem_cgroup_oom_control_read,
		.write_u64 = mem_cgroup_oom_control_write,
	},
	{
		.name = "pressure_level",
		.seq_show = mem_cgroup_dummy_seq_show,
	},
#ifdef CONFIG_NUMA
	{
		.name = "numa_stat",
		.seq_show = memcg_numa_stat_show,
	},
#endif
	{
		.name = "kmem.limit_in_bytes",
		.private = MEMFILE_PRIVATE(_KMEM, RES_LIMIT),
		.write = mem_cgroup_write,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "kmem.usage_in_bytes",
		.private = MEMFILE_PRIVATE(_KMEM, RES_USAGE),
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "kmem.failcnt",
		.private = MEMFILE_PRIVATE(_KMEM, RES_FAILCNT),
		.write = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "kmem.max_usage_in_bytes",
		.private = MEMFILE_PRIVATE(_KMEM, RES_MAX_USAGE),
		.write = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read_u64,
	},
#if defined(CONFIG_MEMCG_KMEM) && \
	(defined(CONFIG_SLAB) || defined(CONFIG_SLUB_DEBUG))
	{
		.name = "kmem.slabinfo",
		.seq_show = mem_cgroup_slab_show,
	},
#endif
	{
		.name = "kmem.tcp.limit_in_bytes",
		.private = MEMFILE_PRIVATE(_TCP, RES_LIMIT),
		.write = mem_cgroup_write,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "kmem.tcp.usage_in_bytes",
		.private = MEMFILE_PRIVATE(_TCP, RES_USAGE),
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "kmem.tcp.failcnt",
		.private = MEMFILE_PRIVATE(_TCP, RES_FAILCNT),
		.write = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "kmem.tcp.max_usage_in_bytes",
		.private = MEMFILE_PRIVATE(_TCP, RES_MAX_USAGE),
		.write = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read_u64,
	},
	{ },	 
};

 

#define MEM_CGROUP_ID_MAX	((1UL << MEM_CGROUP_ID_SHIFT) - 1)
static DEFINE_IDR(mem_cgroup_idr);

static void mem_cgroup_id_remove(struct mem_cgroup *memcg)
{
	if (memcg->id.id > 0) {
		idr_remove(&mem_cgroup_idr, memcg->id.id);
		memcg->id.id = 0;
	}
}

static void __maybe_unused mem_cgroup_id_get_many(struct mem_cgroup *memcg,
						  unsigned int n)
{
	refcount_add(n, &memcg->id.ref);
}

static void mem_cgroup_id_put_many(struct mem_cgroup *memcg, unsigned int n)
{
	if (refcount_sub_and_test(n, &memcg->id.ref)) {
		mem_cgroup_id_remove(memcg);

		 
		css_put(&memcg->css);
	}
}

static inline void mem_cgroup_id_put(struct mem_cgroup *memcg)
{
	mem_cgroup_id_put_many(memcg, 1);
}

 
struct mem_cgroup *mem_cgroup_from_id(unsigned short id)
{
	WARN_ON_ONCE(!rcu_read_lock_held());
	return idr_find(&mem_cgroup_idr, id);
}

#ifdef CONFIG_SHRINKER_DEBUG
struct mem_cgroup *mem_cgroup_get_from_ino(unsigned long ino)
{
	struct cgroup *cgrp;
	struct cgroup_subsys_state *css;
	struct mem_cgroup *memcg;

	cgrp = cgroup_get_from_id(ino);
	if (IS_ERR(cgrp))
		return ERR_CAST(cgrp);

	css = cgroup_get_e_css(cgrp, &memory_cgrp_subsys);
	if (css)
		memcg = container_of(css, struct mem_cgroup, css);
	else
		memcg = ERR_PTR(-ENOENT);

	cgroup_put(cgrp);

	return memcg;
}
#endif

static int alloc_mem_cgroup_per_node_info(struct mem_cgroup *memcg, int node)
{
	struct mem_cgroup_per_node *pn;

	pn = kzalloc_node(sizeof(*pn), GFP_KERNEL, node);
	if (!pn)
		return 1;

	pn->lruvec_stats_percpu = alloc_percpu_gfp(struct lruvec_stats_percpu,
						   GFP_KERNEL_ACCOUNT);
	if (!pn->lruvec_stats_percpu) {
		kfree(pn);
		return 1;
	}

	lruvec_init(&pn->lruvec);
	pn->memcg = memcg;

	memcg->nodeinfo[node] = pn;
	return 0;
}

static void free_mem_cgroup_per_node_info(struct mem_cgroup *memcg, int node)
{
	struct mem_cgroup_per_node *pn = memcg->nodeinfo[node];

	if (!pn)
		return;

	free_percpu(pn->lruvec_stats_percpu);
	kfree(pn);
}

static void __mem_cgroup_free(struct mem_cgroup *memcg)
{
	int node;

	for_each_node(node)
		free_mem_cgroup_per_node_info(memcg, node);
	kfree(memcg->vmstats);
	free_percpu(memcg->vmstats_percpu);
	kfree(memcg);
}

static void mem_cgroup_free(struct mem_cgroup *memcg)
{
	lru_gen_exit_memcg(memcg);
	memcg_wb_domain_exit(memcg);
	__mem_cgroup_free(memcg);
}

static struct mem_cgroup *mem_cgroup_alloc(void)
{
	struct mem_cgroup *memcg;
	int node;
	int __maybe_unused i;
	long error = -ENOMEM;

	memcg = kzalloc(struct_size(memcg, nodeinfo, nr_node_ids), GFP_KERNEL);
	if (!memcg)
		return ERR_PTR(error);

	memcg->id.id = idr_alloc(&mem_cgroup_idr, NULL,
				 1, MEM_CGROUP_ID_MAX + 1, GFP_KERNEL);
	if (memcg->id.id < 0) {
		error = memcg->id.id;
		goto fail;
	}

	memcg->vmstats = kzalloc(sizeof(struct memcg_vmstats), GFP_KERNEL);
	if (!memcg->vmstats)
		goto fail;

	memcg->vmstats_percpu = alloc_percpu_gfp(struct memcg_vmstats_percpu,
						 GFP_KERNEL_ACCOUNT);
	if (!memcg->vmstats_percpu)
		goto fail;

	for_each_node(node)
		if (alloc_mem_cgroup_per_node_info(memcg, node))
			goto fail;

	if (memcg_wb_domain_init(memcg, GFP_KERNEL))
		goto fail;

	INIT_WORK(&memcg->high_work, high_work_func);
	INIT_LIST_HEAD(&memcg->oom_notify);
	mutex_init(&memcg->thresholds_lock);
	spin_lock_init(&memcg->move_lock);
	vmpressure_init(&memcg->vmpressure);
	INIT_LIST_HEAD(&memcg->event_list);
	spin_lock_init(&memcg->event_list_lock);
	memcg->socket_pressure = jiffies;
#ifdef CONFIG_MEMCG_KMEM
	memcg->kmemcg_id = -1;
	INIT_LIST_HEAD(&memcg->objcg_list);
#endif
#ifdef CONFIG_CGROUP_WRITEBACK
	INIT_LIST_HEAD(&memcg->cgwb_list);
	for (i = 0; i < MEMCG_CGWB_FRN_CNT; i++)
		memcg->cgwb_frn[i].done =
			__WB_COMPLETION_INIT(&memcg_cgwb_frn_waitq);
#endif
#ifdef CONFIG_TRANSPARENT_HUGEPAGE
	spin_lock_init(&memcg->deferred_split_queue.split_queue_lock);
	INIT_LIST_HEAD(&memcg->deferred_split_queue.split_queue);
	memcg->deferred_split_queue.split_queue_len = 0;
#endif
	lru_gen_init_memcg(memcg);
	return memcg;
fail:
	mem_cgroup_id_remove(memcg);
	__mem_cgroup_free(memcg);
	return ERR_PTR(error);
}

static struct cgroup_subsys_state * __ref
mem_cgroup_css_alloc(struct cgroup_subsys_state *parent_css)
{
	struct mem_cgroup *parent = mem_cgroup_from_css(parent_css);
	struct mem_cgroup *memcg, *old_memcg;

	old_memcg = set_active_memcg(parent);
	memcg = mem_cgroup_alloc();
	set_active_memcg(old_memcg);
	if (IS_ERR(memcg))
		return ERR_CAST(memcg);

	page_counter_set_high(&memcg->memory, PAGE_COUNTER_MAX);
	WRITE_ONCE(memcg->soft_limit, PAGE_COUNTER_MAX);
#if defined(CONFIG_MEMCG_KMEM) && defined(CONFIG_ZSWAP)
	memcg->zswap_max = PAGE_COUNTER_MAX;
#endif
	page_counter_set_high(&memcg->swap, PAGE_COUNTER_MAX);
	if (parent) {
		WRITE_ONCE(memcg->swappiness, mem_cgroup_swappiness(parent));
		WRITE_ONCE(memcg->oom_kill_disable, READ_ONCE(parent->oom_kill_disable));

		page_counter_init(&memcg->memory, &parent->memory);
		page_counter_init(&memcg->swap, &parent->swap);
		page_counter_init(&memcg->kmem, &parent->kmem);
		page_counter_init(&memcg->tcpmem, &parent->tcpmem);
	} else {
		init_memcg_events();
		page_counter_init(&memcg->memory, NULL);
		page_counter_init(&memcg->swap, NULL);
		page_counter_init(&memcg->kmem, NULL);
		page_counter_init(&memcg->tcpmem, NULL);

		root_mem_cgroup = memcg;
		return &memcg->css;
	}

	if (cgroup_subsys_on_dfl(memory_cgrp_subsys) && !cgroup_memory_nosocket)
		static_branch_inc(&memcg_sockets_enabled_key);

#if defined(CONFIG_MEMCG_KMEM)
	if (!cgroup_memory_nobpf)
		static_branch_inc(&memcg_bpf_enabled_key);
#endif

	return &memcg->css;
}

static int mem_cgroup_css_online(struct cgroup_subsys_state *css)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	if (memcg_online_kmem(memcg))
		goto remove_id;

	 
	if (alloc_shrinker_info(memcg))
		goto offline_kmem;

	if (unlikely(mem_cgroup_is_root(memcg)))
		queue_delayed_work(system_unbound_wq, &stats_flush_dwork,
				   FLUSH_TIME);
	lru_gen_online_memcg(memcg);

	 
	refcount_set(&memcg->id.ref, 1);
	css_get(css);

	 
	idr_replace(&mem_cgroup_idr, memcg, memcg->id.id);

	return 0;
offline_kmem:
	memcg_offline_kmem(memcg);
remove_id:
	mem_cgroup_id_remove(memcg);
	return -ENOMEM;
}

static void mem_cgroup_css_offline(struct cgroup_subsys_state *css)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct mem_cgroup_event *event, *tmp;

	 
	spin_lock_irq(&memcg->event_list_lock);
	list_for_each_entry_safe(event, tmp, &memcg->event_list, list) {
		list_del_init(&event->list);
		schedule_work(&event->remove);
	}
	spin_unlock_irq(&memcg->event_list_lock);

	page_counter_set_min(&memcg->memory, 0);
	page_counter_set_low(&memcg->memory, 0);

	memcg_offline_kmem(memcg);
	reparent_shrinker_deferred(memcg);
	wb_memcg_offline(memcg);
	lru_gen_offline_memcg(memcg);

	drain_all_stock(memcg);

	mem_cgroup_id_put(memcg);
}

static void mem_cgroup_css_released(struct cgroup_subsys_state *css)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	invalidate_reclaim_iterators(memcg);
	lru_gen_release_memcg(memcg);
}

static void mem_cgroup_css_free(struct cgroup_subsys_state *css)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	int __maybe_unused i;

#ifdef CONFIG_CGROUP_WRITEBACK
	for (i = 0; i < MEMCG_CGWB_FRN_CNT; i++)
		wb_wait_for_completion(&memcg->cgwb_frn[i].done);
#endif
	if (cgroup_subsys_on_dfl(memory_cgrp_subsys) && !cgroup_memory_nosocket)
		static_branch_dec(&memcg_sockets_enabled_key);

	if (!cgroup_subsys_on_dfl(memory_cgrp_subsys) && memcg->tcpmem_active)
		static_branch_dec(&memcg_sockets_enabled_key);

#if defined(CONFIG_MEMCG_KMEM)
	if (!cgroup_memory_nobpf)
		static_branch_dec(&memcg_bpf_enabled_key);
#endif

	vmpressure_cleanup(&memcg->vmpressure);
	cancel_work_sync(&memcg->high_work);
	mem_cgroup_remove_from_trees(memcg);
	free_shrinker_info(memcg);
	mem_cgroup_free(memcg);
}

 
static void mem_cgroup_css_reset(struct cgroup_subsys_state *css)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	page_counter_set_max(&memcg->memory, PAGE_COUNTER_MAX);
	page_counter_set_max(&memcg->swap, PAGE_COUNTER_MAX);
	page_counter_set_max(&memcg->kmem, PAGE_COUNTER_MAX);
	page_counter_set_max(&memcg->tcpmem, PAGE_COUNTER_MAX);
	page_counter_set_min(&memcg->memory, 0);
	page_counter_set_low(&memcg->memory, 0);
	page_counter_set_high(&memcg->memory, PAGE_COUNTER_MAX);
	WRITE_ONCE(memcg->soft_limit, PAGE_COUNTER_MAX);
	page_counter_set_high(&memcg->swap, PAGE_COUNTER_MAX);
	memcg_wb_domain_size_changed(memcg);
}

static void mem_cgroup_css_rstat_flush(struct cgroup_subsys_state *css, int cpu)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);
	struct mem_cgroup *parent = parent_mem_cgroup(memcg);
	struct memcg_vmstats_percpu *statc;
	long delta, delta_cpu, v;
	int i, nid;

	statc = per_cpu_ptr(memcg->vmstats_percpu, cpu);

	for (i = 0; i < MEMCG_NR_STAT; i++) {
		 
		delta = memcg->vmstats->state_pending[i];
		if (delta)
			memcg->vmstats->state_pending[i] = 0;

		 
		delta_cpu = 0;
		v = READ_ONCE(statc->state[i]);
		if (v != statc->state_prev[i]) {
			delta_cpu = v - statc->state_prev[i];
			delta += delta_cpu;
			statc->state_prev[i] = v;
		}

		 
		if (delta_cpu)
			memcg->vmstats->state_local[i] += delta_cpu;

		if (delta) {
			memcg->vmstats->state[i] += delta;
			if (parent)
				parent->vmstats->state_pending[i] += delta;
		}
	}

	for (i = 0; i < NR_MEMCG_EVENTS; i++) {
		delta = memcg->vmstats->events_pending[i];
		if (delta)
			memcg->vmstats->events_pending[i] = 0;

		delta_cpu = 0;
		v = READ_ONCE(statc->events[i]);
		if (v != statc->events_prev[i]) {
			delta_cpu = v - statc->events_prev[i];
			delta += delta_cpu;
			statc->events_prev[i] = v;
		}

		if (delta_cpu)
			memcg->vmstats->events_local[i] += delta_cpu;

		if (delta) {
			memcg->vmstats->events[i] += delta;
			if (parent)
				parent->vmstats->events_pending[i] += delta;
		}
	}

	for_each_node_state(nid, N_MEMORY) {
		struct mem_cgroup_per_node *pn = memcg->nodeinfo[nid];
		struct mem_cgroup_per_node *ppn = NULL;
		struct lruvec_stats_percpu *lstatc;

		if (parent)
			ppn = parent->nodeinfo[nid];

		lstatc = per_cpu_ptr(pn->lruvec_stats_percpu, cpu);

		for (i = 0; i < NR_VM_NODE_STAT_ITEMS; i++) {
			delta = pn->lruvec_stats.state_pending[i];
			if (delta)
				pn->lruvec_stats.state_pending[i] = 0;

			delta_cpu = 0;
			v = READ_ONCE(lstatc->state[i]);
			if (v != lstatc->state_prev[i]) {
				delta_cpu = v - lstatc->state_prev[i];
				delta += delta_cpu;
				lstatc->state_prev[i] = v;
			}

			if (delta_cpu)
				pn->lruvec_stats.state_local[i] += delta_cpu;

			if (delta) {
				pn->lruvec_stats.state[i] += delta;
				if (ppn)
					ppn->lruvec_stats.state_pending[i] += delta;
			}
		}
	}
}

#ifdef CONFIG_MMU
 
static int mem_cgroup_do_precharge(unsigned long count)
{
	int ret;

	 
	ret = try_charge(mc.to, GFP_KERNEL & ~__GFP_DIRECT_RECLAIM, count);
	if (!ret) {
		mc.precharge += count;
		return ret;
	}

	 
	while (count--) {
		ret = try_charge(mc.to, GFP_KERNEL | __GFP_NORETRY, 1);
		if (ret)
			return ret;
		mc.precharge++;
		cond_resched();
	}
	return 0;
}

union mc_target {
	struct page	*page;
	swp_entry_t	ent;
};

enum mc_target_type {
	MC_TARGET_NONE = 0,
	MC_TARGET_PAGE,
	MC_TARGET_SWAP,
	MC_TARGET_DEVICE,
};

static struct page *mc_handle_present_pte(struct vm_area_struct *vma,
						unsigned long addr, pte_t ptent)
{
	struct page *page = vm_normal_page(vma, addr, ptent);

	if (!page)
		return NULL;
	if (PageAnon(page)) {
		if (!(mc.flags & MOVE_ANON))
			return NULL;
	} else {
		if (!(mc.flags & MOVE_FILE))
			return NULL;
	}
	get_page(page);

	return page;
}

#if defined(CONFIG_SWAP) || defined(CONFIG_DEVICE_PRIVATE)
static struct page *mc_handle_swap_pte(struct vm_area_struct *vma,
			pte_t ptent, swp_entry_t *entry)
{
	struct page *page = NULL;
	swp_entry_t ent = pte_to_swp_entry(ptent);

	if (!(mc.flags & MOVE_ANON))
		return NULL;

	 
	if (is_device_private_entry(ent)) {
		page = pfn_swap_entry_to_page(ent);
		if (!get_page_unless_zero(page))
			return NULL;
		return page;
	}

	if (non_swap_entry(ent))
		return NULL;

	 
	page = find_get_page(swap_address_space(ent), swp_offset(ent));
	entry->val = ent.val;

	return page;
}
#else
static struct page *mc_handle_swap_pte(struct vm_area_struct *vma,
			pte_t ptent, swp_entry_t *entry)
{
	return NULL;
}
#endif

static struct page *mc_handle_file_pte(struct vm_area_struct *vma,
			unsigned long addr, pte_t ptent)
{
	unsigned long index;
	struct folio *folio;

	if (!vma->vm_file)  
		return NULL;
	if (!(mc.flags & MOVE_FILE))
		return NULL;

	 
	 
	index = linear_page_index(vma, addr);
	folio = filemap_get_incore_folio(vma->vm_file->f_mapping, index);
	if (IS_ERR(folio))
		return NULL;
	return folio_file_page(folio, index);
}

 
static int mem_cgroup_move_account(struct page *page,
				   bool compound,
				   struct mem_cgroup *from,
				   struct mem_cgroup *to)
{
	struct folio *folio = page_folio(page);
	struct lruvec *from_vec, *to_vec;
	struct pglist_data *pgdat;
	unsigned int nr_pages = compound ? folio_nr_pages(folio) : 1;
	int nid, ret;

	VM_BUG_ON(from == to);
	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);
	VM_BUG_ON_FOLIO(folio_test_lru(folio), folio);
	VM_BUG_ON(compound && !folio_test_large(folio));

	ret = -EINVAL;
	if (folio_memcg(folio) != from)
		goto out;

	pgdat = folio_pgdat(folio);
	from_vec = mem_cgroup_lruvec(from, pgdat);
	to_vec = mem_cgroup_lruvec(to, pgdat);

	folio_memcg_lock(folio);

	if (folio_test_anon(folio)) {
		if (folio_mapped(folio)) {
			__mod_lruvec_state(from_vec, NR_ANON_MAPPED, -nr_pages);
			__mod_lruvec_state(to_vec, NR_ANON_MAPPED, nr_pages);
			if (folio_test_pmd_mappable(folio)) {
				__mod_lruvec_state(from_vec, NR_ANON_THPS,
						   -nr_pages);
				__mod_lruvec_state(to_vec, NR_ANON_THPS,
						   nr_pages);
			}
		}
	} else {
		__mod_lruvec_state(from_vec, NR_FILE_PAGES, -nr_pages);
		__mod_lruvec_state(to_vec, NR_FILE_PAGES, nr_pages);

		if (folio_test_swapbacked(folio)) {
			__mod_lruvec_state(from_vec, NR_SHMEM, -nr_pages);
			__mod_lruvec_state(to_vec, NR_SHMEM, nr_pages);
		}

		if (folio_mapped(folio)) {
			__mod_lruvec_state(from_vec, NR_FILE_MAPPED, -nr_pages);
			__mod_lruvec_state(to_vec, NR_FILE_MAPPED, nr_pages);
		}

		if (folio_test_dirty(folio)) {
			struct address_space *mapping = folio_mapping(folio);

			if (mapping_can_writeback(mapping)) {
				__mod_lruvec_state(from_vec, NR_FILE_DIRTY,
						   -nr_pages);
				__mod_lruvec_state(to_vec, NR_FILE_DIRTY,
						   nr_pages);
			}
		}
	}

#ifdef CONFIG_SWAP
	if (folio_test_swapcache(folio)) {
		__mod_lruvec_state(from_vec, NR_SWAPCACHE, -nr_pages);
		__mod_lruvec_state(to_vec, NR_SWAPCACHE, nr_pages);
	}
#endif
	if (folio_test_writeback(folio)) {
		__mod_lruvec_state(from_vec, NR_WRITEBACK, -nr_pages);
		__mod_lruvec_state(to_vec, NR_WRITEBACK, nr_pages);
	}

	 
	smp_mb();

	css_get(&to->css);
	css_put(&from->css);

	folio->memcg_data = (unsigned long)to;

	__folio_memcg_unlock(from);

	ret = 0;
	nid = folio_nid(folio);

	local_irq_disable();
	mem_cgroup_charge_statistics(to, nr_pages);
	memcg_check_events(to, nid);
	mem_cgroup_charge_statistics(from, -nr_pages);
	memcg_check_events(from, nid);
	local_irq_enable();
out:
	return ret;
}

 
static enum mc_target_type get_mctgt_type(struct vm_area_struct *vma,
		unsigned long addr, pte_t ptent, union mc_target *target)
{
	struct page *page = NULL;
	enum mc_target_type ret = MC_TARGET_NONE;
	swp_entry_t ent = { .val = 0 };

	if (pte_present(ptent))
		page = mc_handle_present_pte(vma, addr, ptent);
	else if (pte_none_mostly(ptent))
		 
		page = mc_handle_file_pte(vma, addr, ptent);
	else if (is_swap_pte(ptent))
		page = mc_handle_swap_pte(vma, ptent, &ent);

	if (target && page) {
		if (!trylock_page(page)) {
			put_page(page);
			return ret;
		}
		 
		if (!pte_present(ptent) && page_mapped(page)) {
			unlock_page(page);
			put_page(page);
			return ret;
		}
	}

	if (!page && !ent.val)
		return ret;
	if (page) {
		 
		if (page_memcg(page) == mc.from) {
			ret = MC_TARGET_PAGE;
			if (is_device_private_page(page) ||
			    is_device_coherent_page(page))
				ret = MC_TARGET_DEVICE;
			if (target)
				target->page = page;
		}
		if (!ret || !target) {
			if (target)
				unlock_page(page);
			put_page(page);
		}
	}
	 
	if (ent.val && !ret && (!page || !PageTransCompound(page)) &&
	    mem_cgroup_id(mc.from) == lookup_swap_cgroup_id(ent)) {
		ret = MC_TARGET_SWAP;
		if (target)
			target->ent = ent;
	}
	return ret;
}

#ifdef CONFIG_TRANSPARENT_HUGEPAGE
 
static enum mc_target_type get_mctgt_type_thp(struct vm_area_struct *vma,
		unsigned long addr, pmd_t pmd, union mc_target *target)
{
	struct page *page = NULL;
	enum mc_target_type ret = MC_TARGET_NONE;

	if (unlikely(is_swap_pmd(pmd))) {
		VM_BUG_ON(thp_migration_supported() &&
				  !is_pmd_migration_entry(pmd));
		return ret;
	}
	page = pmd_page(pmd);
	VM_BUG_ON_PAGE(!page || !PageHead(page), page);
	if (!(mc.flags & MOVE_ANON))
		return ret;
	if (page_memcg(page) == mc.from) {
		ret = MC_TARGET_PAGE;
		if (target) {
			get_page(page);
			if (!trylock_page(page)) {
				put_page(page);
				return MC_TARGET_NONE;
			}
			target->page = page;
		}
	}
	return ret;
}
#else
static inline enum mc_target_type get_mctgt_type_thp(struct vm_area_struct *vma,
		unsigned long addr, pmd_t pmd, union mc_target *target)
{
	return MC_TARGET_NONE;
}
#endif

static int mem_cgroup_count_precharge_pte_range(pmd_t *pmd,
					unsigned long addr, unsigned long end,
					struct mm_walk *walk)
{
	struct vm_area_struct *vma = walk->vma;
	pte_t *pte;
	spinlock_t *ptl;

	ptl = pmd_trans_huge_lock(pmd, vma);
	if (ptl) {
		 
		if (get_mctgt_type_thp(vma, addr, *pmd, NULL) == MC_TARGET_PAGE)
			mc.precharge += HPAGE_PMD_NR;
		spin_unlock(ptl);
		return 0;
	}

	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	if (!pte)
		return 0;
	for (; addr != end; pte++, addr += PAGE_SIZE)
		if (get_mctgt_type(vma, addr, ptep_get(pte), NULL))
			mc.precharge++;	 
	pte_unmap_unlock(pte - 1, ptl);
	cond_resched();

	return 0;
}

static const struct mm_walk_ops precharge_walk_ops = {
	.pmd_entry	= mem_cgroup_count_precharge_pte_range,
	.walk_lock	= PGWALK_RDLOCK,
};

static unsigned long mem_cgroup_count_precharge(struct mm_struct *mm)
{
	unsigned long precharge;

	mmap_read_lock(mm);
	walk_page_range(mm, 0, ULONG_MAX, &precharge_walk_ops, NULL);
	mmap_read_unlock(mm);

	precharge = mc.precharge;
	mc.precharge = 0;

	return precharge;
}

static int mem_cgroup_precharge_mc(struct mm_struct *mm)
{
	unsigned long precharge = mem_cgroup_count_precharge(mm);

	VM_BUG_ON(mc.moving_task);
	mc.moving_task = current;
	return mem_cgroup_do_precharge(precharge);
}

 
static void __mem_cgroup_clear_mc(void)
{
	struct mem_cgroup *from = mc.from;
	struct mem_cgroup *to = mc.to;

	 
	if (mc.precharge) {
		cancel_charge(mc.to, mc.precharge);
		mc.precharge = 0;
	}
	 
	if (mc.moved_charge) {
		cancel_charge(mc.from, mc.moved_charge);
		mc.moved_charge = 0;
	}
	 
	if (mc.moved_swap) {
		 
		if (!mem_cgroup_is_root(mc.from))
			page_counter_uncharge(&mc.from->memsw, mc.moved_swap);

		mem_cgroup_id_put_many(mc.from, mc.moved_swap);

		 
		if (!mem_cgroup_is_root(mc.to))
			page_counter_uncharge(&mc.to->memory, mc.moved_swap);

		mc.moved_swap = 0;
	}
	memcg_oom_recover(from);
	memcg_oom_recover(to);
	wake_up_all(&mc.waitq);
}

static void mem_cgroup_clear_mc(void)
{
	struct mm_struct *mm = mc.mm;

	 
	mc.moving_task = NULL;
	__mem_cgroup_clear_mc();
	spin_lock(&mc.lock);
	mc.from = NULL;
	mc.to = NULL;
	mc.mm = NULL;
	spin_unlock(&mc.lock);

	mmput(mm);
}

static int mem_cgroup_can_attach(struct cgroup_taskset *tset)
{
	struct cgroup_subsys_state *css;
	struct mem_cgroup *memcg = NULL;  
	struct mem_cgroup *from;
	struct task_struct *leader, *p;
	struct mm_struct *mm;
	unsigned long move_flags;
	int ret = 0;

	 
	if (cgroup_subsys_on_dfl(memory_cgrp_subsys))
		return 0;

	 
	p = NULL;
	cgroup_taskset_for_each_leader(leader, css, tset) {
		WARN_ON_ONCE(p);
		p = leader;
		memcg = mem_cgroup_from_css(css);
	}
	if (!p)
		return 0;

	 
	move_flags = READ_ONCE(memcg->move_charge_at_immigrate);
	if (!move_flags)
		return 0;

	from = mem_cgroup_from_task(p);

	VM_BUG_ON(from == memcg);

	mm = get_task_mm(p);
	if (!mm)
		return 0;
	 
	if (mm->owner == p) {
		VM_BUG_ON(mc.from);
		VM_BUG_ON(mc.to);
		VM_BUG_ON(mc.precharge);
		VM_BUG_ON(mc.moved_charge);
		VM_BUG_ON(mc.moved_swap);

		spin_lock(&mc.lock);
		mc.mm = mm;
		mc.from = from;
		mc.to = memcg;
		mc.flags = move_flags;
		spin_unlock(&mc.lock);
		 

		ret = mem_cgroup_precharge_mc(mm);
		if (ret)
			mem_cgroup_clear_mc();
	} else {
		mmput(mm);
	}
	return ret;
}

static void mem_cgroup_cancel_attach(struct cgroup_taskset *tset)
{
	if (mc.to)
		mem_cgroup_clear_mc();
}

static int mem_cgroup_move_charge_pte_range(pmd_t *pmd,
				unsigned long addr, unsigned long end,
				struct mm_walk *walk)
{
	int ret = 0;
	struct vm_area_struct *vma = walk->vma;
	pte_t *pte;
	spinlock_t *ptl;
	enum mc_target_type target_type;
	union mc_target target;
	struct page *page;

	ptl = pmd_trans_huge_lock(pmd, vma);
	if (ptl) {
		if (mc.precharge < HPAGE_PMD_NR) {
			spin_unlock(ptl);
			return 0;
		}
		target_type = get_mctgt_type_thp(vma, addr, *pmd, &target);
		if (target_type == MC_TARGET_PAGE) {
			page = target.page;
			if (isolate_lru_page(page)) {
				if (!mem_cgroup_move_account(page, true,
							     mc.from, mc.to)) {
					mc.precharge -= HPAGE_PMD_NR;
					mc.moved_charge += HPAGE_PMD_NR;
				}
				putback_lru_page(page);
			}
			unlock_page(page);
			put_page(page);
		} else if (target_type == MC_TARGET_DEVICE) {
			page = target.page;
			if (!mem_cgroup_move_account(page, true,
						     mc.from, mc.to)) {
				mc.precharge -= HPAGE_PMD_NR;
				mc.moved_charge += HPAGE_PMD_NR;
			}
			unlock_page(page);
			put_page(page);
		}
		spin_unlock(ptl);
		return 0;
	}

retry:
	pte = pte_offset_map_lock(vma->vm_mm, pmd, addr, &ptl);
	if (!pte)
		return 0;
	for (; addr != end; addr += PAGE_SIZE) {
		pte_t ptent = ptep_get(pte++);
		bool device = false;
		swp_entry_t ent;

		if (!mc.precharge)
			break;

		switch (get_mctgt_type(vma, addr, ptent, &target)) {
		case MC_TARGET_DEVICE:
			device = true;
			fallthrough;
		case MC_TARGET_PAGE:
			page = target.page;
			 
			if (PageTransCompound(page))
				goto put;
			if (!device && !isolate_lru_page(page))
				goto put;
			if (!mem_cgroup_move_account(page, false,
						mc.from, mc.to)) {
				mc.precharge--;
				 
				mc.moved_charge++;
			}
			if (!device)
				putback_lru_page(page);
put:			 
			unlock_page(page);
			put_page(page);
			break;
		case MC_TARGET_SWAP:
			ent = target.ent;
			if (!mem_cgroup_move_swap_account(ent, mc.from, mc.to)) {
				mc.precharge--;
				mem_cgroup_id_get_many(mc.to, 1);
				 
				mc.moved_swap++;
			}
			break;
		default:
			break;
		}
	}
	pte_unmap_unlock(pte - 1, ptl);
	cond_resched();

	if (addr != end) {
		 
		ret = mem_cgroup_do_precharge(1);
		if (!ret)
			goto retry;
	}

	return ret;
}

static const struct mm_walk_ops charge_walk_ops = {
	.pmd_entry	= mem_cgroup_move_charge_pte_range,
	.walk_lock	= PGWALK_RDLOCK,
};

static void mem_cgroup_move_charge(void)
{
	lru_add_drain_all();
	 
	atomic_inc(&mc.from->moving_account);
	synchronize_rcu();
retry:
	if (unlikely(!mmap_read_trylock(mc.mm))) {
		 
		__mem_cgroup_clear_mc();
		cond_resched();
		goto retry;
	}
	 
	walk_page_range(mc.mm, 0, ULONG_MAX, &charge_walk_ops, NULL);
	mmap_read_unlock(mc.mm);
	atomic_dec(&mc.from->moving_account);
}

static void mem_cgroup_move_task(void)
{
	if (mc.to) {
		mem_cgroup_move_charge();
		mem_cgroup_clear_mc();
	}
}
#else	 
static int mem_cgroup_can_attach(struct cgroup_taskset *tset)
{
	return 0;
}
static void mem_cgroup_cancel_attach(struct cgroup_taskset *tset)
{
}
static void mem_cgroup_move_task(void)
{
}
#endif

#ifdef CONFIG_LRU_GEN
static void mem_cgroup_attach(struct cgroup_taskset *tset)
{
	struct task_struct *task;
	struct cgroup_subsys_state *css;

	 
	cgroup_taskset_for_each_leader(task, css, tset)
		break;

	if (!task)
		return;

	task_lock(task);
	if (task->mm && READ_ONCE(task->mm->owner) == task)
		lru_gen_migrate_mm(task->mm);
	task_unlock(task);
}
#else
static void mem_cgroup_attach(struct cgroup_taskset *tset)
{
}
#endif  

static int seq_puts_memcg_tunable(struct seq_file *m, unsigned long value)
{
	if (value == PAGE_COUNTER_MAX)
		seq_puts(m, "max\n");
	else
		seq_printf(m, "%llu\n", (u64)value * PAGE_SIZE);

	return 0;
}

static u64 memory_current_read(struct cgroup_subsys_state *css,
			       struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	return (u64)page_counter_read(&memcg->memory) * PAGE_SIZE;
}

static u64 memory_peak_read(struct cgroup_subsys_state *css,
			    struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	return (u64)memcg->memory.watermark * PAGE_SIZE;
}

static int memory_min_show(struct seq_file *m, void *v)
{
	return seq_puts_memcg_tunable(m,
		READ_ONCE(mem_cgroup_from_seq(m)->memory.min));
}

static ssize_t memory_min_write(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned long min;
	int err;

	buf = strstrip(buf);
	err = page_counter_memparse(buf, "max", &min);
	if (err)
		return err;

	page_counter_set_min(&memcg->memory, min);

	return nbytes;
}

static int memory_low_show(struct seq_file *m, void *v)
{
	return seq_puts_memcg_tunable(m,
		READ_ONCE(mem_cgroup_from_seq(m)->memory.low));
}

static ssize_t memory_low_write(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned long low;
	int err;

	buf = strstrip(buf);
	err = page_counter_memparse(buf, "max", &low);
	if (err)
		return err;

	page_counter_set_low(&memcg->memory, low);

	return nbytes;
}

static int memory_high_show(struct seq_file *m, void *v)
{
	return seq_puts_memcg_tunable(m,
		READ_ONCE(mem_cgroup_from_seq(m)->memory.high));
}

static ssize_t memory_high_write(struct kernfs_open_file *of,
				 char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned int nr_retries = MAX_RECLAIM_RETRIES;
	bool drained = false;
	unsigned long high;
	int err;

	buf = strstrip(buf);
	err = page_counter_memparse(buf, "max", &high);
	if (err)
		return err;

	page_counter_set_high(&memcg->memory, high);

	for (;;) {
		unsigned long nr_pages = page_counter_read(&memcg->memory);
		unsigned long reclaimed;

		if (nr_pages <= high)
			break;

		if (signal_pending(current))
			break;

		if (!drained) {
			drain_all_stock(memcg);
			drained = true;
			continue;
		}

		reclaimed = try_to_free_mem_cgroup_pages(memcg, nr_pages - high,
					GFP_KERNEL, MEMCG_RECLAIM_MAY_SWAP);

		if (!reclaimed && !nr_retries--)
			break;
	}

	memcg_wb_domain_size_changed(memcg);
	return nbytes;
}

static int memory_max_show(struct seq_file *m, void *v)
{
	return seq_puts_memcg_tunable(m,
		READ_ONCE(mem_cgroup_from_seq(m)->memory.max));
}

static ssize_t memory_max_write(struct kernfs_open_file *of,
				char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned int nr_reclaims = MAX_RECLAIM_RETRIES;
	bool drained = false;
	unsigned long max;
	int err;

	buf = strstrip(buf);
	err = page_counter_memparse(buf, "max", &max);
	if (err)
		return err;

	xchg(&memcg->memory.max, max);

	for (;;) {
		unsigned long nr_pages = page_counter_read(&memcg->memory);

		if (nr_pages <= max)
			break;

		if (signal_pending(current))
			break;

		if (!drained) {
			drain_all_stock(memcg);
			drained = true;
			continue;
		}

		if (nr_reclaims) {
			if (!try_to_free_mem_cgroup_pages(memcg, nr_pages - max,
					GFP_KERNEL, MEMCG_RECLAIM_MAY_SWAP))
				nr_reclaims--;
			continue;
		}

		memcg_memory_event(memcg, MEMCG_OOM);
		if (!mem_cgroup_out_of_memory(memcg, GFP_KERNEL, 0))
			break;
	}

	memcg_wb_domain_size_changed(memcg);
	return nbytes;
}

static void __memory_events_show(struct seq_file *m, atomic_long_t *events)
{
	seq_printf(m, "low %lu\n", atomic_long_read(&events[MEMCG_LOW]));
	seq_printf(m, "high %lu\n", atomic_long_read(&events[MEMCG_HIGH]));
	seq_printf(m, "max %lu\n", atomic_long_read(&events[MEMCG_MAX]));
	seq_printf(m, "oom %lu\n", atomic_long_read(&events[MEMCG_OOM]));
	seq_printf(m, "oom_kill %lu\n",
		   atomic_long_read(&events[MEMCG_OOM_KILL]));
	seq_printf(m, "oom_group_kill %lu\n",
		   atomic_long_read(&events[MEMCG_OOM_GROUP_KILL]));
}

static int memory_events_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_seq(m);

	__memory_events_show(m, memcg->memory_events);
	return 0;
}

static int memory_events_local_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_seq(m);

	__memory_events_show(m, memcg->memory_events_local);
	return 0;
}

static int memory_stat_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_seq(m);
	char *buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	struct seq_buf s;

	if (!buf)
		return -ENOMEM;
	seq_buf_init(&s, buf, PAGE_SIZE);
	memory_stat_format(memcg, &s);
	seq_puts(m, buf);
	kfree(buf);
	return 0;
}

#ifdef CONFIG_NUMA
static inline unsigned long lruvec_page_state_output(struct lruvec *lruvec,
						     int item)
{
	return lruvec_page_state(lruvec, item) * memcg_page_state_unit(item);
}

static int memory_numa_stat_show(struct seq_file *m, void *v)
{
	int i;
	struct mem_cgroup *memcg = mem_cgroup_from_seq(m);

	mem_cgroup_flush_stats();

	for (i = 0; i < ARRAY_SIZE(memory_stats); i++) {
		int nid;

		if (memory_stats[i].idx >= NR_VM_NODE_STAT_ITEMS)
			continue;

		seq_printf(m, "%s", memory_stats[i].name);
		for_each_node_state(nid, N_MEMORY) {
			u64 size;
			struct lruvec *lruvec;

			lruvec = mem_cgroup_lruvec(memcg, NODE_DATA(nid));
			size = lruvec_page_state_output(lruvec,
							memory_stats[i].idx);
			seq_printf(m, " N%d=%llu", nid, size);
		}
		seq_putc(m, '\n');
	}

	return 0;
}
#endif

static int memory_oom_group_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_seq(m);

	seq_printf(m, "%d\n", READ_ONCE(memcg->oom_group));

	return 0;
}

static ssize_t memory_oom_group_write(struct kernfs_open_file *of,
				      char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	int ret, oom_group;

	buf = strstrip(buf);
	if (!buf)
		return -EINVAL;

	ret = kstrtoint(buf, 0, &oom_group);
	if (ret)
		return ret;

	if (oom_group != 0 && oom_group != 1)
		return -EINVAL;

	WRITE_ONCE(memcg->oom_group, oom_group);

	return nbytes;
}

static ssize_t memory_reclaim(struct kernfs_open_file *of, char *buf,
			      size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned int nr_retries = MAX_RECLAIM_RETRIES;
	unsigned long nr_to_reclaim, nr_reclaimed = 0;
	unsigned int reclaim_options;
	int err;

	buf = strstrip(buf);
	err = page_counter_memparse(buf, "", &nr_to_reclaim);
	if (err)
		return err;

	reclaim_options	= MEMCG_RECLAIM_MAY_SWAP | MEMCG_RECLAIM_PROACTIVE;
	while (nr_reclaimed < nr_to_reclaim) {
		unsigned long reclaimed;

		if (signal_pending(current))
			return -EINTR;

		 
		if (!nr_retries)
			lru_add_drain_all();

		reclaimed = try_to_free_mem_cgroup_pages(memcg,
					min(nr_to_reclaim - nr_reclaimed, SWAP_CLUSTER_MAX),
					GFP_KERNEL, reclaim_options);

		if (!reclaimed && !nr_retries--)
			return -EAGAIN;

		nr_reclaimed += reclaimed;
	}

	return nbytes;
}

static struct cftype memory_files[] = {
	{
		.name = "current",
		.flags = CFTYPE_NOT_ON_ROOT,
		.read_u64 = memory_current_read,
	},
	{
		.name = "peak",
		.flags = CFTYPE_NOT_ON_ROOT,
		.read_u64 = memory_peak_read,
	},
	{
		.name = "min",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = memory_min_show,
		.write = memory_min_write,
	},
	{
		.name = "low",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = memory_low_show,
		.write = memory_low_write,
	},
	{
		.name = "high",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = memory_high_show,
		.write = memory_high_write,
	},
	{
		.name = "max",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = memory_max_show,
		.write = memory_max_write,
	},
	{
		.name = "events",
		.flags = CFTYPE_NOT_ON_ROOT,
		.file_offset = offsetof(struct mem_cgroup, events_file),
		.seq_show = memory_events_show,
	},
	{
		.name = "events.local",
		.flags = CFTYPE_NOT_ON_ROOT,
		.file_offset = offsetof(struct mem_cgroup, events_local_file),
		.seq_show = memory_events_local_show,
	},
	{
		.name = "stat",
		.seq_show = memory_stat_show,
	},
#ifdef CONFIG_NUMA
	{
		.name = "numa_stat",
		.seq_show = memory_numa_stat_show,
	},
#endif
	{
		.name = "oom.group",
		.flags = CFTYPE_NOT_ON_ROOT | CFTYPE_NS_DELEGATABLE,
		.seq_show = memory_oom_group_show,
		.write = memory_oom_group_write,
	},
	{
		.name = "reclaim",
		.flags = CFTYPE_NS_DELEGATABLE,
		.write = memory_reclaim,
	},
	{ }	 
};

struct cgroup_subsys memory_cgrp_subsys = {
	.css_alloc = mem_cgroup_css_alloc,
	.css_online = mem_cgroup_css_online,
	.css_offline = mem_cgroup_css_offline,
	.css_released = mem_cgroup_css_released,
	.css_free = mem_cgroup_css_free,
	.css_reset = mem_cgroup_css_reset,
	.css_rstat_flush = mem_cgroup_css_rstat_flush,
	.can_attach = mem_cgroup_can_attach,
	.attach = mem_cgroup_attach,
	.cancel_attach = mem_cgroup_cancel_attach,
	.post_attach = mem_cgroup_move_task,
	.dfl_cftypes = memory_files,
	.legacy_cftypes = mem_cgroup_legacy_files,
	.early_init = 0,
};

 
static unsigned long effective_protection(unsigned long usage,
					  unsigned long parent_usage,
					  unsigned long setting,
					  unsigned long parent_effective,
					  unsigned long siblings_protected)
{
	unsigned long protected;
	unsigned long ep;

	protected = min(usage, setting);
	 
	if (siblings_protected > parent_effective)
		return protected * parent_effective / siblings_protected;

	 
	ep = protected;

	 
	if (!(cgrp_dfl_root.flags & CGRP_ROOT_MEMORY_RECURSIVE_PROT))
		return ep;
	if (parent_effective > siblings_protected &&
	    parent_usage > siblings_protected &&
	    usage > protected) {
		unsigned long unclaimed;

		unclaimed = parent_effective - siblings_protected;
		unclaimed *= usage - protected;
		unclaimed /= parent_usage - siblings_protected;

		ep += unclaimed;
	}

	return ep;
}

 
void mem_cgroup_calculate_protection(struct mem_cgroup *root,
				     struct mem_cgroup *memcg)
{
	unsigned long usage, parent_usage;
	struct mem_cgroup *parent;

	if (mem_cgroup_disabled())
		return;

	if (!root)
		root = root_mem_cgroup;

	 
	if (memcg == root)
		return;

	usage = page_counter_read(&memcg->memory);
	if (!usage)
		return;

	parent = parent_mem_cgroup(memcg);

	if (parent == root) {
		memcg->memory.emin = READ_ONCE(memcg->memory.min);
		memcg->memory.elow = READ_ONCE(memcg->memory.low);
		return;
	}

	parent_usage = page_counter_read(&parent->memory);

	WRITE_ONCE(memcg->memory.emin, effective_protection(usage, parent_usage,
			READ_ONCE(memcg->memory.min),
			READ_ONCE(parent->memory.emin),
			atomic_long_read(&parent->memory.children_min_usage)));

	WRITE_ONCE(memcg->memory.elow, effective_protection(usage, parent_usage,
			READ_ONCE(memcg->memory.low),
			READ_ONCE(parent->memory.elow),
			atomic_long_read(&parent->memory.children_low_usage)));
}

static int charge_memcg(struct folio *folio, struct mem_cgroup *memcg,
			gfp_t gfp)
{
	long nr_pages = folio_nr_pages(folio);
	int ret;

	ret = try_charge(memcg, gfp, nr_pages);
	if (ret)
		goto out;

	css_get(&memcg->css);
	commit_charge(folio, memcg);

	local_irq_disable();
	mem_cgroup_charge_statistics(memcg, nr_pages);
	memcg_check_events(memcg, folio_nid(folio));
	local_irq_enable();
out:
	return ret;
}

int __mem_cgroup_charge(struct folio *folio, struct mm_struct *mm, gfp_t gfp)
{
	struct mem_cgroup *memcg;
	int ret;

	memcg = get_mem_cgroup_from_mm(mm);
	ret = charge_memcg(folio, memcg, gfp);
	css_put(&memcg->css);

	return ret;
}

 
int mem_cgroup_swapin_charge_folio(struct folio *folio, struct mm_struct *mm,
				  gfp_t gfp, swp_entry_t entry)
{
	struct mem_cgroup *memcg;
	unsigned short id;
	int ret;

	if (mem_cgroup_disabled())
		return 0;

	id = lookup_swap_cgroup_id(entry);
	rcu_read_lock();
	memcg = mem_cgroup_from_id(id);
	if (!memcg || !css_tryget_online(&memcg->css))
		memcg = get_mem_cgroup_from_mm(mm);
	rcu_read_unlock();

	ret = charge_memcg(folio, memcg, gfp);

	css_put(&memcg->css);
	return ret;
}

 
void mem_cgroup_swapin_uncharge_swap(swp_entry_t entry)
{
	 
	if (!mem_cgroup_disabled() && do_memsw_account()) {
		 
		mem_cgroup_uncharge_swap(entry, 1);
	}
}

struct uncharge_gather {
	struct mem_cgroup *memcg;
	unsigned long nr_memory;
	unsigned long pgpgout;
	unsigned long nr_kmem;
	int nid;
};

static inline void uncharge_gather_clear(struct uncharge_gather *ug)
{
	memset(ug, 0, sizeof(*ug));
}

static void uncharge_batch(const struct uncharge_gather *ug)
{
	unsigned long flags;

	if (ug->nr_memory) {
		page_counter_uncharge(&ug->memcg->memory, ug->nr_memory);
		if (do_memsw_account())
			page_counter_uncharge(&ug->memcg->memsw, ug->nr_memory);
		if (ug->nr_kmem)
			memcg_account_kmem(ug->memcg, -ug->nr_kmem);
		memcg_oom_recover(ug->memcg);
	}

	local_irq_save(flags);
	__count_memcg_events(ug->memcg, PGPGOUT, ug->pgpgout);
	__this_cpu_add(ug->memcg->vmstats_percpu->nr_page_events, ug->nr_memory);
	memcg_check_events(ug->memcg, ug->nid);
	local_irq_restore(flags);

	 
	css_put(&ug->memcg->css);
}

static void uncharge_folio(struct folio *folio, struct uncharge_gather *ug)
{
	long nr_pages;
	struct mem_cgroup *memcg;
	struct obj_cgroup *objcg;

	VM_BUG_ON_FOLIO(folio_test_lru(folio), folio);

	 
	if (folio_memcg_kmem(folio)) {
		objcg = __folio_objcg(folio);
		 
		memcg = get_mem_cgroup_from_objcg(objcg);
	} else {
		memcg = __folio_memcg(folio);
	}

	if (!memcg)
		return;

	if (ug->memcg != memcg) {
		if (ug->memcg) {
			uncharge_batch(ug);
			uncharge_gather_clear(ug);
		}
		ug->memcg = memcg;
		ug->nid = folio_nid(folio);

		 
		css_get(&memcg->css);
	}

	nr_pages = folio_nr_pages(folio);

	if (folio_memcg_kmem(folio)) {
		ug->nr_memory += nr_pages;
		ug->nr_kmem += nr_pages;

		folio->memcg_data = 0;
		obj_cgroup_put(objcg);
	} else {
		 
		if (!mem_cgroup_is_root(memcg))
			ug->nr_memory += nr_pages;
		ug->pgpgout++;

		folio->memcg_data = 0;
	}

	css_put(&memcg->css);
}

void __mem_cgroup_uncharge(struct folio *folio)
{
	struct uncharge_gather ug;

	 
	if (!folio_memcg(folio))
		return;

	uncharge_gather_clear(&ug);
	uncharge_folio(folio, &ug);
	uncharge_batch(&ug);
}

 
void __mem_cgroup_uncharge_list(struct list_head *page_list)
{
	struct uncharge_gather ug;
	struct folio *folio;

	uncharge_gather_clear(&ug);
	list_for_each_entry(folio, page_list, lru)
		uncharge_folio(folio, &ug);
	if (ug.memcg)
		uncharge_batch(&ug);
}

 
void mem_cgroup_migrate(struct folio *old, struct folio *new)
{
	struct mem_cgroup *memcg;
	long nr_pages = folio_nr_pages(new);
	unsigned long flags;

	VM_BUG_ON_FOLIO(!folio_test_locked(old), old);
	VM_BUG_ON_FOLIO(!folio_test_locked(new), new);
	VM_BUG_ON_FOLIO(folio_test_anon(old) != folio_test_anon(new), new);
	VM_BUG_ON_FOLIO(folio_nr_pages(old) != nr_pages, new);

	if (mem_cgroup_disabled())
		return;

	 
	if (folio_memcg(new))
		return;

	memcg = folio_memcg(old);
	VM_WARN_ON_ONCE_FOLIO(!memcg, old);
	if (!memcg)
		return;

	 
	if (!mem_cgroup_is_root(memcg)) {
		page_counter_charge(&memcg->memory, nr_pages);
		if (do_memsw_account())
			page_counter_charge(&memcg->memsw, nr_pages);
	}

	css_get(&memcg->css);
	commit_charge(new, memcg);

	local_irq_save(flags);
	mem_cgroup_charge_statistics(memcg, nr_pages);
	memcg_check_events(memcg, folio_nid(new));
	local_irq_restore(flags);
}

DEFINE_STATIC_KEY_FALSE(memcg_sockets_enabled_key);
EXPORT_SYMBOL(memcg_sockets_enabled_key);

void mem_cgroup_sk_alloc(struct sock *sk)
{
	struct mem_cgroup *memcg;

	if (!mem_cgroup_sockets_enabled)
		return;

	 
	if (!in_task())
		return;

	rcu_read_lock();
	memcg = mem_cgroup_from_task(current);
	if (mem_cgroup_is_root(memcg))
		goto out;
	if (!cgroup_subsys_on_dfl(memory_cgrp_subsys) && !memcg->tcpmem_active)
		goto out;
	if (css_tryget(&memcg->css))
		sk->sk_memcg = memcg;
out:
	rcu_read_unlock();
}

void mem_cgroup_sk_free(struct sock *sk)
{
	if (sk->sk_memcg)
		css_put(&sk->sk_memcg->css);
}

 
bool mem_cgroup_charge_skmem(struct mem_cgroup *memcg, unsigned int nr_pages,
			     gfp_t gfp_mask)
{
	if (!cgroup_subsys_on_dfl(memory_cgrp_subsys)) {
		struct page_counter *fail;

		if (page_counter_try_charge(&memcg->tcpmem, nr_pages, &fail)) {
			memcg->tcpmem_pressure = 0;
			return true;
		}
		memcg->tcpmem_pressure = 1;
		if (gfp_mask & __GFP_NOFAIL) {
			page_counter_charge(&memcg->tcpmem, nr_pages);
			return true;
		}
		return false;
	}

	if (try_charge(memcg, gfp_mask, nr_pages) == 0) {
		mod_memcg_state(memcg, MEMCG_SOCK, nr_pages);
		return true;
	}

	return false;
}

 
void mem_cgroup_uncharge_skmem(struct mem_cgroup *memcg, unsigned int nr_pages)
{
	if (!cgroup_subsys_on_dfl(memory_cgrp_subsys)) {
		page_counter_uncharge(&memcg->tcpmem, nr_pages);
		return;
	}

	mod_memcg_state(memcg, MEMCG_SOCK, -nr_pages);

	refill_stock(memcg, nr_pages);
}

static int __init cgroup_memory(char *s)
{
	char *token;

	while ((token = strsep(&s, ",")) != NULL) {
		if (!*token)
			continue;
		if (!strcmp(token, "nosocket"))
			cgroup_memory_nosocket = true;
		if (!strcmp(token, "nokmem"))
			cgroup_memory_nokmem = true;
		if (!strcmp(token, "nobpf"))
			cgroup_memory_nobpf = true;
	}
	return 1;
}
__setup("cgroup.memory=", cgroup_memory);

 
static int __init mem_cgroup_init(void)
{
	int cpu, node;

	 
	BUILD_BUG_ON(MEMCG_CHARGE_BATCH > S32_MAX / PAGE_SIZE);

	cpuhp_setup_state_nocalls(CPUHP_MM_MEMCQ_DEAD, "mm/memctrl:dead", NULL,
				  memcg_hotplug_cpu_dead);

	for_each_possible_cpu(cpu)
		INIT_WORK(&per_cpu_ptr(&memcg_stock, cpu)->work,
			  drain_local_stock);

	for_each_node(node) {
		struct mem_cgroup_tree_per_node *rtpn;

		rtpn = kzalloc_node(sizeof(*rtpn), GFP_KERNEL, node);

		rtpn->rb_root = RB_ROOT;
		rtpn->rb_rightmost = NULL;
		spin_lock_init(&rtpn->lock);
		soft_limit_tree.rb_tree_per_node[node] = rtpn;
	}

	return 0;
}
subsys_initcall(mem_cgroup_init);

#ifdef CONFIG_SWAP
static struct mem_cgroup *mem_cgroup_id_get_online(struct mem_cgroup *memcg)
{
	while (!refcount_inc_not_zero(&memcg->id.ref)) {
		 
		if (WARN_ON_ONCE(mem_cgroup_is_root(memcg))) {
			VM_BUG_ON(1);
			break;
		}
		memcg = parent_mem_cgroup(memcg);
		if (!memcg)
			memcg = root_mem_cgroup;
	}
	return memcg;
}

 
void mem_cgroup_swapout(struct folio *folio, swp_entry_t entry)
{
	struct mem_cgroup *memcg, *swap_memcg;
	unsigned int nr_entries;
	unsigned short oldid;

	VM_BUG_ON_FOLIO(folio_test_lru(folio), folio);
	VM_BUG_ON_FOLIO(folio_ref_count(folio), folio);

	if (mem_cgroup_disabled())
		return;

	if (!do_memsw_account())
		return;

	memcg = folio_memcg(folio);

	VM_WARN_ON_ONCE_FOLIO(!memcg, folio);
	if (!memcg)
		return;

	 
	swap_memcg = mem_cgroup_id_get_online(memcg);
	nr_entries = folio_nr_pages(folio);
	 
	if (nr_entries > 1)
		mem_cgroup_id_get_many(swap_memcg, nr_entries - 1);
	oldid = swap_cgroup_record(entry, mem_cgroup_id(swap_memcg),
				   nr_entries);
	VM_BUG_ON_FOLIO(oldid, folio);
	mod_memcg_state(swap_memcg, MEMCG_SWAP, nr_entries);

	folio->memcg_data = 0;

	if (!mem_cgroup_is_root(memcg))
		page_counter_uncharge(&memcg->memory, nr_entries);

	if (memcg != swap_memcg) {
		if (!mem_cgroup_is_root(swap_memcg))
			page_counter_charge(&swap_memcg->memsw, nr_entries);
		page_counter_uncharge(&memcg->memsw, nr_entries);
	}

	 
	memcg_stats_lock();
	mem_cgroup_charge_statistics(memcg, -nr_entries);
	memcg_stats_unlock();
	memcg_check_events(memcg, folio_nid(folio));

	css_put(&memcg->css);
}

 
int __mem_cgroup_try_charge_swap(struct folio *folio, swp_entry_t entry)
{
	unsigned int nr_pages = folio_nr_pages(folio);
	struct page_counter *counter;
	struct mem_cgroup *memcg;
	unsigned short oldid;

	if (do_memsw_account())
		return 0;

	memcg = folio_memcg(folio);

	VM_WARN_ON_ONCE_FOLIO(!memcg, folio);
	if (!memcg)
		return 0;

	if (!entry.val) {
		memcg_memory_event(memcg, MEMCG_SWAP_FAIL);
		return 0;
	}

	memcg = mem_cgroup_id_get_online(memcg);

	if (!mem_cgroup_is_root(memcg) &&
	    !page_counter_try_charge(&memcg->swap, nr_pages, &counter)) {
		memcg_memory_event(memcg, MEMCG_SWAP_MAX);
		memcg_memory_event(memcg, MEMCG_SWAP_FAIL);
		mem_cgroup_id_put(memcg);
		return -ENOMEM;
	}

	 
	if (nr_pages > 1)
		mem_cgroup_id_get_many(memcg, nr_pages - 1);
	oldid = swap_cgroup_record(entry, mem_cgroup_id(memcg), nr_pages);
	VM_BUG_ON_FOLIO(oldid, folio);
	mod_memcg_state(memcg, MEMCG_SWAP, nr_pages);

	return 0;
}

 
void __mem_cgroup_uncharge_swap(swp_entry_t entry, unsigned int nr_pages)
{
	struct mem_cgroup *memcg;
	unsigned short id;

	id = swap_cgroup_record(entry, 0, nr_pages);
	rcu_read_lock();
	memcg = mem_cgroup_from_id(id);
	if (memcg) {
		if (!mem_cgroup_is_root(memcg)) {
			if (do_memsw_account())
				page_counter_uncharge(&memcg->memsw, nr_pages);
			else
				page_counter_uncharge(&memcg->swap, nr_pages);
		}
		mod_memcg_state(memcg, MEMCG_SWAP, -nr_pages);
		mem_cgroup_id_put_many(memcg, nr_pages);
	}
	rcu_read_unlock();
}

long mem_cgroup_get_nr_swap_pages(struct mem_cgroup *memcg)
{
	long nr_swap_pages = get_nr_swap_pages();

	if (mem_cgroup_disabled() || do_memsw_account())
		return nr_swap_pages;
	for (; !mem_cgroup_is_root(memcg); memcg = parent_mem_cgroup(memcg))
		nr_swap_pages = min_t(long, nr_swap_pages,
				      READ_ONCE(memcg->swap.max) -
				      page_counter_read(&memcg->swap));
	return nr_swap_pages;
}

bool mem_cgroup_swap_full(struct folio *folio)
{
	struct mem_cgroup *memcg;

	VM_BUG_ON_FOLIO(!folio_test_locked(folio), folio);

	if (vm_swap_full())
		return true;
	if (do_memsw_account())
		return false;

	memcg = folio_memcg(folio);
	if (!memcg)
		return false;

	for (; !mem_cgroup_is_root(memcg); memcg = parent_mem_cgroup(memcg)) {
		unsigned long usage = page_counter_read(&memcg->swap);

		if (usage * 2 >= READ_ONCE(memcg->swap.high) ||
		    usage * 2 >= READ_ONCE(memcg->swap.max))
			return true;
	}

	return false;
}

static int __init setup_swap_account(char *s)
{
	pr_warn_once("The swapaccount= commandline option is deprecated. "
		     "Please report your usecase to linux-mm@kvack.org if you "
		     "depend on this functionality.\n");
	return 1;
}
__setup("swapaccount=", setup_swap_account);

static u64 swap_current_read(struct cgroup_subsys_state *css,
			     struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	return (u64)page_counter_read(&memcg->swap) * PAGE_SIZE;
}

static u64 swap_peak_read(struct cgroup_subsys_state *css,
			  struct cftype *cft)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(css);

	return (u64)memcg->swap.watermark * PAGE_SIZE;
}

static int swap_high_show(struct seq_file *m, void *v)
{
	return seq_puts_memcg_tunable(m,
		READ_ONCE(mem_cgroup_from_seq(m)->swap.high));
}

static ssize_t swap_high_write(struct kernfs_open_file *of,
			       char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned long high;
	int err;

	buf = strstrip(buf);
	err = page_counter_memparse(buf, "max", &high);
	if (err)
		return err;

	page_counter_set_high(&memcg->swap, high);

	return nbytes;
}

static int swap_max_show(struct seq_file *m, void *v)
{
	return seq_puts_memcg_tunable(m,
		READ_ONCE(mem_cgroup_from_seq(m)->swap.max));
}

static ssize_t swap_max_write(struct kernfs_open_file *of,
			      char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned long max;
	int err;

	buf = strstrip(buf);
	err = page_counter_memparse(buf, "max", &max);
	if (err)
		return err;

	xchg(&memcg->swap.max, max);

	return nbytes;
}

static int swap_events_show(struct seq_file *m, void *v)
{
	struct mem_cgroup *memcg = mem_cgroup_from_seq(m);

	seq_printf(m, "high %lu\n",
		   atomic_long_read(&memcg->memory_events[MEMCG_SWAP_HIGH]));
	seq_printf(m, "max %lu\n",
		   atomic_long_read(&memcg->memory_events[MEMCG_SWAP_MAX]));
	seq_printf(m, "fail %lu\n",
		   atomic_long_read(&memcg->memory_events[MEMCG_SWAP_FAIL]));

	return 0;
}

static struct cftype swap_files[] = {
	{
		.name = "swap.current",
		.flags = CFTYPE_NOT_ON_ROOT,
		.read_u64 = swap_current_read,
	},
	{
		.name = "swap.high",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = swap_high_show,
		.write = swap_high_write,
	},
	{
		.name = "swap.max",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = swap_max_show,
		.write = swap_max_write,
	},
	{
		.name = "swap.peak",
		.flags = CFTYPE_NOT_ON_ROOT,
		.read_u64 = swap_peak_read,
	},
	{
		.name = "swap.events",
		.flags = CFTYPE_NOT_ON_ROOT,
		.file_offset = offsetof(struct mem_cgroup, swap_events_file),
		.seq_show = swap_events_show,
	},
	{ }	 
};

static struct cftype memsw_files[] = {
	{
		.name = "memsw.usage_in_bytes",
		.private = MEMFILE_PRIVATE(_MEMSWAP, RES_USAGE),
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "memsw.max_usage_in_bytes",
		.private = MEMFILE_PRIVATE(_MEMSWAP, RES_MAX_USAGE),
		.write = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "memsw.limit_in_bytes",
		.private = MEMFILE_PRIVATE(_MEMSWAP, RES_LIMIT),
		.write = mem_cgroup_write,
		.read_u64 = mem_cgroup_read_u64,
	},
	{
		.name = "memsw.failcnt",
		.private = MEMFILE_PRIVATE(_MEMSWAP, RES_FAILCNT),
		.write = mem_cgroup_reset,
		.read_u64 = mem_cgroup_read_u64,
	},
	{ },	 
};

#if defined(CONFIG_MEMCG_KMEM) && defined(CONFIG_ZSWAP)
 
bool obj_cgroup_may_zswap(struct obj_cgroup *objcg)
{
	struct mem_cgroup *memcg, *original_memcg;
	bool ret = true;

	if (!cgroup_subsys_on_dfl(memory_cgrp_subsys))
		return true;

	original_memcg = get_mem_cgroup_from_objcg(objcg);
	for (memcg = original_memcg; !mem_cgroup_is_root(memcg);
	     memcg = parent_mem_cgroup(memcg)) {
		unsigned long max = READ_ONCE(memcg->zswap_max);
		unsigned long pages;

		if (max == PAGE_COUNTER_MAX)
			continue;
		if (max == 0) {
			ret = false;
			break;
		}

		cgroup_rstat_flush(memcg->css.cgroup);
		pages = memcg_page_state(memcg, MEMCG_ZSWAP_B) / PAGE_SIZE;
		if (pages < max)
			continue;
		ret = false;
		break;
	}
	mem_cgroup_put(original_memcg);
	return ret;
}

 
void obj_cgroup_charge_zswap(struct obj_cgroup *objcg, size_t size)
{
	struct mem_cgroup *memcg;

	if (!cgroup_subsys_on_dfl(memory_cgrp_subsys))
		return;

	VM_WARN_ON_ONCE(!(current->flags & PF_MEMALLOC));

	 
	if (obj_cgroup_charge(objcg, GFP_KERNEL, size))
		VM_WARN_ON_ONCE(1);

	rcu_read_lock();
	memcg = obj_cgroup_memcg(objcg);
	mod_memcg_state(memcg, MEMCG_ZSWAP_B, size);
	mod_memcg_state(memcg, MEMCG_ZSWAPPED, 1);
	rcu_read_unlock();
}

 
void obj_cgroup_uncharge_zswap(struct obj_cgroup *objcg, size_t size)
{
	struct mem_cgroup *memcg;

	if (!cgroup_subsys_on_dfl(memory_cgrp_subsys))
		return;

	obj_cgroup_uncharge(objcg, size);

	rcu_read_lock();
	memcg = obj_cgroup_memcg(objcg);
	mod_memcg_state(memcg, MEMCG_ZSWAP_B, -size);
	mod_memcg_state(memcg, MEMCG_ZSWAPPED, -1);
	rcu_read_unlock();
}

static u64 zswap_current_read(struct cgroup_subsys_state *css,
			      struct cftype *cft)
{
	cgroup_rstat_flush(css->cgroup);
	return memcg_page_state(mem_cgroup_from_css(css), MEMCG_ZSWAP_B);
}

static int zswap_max_show(struct seq_file *m, void *v)
{
	return seq_puts_memcg_tunable(m,
		READ_ONCE(mem_cgroup_from_seq(m)->zswap_max));
}

static ssize_t zswap_max_write(struct kernfs_open_file *of,
			       char *buf, size_t nbytes, loff_t off)
{
	struct mem_cgroup *memcg = mem_cgroup_from_css(of_css(of));
	unsigned long max;
	int err;

	buf = strstrip(buf);
	err = page_counter_memparse(buf, "max", &max);
	if (err)
		return err;

	xchg(&memcg->zswap_max, max);

	return nbytes;
}

static struct cftype zswap_files[] = {
	{
		.name = "zswap.current",
		.flags = CFTYPE_NOT_ON_ROOT,
		.read_u64 = zswap_current_read,
	},
	{
		.name = "zswap.max",
		.flags = CFTYPE_NOT_ON_ROOT,
		.seq_show = zswap_max_show,
		.write = zswap_max_write,
	},
	{ }	 
};
#endif  

static int __init mem_cgroup_swap_init(void)
{
	if (mem_cgroup_disabled())
		return 0;

	WARN_ON(cgroup_add_dfl_cftypes(&memory_cgrp_subsys, swap_files));
	WARN_ON(cgroup_add_legacy_cftypes(&memory_cgrp_subsys, memsw_files));
#if defined(CONFIG_MEMCG_KMEM) && defined(CONFIG_ZSWAP)
	WARN_ON(cgroup_add_dfl_cftypes(&memory_cgrp_subsys, zswap_files));
#endif
	return 0;
}
subsys_initcall(mem_cgroup_swap_init);

#endif  
