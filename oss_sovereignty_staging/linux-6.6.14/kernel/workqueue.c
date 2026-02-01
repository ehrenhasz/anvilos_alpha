
 

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/signal.h>
#include <linux/completion.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/kthread.h>
#include <linux/hardirq.h>
#include <linux/mempolicy.h>
#include <linux/freezer.h>
#include <linux/debug_locks.h>
#include <linux/lockdep.h>
#include <linux/idr.h>
#include <linux/jhash.h>
#include <linux/hashtable.h>
#include <linux/rculist.h>
#include <linux/nodemask.h>
#include <linux/moduleparam.h>
#include <linux/uaccess.h>
#include <linux/sched/isolation.h>
#include <linux/sched/debug.h>
#include <linux/nmi.h>
#include <linux/kvm_para.h>
#include <linux/delay.h>

#include "workqueue_internal.h"

enum {
	 
	POOL_MANAGER_ACTIVE	= 1 << 0,	 
	POOL_DISASSOCIATED	= 1 << 2,	 

	 
	WORKER_DIE		= 1 << 1,	 
	WORKER_IDLE		= 1 << 2,	 
	WORKER_PREP		= 1 << 3,	 
	WORKER_CPU_INTENSIVE	= 1 << 6,	 
	WORKER_UNBOUND		= 1 << 7,	 
	WORKER_REBOUND		= 1 << 8,	 

	WORKER_NOT_RUNNING	= WORKER_PREP | WORKER_CPU_INTENSIVE |
				  WORKER_UNBOUND | WORKER_REBOUND,

	NR_STD_WORKER_POOLS	= 2,		 

	UNBOUND_POOL_HASH_ORDER	= 6,		 
	BUSY_WORKER_HASH_ORDER	= 6,		 

	MAX_IDLE_WORKERS_RATIO	= 4,		 
	IDLE_WORKER_TIMEOUT	= 300 * HZ,	 

	MAYDAY_INITIAL_TIMEOUT  = HZ / 100 >= 2 ? HZ / 100 : 2,
						 
	MAYDAY_INTERVAL		= HZ / 10,	 
	CREATE_COOLDOWN		= HZ,		 

	 
	RESCUER_NICE_LEVEL	= MIN_NICE,
	HIGHPRI_NICE_LEVEL	= MIN_NICE,

	WQ_NAME_LEN		= 24,
};

 

 

struct worker_pool {
	raw_spinlock_t		lock;		 
	int			cpu;		 
	int			node;		 
	int			id;		 
	unsigned int		flags;		 

	unsigned long		watchdog_ts;	 
	bool			cpu_stall;	 

	 
	int			nr_running;

	struct list_head	worklist;	 

	int			nr_workers;	 
	int			nr_idle;	 

	struct list_head	idle_list;	 
	struct timer_list	idle_timer;	 
	struct work_struct      idle_cull_work;  

	struct timer_list	mayday_timer;	   

	 
	DECLARE_HASHTABLE(busy_hash, BUSY_WORKER_HASH_ORDER);
						 

	struct worker		*manager;	 
	struct list_head	workers;	 
	struct list_head        dying_workers;   
	struct completion	*detach_completion;  

	struct ida		worker_ida;	 

	struct workqueue_attrs	*attrs;		 
	struct hlist_node	hash_node;	 
	int			refcnt;		 

	 
	struct rcu_head		rcu;
};

 
enum pool_workqueue_stats {
	PWQ_STAT_STARTED,	 
	PWQ_STAT_COMPLETED,	 
	PWQ_STAT_CPU_TIME,	 
	PWQ_STAT_CPU_INTENSIVE,	 
	PWQ_STAT_CM_WAKEUP,	 
	PWQ_STAT_REPATRIATED,	 
	PWQ_STAT_MAYDAY,	 
	PWQ_STAT_RESCUED,	 

	PWQ_NR_STATS,
};

 
struct pool_workqueue {
	struct worker_pool	*pool;		 
	struct workqueue_struct *wq;		 
	int			work_color;	 
	int			flush_color;	 
	int			refcnt;		 
	int			nr_in_flight[WORK_NR_COLORS];
						 

	 
	int			nr_active;	 
	int			max_active;	 
	struct list_head	inactive_works;	 
	struct list_head	pwqs_node;	 
	struct list_head	mayday_node;	 

	u64			stats[PWQ_NR_STATS];

	 
	struct kthread_work	release_work;
	struct rcu_head		rcu;
} __aligned(1 << WORK_STRUCT_FLAG_BITS);

 
struct wq_flusher {
	struct list_head	list;		 
	int			flush_color;	 
	struct completion	done;		 
};

struct wq_device;

 
struct workqueue_struct {
	struct list_head	pwqs;		 
	struct list_head	list;		 

	struct mutex		mutex;		 
	int			work_color;	 
	int			flush_color;	 
	atomic_t		nr_pwqs_to_flush;  
	struct wq_flusher	*first_flusher;	 
	struct list_head	flusher_queue;	 
	struct list_head	flusher_overflow;  

	struct list_head	maydays;	 
	struct worker		*rescuer;	 

	int			nr_drainers;	 
	int			saved_max_active;  

	struct workqueue_attrs	*unbound_attrs;	 
	struct pool_workqueue	*dfl_pwq;	 

#ifdef CONFIG_SYSFS
	struct wq_device	*wq_dev;	 
#endif
#ifdef CONFIG_LOCKDEP
	char			*lock_name;
	struct lock_class_key	key;
	struct lockdep_map	lockdep_map;
#endif
	char			name[WQ_NAME_LEN];  

	 
	struct rcu_head		rcu;

	 
	unsigned int		flags ____cacheline_aligned;  
	struct pool_workqueue __percpu __rcu **cpu_pwq;  
};

static struct kmem_cache *pwq_cache;

 
struct wq_pod_type {
	int			nr_pods;	 
	cpumask_var_t		*pod_cpus;	 
	int			*pod_node;	 
	int			*cpu_pod;	 
};

static struct wq_pod_type wq_pod_types[WQ_AFFN_NR_TYPES];
static enum wq_affn_scope wq_affn_dfl = WQ_AFFN_CACHE;

static const char *wq_affn_names[WQ_AFFN_NR_TYPES] = {
	[WQ_AFFN_DFL]			= "default",
	[WQ_AFFN_CPU]			= "cpu",
	[WQ_AFFN_SMT]			= "smt",
	[WQ_AFFN_CACHE]			= "cache",
	[WQ_AFFN_NUMA]			= "numa",
	[WQ_AFFN_SYSTEM]		= "system",
};

 
static unsigned long wq_cpu_intensive_thresh_us = ULONG_MAX;
module_param_named(cpu_intensive_thresh_us, wq_cpu_intensive_thresh_us, ulong, 0644);

 
static bool wq_power_efficient = IS_ENABLED(CONFIG_WQ_POWER_EFFICIENT_DEFAULT);
module_param_named(power_efficient, wq_power_efficient, bool, 0444);

static bool wq_online;			 

 
static struct workqueue_attrs *wq_update_pod_attrs_buf;

static DEFINE_MUTEX(wq_pool_mutex);	 
static DEFINE_MUTEX(wq_pool_attach_mutex);  
static DEFINE_RAW_SPINLOCK(wq_mayday_lock);	 
 
static struct rcuwait manager_wait = __RCUWAIT_INITIALIZER(manager_wait);

static LIST_HEAD(workqueues);		 
static bool workqueue_freezing;		 

 
static cpumask_var_t wq_unbound_cpumask;

 
static struct cpumask wq_cmdline_cpumask __initdata;

 
static DEFINE_PER_CPU(int, wq_rr_cpu_last);

 
#ifdef CONFIG_DEBUG_WQ_FORCE_RR_CPU
static bool wq_debug_force_rr_cpu = true;
#else
static bool wq_debug_force_rr_cpu = false;
#endif
module_param_named(debug_force_rr_cpu, wq_debug_force_rr_cpu, bool, 0644);

 
static DEFINE_PER_CPU_SHARED_ALIGNED(struct worker_pool [NR_STD_WORKER_POOLS], cpu_worker_pools);

static DEFINE_IDR(worker_pool_idr);	 

 
static DEFINE_HASHTABLE(unbound_pool_hash, UNBOUND_POOL_HASH_ORDER);

 
static struct workqueue_attrs *unbound_std_wq_attrs[NR_STD_WORKER_POOLS];

 
static struct workqueue_attrs *ordered_wq_attrs[NR_STD_WORKER_POOLS];

 
static struct kthread_worker *pwq_release_worker;

struct workqueue_struct *system_wq __read_mostly;
EXPORT_SYMBOL(system_wq);
struct workqueue_struct *system_highpri_wq __read_mostly;
EXPORT_SYMBOL_GPL(system_highpri_wq);
struct workqueue_struct *system_long_wq __read_mostly;
EXPORT_SYMBOL_GPL(system_long_wq);
struct workqueue_struct *system_unbound_wq __read_mostly;
EXPORT_SYMBOL_GPL(system_unbound_wq);
struct workqueue_struct *system_freezable_wq __read_mostly;
EXPORT_SYMBOL_GPL(system_freezable_wq);
struct workqueue_struct *system_power_efficient_wq __read_mostly;
EXPORT_SYMBOL_GPL(system_power_efficient_wq);
struct workqueue_struct *system_freezable_power_efficient_wq __read_mostly;
EXPORT_SYMBOL_GPL(system_freezable_power_efficient_wq);

static int worker_thread(void *__worker);
static void workqueue_sysfs_unregister(struct workqueue_struct *wq);
static void show_pwq(struct pool_workqueue *pwq);
static void show_one_worker_pool(struct worker_pool *pool);

#define CREATE_TRACE_POINTS
#include <trace/events/workqueue.h>

#define assert_rcu_or_pool_mutex()					\
	RCU_LOCKDEP_WARN(!rcu_read_lock_held() &&			\
			 !lockdep_is_held(&wq_pool_mutex),		\
			 "RCU or wq_pool_mutex should be held")

#define assert_rcu_or_wq_mutex_or_pool_mutex(wq)			\
	RCU_LOCKDEP_WARN(!rcu_read_lock_held() &&			\
			 !lockdep_is_held(&wq->mutex) &&		\
			 !lockdep_is_held(&wq_pool_mutex),		\
			 "RCU, wq->mutex or wq_pool_mutex should be held")

#define for_each_cpu_worker_pool(pool, cpu)				\
	for ((pool) = &per_cpu(cpu_worker_pools, cpu)[0];		\
	     (pool) < &per_cpu(cpu_worker_pools, cpu)[NR_STD_WORKER_POOLS]; \
	     (pool)++)

 
#define for_each_pool(pool, pi)						\
	idr_for_each_entry(&worker_pool_idr, pool, pi)			\
		if (({ assert_rcu_or_pool_mutex(); false; })) { }	\
		else

 
#define for_each_pool_worker(worker, pool)				\
	list_for_each_entry((worker), &(pool)->workers, node)		\
		if (({ lockdep_assert_held(&wq_pool_attach_mutex); false; })) { } \
		else

 
#define for_each_pwq(pwq, wq)						\
	list_for_each_entry_rcu((pwq), &(wq)->pwqs, pwqs_node,		\
				 lockdep_is_held(&(wq->mutex)))

#ifdef CONFIG_DEBUG_OBJECTS_WORK

static const struct debug_obj_descr work_debug_descr;

static void *work_debug_hint(void *addr)
{
	return ((struct work_struct *) addr)->func;
}

static bool work_is_static_object(void *addr)
{
	struct work_struct *work = addr;

	return test_bit(WORK_STRUCT_STATIC_BIT, work_data_bits(work));
}

 
static bool work_fixup_init(void *addr, enum debug_obj_state state)
{
	struct work_struct *work = addr;

	switch (state) {
	case ODEBUG_STATE_ACTIVE:
		cancel_work_sync(work);
		debug_object_init(work, &work_debug_descr);
		return true;
	default:
		return false;
	}
}

 
static bool work_fixup_free(void *addr, enum debug_obj_state state)
{
	struct work_struct *work = addr;

	switch (state) {
	case ODEBUG_STATE_ACTIVE:
		cancel_work_sync(work);
		debug_object_free(work, &work_debug_descr);
		return true;
	default:
		return false;
	}
}

static const struct debug_obj_descr work_debug_descr = {
	.name		= "work_struct",
	.debug_hint	= work_debug_hint,
	.is_static_object = work_is_static_object,
	.fixup_init	= work_fixup_init,
	.fixup_free	= work_fixup_free,
};

static inline void debug_work_activate(struct work_struct *work)
{
	debug_object_activate(work, &work_debug_descr);
}

static inline void debug_work_deactivate(struct work_struct *work)
{
	debug_object_deactivate(work, &work_debug_descr);
}

void __init_work(struct work_struct *work, int onstack)
{
	if (onstack)
		debug_object_init_on_stack(work, &work_debug_descr);
	else
		debug_object_init(work, &work_debug_descr);
}
EXPORT_SYMBOL_GPL(__init_work);

void destroy_work_on_stack(struct work_struct *work)
{
	debug_object_free(work, &work_debug_descr);
}
EXPORT_SYMBOL_GPL(destroy_work_on_stack);

void destroy_delayed_work_on_stack(struct delayed_work *work)
{
	destroy_timer_on_stack(&work->timer);
	debug_object_free(&work->work, &work_debug_descr);
}
EXPORT_SYMBOL_GPL(destroy_delayed_work_on_stack);

#else
static inline void debug_work_activate(struct work_struct *work) { }
static inline void debug_work_deactivate(struct work_struct *work) { }
#endif

 
static int worker_pool_assign_id(struct worker_pool *pool)
{
	int ret;

	lockdep_assert_held(&wq_pool_mutex);

	ret = idr_alloc(&worker_pool_idr, pool, 0, WORK_OFFQ_POOL_NONE,
			GFP_KERNEL);
	if (ret >= 0) {
		pool->id = ret;
		return 0;
	}
	return ret;
}

static unsigned int work_color_to_flags(int color)
{
	return color << WORK_STRUCT_COLOR_SHIFT;
}

static int get_work_color(unsigned long work_data)
{
	return (work_data >> WORK_STRUCT_COLOR_SHIFT) &
		((1 << WORK_STRUCT_COLOR_BITS) - 1);
}

static int work_next_color(int color)
{
	return (color + 1) % WORK_NR_COLORS;
}

 
static inline void set_work_data(struct work_struct *work, unsigned long data,
				 unsigned long flags)
{
	WARN_ON_ONCE(!work_pending(work));
	atomic_long_set(&work->data, data | flags | work_static(work));
}

static void set_work_pwq(struct work_struct *work, struct pool_workqueue *pwq,
			 unsigned long extra_flags)
{
	set_work_data(work, (unsigned long)pwq,
		      WORK_STRUCT_PENDING | WORK_STRUCT_PWQ | extra_flags);
}

static void set_work_pool_and_keep_pending(struct work_struct *work,
					   int pool_id)
{
	set_work_data(work, (unsigned long)pool_id << WORK_OFFQ_POOL_SHIFT,
		      WORK_STRUCT_PENDING);
}

static void set_work_pool_and_clear_pending(struct work_struct *work,
					    int pool_id)
{
	 
	smp_wmb();
	set_work_data(work, (unsigned long)pool_id << WORK_OFFQ_POOL_SHIFT, 0);
	 
	smp_mb();
}

static void clear_work_data(struct work_struct *work)
{
	smp_wmb();	 
	set_work_data(work, WORK_STRUCT_NO_POOL, 0);
}

static inline struct pool_workqueue *work_struct_pwq(unsigned long data)
{
	return (struct pool_workqueue *)(data & WORK_STRUCT_WQ_DATA_MASK);
}

static struct pool_workqueue *get_work_pwq(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);

	if (data & WORK_STRUCT_PWQ)
		return work_struct_pwq(data);
	else
		return NULL;
}

 
static struct worker_pool *get_work_pool(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);
	int pool_id;

	assert_rcu_or_pool_mutex();

	if (data & WORK_STRUCT_PWQ)
		return work_struct_pwq(data)->pool;

	pool_id = data >> WORK_OFFQ_POOL_SHIFT;
	if (pool_id == WORK_OFFQ_POOL_NONE)
		return NULL;

	return idr_find(&worker_pool_idr, pool_id);
}

 
static int get_work_pool_id(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);

	if (data & WORK_STRUCT_PWQ)
		return work_struct_pwq(data)->pool->id;

	return data >> WORK_OFFQ_POOL_SHIFT;
}

static void mark_work_canceling(struct work_struct *work)
{
	unsigned long pool_id = get_work_pool_id(work);

	pool_id <<= WORK_OFFQ_POOL_SHIFT;
	set_work_data(work, pool_id | WORK_OFFQ_CANCELING, WORK_STRUCT_PENDING);
}

static bool work_is_canceling(struct work_struct *work)
{
	unsigned long data = atomic_long_read(&work->data);

	return !(data & WORK_STRUCT_PWQ) && (data & WORK_OFFQ_CANCELING);
}

 

 
static bool need_more_worker(struct worker_pool *pool)
{
	return !list_empty(&pool->worklist) && !pool->nr_running;
}

 
static bool may_start_working(struct worker_pool *pool)
{
	return pool->nr_idle;
}

 
static bool keep_working(struct worker_pool *pool)
{
	return !list_empty(&pool->worklist) && (pool->nr_running <= 1);
}

 
static bool need_to_create_worker(struct worker_pool *pool)
{
	return need_more_worker(pool) && !may_start_working(pool);
}

 
static bool too_many_workers(struct worker_pool *pool)
{
	bool managing = pool->flags & POOL_MANAGER_ACTIVE;
	int nr_idle = pool->nr_idle + managing;  
	int nr_busy = pool->nr_workers - nr_idle;

	return nr_idle > 2 && (nr_idle - 2) * MAX_IDLE_WORKERS_RATIO >= nr_busy;
}

 
static inline void worker_set_flags(struct worker *worker, unsigned int flags)
{
	struct worker_pool *pool = worker->pool;

	lockdep_assert_held(&pool->lock);

	 
	if ((flags & WORKER_NOT_RUNNING) &&
	    !(worker->flags & WORKER_NOT_RUNNING)) {
		pool->nr_running--;
	}

	worker->flags |= flags;
}

 
static inline void worker_clr_flags(struct worker *worker, unsigned int flags)
{
	struct worker_pool *pool = worker->pool;
	unsigned int oflags = worker->flags;

	lockdep_assert_held(&pool->lock);

	worker->flags &= ~flags;

	 
	if ((flags & WORKER_NOT_RUNNING) && (oflags & WORKER_NOT_RUNNING))
		if (!(worker->flags & WORKER_NOT_RUNNING))
			pool->nr_running++;
}

 
static struct worker *first_idle_worker(struct worker_pool *pool)
{
	if (unlikely(list_empty(&pool->idle_list)))
		return NULL;

	return list_first_entry(&pool->idle_list, struct worker, entry);
}

 
static void worker_enter_idle(struct worker *worker)
{
	struct worker_pool *pool = worker->pool;

	if (WARN_ON_ONCE(worker->flags & WORKER_IDLE) ||
	    WARN_ON_ONCE(!list_empty(&worker->entry) &&
			 (worker->hentry.next || worker->hentry.pprev)))
		return;

	 
	worker->flags |= WORKER_IDLE;
	pool->nr_idle++;
	worker->last_active = jiffies;

	 
	list_add(&worker->entry, &pool->idle_list);

	if (too_many_workers(pool) && !timer_pending(&pool->idle_timer))
		mod_timer(&pool->idle_timer, jiffies + IDLE_WORKER_TIMEOUT);

	 
	WARN_ON_ONCE(pool->nr_workers == pool->nr_idle && pool->nr_running);
}

 
static void worker_leave_idle(struct worker *worker)
{
	struct worker_pool *pool = worker->pool;

	if (WARN_ON_ONCE(!(worker->flags & WORKER_IDLE)))
		return;
	worker_clr_flags(worker, WORKER_IDLE);
	pool->nr_idle--;
	list_del_init(&worker->entry);
}

 
static struct worker *find_worker_executing_work(struct worker_pool *pool,
						 struct work_struct *work)
{
	struct worker *worker;

	hash_for_each_possible(pool->busy_hash, worker, hentry,
			       (unsigned long)work)
		if (worker->current_work == work &&
		    worker->current_func == work->func)
			return worker;

	return NULL;
}

 
static void move_linked_works(struct work_struct *work, struct list_head *head,
			      struct work_struct **nextp)
{
	struct work_struct *n;

	 
	list_for_each_entry_safe_from(work, n, NULL, entry) {
		list_move_tail(&work->entry, head);
		if (!(*work_data_bits(work) & WORK_STRUCT_LINKED))
			break;
	}

	 
	if (nextp)
		*nextp = n;
}

 
static bool assign_work(struct work_struct *work, struct worker *worker,
			struct work_struct **nextp)
{
	struct worker_pool *pool = worker->pool;
	struct worker *collision;

	lockdep_assert_held(&pool->lock);

	 
	collision = find_worker_executing_work(pool, work);
	if (unlikely(collision)) {
		move_linked_works(work, &collision->scheduled, nextp);
		return false;
	}

	move_linked_works(work, &worker->scheduled, nextp);
	return true;
}

 
static bool kick_pool(struct worker_pool *pool)
{
	struct worker *worker = first_idle_worker(pool);
	struct task_struct *p;

	lockdep_assert_held(&pool->lock);

	if (!need_more_worker(pool) || !worker)
		return false;

	p = worker->task;

#ifdef CONFIG_SMP
	 
	if (!pool->attrs->affn_strict &&
	    !cpumask_test_cpu(p->wake_cpu, pool->attrs->__pod_cpumask)) {
		struct work_struct *work = list_first_entry(&pool->worklist,
						struct work_struct, entry);
		p->wake_cpu = cpumask_any_distribute(pool->attrs->__pod_cpumask);
		get_work_pwq(work)->stats[PWQ_STAT_REPATRIATED]++;
	}
#endif
	wake_up_process(p);
	return true;
}

#ifdef CONFIG_WQ_CPU_INTENSIVE_REPORT

 
#define WCI_MAX_ENTS 128

struct wci_ent {
	work_func_t		func;
	atomic64_t		cnt;
	struct hlist_node	hash_node;
};

static struct wci_ent wci_ents[WCI_MAX_ENTS];
static int wci_nr_ents;
static DEFINE_RAW_SPINLOCK(wci_lock);
static DEFINE_HASHTABLE(wci_hash, ilog2(WCI_MAX_ENTS));

static struct wci_ent *wci_find_ent(work_func_t func)
{
	struct wci_ent *ent;

	hash_for_each_possible_rcu(wci_hash, ent, hash_node,
				   (unsigned long)func) {
		if (ent->func == func)
			return ent;
	}
	return NULL;
}

static void wq_cpu_intensive_report(work_func_t func)
{
	struct wci_ent *ent;

restart:
	ent = wci_find_ent(func);
	if (ent) {
		u64 cnt;

		 
		cnt = atomic64_inc_return_relaxed(&ent->cnt);
		if (cnt >= 4 && is_power_of_2(cnt))
			printk_deferred(KERN_WARNING "workqueue: %ps hogged CPU for >%luus %llu times, consider switching to WQ_UNBOUND\n",
					ent->func, wq_cpu_intensive_thresh_us,
					atomic64_read(&ent->cnt));
		return;
	}

	 
	if (wci_nr_ents >= WCI_MAX_ENTS)
		return;

	raw_spin_lock(&wci_lock);

	if (wci_nr_ents >= WCI_MAX_ENTS) {
		raw_spin_unlock(&wci_lock);
		return;
	}

	if (wci_find_ent(func)) {
		raw_spin_unlock(&wci_lock);
		goto restart;
	}

	ent = &wci_ents[wci_nr_ents++];
	ent->func = func;
	atomic64_set(&ent->cnt, 1);
	hash_add_rcu(wci_hash, &ent->hash_node, (unsigned long)func);

	raw_spin_unlock(&wci_lock);
}

#else	 
static void wq_cpu_intensive_report(work_func_t func) {}
#endif	 

 
void wq_worker_running(struct task_struct *task)
{
	struct worker *worker = kthread_data(task);

	if (!READ_ONCE(worker->sleeping))
		return;

	 
	preempt_disable();
	if (!(worker->flags & WORKER_NOT_RUNNING))
		worker->pool->nr_running++;
	preempt_enable();

	 
	worker->current_at = worker->task->se.sum_exec_runtime;

	WRITE_ONCE(worker->sleeping, 0);
}

 
void wq_worker_sleeping(struct task_struct *task)
{
	struct worker *worker = kthread_data(task);
	struct worker_pool *pool;

	 
	if (worker->flags & WORKER_NOT_RUNNING)
		return;

	pool = worker->pool;

	 
	if (READ_ONCE(worker->sleeping))
		return;

	WRITE_ONCE(worker->sleeping, 1);
	raw_spin_lock_irq(&pool->lock);

	 
	if (worker->flags & WORKER_NOT_RUNNING) {
		raw_spin_unlock_irq(&pool->lock);
		return;
	}

	pool->nr_running--;
	if (kick_pool(pool))
		worker->current_pwq->stats[PWQ_STAT_CM_WAKEUP]++;

	raw_spin_unlock_irq(&pool->lock);
}

 
void wq_worker_tick(struct task_struct *task)
{
	struct worker *worker = kthread_data(task);
	struct pool_workqueue *pwq = worker->current_pwq;
	struct worker_pool *pool = worker->pool;

	if (!pwq)
		return;

	pwq->stats[PWQ_STAT_CPU_TIME] += TICK_USEC;

	if (!wq_cpu_intensive_thresh_us)
		return;

	 
	if ((worker->flags & WORKER_NOT_RUNNING) || READ_ONCE(worker->sleeping) ||
	    worker->task->se.sum_exec_runtime - worker->current_at <
	    wq_cpu_intensive_thresh_us * NSEC_PER_USEC)
		return;

	raw_spin_lock(&pool->lock);

	worker_set_flags(worker, WORKER_CPU_INTENSIVE);
	wq_cpu_intensive_report(worker->current_func);
	pwq->stats[PWQ_STAT_CPU_INTENSIVE]++;

	if (kick_pool(pool))
		pwq->stats[PWQ_STAT_CM_WAKEUP]++;

	raw_spin_unlock(&pool->lock);
}

 
work_func_t wq_worker_last_func(struct task_struct *task)
{
	struct worker *worker = kthread_data(task);

	return worker->last_func;
}

 
static void get_pwq(struct pool_workqueue *pwq)
{
	lockdep_assert_held(&pwq->pool->lock);
	WARN_ON_ONCE(pwq->refcnt <= 0);
	pwq->refcnt++;
}

 
static void put_pwq(struct pool_workqueue *pwq)
{
	lockdep_assert_held(&pwq->pool->lock);
	if (likely(--pwq->refcnt))
		return;
	 
	kthread_queue_work(pwq_release_worker, &pwq->release_work);
}

 
static void put_pwq_unlocked(struct pool_workqueue *pwq)
{
	if (pwq) {
		 
		raw_spin_lock_irq(&pwq->pool->lock);
		put_pwq(pwq);
		raw_spin_unlock_irq(&pwq->pool->lock);
	}
}

static void pwq_activate_inactive_work(struct work_struct *work)
{
	struct pool_workqueue *pwq = get_work_pwq(work);

	trace_workqueue_activate_work(work);
	if (list_empty(&pwq->pool->worklist))
		pwq->pool->watchdog_ts = jiffies;
	move_linked_works(work, &pwq->pool->worklist, NULL);
	__clear_bit(WORK_STRUCT_INACTIVE_BIT, work_data_bits(work));
	pwq->nr_active++;
}

static void pwq_activate_first_inactive(struct pool_workqueue *pwq)
{
	struct work_struct *work = list_first_entry(&pwq->inactive_works,
						    struct work_struct, entry);

	pwq_activate_inactive_work(work);
}

 
static void pwq_dec_nr_in_flight(struct pool_workqueue *pwq, unsigned long work_data)
{
	int color = get_work_color(work_data);

	if (!(work_data & WORK_STRUCT_INACTIVE)) {
		pwq->nr_active--;
		if (!list_empty(&pwq->inactive_works)) {
			 
			if (pwq->nr_active < pwq->max_active)
				pwq_activate_first_inactive(pwq);
		}
	}

	pwq->nr_in_flight[color]--;

	 
	if (likely(pwq->flush_color != color))
		goto out_put;

	 
	if (pwq->nr_in_flight[color])
		goto out_put;

	 
	pwq->flush_color = -1;

	 
	if (atomic_dec_and_test(&pwq->wq->nr_pwqs_to_flush))
		complete(&pwq->wq->first_flusher->done);
out_put:
	put_pwq(pwq);
}

 
static int try_to_grab_pending(struct work_struct *work, bool is_dwork,
			       unsigned long *flags)
{
	struct worker_pool *pool;
	struct pool_workqueue *pwq;

	local_irq_save(*flags);

	 
	if (is_dwork) {
		struct delayed_work *dwork = to_delayed_work(work);

		 
		if (likely(del_timer(&dwork->timer)))
			return 1;
	}

	 
	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work)))
		return 0;

	rcu_read_lock();
	 
	pool = get_work_pool(work);
	if (!pool)
		goto fail;

	raw_spin_lock(&pool->lock);
	 
	pwq = get_work_pwq(work);
	if (pwq && pwq->pool == pool) {
		debug_work_deactivate(work);

		 
		if (*work_data_bits(work) & WORK_STRUCT_INACTIVE)
			pwq_activate_inactive_work(work);

		list_del_init(&work->entry);
		pwq_dec_nr_in_flight(pwq, *work_data_bits(work));

		 
		set_work_pool_and_keep_pending(work, pool->id);

		raw_spin_unlock(&pool->lock);
		rcu_read_unlock();
		return 1;
	}
	raw_spin_unlock(&pool->lock);
fail:
	rcu_read_unlock();
	local_irq_restore(*flags);
	if (work_is_canceling(work))
		return -ENOENT;
	cpu_relax();
	return -EAGAIN;
}

 
static void insert_work(struct pool_workqueue *pwq, struct work_struct *work,
			struct list_head *head, unsigned int extra_flags)
{
	debug_work_activate(work);

	 
	kasan_record_aux_stack_noalloc(work);

	 
	set_work_pwq(work, pwq, extra_flags);
	list_add_tail(&work->entry, head);
	get_pwq(pwq);
}

 
static bool is_chained_work(struct workqueue_struct *wq)
{
	struct worker *worker;

	worker = current_wq_worker();
	 
	return worker && worker->current_pwq->wq == wq;
}

 
static int wq_select_unbound_cpu(int cpu)
{
	int new_cpu;

	if (likely(!wq_debug_force_rr_cpu)) {
		if (cpumask_test_cpu(cpu, wq_unbound_cpumask))
			return cpu;
	} else {
		pr_warn_once("workqueue: round-robin CPU selection forced, expect performance impact\n");
	}

	new_cpu = __this_cpu_read(wq_rr_cpu_last);
	new_cpu = cpumask_next_and(new_cpu, wq_unbound_cpumask, cpu_online_mask);
	if (unlikely(new_cpu >= nr_cpu_ids)) {
		new_cpu = cpumask_first_and(wq_unbound_cpumask, cpu_online_mask);
		if (unlikely(new_cpu >= nr_cpu_ids))
			return cpu;
	}
	__this_cpu_write(wq_rr_cpu_last, new_cpu);

	return new_cpu;
}

static void __queue_work(int cpu, struct workqueue_struct *wq,
			 struct work_struct *work)
{
	struct pool_workqueue *pwq;
	struct worker_pool *last_pool, *pool;
	unsigned int work_flags;
	unsigned int req_cpu = cpu;

	 
	lockdep_assert_irqs_disabled();


	 
	if (unlikely(wq->flags & (__WQ_DESTROYING | __WQ_DRAINING) &&
		     WARN_ON_ONCE(!is_chained_work(wq))))
		return;
	rcu_read_lock();
retry:
	 
	if (req_cpu == WORK_CPU_UNBOUND) {
		if (wq->flags & WQ_UNBOUND)
			cpu = wq_select_unbound_cpu(raw_smp_processor_id());
		else
			cpu = raw_smp_processor_id();
	}

	pwq = rcu_dereference(*per_cpu_ptr(wq->cpu_pwq, cpu));
	pool = pwq->pool;

	 
	last_pool = get_work_pool(work);
	if (last_pool && last_pool != pool) {
		struct worker *worker;

		raw_spin_lock(&last_pool->lock);

		worker = find_worker_executing_work(last_pool, work);

		if (worker && worker->current_pwq->wq == wq) {
			pwq = worker->current_pwq;
			pool = pwq->pool;
			WARN_ON_ONCE(pool != last_pool);
		} else {
			 
			raw_spin_unlock(&last_pool->lock);
			raw_spin_lock(&pool->lock);
		}
	} else {
		raw_spin_lock(&pool->lock);
	}

	 
	if (unlikely(!pwq->refcnt)) {
		if (wq->flags & WQ_UNBOUND) {
			raw_spin_unlock(&pool->lock);
			cpu_relax();
			goto retry;
		}
		 
		WARN_ONCE(true, "workqueue: per-cpu pwq for %s on cpu%d has 0 refcnt",
			  wq->name, cpu);
	}

	 
	trace_workqueue_queue_work(req_cpu, pwq, work);

	if (WARN_ON(!list_empty(&work->entry)))
		goto out;

	pwq->nr_in_flight[pwq->work_color]++;
	work_flags = work_color_to_flags(pwq->work_color);

	if (likely(pwq->nr_active < pwq->max_active)) {
		if (list_empty(&pool->worklist))
			pool->watchdog_ts = jiffies;

		trace_workqueue_activate_work(work);
		pwq->nr_active++;
		insert_work(pwq, work, &pool->worklist, work_flags);
		kick_pool(pool);
	} else {
		work_flags |= WORK_STRUCT_INACTIVE;
		insert_work(pwq, work, &pwq->inactive_works, work_flags);
	}

out:
	raw_spin_unlock(&pool->lock);
	rcu_read_unlock();
}

 
bool queue_work_on(int cpu, struct workqueue_struct *wq,
		   struct work_struct *work)
{
	bool ret = false;
	unsigned long flags;

	local_irq_save(flags);

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		__queue_work(cpu, wq, work);
		ret = true;
	}

	local_irq_restore(flags);
	return ret;
}
EXPORT_SYMBOL(queue_work_on);

 
static int select_numa_node_cpu(int node)
{
	int cpu;

	 
	if (node < 0 || node >= MAX_NUMNODES || !node_online(node))
		return WORK_CPU_UNBOUND;

	 
	cpu = raw_smp_processor_id();
	if (node == cpu_to_node(cpu))
		return cpu;

	 
	cpu = cpumask_any_and(cpumask_of_node(node), cpu_online_mask);

	 
	return cpu < nr_cpu_ids ? cpu : WORK_CPU_UNBOUND;
}

 
bool queue_work_node(int node, struct workqueue_struct *wq,
		     struct work_struct *work)
{
	unsigned long flags;
	bool ret = false;

	 
	WARN_ON_ONCE(!(wq->flags & WQ_UNBOUND));

	local_irq_save(flags);

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		int cpu = select_numa_node_cpu(node);

		__queue_work(cpu, wq, work);
		ret = true;
	}

	local_irq_restore(flags);
	return ret;
}
EXPORT_SYMBOL_GPL(queue_work_node);

void delayed_work_timer_fn(struct timer_list *t)
{
	struct delayed_work *dwork = from_timer(dwork, t, timer);

	 
	__queue_work(dwork->cpu, dwork->wq, &dwork->work);
}
EXPORT_SYMBOL(delayed_work_timer_fn);

static void __queue_delayed_work(int cpu, struct workqueue_struct *wq,
				struct delayed_work *dwork, unsigned long delay)
{
	struct timer_list *timer = &dwork->timer;
	struct work_struct *work = &dwork->work;

	WARN_ON_ONCE(!wq);
	WARN_ON_ONCE(timer->function != delayed_work_timer_fn);
	WARN_ON_ONCE(timer_pending(timer));
	WARN_ON_ONCE(!list_empty(&work->entry));

	 
	if (!delay) {
		__queue_work(cpu, wq, &dwork->work);
		return;
	}

	dwork->wq = wq;
	dwork->cpu = cpu;
	timer->expires = jiffies + delay;

	if (unlikely(cpu != WORK_CPU_UNBOUND))
		add_timer_on(timer, cpu);
	else
		add_timer(timer);
}

 
bool queue_delayed_work_on(int cpu, struct workqueue_struct *wq,
			   struct delayed_work *dwork, unsigned long delay)
{
	struct work_struct *work = &dwork->work;
	bool ret = false;
	unsigned long flags;

	 
	local_irq_save(flags);

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		__queue_delayed_work(cpu, wq, dwork, delay);
		ret = true;
	}

	local_irq_restore(flags);
	return ret;
}
EXPORT_SYMBOL(queue_delayed_work_on);

 
bool mod_delayed_work_on(int cpu, struct workqueue_struct *wq,
			 struct delayed_work *dwork, unsigned long delay)
{
	unsigned long flags;
	int ret;

	do {
		ret = try_to_grab_pending(&dwork->work, true, &flags);
	} while (unlikely(ret == -EAGAIN));

	if (likely(ret >= 0)) {
		__queue_delayed_work(cpu, wq, dwork, delay);
		local_irq_restore(flags);
	}

	 
	return ret;
}
EXPORT_SYMBOL_GPL(mod_delayed_work_on);

static void rcu_work_rcufn(struct rcu_head *rcu)
{
	struct rcu_work *rwork = container_of(rcu, struct rcu_work, rcu);

	 
	local_irq_disable();
	__queue_work(WORK_CPU_UNBOUND, rwork->wq, &rwork->work);
	local_irq_enable();
}

 
bool queue_rcu_work(struct workqueue_struct *wq, struct rcu_work *rwork)
{
	struct work_struct *work = &rwork->work;

	if (!test_and_set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(work))) {
		rwork->wq = wq;
		call_rcu_hurry(&rwork->rcu, rcu_work_rcufn);
		return true;
	}

	return false;
}
EXPORT_SYMBOL(queue_rcu_work);

static struct worker *alloc_worker(int node)
{
	struct worker *worker;

	worker = kzalloc_node(sizeof(*worker), GFP_KERNEL, node);
	if (worker) {
		INIT_LIST_HEAD(&worker->entry);
		INIT_LIST_HEAD(&worker->scheduled);
		INIT_LIST_HEAD(&worker->node);
		 
		worker->flags = WORKER_PREP;
	}
	return worker;
}

static cpumask_t *pool_allowed_cpus(struct worker_pool *pool)
{
	if (pool->cpu < 0 && pool->attrs->affn_strict)
		return pool->attrs->__pod_cpumask;
	else
		return pool->attrs->cpumask;
}

 
static void worker_attach_to_pool(struct worker *worker,
				   struct worker_pool *pool)
{
	mutex_lock(&wq_pool_attach_mutex);

	 
	if (pool->flags & POOL_DISASSOCIATED)
		worker->flags |= WORKER_UNBOUND;
	else
		kthread_set_per_cpu(worker->task, pool->cpu);

	if (worker->rescue_wq)
		set_cpus_allowed_ptr(worker->task, pool_allowed_cpus(pool));

	list_add_tail(&worker->node, &pool->workers);
	worker->pool = pool;

	mutex_unlock(&wq_pool_attach_mutex);
}

 
static void worker_detach_from_pool(struct worker *worker)
{
	struct worker_pool *pool = worker->pool;
	struct completion *detach_completion = NULL;

	mutex_lock(&wq_pool_attach_mutex);

	kthread_set_per_cpu(worker->task, -1);
	list_del(&worker->node);
	worker->pool = NULL;

	if (list_empty(&pool->workers) && list_empty(&pool->dying_workers))
		detach_completion = pool->detach_completion;
	mutex_unlock(&wq_pool_attach_mutex);

	 
	worker->flags &= ~(WORKER_UNBOUND | WORKER_REBOUND);

	if (detach_completion)
		complete(detach_completion);
}

 
static struct worker *create_worker(struct worker_pool *pool)
{
	struct worker *worker;
	int id;
	char id_buf[23];

	 
	id = ida_alloc(&pool->worker_ida, GFP_KERNEL);
	if (id < 0) {
		pr_err_once("workqueue: Failed to allocate a worker ID: %pe\n",
			    ERR_PTR(id));
		return NULL;
	}

	worker = alloc_worker(pool->node);
	if (!worker) {
		pr_err_once("workqueue: Failed to allocate a worker\n");
		goto fail;
	}

	worker->id = id;

	if (pool->cpu >= 0)
		snprintf(id_buf, sizeof(id_buf), "%d:%d%s", pool->cpu, id,
			 pool->attrs->nice < 0  ? "H" : "");
	else
		snprintf(id_buf, sizeof(id_buf), "u%d:%d", pool->id, id);

	worker->task = kthread_create_on_node(worker_thread, worker, pool->node,
					      "kworker/%s", id_buf);
	if (IS_ERR(worker->task)) {
		if (PTR_ERR(worker->task) == -EINTR) {
			pr_err("workqueue: Interrupted when creating a worker thread \"kworker/%s\"\n",
			       id_buf);
		} else {
			pr_err_once("workqueue: Failed to create a worker thread: %pe",
				    worker->task);
		}
		goto fail;
	}

	set_user_nice(worker->task, pool->attrs->nice);
	kthread_bind_mask(worker->task, pool_allowed_cpus(pool));

	 
	worker_attach_to_pool(worker, pool);

	 
	raw_spin_lock_irq(&pool->lock);

	worker->pool->nr_workers++;
	worker_enter_idle(worker);
	kick_pool(pool);

	 
	wake_up_process(worker->task);

	raw_spin_unlock_irq(&pool->lock);

	return worker;

fail:
	ida_free(&pool->worker_ida, id);
	kfree(worker);
	return NULL;
}

static void unbind_worker(struct worker *worker)
{
	lockdep_assert_held(&wq_pool_attach_mutex);

	kthread_set_per_cpu(worker->task, -1);
	if (cpumask_intersects(wq_unbound_cpumask, cpu_active_mask))
		WARN_ON_ONCE(set_cpus_allowed_ptr(worker->task, wq_unbound_cpumask) < 0);
	else
		WARN_ON_ONCE(set_cpus_allowed_ptr(worker->task, cpu_possible_mask) < 0);
}

static void wake_dying_workers(struct list_head *cull_list)
{
	struct worker *worker, *tmp;

	list_for_each_entry_safe(worker, tmp, cull_list, entry) {
		list_del_init(&worker->entry);
		unbind_worker(worker);
		 
		wake_up_process(worker->task);
	}
}

 
static void set_worker_dying(struct worker *worker, struct list_head *list)
{
	struct worker_pool *pool = worker->pool;

	lockdep_assert_held(&pool->lock);
	lockdep_assert_held(&wq_pool_attach_mutex);

	 
	if (WARN_ON(worker->current_work) ||
	    WARN_ON(!list_empty(&worker->scheduled)) ||
	    WARN_ON(!(worker->flags & WORKER_IDLE)))
		return;

	pool->nr_workers--;
	pool->nr_idle--;

	worker->flags |= WORKER_DIE;

	list_move(&worker->entry, list);
	list_move(&worker->node, &pool->dying_workers);
}

 
static void idle_worker_timeout(struct timer_list *t)
{
	struct worker_pool *pool = from_timer(pool, t, idle_timer);
	bool do_cull = false;

	if (work_pending(&pool->idle_cull_work))
		return;

	raw_spin_lock_irq(&pool->lock);

	if (too_many_workers(pool)) {
		struct worker *worker;
		unsigned long expires;

		 
		worker = list_entry(pool->idle_list.prev, struct worker, entry);
		expires = worker->last_active + IDLE_WORKER_TIMEOUT;
		do_cull = !time_before(jiffies, expires);

		if (!do_cull)
			mod_timer(&pool->idle_timer, expires);
	}
	raw_spin_unlock_irq(&pool->lock);

	if (do_cull)
		queue_work(system_unbound_wq, &pool->idle_cull_work);
}

 
static void idle_cull_fn(struct work_struct *work)
{
	struct worker_pool *pool = container_of(work, struct worker_pool, idle_cull_work);
	LIST_HEAD(cull_list);

	 
	mutex_lock(&wq_pool_attach_mutex);
	raw_spin_lock_irq(&pool->lock);

	while (too_many_workers(pool)) {
		struct worker *worker;
		unsigned long expires;

		worker = list_entry(pool->idle_list.prev, struct worker, entry);
		expires = worker->last_active + IDLE_WORKER_TIMEOUT;

		if (time_before(jiffies, expires)) {
			mod_timer(&pool->idle_timer, expires);
			break;
		}

		set_worker_dying(worker, &cull_list);
	}

	raw_spin_unlock_irq(&pool->lock);
	wake_dying_workers(&cull_list);
	mutex_unlock(&wq_pool_attach_mutex);
}

static void send_mayday(struct work_struct *work)
{
	struct pool_workqueue *pwq = get_work_pwq(work);
	struct workqueue_struct *wq = pwq->wq;

	lockdep_assert_held(&wq_mayday_lock);

	if (!wq->rescuer)
		return;

	 
	if (list_empty(&pwq->mayday_node)) {
		 
		get_pwq(pwq);
		list_add_tail(&pwq->mayday_node, &wq->maydays);
		wake_up_process(wq->rescuer->task);
		pwq->stats[PWQ_STAT_MAYDAY]++;
	}
}

static void pool_mayday_timeout(struct timer_list *t)
{
	struct worker_pool *pool = from_timer(pool, t, mayday_timer);
	struct work_struct *work;

	raw_spin_lock_irq(&pool->lock);
	raw_spin_lock(&wq_mayday_lock);		 

	if (need_to_create_worker(pool)) {
		 
		list_for_each_entry(work, &pool->worklist, entry)
			send_mayday(work);
	}

	raw_spin_unlock(&wq_mayday_lock);
	raw_spin_unlock_irq(&pool->lock);

	mod_timer(&pool->mayday_timer, jiffies + MAYDAY_INTERVAL);
}

 
static void maybe_create_worker(struct worker_pool *pool)
__releases(&pool->lock)
__acquires(&pool->lock)
{
restart:
	raw_spin_unlock_irq(&pool->lock);

	 
	mod_timer(&pool->mayday_timer, jiffies + MAYDAY_INITIAL_TIMEOUT);

	while (true) {
		if (create_worker(pool) || !need_to_create_worker(pool))
			break;

		schedule_timeout_interruptible(CREATE_COOLDOWN);

		if (!need_to_create_worker(pool))
			break;
	}

	del_timer_sync(&pool->mayday_timer);
	raw_spin_lock_irq(&pool->lock);
	 
	if (need_to_create_worker(pool))
		goto restart;
}

 
static bool manage_workers(struct worker *worker)
{
	struct worker_pool *pool = worker->pool;

	if (pool->flags & POOL_MANAGER_ACTIVE)
		return false;

	pool->flags |= POOL_MANAGER_ACTIVE;
	pool->manager = worker;

	maybe_create_worker(pool);

	pool->manager = NULL;
	pool->flags &= ~POOL_MANAGER_ACTIVE;
	rcuwait_wake_up(&manager_wait);
	return true;
}

 
static void process_one_work(struct worker *worker, struct work_struct *work)
__releases(&pool->lock)
__acquires(&pool->lock)
{
	struct pool_workqueue *pwq = get_work_pwq(work);
	struct worker_pool *pool = worker->pool;
	unsigned long work_data;
#ifdef CONFIG_LOCKDEP
	 
	struct lockdep_map lockdep_map;

	lockdep_copy_map(&lockdep_map, &work->lockdep_map);
#endif
	 
	WARN_ON_ONCE(!(pool->flags & POOL_DISASSOCIATED) &&
		     raw_smp_processor_id() != pool->cpu);

	 
	debug_work_deactivate(work);
	hash_add(pool->busy_hash, &worker->hentry, (unsigned long)work);
	worker->current_work = work;
	worker->current_func = work->func;
	worker->current_pwq = pwq;
	worker->current_at = worker->task->se.sum_exec_runtime;
	work_data = *work_data_bits(work);
	worker->current_color = get_work_color(work_data);

	 
	strscpy(worker->desc, pwq->wq->name, WORKER_DESC_LEN);

	list_del_init(&work->entry);

	 
	if (unlikely(pwq->wq->flags & WQ_CPU_INTENSIVE))
		worker_set_flags(worker, WORKER_CPU_INTENSIVE);

	 
	kick_pool(pool);

	 
	set_work_pool_and_clear_pending(work, pool->id);

	pwq->stats[PWQ_STAT_STARTED]++;
	raw_spin_unlock_irq(&pool->lock);

	lock_map_acquire(&pwq->wq->lockdep_map);
	lock_map_acquire(&lockdep_map);
	 
	lockdep_invariant_state(true);
	trace_workqueue_execute_start(work);
	worker->current_func(work);
	 
	trace_workqueue_execute_end(work, worker->current_func);
	pwq->stats[PWQ_STAT_COMPLETED]++;
	lock_map_release(&lockdep_map);
	lock_map_release(&pwq->wq->lockdep_map);

	if (unlikely(in_atomic() || lockdep_depth(current) > 0)) {
		pr_err("BUG: workqueue leaked lock or atomic: %s/0x%08x/%d\n"
		       "     last function: %ps\n",
		       current->comm, preempt_count(), task_pid_nr(current),
		       worker->current_func);
		debug_show_held_locks(current);
		dump_stack();
	}

	 
	cond_resched();

	raw_spin_lock_irq(&pool->lock);

	 
	worker_clr_flags(worker, WORKER_CPU_INTENSIVE);

	 
	worker->last_func = worker->current_func;

	 
	hash_del(&worker->hentry);
	worker->current_work = NULL;
	worker->current_func = NULL;
	worker->current_pwq = NULL;
	worker->current_color = INT_MAX;
	pwq_dec_nr_in_flight(pwq, work_data);
}

 
static void process_scheduled_works(struct worker *worker)
{
	struct work_struct *work;
	bool first = true;

	while ((work = list_first_entry_or_null(&worker->scheduled,
						struct work_struct, entry))) {
		if (first) {
			worker->pool->watchdog_ts = jiffies;
			first = false;
		}
		process_one_work(worker, work);
	}
}

static void set_pf_worker(bool val)
{
	mutex_lock(&wq_pool_attach_mutex);
	if (val)
		current->flags |= PF_WQ_WORKER;
	else
		current->flags &= ~PF_WQ_WORKER;
	mutex_unlock(&wq_pool_attach_mutex);
}

 
static int worker_thread(void *__worker)
{
	struct worker *worker = __worker;
	struct worker_pool *pool = worker->pool;

	 
	set_pf_worker(true);
woke_up:
	raw_spin_lock_irq(&pool->lock);

	 
	if (unlikely(worker->flags & WORKER_DIE)) {
		raw_spin_unlock_irq(&pool->lock);
		set_pf_worker(false);

		set_task_comm(worker->task, "kworker/dying");
		ida_free(&pool->worker_ida, worker->id);
		worker_detach_from_pool(worker);
		WARN_ON_ONCE(!list_empty(&worker->entry));
		kfree(worker);
		return 0;
	}

	worker_leave_idle(worker);
recheck:
	 
	if (!need_more_worker(pool))
		goto sleep;

	 
	if (unlikely(!may_start_working(pool)) && manage_workers(worker))
		goto recheck;

	 
	WARN_ON_ONCE(!list_empty(&worker->scheduled));

	 
	worker_clr_flags(worker, WORKER_PREP | WORKER_REBOUND);

	do {
		struct work_struct *work =
			list_first_entry(&pool->worklist,
					 struct work_struct, entry);

		if (assign_work(work, worker, NULL))
			process_scheduled_works(worker);
	} while (keep_working(pool));

	worker_set_flags(worker, WORKER_PREP);
sleep:
	 
	worker_enter_idle(worker);
	__set_current_state(TASK_IDLE);
	raw_spin_unlock_irq(&pool->lock);
	schedule();
	goto woke_up;
}

 
static int rescuer_thread(void *__rescuer)
{
	struct worker *rescuer = __rescuer;
	struct workqueue_struct *wq = rescuer->rescue_wq;
	bool should_stop;

	set_user_nice(current, RESCUER_NICE_LEVEL);

	 
	set_pf_worker(true);
repeat:
	set_current_state(TASK_IDLE);

	 
	should_stop = kthread_should_stop();

	 
	raw_spin_lock_irq(&wq_mayday_lock);

	while (!list_empty(&wq->maydays)) {
		struct pool_workqueue *pwq = list_first_entry(&wq->maydays,
					struct pool_workqueue, mayday_node);
		struct worker_pool *pool = pwq->pool;
		struct work_struct *work, *n;

		__set_current_state(TASK_RUNNING);
		list_del_init(&pwq->mayday_node);

		raw_spin_unlock_irq(&wq_mayday_lock);

		worker_attach_to_pool(rescuer, pool);

		raw_spin_lock_irq(&pool->lock);

		 
		WARN_ON_ONCE(!list_empty(&rescuer->scheduled));
		list_for_each_entry_safe(work, n, &pool->worklist, entry) {
			if (get_work_pwq(work) == pwq &&
			    assign_work(work, rescuer, &n))
				pwq->stats[PWQ_STAT_RESCUED]++;
		}

		if (!list_empty(&rescuer->scheduled)) {
			process_scheduled_works(rescuer);

			 
			if (pwq->nr_active && need_to_create_worker(pool)) {
				raw_spin_lock(&wq_mayday_lock);
				 
				if (wq->rescuer && list_empty(&pwq->mayday_node)) {
					get_pwq(pwq);
					list_add_tail(&pwq->mayday_node, &wq->maydays);
				}
				raw_spin_unlock(&wq_mayday_lock);
			}
		}

		 
		put_pwq(pwq);

		 
		kick_pool(pool);

		raw_spin_unlock_irq(&pool->lock);

		worker_detach_from_pool(rescuer);

		raw_spin_lock_irq(&wq_mayday_lock);
	}

	raw_spin_unlock_irq(&wq_mayday_lock);

	if (should_stop) {
		__set_current_state(TASK_RUNNING);
		set_pf_worker(false);
		return 0;
	}

	 
	WARN_ON_ONCE(!(rescuer->flags & WORKER_NOT_RUNNING));
	schedule();
	goto repeat;
}

 
static void check_flush_dependency(struct workqueue_struct *target_wq,
				   struct work_struct *target_work)
{
	work_func_t target_func = target_work ? target_work->func : NULL;
	struct worker *worker;

	if (target_wq->flags & WQ_MEM_RECLAIM)
		return;

	worker = current_wq_worker();

	WARN_ONCE(current->flags & PF_MEMALLOC,
		  "workqueue: PF_MEMALLOC task %d(%s) is flushing !WQ_MEM_RECLAIM %s:%ps",
		  current->pid, current->comm, target_wq->name, target_func);
	WARN_ONCE(worker && ((worker->current_pwq->wq->flags &
			      (WQ_MEM_RECLAIM | __WQ_LEGACY)) == WQ_MEM_RECLAIM),
		  "workqueue: WQ_MEM_RECLAIM %s:%ps is flushing !WQ_MEM_RECLAIM %s:%ps",
		  worker->current_pwq->wq->name, worker->current_func,
		  target_wq->name, target_func);
}

struct wq_barrier {
	struct work_struct	work;
	struct completion	done;
	struct task_struct	*task;	 
};

static void wq_barrier_func(struct work_struct *work)
{
	struct wq_barrier *barr = container_of(work, struct wq_barrier, work);
	complete(&barr->done);
}

 
static void insert_wq_barrier(struct pool_workqueue *pwq,
			      struct wq_barrier *barr,
			      struct work_struct *target, struct worker *worker)
{
	unsigned int work_flags = 0;
	unsigned int work_color;
	struct list_head *head;

	 
	INIT_WORK_ONSTACK(&barr->work, wq_barrier_func);
	__set_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(&barr->work));

	init_completion_map(&barr->done, &target->lockdep_map);

	barr->task = current;

	 
	work_flags |= WORK_STRUCT_INACTIVE;

	 
	if (worker) {
		head = worker->scheduled.next;
		work_color = worker->current_color;
	} else {
		unsigned long *bits = work_data_bits(target);

		head = target->entry.next;
		 
		work_flags |= *bits & WORK_STRUCT_LINKED;
		work_color = get_work_color(*bits);
		__set_bit(WORK_STRUCT_LINKED_BIT, bits);
	}

	pwq->nr_in_flight[work_color]++;
	work_flags |= work_color_to_flags(work_color);

	insert_work(pwq, &barr->work, head, work_flags);
}

 
static bool flush_workqueue_prep_pwqs(struct workqueue_struct *wq,
				      int flush_color, int work_color)
{
	bool wait = false;
	struct pool_workqueue *pwq;

	if (flush_color >= 0) {
		WARN_ON_ONCE(atomic_read(&wq->nr_pwqs_to_flush));
		atomic_set(&wq->nr_pwqs_to_flush, 1);
	}

	for_each_pwq(pwq, wq) {
		struct worker_pool *pool = pwq->pool;

		raw_spin_lock_irq(&pool->lock);

		if (flush_color >= 0) {
			WARN_ON_ONCE(pwq->flush_color != -1);

			if (pwq->nr_in_flight[flush_color]) {
				pwq->flush_color = flush_color;
				atomic_inc(&wq->nr_pwqs_to_flush);
				wait = true;
			}
		}

		if (work_color >= 0) {
			WARN_ON_ONCE(work_color != work_next_color(pwq->work_color));
			pwq->work_color = work_color;
		}

		raw_spin_unlock_irq(&pool->lock);
	}

	if (flush_color >= 0 && atomic_dec_and_test(&wq->nr_pwqs_to_flush))
		complete(&wq->first_flusher->done);

	return wait;
}

 
void __flush_workqueue(struct workqueue_struct *wq)
{
	struct wq_flusher this_flusher = {
		.list = LIST_HEAD_INIT(this_flusher.list),
		.flush_color = -1,
		.done = COMPLETION_INITIALIZER_ONSTACK_MAP(this_flusher.done, wq->lockdep_map),
	};
	int next_color;

	if (WARN_ON(!wq_online))
		return;

	lock_map_acquire(&wq->lockdep_map);
	lock_map_release(&wq->lockdep_map);

	mutex_lock(&wq->mutex);

	 
	next_color = work_next_color(wq->work_color);

	if (next_color != wq->flush_color) {
		 
		WARN_ON_ONCE(!list_empty(&wq->flusher_overflow));
		this_flusher.flush_color = wq->work_color;
		wq->work_color = next_color;

		if (!wq->first_flusher) {
			 
			WARN_ON_ONCE(wq->flush_color != this_flusher.flush_color);

			wq->first_flusher = &this_flusher;

			if (!flush_workqueue_prep_pwqs(wq, wq->flush_color,
						       wq->work_color)) {
				 
				wq->flush_color = next_color;
				wq->first_flusher = NULL;
				goto out_unlock;
			}
		} else {
			 
			WARN_ON_ONCE(wq->flush_color == this_flusher.flush_color);
			list_add_tail(&this_flusher.list, &wq->flusher_queue);
			flush_workqueue_prep_pwqs(wq, -1, wq->work_color);
		}
	} else {
		 
		list_add_tail(&this_flusher.list, &wq->flusher_overflow);
	}

	check_flush_dependency(wq, NULL);

	mutex_unlock(&wq->mutex);

	wait_for_completion(&this_flusher.done);

	 
	if (READ_ONCE(wq->first_flusher) != &this_flusher)
		return;

	mutex_lock(&wq->mutex);

	 
	if (wq->first_flusher != &this_flusher)
		goto out_unlock;

	WRITE_ONCE(wq->first_flusher, NULL);

	WARN_ON_ONCE(!list_empty(&this_flusher.list));
	WARN_ON_ONCE(wq->flush_color != this_flusher.flush_color);

	while (true) {
		struct wq_flusher *next, *tmp;

		 
		list_for_each_entry_safe(next, tmp, &wq->flusher_queue, list) {
			if (next->flush_color != wq->flush_color)
				break;
			list_del_init(&next->list);
			complete(&next->done);
		}

		WARN_ON_ONCE(!list_empty(&wq->flusher_overflow) &&
			     wq->flush_color != work_next_color(wq->work_color));

		 
		wq->flush_color = work_next_color(wq->flush_color);

		 
		if (!list_empty(&wq->flusher_overflow)) {
			 
			list_for_each_entry(tmp, &wq->flusher_overflow, list)
				tmp->flush_color = wq->work_color;

			wq->work_color = work_next_color(wq->work_color);

			list_splice_tail_init(&wq->flusher_overflow,
					      &wq->flusher_queue);
			flush_workqueue_prep_pwqs(wq, -1, wq->work_color);
		}

		if (list_empty(&wq->flusher_queue)) {
			WARN_ON_ONCE(wq->flush_color != wq->work_color);
			break;
		}

		 
		WARN_ON_ONCE(wq->flush_color == wq->work_color);
		WARN_ON_ONCE(wq->flush_color != next->flush_color);

		list_del_init(&next->list);
		wq->first_flusher = next;

		if (flush_workqueue_prep_pwqs(wq, wq->flush_color, -1))
			break;

		 
		wq->first_flusher = NULL;
	}

out_unlock:
	mutex_unlock(&wq->mutex);
}
EXPORT_SYMBOL(__flush_workqueue);

 
void drain_workqueue(struct workqueue_struct *wq)
{
	unsigned int flush_cnt = 0;
	struct pool_workqueue *pwq;

	 
	mutex_lock(&wq->mutex);
	if (!wq->nr_drainers++)
		wq->flags |= __WQ_DRAINING;
	mutex_unlock(&wq->mutex);
reflush:
	__flush_workqueue(wq);

	mutex_lock(&wq->mutex);

	for_each_pwq(pwq, wq) {
		bool drained;

		raw_spin_lock_irq(&pwq->pool->lock);
		drained = !pwq->nr_active && list_empty(&pwq->inactive_works);
		raw_spin_unlock_irq(&pwq->pool->lock);

		if (drained)
			continue;

		if (++flush_cnt == 10 ||
		    (flush_cnt % 100 == 0 && flush_cnt <= 1000))
			pr_warn("workqueue %s: %s() isn't complete after %u tries\n",
				wq->name, __func__, flush_cnt);

		mutex_unlock(&wq->mutex);
		goto reflush;
	}

	if (!--wq->nr_drainers)
		wq->flags &= ~__WQ_DRAINING;
	mutex_unlock(&wq->mutex);
}
EXPORT_SYMBOL_GPL(drain_workqueue);

static bool start_flush_work(struct work_struct *work, struct wq_barrier *barr,
			     bool from_cancel)
{
	struct worker *worker = NULL;
	struct worker_pool *pool;
	struct pool_workqueue *pwq;

	might_sleep();

	rcu_read_lock();
	pool = get_work_pool(work);
	if (!pool) {
		rcu_read_unlock();
		return false;
	}

	raw_spin_lock_irq(&pool->lock);
	 
	pwq = get_work_pwq(work);
	if (pwq) {
		if (unlikely(pwq->pool != pool))
			goto already_gone;
	} else {
		worker = find_worker_executing_work(pool, work);
		if (!worker)
			goto already_gone;
		pwq = worker->current_pwq;
	}

	check_flush_dependency(pwq->wq, work);

	insert_wq_barrier(pwq, barr, work, worker);
	raw_spin_unlock_irq(&pool->lock);

	 
	if (!from_cancel &&
	    (pwq->wq->saved_max_active == 1 || pwq->wq->rescuer)) {
		lock_map_acquire(&pwq->wq->lockdep_map);
		lock_map_release(&pwq->wq->lockdep_map);
	}
	rcu_read_unlock();
	return true;
already_gone:
	raw_spin_unlock_irq(&pool->lock);
	rcu_read_unlock();
	return false;
}

static bool __flush_work(struct work_struct *work, bool from_cancel)
{
	struct wq_barrier barr;

	if (WARN_ON(!wq_online))
		return false;

	if (WARN_ON(!work->func))
		return false;

	lock_map_acquire(&work->lockdep_map);
	lock_map_release(&work->lockdep_map);

	if (start_flush_work(work, &barr, from_cancel)) {
		wait_for_completion(&barr.done);
		destroy_work_on_stack(&barr.work);
		return true;
	} else {
		return false;
	}
}

 
bool flush_work(struct work_struct *work)
{
	return __flush_work(work, false);
}
EXPORT_SYMBOL_GPL(flush_work);

struct cwt_wait {
	wait_queue_entry_t		wait;
	struct work_struct	*work;
};

static int cwt_wakefn(wait_queue_entry_t *wait, unsigned mode, int sync, void *key)
{
	struct cwt_wait *cwait = container_of(wait, struct cwt_wait, wait);

	if (cwait->work != key)
		return 0;
	return autoremove_wake_function(wait, mode, sync, key);
}

static bool __cancel_work_timer(struct work_struct *work, bool is_dwork)
{
	static DECLARE_WAIT_QUEUE_HEAD(cancel_waitq);
	unsigned long flags;
	int ret;

	do {
		ret = try_to_grab_pending(work, is_dwork, &flags);
		 
		if (unlikely(ret == -ENOENT)) {
			struct cwt_wait cwait;

			init_wait(&cwait.wait);
			cwait.wait.func = cwt_wakefn;
			cwait.work = work;

			prepare_to_wait_exclusive(&cancel_waitq, &cwait.wait,
						  TASK_UNINTERRUPTIBLE);
			if (work_is_canceling(work))
				schedule();
			finish_wait(&cancel_waitq, &cwait.wait);
		}
	} while (unlikely(ret < 0));

	 
	mark_work_canceling(work);
	local_irq_restore(flags);

	 
	if (wq_online)
		__flush_work(work, true);

	clear_work_data(work);

	 
	smp_mb();
	if (waitqueue_active(&cancel_waitq))
		__wake_up(&cancel_waitq, TASK_NORMAL, 1, work);

	return ret;
}

 
bool cancel_work_sync(struct work_struct *work)
{
	return __cancel_work_timer(work, false);
}
EXPORT_SYMBOL_GPL(cancel_work_sync);

 
bool flush_delayed_work(struct delayed_work *dwork)
{
	local_irq_disable();
	if (del_timer_sync(&dwork->timer))
		__queue_work(dwork->cpu, dwork->wq, &dwork->work);
	local_irq_enable();
	return flush_work(&dwork->work);
}
EXPORT_SYMBOL(flush_delayed_work);

 
bool flush_rcu_work(struct rcu_work *rwork)
{
	if (test_bit(WORK_STRUCT_PENDING_BIT, work_data_bits(&rwork->work))) {
		rcu_barrier();
		flush_work(&rwork->work);
		return true;
	} else {
		return flush_work(&rwork->work);
	}
}
EXPORT_SYMBOL(flush_rcu_work);

static bool __cancel_work(struct work_struct *work, bool is_dwork)
{
	unsigned long flags;
	int ret;

	do {
		ret = try_to_grab_pending(work, is_dwork, &flags);
	} while (unlikely(ret == -EAGAIN));

	if (unlikely(ret < 0))
		return false;

	set_work_pool_and_clear_pending(work, get_work_pool_id(work));
	local_irq_restore(flags);
	return ret;
}

 
bool cancel_work(struct work_struct *work)
{
	return __cancel_work(work, false);
}
EXPORT_SYMBOL(cancel_work);

 
bool cancel_delayed_work(struct delayed_work *dwork)
{
	return __cancel_work(&dwork->work, true);
}
EXPORT_SYMBOL(cancel_delayed_work);

 
bool cancel_delayed_work_sync(struct delayed_work *dwork)
{
	return __cancel_work_timer(&dwork->work, true);
}
EXPORT_SYMBOL(cancel_delayed_work_sync);

 
int schedule_on_each_cpu(work_func_t func)
{
	int cpu;
	struct work_struct __percpu *works;

	works = alloc_percpu(struct work_struct);
	if (!works)
		return -ENOMEM;

	cpus_read_lock();

	for_each_online_cpu(cpu) {
		struct work_struct *work = per_cpu_ptr(works, cpu);

		INIT_WORK(work, func);
		schedule_work_on(cpu, work);
	}

	for_each_online_cpu(cpu)
		flush_work(per_cpu_ptr(works, cpu));

	cpus_read_unlock();
	free_percpu(works);
	return 0;
}

 
int execute_in_process_context(work_func_t fn, struct execute_work *ew)
{
	if (!in_interrupt()) {
		fn(&ew->work);
		return 0;
	}

	INIT_WORK(&ew->work, fn);
	schedule_work(&ew->work);

	return 1;
}
EXPORT_SYMBOL_GPL(execute_in_process_context);

 
void free_workqueue_attrs(struct workqueue_attrs *attrs)
{
	if (attrs) {
		free_cpumask_var(attrs->cpumask);
		free_cpumask_var(attrs->__pod_cpumask);
		kfree(attrs);
	}
}

 
struct workqueue_attrs *alloc_workqueue_attrs(void)
{
	struct workqueue_attrs *attrs;

	attrs = kzalloc(sizeof(*attrs), GFP_KERNEL);
	if (!attrs)
		goto fail;
	if (!alloc_cpumask_var(&attrs->cpumask, GFP_KERNEL))
		goto fail;
	if (!alloc_cpumask_var(&attrs->__pod_cpumask, GFP_KERNEL))
		goto fail;

	cpumask_copy(attrs->cpumask, cpu_possible_mask);
	attrs->affn_scope = WQ_AFFN_DFL;
	return attrs;
fail:
	free_workqueue_attrs(attrs);
	return NULL;
}

static void copy_workqueue_attrs(struct workqueue_attrs *to,
				 const struct workqueue_attrs *from)
{
	to->nice = from->nice;
	cpumask_copy(to->cpumask, from->cpumask);
	cpumask_copy(to->__pod_cpumask, from->__pod_cpumask);
	to->affn_strict = from->affn_strict;

	 
	to->affn_scope = from->affn_scope;
	to->ordered = from->ordered;
}

 
static void wqattrs_clear_for_pool(struct workqueue_attrs *attrs)
{
	attrs->affn_scope = WQ_AFFN_NR_TYPES;
	attrs->ordered = false;
}

 
static u32 wqattrs_hash(const struct workqueue_attrs *attrs)
{
	u32 hash = 0;

	hash = jhash_1word(attrs->nice, hash);
	hash = jhash(cpumask_bits(attrs->cpumask),
		     BITS_TO_LONGS(nr_cpumask_bits) * sizeof(long), hash);
	hash = jhash(cpumask_bits(attrs->__pod_cpumask),
		     BITS_TO_LONGS(nr_cpumask_bits) * sizeof(long), hash);
	hash = jhash_1word(attrs->affn_strict, hash);
	return hash;
}

 
static bool wqattrs_equal(const struct workqueue_attrs *a,
			  const struct workqueue_attrs *b)
{
	if (a->nice != b->nice)
		return false;
	if (!cpumask_equal(a->cpumask, b->cpumask))
		return false;
	if (!cpumask_equal(a->__pod_cpumask, b->__pod_cpumask))
		return false;
	if (a->affn_strict != b->affn_strict)
		return false;
	return true;
}

 
static void wqattrs_actualize_cpumask(struct workqueue_attrs *attrs,
				      const cpumask_t *unbound_cpumask)
{
	 
	cpumask_and(attrs->cpumask, attrs->cpumask, unbound_cpumask);
	if (unlikely(cpumask_empty(attrs->cpumask)))
		cpumask_copy(attrs->cpumask, unbound_cpumask);
}

 
static const struct wq_pod_type *
wqattrs_pod_type(const struct workqueue_attrs *attrs)
{
	enum wq_affn_scope scope;
	struct wq_pod_type *pt;

	 
	lockdep_assert_held(&wq_pool_mutex);

	if (attrs->affn_scope == WQ_AFFN_DFL)
		scope = wq_affn_dfl;
	else
		scope = attrs->affn_scope;

	pt = &wq_pod_types[scope];

	if (!WARN_ON_ONCE(attrs->affn_scope == WQ_AFFN_NR_TYPES) &&
	    likely(pt->nr_pods))
		return pt;

	 
	pt = &wq_pod_types[WQ_AFFN_SYSTEM];
	BUG_ON(!pt->nr_pods);
	return pt;
}

 
static int init_worker_pool(struct worker_pool *pool)
{
	raw_spin_lock_init(&pool->lock);
	pool->id = -1;
	pool->cpu = -1;
	pool->node = NUMA_NO_NODE;
	pool->flags |= POOL_DISASSOCIATED;
	pool->watchdog_ts = jiffies;
	INIT_LIST_HEAD(&pool->worklist);
	INIT_LIST_HEAD(&pool->idle_list);
	hash_init(pool->busy_hash);

	timer_setup(&pool->idle_timer, idle_worker_timeout, TIMER_DEFERRABLE);
	INIT_WORK(&pool->idle_cull_work, idle_cull_fn);

	timer_setup(&pool->mayday_timer, pool_mayday_timeout, 0);

	INIT_LIST_HEAD(&pool->workers);
	INIT_LIST_HEAD(&pool->dying_workers);

	ida_init(&pool->worker_ida);
	INIT_HLIST_NODE(&pool->hash_node);
	pool->refcnt = 1;

	 
	pool->attrs = alloc_workqueue_attrs();
	if (!pool->attrs)
		return -ENOMEM;

	wqattrs_clear_for_pool(pool->attrs);

	return 0;
}

#ifdef CONFIG_LOCKDEP
static void wq_init_lockdep(struct workqueue_struct *wq)
{
	char *lock_name;

	lockdep_register_key(&wq->key);
	lock_name = kasprintf(GFP_KERNEL, "%s%s", "(wq_completion)", wq->name);
	if (!lock_name)
		lock_name = wq->name;

	wq->lock_name = lock_name;
	lockdep_init_map(&wq->lockdep_map, lock_name, &wq->key, 0);
}

static void wq_unregister_lockdep(struct workqueue_struct *wq)
{
	lockdep_unregister_key(&wq->key);
}

static void wq_free_lockdep(struct workqueue_struct *wq)
{
	if (wq->lock_name != wq->name)
		kfree(wq->lock_name);
}
#else
static void wq_init_lockdep(struct workqueue_struct *wq)
{
}

static void wq_unregister_lockdep(struct workqueue_struct *wq)
{
}

static void wq_free_lockdep(struct workqueue_struct *wq)
{
}
#endif

static void rcu_free_wq(struct rcu_head *rcu)
{
	struct workqueue_struct *wq =
		container_of(rcu, struct workqueue_struct, rcu);

	wq_free_lockdep(wq);
	free_percpu(wq->cpu_pwq);
	free_workqueue_attrs(wq->unbound_attrs);
	kfree(wq);
}

static void rcu_free_pool(struct rcu_head *rcu)
{
	struct worker_pool *pool = container_of(rcu, struct worker_pool, rcu);

	ida_destroy(&pool->worker_ida);
	free_workqueue_attrs(pool->attrs);
	kfree(pool);
}

 
static void put_unbound_pool(struct worker_pool *pool)
{
	DECLARE_COMPLETION_ONSTACK(detach_completion);
	struct worker *worker;
	LIST_HEAD(cull_list);

	lockdep_assert_held(&wq_pool_mutex);

	if (--pool->refcnt)
		return;

	 
	if (WARN_ON(!(pool->cpu < 0)) ||
	    WARN_ON(!list_empty(&pool->worklist)))
		return;

	 
	if (pool->id >= 0)
		idr_remove(&worker_pool_idr, pool->id);
	hash_del(&pool->hash_node);

	 
	while (true) {
		rcuwait_wait_event(&manager_wait,
				   !(pool->flags & POOL_MANAGER_ACTIVE),
				   TASK_UNINTERRUPTIBLE);

		mutex_lock(&wq_pool_attach_mutex);
		raw_spin_lock_irq(&pool->lock);
		if (!(pool->flags & POOL_MANAGER_ACTIVE)) {
			pool->flags |= POOL_MANAGER_ACTIVE;
			break;
		}
		raw_spin_unlock_irq(&pool->lock);
		mutex_unlock(&wq_pool_attach_mutex);
	}

	while ((worker = first_idle_worker(pool)))
		set_worker_dying(worker, &cull_list);
	WARN_ON(pool->nr_workers || pool->nr_idle);
	raw_spin_unlock_irq(&pool->lock);

	wake_dying_workers(&cull_list);

	if (!list_empty(&pool->workers) || !list_empty(&pool->dying_workers))
		pool->detach_completion = &detach_completion;
	mutex_unlock(&wq_pool_attach_mutex);

	if (pool->detach_completion)
		wait_for_completion(pool->detach_completion);

	 
	del_timer_sync(&pool->idle_timer);
	cancel_work_sync(&pool->idle_cull_work);
	del_timer_sync(&pool->mayday_timer);

	 
	call_rcu(&pool->rcu, rcu_free_pool);
}

 
static struct worker_pool *get_unbound_pool(const struct workqueue_attrs *attrs)
{
	struct wq_pod_type *pt = &wq_pod_types[WQ_AFFN_NUMA];
	u32 hash = wqattrs_hash(attrs);
	struct worker_pool *pool;
	int pod, node = NUMA_NO_NODE;

	lockdep_assert_held(&wq_pool_mutex);

	 
	hash_for_each_possible(unbound_pool_hash, pool, hash_node, hash) {
		if (wqattrs_equal(pool->attrs, attrs)) {
			pool->refcnt++;
			return pool;
		}
	}

	 
	for (pod = 0; pod < pt->nr_pods; pod++) {
		if (cpumask_subset(attrs->__pod_cpumask, pt->pod_cpus[pod])) {
			node = pt->pod_node[pod];
			break;
		}
	}

	 
	pool = kzalloc_node(sizeof(*pool), GFP_KERNEL, node);
	if (!pool || init_worker_pool(pool) < 0)
		goto fail;

	pool->node = node;
	copy_workqueue_attrs(pool->attrs, attrs);
	wqattrs_clear_for_pool(pool->attrs);

	if (worker_pool_assign_id(pool) < 0)
		goto fail;

	 
	if (wq_online && !create_worker(pool))
		goto fail;

	 
	hash_add(unbound_pool_hash, &pool->hash_node, hash);

	return pool;
fail:
	if (pool)
		put_unbound_pool(pool);
	return NULL;
}

static void rcu_free_pwq(struct rcu_head *rcu)
{
	kmem_cache_free(pwq_cache,
			container_of(rcu, struct pool_workqueue, rcu));
}

 
static void pwq_release_workfn(struct kthread_work *work)
{
	struct pool_workqueue *pwq = container_of(work, struct pool_workqueue,
						  release_work);
	struct workqueue_struct *wq = pwq->wq;
	struct worker_pool *pool = pwq->pool;
	bool is_last = false;

	 
	if (!list_empty(&pwq->pwqs_node)) {
		mutex_lock(&wq->mutex);
		list_del_rcu(&pwq->pwqs_node);
		is_last = list_empty(&wq->pwqs);
		mutex_unlock(&wq->mutex);
	}

	if (wq->flags & WQ_UNBOUND) {
		mutex_lock(&wq_pool_mutex);
		put_unbound_pool(pool);
		mutex_unlock(&wq_pool_mutex);
	}

	call_rcu(&pwq->rcu, rcu_free_pwq);

	 
	if (is_last) {
		wq_unregister_lockdep(wq);
		call_rcu(&wq->rcu, rcu_free_wq);
	}
}

 
static void pwq_adjust_max_active(struct pool_workqueue *pwq)
{
	struct workqueue_struct *wq = pwq->wq;
	bool freezable = wq->flags & WQ_FREEZABLE;
	unsigned long flags;

	 
	lockdep_assert_held(&wq->mutex);

	 
	if (!freezable && pwq->max_active == wq->saved_max_active)
		return;

	 
	raw_spin_lock_irqsave(&pwq->pool->lock, flags);

	 
	if (!freezable || !workqueue_freezing) {
		pwq->max_active = wq->saved_max_active;

		while (!list_empty(&pwq->inactive_works) &&
		       pwq->nr_active < pwq->max_active)
			pwq_activate_first_inactive(pwq);

		kick_pool(pwq->pool);
	} else {
		pwq->max_active = 0;
	}

	raw_spin_unlock_irqrestore(&pwq->pool->lock, flags);
}

 
static void init_pwq(struct pool_workqueue *pwq, struct workqueue_struct *wq,
		     struct worker_pool *pool)
{
	BUG_ON((unsigned long)pwq & WORK_STRUCT_FLAG_MASK);

	memset(pwq, 0, sizeof(*pwq));

	pwq->pool = pool;
	pwq->wq = wq;
	pwq->flush_color = -1;
	pwq->refcnt = 1;
	INIT_LIST_HEAD(&pwq->inactive_works);
	INIT_LIST_HEAD(&pwq->pwqs_node);
	INIT_LIST_HEAD(&pwq->mayday_node);
	kthread_init_work(&pwq->release_work, pwq_release_workfn);
}

 
static void link_pwq(struct pool_workqueue *pwq)
{
	struct workqueue_struct *wq = pwq->wq;

	lockdep_assert_held(&wq->mutex);

	 
	if (!list_empty(&pwq->pwqs_node))
		return;

	 
	pwq->work_color = wq->work_color;

	 
	pwq_adjust_max_active(pwq);

	 
	list_add_rcu(&pwq->pwqs_node, &wq->pwqs);
}

 
static struct pool_workqueue *alloc_unbound_pwq(struct workqueue_struct *wq,
					const struct workqueue_attrs *attrs)
{
	struct worker_pool *pool;
	struct pool_workqueue *pwq;

	lockdep_assert_held(&wq_pool_mutex);

	pool = get_unbound_pool(attrs);
	if (!pool)
		return NULL;

	pwq = kmem_cache_alloc_node(pwq_cache, GFP_KERNEL, pool->node);
	if (!pwq) {
		put_unbound_pool(pool);
		return NULL;
	}

	init_pwq(pwq, wq, pool);
	return pwq;
}

 
static void wq_calc_pod_cpumask(struct workqueue_attrs *attrs, int cpu,
				int cpu_going_down)
{
	const struct wq_pod_type *pt = wqattrs_pod_type(attrs);
	int pod = pt->cpu_pod[cpu];

	 
	cpumask_and(attrs->__pod_cpumask, pt->pod_cpus[pod], attrs->cpumask);
	cpumask_and(attrs->__pod_cpumask, attrs->__pod_cpumask, cpu_online_mask);
	if (cpu_going_down >= 0)
		cpumask_clear_cpu(cpu_going_down, attrs->__pod_cpumask);

	if (cpumask_empty(attrs->__pod_cpumask)) {
		cpumask_copy(attrs->__pod_cpumask, attrs->cpumask);
		return;
	}

	 
	cpumask_and(attrs->__pod_cpumask, attrs->cpumask, pt->pod_cpus[pod]);

	if (cpumask_empty(attrs->__pod_cpumask))
		pr_warn_once("WARNING: workqueue cpumask: online intersect > "
				"possible intersect\n");
}

 
static struct pool_workqueue *install_unbound_pwq(struct workqueue_struct *wq,
					int cpu, struct pool_workqueue *pwq)
{
	struct pool_workqueue *old_pwq;

	lockdep_assert_held(&wq_pool_mutex);
	lockdep_assert_held(&wq->mutex);

	 
	link_pwq(pwq);

	old_pwq = rcu_access_pointer(*per_cpu_ptr(wq->cpu_pwq, cpu));
	rcu_assign_pointer(*per_cpu_ptr(wq->cpu_pwq, cpu), pwq);
	return old_pwq;
}

 
struct apply_wqattrs_ctx {
	struct workqueue_struct	*wq;		 
	struct workqueue_attrs	*attrs;		 
	struct list_head	list;		 
	struct pool_workqueue	*dfl_pwq;
	struct pool_workqueue	*pwq_tbl[];
};

 
static void apply_wqattrs_cleanup(struct apply_wqattrs_ctx *ctx)
{
	if (ctx) {
		int cpu;

		for_each_possible_cpu(cpu)
			put_pwq_unlocked(ctx->pwq_tbl[cpu]);
		put_pwq_unlocked(ctx->dfl_pwq);

		free_workqueue_attrs(ctx->attrs);

		kfree(ctx);
	}
}

 
static struct apply_wqattrs_ctx *
apply_wqattrs_prepare(struct workqueue_struct *wq,
		      const struct workqueue_attrs *attrs,
		      const cpumask_var_t unbound_cpumask)
{
	struct apply_wqattrs_ctx *ctx;
	struct workqueue_attrs *new_attrs;
	int cpu;

	lockdep_assert_held(&wq_pool_mutex);

	if (WARN_ON(attrs->affn_scope < 0 ||
		    attrs->affn_scope >= WQ_AFFN_NR_TYPES))
		return ERR_PTR(-EINVAL);

	ctx = kzalloc(struct_size(ctx, pwq_tbl, nr_cpu_ids), GFP_KERNEL);

	new_attrs = alloc_workqueue_attrs();
	if (!ctx || !new_attrs)
		goto out_free;

	 
	copy_workqueue_attrs(new_attrs, attrs);
	wqattrs_actualize_cpumask(new_attrs, unbound_cpumask);
	cpumask_copy(new_attrs->__pod_cpumask, new_attrs->cpumask);
	ctx->dfl_pwq = alloc_unbound_pwq(wq, new_attrs);
	if (!ctx->dfl_pwq)
		goto out_free;

	for_each_possible_cpu(cpu) {
		if (new_attrs->ordered) {
			ctx->dfl_pwq->refcnt++;
			ctx->pwq_tbl[cpu] = ctx->dfl_pwq;
		} else {
			wq_calc_pod_cpumask(new_attrs, cpu, -1);
			ctx->pwq_tbl[cpu] = alloc_unbound_pwq(wq, new_attrs);
			if (!ctx->pwq_tbl[cpu])
				goto out_free;
		}
	}

	 
	copy_workqueue_attrs(new_attrs, attrs);
	cpumask_and(new_attrs->cpumask, new_attrs->cpumask, cpu_possible_mask);
	cpumask_copy(new_attrs->__pod_cpumask, new_attrs->cpumask);
	ctx->attrs = new_attrs;

	ctx->wq = wq;
	return ctx;

out_free:
	free_workqueue_attrs(new_attrs);
	apply_wqattrs_cleanup(ctx);
	return ERR_PTR(-ENOMEM);
}

 
static void apply_wqattrs_commit(struct apply_wqattrs_ctx *ctx)
{
	int cpu;

	 
	mutex_lock(&ctx->wq->mutex);

	copy_workqueue_attrs(ctx->wq->unbound_attrs, ctx->attrs);

	 
	for_each_possible_cpu(cpu)
		ctx->pwq_tbl[cpu] = install_unbound_pwq(ctx->wq, cpu,
							ctx->pwq_tbl[cpu]);

	 
	link_pwq(ctx->dfl_pwq);
	swap(ctx->wq->dfl_pwq, ctx->dfl_pwq);

	mutex_unlock(&ctx->wq->mutex);
}

static void apply_wqattrs_lock(void)
{
	 
	cpus_read_lock();
	mutex_lock(&wq_pool_mutex);
}

static void apply_wqattrs_unlock(void)
{
	mutex_unlock(&wq_pool_mutex);
	cpus_read_unlock();
}

static int apply_workqueue_attrs_locked(struct workqueue_struct *wq,
					const struct workqueue_attrs *attrs)
{
	struct apply_wqattrs_ctx *ctx;

	 
	if (WARN_ON(!(wq->flags & WQ_UNBOUND)))
		return -EINVAL;

	 
	if (!list_empty(&wq->pwqs)) {
		if (WARN_ON(wq->flags & __WQ_ORDERED_EXPLICIT))
			return -EINVAL;

		wq->flags &= ~__WQ_ORDERED;
	}

	ctx = apply_wqattrs_prepare(wq, attrs, wq_unbound_cpumask);
	if (IS_ERR(ctx))
		return PTR_ERR(ctx);

	 
	apply_wqattrs_commit(ctx);
	apply_wqattrs_cleanup(ctx);

	return 0;
}

 
int apply_workqueue_attrs(struct workqueue_struct *wq,
			  const struct workqueue_attrs *attrs)
{
	int ret;

	lockdep_assert_cpus_held();

	mutex_lock(&wq_pool_mutex);
	ret = apply_workqueue_attrs_locked(wq, attrs);
	mutex_unlock(&wq_pool_mutex);

	return ret;
}

 
static void wq_update_pod(struct workqueue_struct *wq, int cpu,
			  int hotplug_cpu, bool online)
{
	int off_cpu = online ? -1 : hotplug_cpu;
	struct pool_workqueue *old_pwq = NULL, *pwq;
	struct workqueue_attrs *target_attrs;

	lockdep_assert_held(&wq_pool_mutex);

	if (!(wq->flags & WQ_UNBOUND) || wq->unbound_attrs->ordered)
		return;

	 
	target_attrs = wq_update_pod_attrs_buf;

	copy_workqueue_attrs(target_attrs, wq->unbound_attrs);
	wqattrs_actualize_cpumask(target_attrs, wq_unbound_cpumask);

	 
	wq_calc_pod_cpumask(target_attrs, cpu, off_cpu);
	pwq = rcu_dereference_protected(*per_cpu_ptr(wq->cpu_pwq, cpu),
					lockdep_is_held(&wq_pool_mutex));
	if (wqattrs_equal(target_attrs, pwq->pool->attrs))
		return;

	 
	pwq = alloc_unbound_pwq(wq, target_attrs);
	if (!pwq) {
		pr_warn("workqueue: allocation failed while updating CPU pod affinity of \"%s\"\n",
			wq->name);
		goto use_dfl_pwq;
	}

	 
	mutex_lock(&wq->mutex);
	old_pwq = install_unbound_pwq(wq, cpu, pwq);
	goto out_unlock;

use_dfl_pwq:
	mutex_lock(&wq->mutex);
	raw_spin_lock_irq(&wq->dfl_pwq->pool->lock);
	get_pwq(wq->dfl_pwq);
	raw_spin_unlock_irq(&wq->dfl_pwq->pool->lock);
	old_pwq = install_unbound_pwq(wq, cpu, wq->dfl_pwq);
out_unlock:
	mutex_unlock(&wq->mutex);
	put_pwq_unlocked(old_pwq);
}

static int alloc_and_link_pwqs(struct workqueue_struct *wq)
{
	bool highpri = wq->flags & WQ_HIGHPRI;
	int cpu, ret;

	wq->cpu_pwq = alloc_percpu(struct pool_workqueue *);
	if (!wq->cpu_pwq)
		goto enomem;

	if (!(wq->flags & WQ_UNBOUND)) {
		for_each_possible_cpu(cpu) {
			struct pool_workqueue **pwq_p =
				per_cpu_ptr(wq->cpu_pwq, cpu);
			struct worker_pool *pool =
				&(per_cpu_ptr(cpu_worker_pools, cpu)[highpri]);

			*pwq_p = kmem_cache_alloc_node(pwq_cache, GFP_KERNEL,
						       pool->node);
			if (!*pwq_p)
				goto enomem;

			init_pwq(*pwq_p, wq, pool);

			mutex_lock(&wq->mutex);
			link_pwq(*pwq_p);
			mutex_unlock(&wq->mutex);
		}
		return 0;
	}

	cpus_read_lock();
	if (wq->flags & __WQ_ORDERED) {
		ret = apply_workqueue_attrs(wq, ordered_wq_attrs[highpri]);
		 
		WARN(!ret && (wq->pwqs.next != &wq->dfl_pwq->pwqs_node ||
			      wq->pwqs.prev != &wq->dfl_pwq->pwqs_node),
		     "ordering guarantee broken for workqueue %s\n", wq->name);
	} else {
		ret = apply_workqueue_attrs(wq, unbound_std_wq_attrs[highpri]);
	}
	cpus_read_unlock();

	 
	if (ret)
		kthread_flush_worker(pwq_release_worker);

	return ret;

enomem:
	if (wq->cpu_pwq) {
		for_each_possible_cpu(cpu) {
			struct pool_workqueue *pwq = *per_cpu_ptr(wq->cpu_pwq, cpu);

			if (pwq)
				kmem_cache_free(pwq_cache, pwq);
		}
		free_percpu(wq->cpu_pwq);
		wq->cpu_pwq = NULL;
	}
	return -ENOMEM;
}

static int wq_clamp_max_active(int max_active, unsigned int flags,
			       const char *name)
{
	if (max_active < 1 || max_active > WQ_MAX_ACTIVE)
		pr_warn("workqueue: max_active %d requested for %s is out of range, clamping between %d and %d\n",
			max_active, name, 1, WQ_MAX_ACTIVE);

	return clamp_val(max_active, 1, WQ_MAX_ACTIVE);
}

 
static int init_rescuer(struct workqueue_struct *wq)
{
	struct worker *rescuer;
	int ret;

	if (!(wq->flags & WQ_MEM_RECLAIM))
		return 0;

	rescuer = alloc_worker(NUMA_NO_NODE);
	if (!rescuer) {
		pr_err("workqueue: Failed to allocate a rescuer for wq \"%s\"\n",
		       wq->name);
		return -ENOMEM;
	}

	rescuer->rescue_wq = wq;
	rescuer->task = kthread_create(rescuer_thread, rescuer, "kworker/R-%s", wq->name);
	if (IS_ERR(rescuer->task)) {
		ret = PTR_ERR(rescuer->task);
		pr_err("workqueue: Failed to create a rescuer kthread for wq \"%s\": %pe",
		       wq->name, ERR_PTR(ret));
		kfree(rescuer);
		return ret;
	}

	wq->rescuer = rescuer;
	kthread_bind_mask(rescuer->task, cpu_possible_mask);
	wake_up_process(rescuer->task);

	return 0;
}

__printf(1, 4)
struct workqueue_struct *alloc_workqueue(const char *fmt,
					 unsigned int flags,
					 int max_active, ...)
{
	va_list args;
	struct workqueue_struct *wq;
	struct pool_workqueue *pwq;

	 
	if ((flags & WQ_UNBOUND) && max_active == 1)
		flags |= __WQ_ORDERED;

	 
	if ((flags & WQ_POWER_EFFICIENT) && wq_power_efficient)
		flags |= WQ_UNBOUND;

	 
	wq = kzalloc(sizeof(*wq), GFP_KERNEL);
	if (!wq)
		return NULL;

	if (flags & WQ_UNBOUND) {
		wq->unbound_attrs = alloc_workqueue_attrs();
		if (!wq->unbound_attrs)
			goto err_free_wq;
	}

	va_start(args, max_active);
	vsnprintf(wq->name, sizeof(wq->name), fmt, args);
	va_end(args);

	max_active = max_active ?: WQ_DFL_ACTIVE;
	max_active = wq_clamp_max_active(max_active, flags, wq->name);

	 
	wq->flags = flags;
	wq->saved_max_active = max_active;
	mutex_init(&wq->mutex);
	atomic_set(&wq->nr_pwqs_to_flush, 0);
	INIT_LIST_HEAD(&wq->pwqs);
	INIT_LIST_HEAD(&wq->flusher_queue);
	INIT_LIST_HEAD(&wq->flusher_overflow);
	INIT_LIST_HEAD(&wq->maydays);

	wq_init_lockdep(wq);
	INIT_LIST_HEAD(&wq->list);

	if (alloc_and_link_pwqs(wq) < 0)
		goto err_unreg_lockdep;

	if (wq_online && init_rescuer(wq) < 0)
		goto err_destroy;

	if ((wq->flags & WQ_SYSFS) && workqueue_sysfs_register(wq))
		goto err_destroy;

	 
	mutex_lock(&wq_pool_mutex);

	mutex_lock(&wq->mutex);
	for_each_pwq(pwq, wq)
		pwq_adjust_max_active(pwq);
	mutex_unlock(&wq->mutex);

	list_add_tail_rcu(&wq->list, &workqueues);

	mutex_unlock(&wq_pool_mutex);

	return wq;

err_unreg_lockdep:
	wq_unregister_lockdep(wq);
	wq_free_lockdep(wq);
err_free_wq:
	free_workqueue_attrs(wq->unbound_attrs);
	kfree(wq);
	return NULL;
err_destroy:
	destroy_workqueue(wq);
	return NULL;
}
EXPORT_SYMBOL_GPL(alloc_workqueue);

static bool pwq_busy(struct pool_workqueue *pwq)
{
	int i;

	for (i = 0; i < WORK_NR_COLORS; i++)
		if (pwq->nr_in_flight[i])
			return true;

	if ((pwq != pwq->wq->dfl_pwq) && (pwq->refcnt > 1))
		return true;
	if (pwq->nr_active || !list_empty(&pwq->inactive_works))
		return true;

	return false;
}

 
void destroy_workqueue(struct workqueue_struct *wq)
{
	struct pool_workqueue *pwq;
	int cpu;

	 
	workqueue_sysfs_unregister(wq);

	 
	mutex_lock(&wq->mutex);
	wq->flags |= __WQ_DESTROYING;
	mutex_unlock(&wq->mutex);

	 
	drain_workqueue(wq);

	 
	if (wq->rescuer) {
		struct worker *rescuer = wq->rescuer;

		 
		raw_spin_lock_irq(&wq_mayday_lock);
		wq->rescuer = NULL;
		raw_spin_unlock_irq(&wq_mayday_lock);

		 
		kthread_stop(rescuer->task);
		kfree(rescuer);
	}

	 
	mutex_lock(&wq_pool_mutex);
	mutex_lock(&wq->mutex);
	for_each_pwq(pwq, wq) {
		raw_spin_lock_irq(&pwq->pool->lock);
		if (WARN_ON(pwq_busy(pwq))) {
			pr_warn("%s: %s has the following busy pwq\n",
				__func__, wq->name);
			show_pwq(pwq);
			raw_spin_unlock_irq(&pwq->pool->lock);
			mutex_unlock(&wq->mutex);
			mutex_unlock(&wq_pool_mutex);
			show_one_workqueue(wq);
			return;
		}
		raw_spin_unlock_irq(&pwq->pool->lock);
	}
	mutex_unlock(&wq->mutex);

	 
	list_del_rcu(&wq->list);
	mutex_unlock(&wq_pool_mutex);

	 
	rcu_read_lock();

	for_each_possible_cpu(cpu) {
		pwq = rcu_access_pointer(*per_cpu_ptr(wq->cpu_pwq, cpu));
		RCU_INIT_POINTER(*per_cpu_ptr(wq->cpu_pwq, cpu), NULL);
		put_pwq_unlocked(pwq);
	}

	put_pwq_unlocked(wq->dfl_pwq);
	wq->dfl_pwq = NULL;

	rcu_read_unlock();
}
EXPORT_SYMBOL_GPL(destroy_workqueue);

 
void workqueue_set_max_active(struct workqueue_struct *wq, int max_active)
{
	struct pool_workqueue *pwq;

	 
	if (WARN_ON(wq->flags & __WQ_ORDERED_EXPLICIT))
		return;

	max_active = wq_clamp_max_active(max_active, wq->flags, wq->name);

	mutex_lock(&wq->mutex);

	wq->flags &= ~__WQ_ORDERED;
	wq->saved_max_active = max_active;

	for_each_pwq(pwq, wq)
		pwq_adjust_max_active(pwq);

	mutex_unlock(&wq->mutex);
}
EXPORT_SYMBOL_GPL(workqueue_set_max_active);

 
struct work_struct *current_work(void)
{
	struct worker *worker = current_wq_worker();

	return worker ? worker->current_work : NULL;
}
EXPORT_SYMBOL(current_work);

 
bool current_is_workqueue_rescuer(void)
{
	struct worker *worker = current_wq_worker();

	return worker && worker->rescue_wq;
}

 
bool workqueue_congested(int cpu, struct workqueue_struct *wq)
{
	struct pool_workqueue *pwq;
	bool ret;

	rcu_read_lock();
	preempt_disable();

	if (cpu == WORK_CPU_UNBOUND)
		cpu = smp_processor_id();

	pwq = *per_cpu_ptr(wq->cpu_pwq, cpu);
	ret = !list_empty(&pwq->inactive_works);

	preempt_enable();
	rcu_read_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(workqueue_congested);

 
unsigned int work_busy(struct work_struct *work)
{
	struct worker_pool *pool;
	unsigned long flags;
	unsigned int ret = 0;

	if (work_pending(work))
		ret |= WORK_BUSY_PENDING;

	rcu_read_lock();
	pool = get_work_pool(work);
	if (pool) {
		raw_spin_lock_irqsave(&pool->lock, flags);
		if (find_worker_executing_work(pool, work))
			ret |= WORK_BUSY_RUNNING;
		raw_spin_unlock_irqrestore(&pool->lock, flags);
	}
	rcu_read_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(work_busy);

 
void set_worker_desc(const char *fmt, ...)
{
	struct worker *worker = current_wq_worker();
	va_list args;

	if (worker) {
		va_start(args, fmt);
		vsnprintf(worker->desc, sizeof(worker->desc), fmt, args);
		va_end(args);
	}
}
EXPORT_SYMBOL_GPL(set_worker_desc);

 
void print_worker_info(const char *log_lvl, struct task_struct *task)
{
	work_func_t *fn = NULL;
	char name[WQ_NAME_LEN] = { };
	char desc[WORKER_DESC_LEN] = { };
	struct pool_workqueue *pwq = NULL;
	struct workqueue_struct *wq = NULL;
	struct worker *worker;

	if (!(task->flags & PF_WQ_WORKER))
		return;

	 
	worker = kthread_probe_data(task);

	 
	copy_from_kernel_nofault(&fn, &worker->current_func, sizeof(fn));
	copy_from_kernel_nofault(&pwq, &worker->current_pwq, sizeof(pwq));
	copy_from_kernel_nofault(&wq, &pwq->wq, sizeof(wq));
	copy_from_kernel_nofault(name, wq->name, sizeof(name) - 1);
	copy_from_kernel_nofault(desc, worker->desc, sizeof(desc) - 1);

	if (fn || name[0] || desc[0]) {
		printk("%sWorkqueue: %s %ps", log_lvl, name, fn);
		if (strcmp(name, desc))
			pr_cont(" (%s)", desc);
		pr_cont("\n");
	}
}

static void pr_cont_pool_info(struct worker_pool *pool)
{
	pr_cont(" cpus=%*pbl", nr_cpumask_bits, pool->attrs->cpumask);
	if (pool->node != NUMA_NO_NODE)
		pr_cont(" node=%d", pool->node);
	pr_cont(" flags=0x%x nice=%d", pool->flags, pool->attrs->nice);
}

struct pr_cont_work_struct {
	bool comma;
	work_func_t func;
	long ctr;
};

static void pr_cont_work_flush(bool comma, work_func_t func, struct pr_cont_work_struct *pcwsp)
{
	if (!pcwsp->ctr)
		goto out_record;
	if (func == pcwsp->func) {
		pcwsp->ctr++;
		return;
	}
	if (pcwsp->ctr == 1)
		pr_cont("%s %ps", pcwsp->comma ? "," : "", pcwsp->func);
	else
		pr_cont("%s %ld*%ps", pcwsp->comma ? "," : "", pcwsp->ctr, pcwsp->func);
	pcwsp->ctr = 0;
out_record:
	if ((long)func == -1L)
		return;
	pcwsp->comma = comma;
	pcwsp->func = func;
	pcwsp->ctr = 1;
}

static void pr_cont_work(bool comma, struct work_struct *work, struct pr_cont_work_struct *pcwsp)
{
	if (work->func == wq_barrier_func) {
		struct wq_barrier *barr;

		barr = container_of(work, struct wq_barrier, work);

		pr_cont_work_flush(comma, (work_func_t)-1, pcwsp);
		pr_cont("%s BAR(%d)", comma ? "," : "",
			task_pid_nr(barr->task));
	} else {
		if (!comma)
			pr_cont_work_flush(comma, (work_func_t)-1, pcwsp);
		pr_cont_work_flush(comma, work->func, pcwsp);
	}
}

static void show_pwq(struct pool_workqueue *pwq)
{
	struct pr_cont_work_struct pcws = { .ctr = 0, };
	struct worker_pool *pool = pwq->pool;
	struct work_struct *work;
	struct worker *worker;
	bool has_in_flight = false, has_pending = false;
	int bkt;

	pr_info("  pwq %d:", pool->id);
	pr_cont_pool_info(pool);

	pr_cont(" active=%d/%d refcnt=%d%s\n",
		pwq->nr_active, pwq->max_active, pwq->refcnt,
		!list_empty(&pwq->mayday_node) ? " MAYDAY" : "");

	hash_for_each(pool->busy_hash, bkt, worker, hentry) {
		if (worker->current_pwq == pwq) {
			has_in_flight = true;
			break;
		}
	}
	if (has_in_flight) {
		bool comma = false;

		pr_info("    in-flight:");
		hash_for_each(pool->busy_hash, bkt, worker, hentry) {
			if (worker->current_pwq != pwq)
				continue;

			pr_cont("%s %d%s:%ps", comma ? "," : "",
				task_pid_nr(worker->task),
				worker->rescue_wq ? "(RESCUER)" : "",
				worker->current_func);
			list_for_each_entry(work, &worker->scheduled, entry)
				pr_cont_work(false, work, &pcws);
			pr_cont_work_flush(comma, (work_func_t)-1L, &pcws);
			comma = true;
		}
		pr_cont("\n");
	}

	list_for_each_entry(work, &pool->worklist, entry) {
		if (get_work_pwq(work) == pwq) {
			has_pending = true;
			break;
		}
	}
	if (has_pending) {
		bool comma = false;

		pr_info("    pending:");
		list_for_each_entry(work, &pool->worklist, entry) {
			if (get_work_pwq(work) != pwq)
				continue;

			pr_cont_work(comma, work, &pcws);
			comma = !(*work_data_bits(work) & WORK_STRUCT_LINKED);
		}
		pr_cont_work_flush(comma, (work_func_t)-1L, &pcws);
		pr_cont("\n");
	}

	if (!list_empty(&pwq->inactive_works)) {
		bool comma = false;

		pr_info("    inactive:");
		list_for_each_entry(work, &pwq->inactive_works, entry) {
			pr_cont_work(comma, work, &pcws);
			comma = !(*work_data_bits(work) & WORK_STRUCT_LINKED);
		}
		pr_cont_work_flush(comma, (work_func_t)-1L, &pcws);
		pr_cont("\n");
	}
}

 
void show_one_workqueue(struct workqueue_struct *wq)
{
	struct pool_workqueue *pwq;
	bool idle = true;
	unsigned long flags;

	for_each_pwq(pwq, wq) {
		if (pwq->nr_active || !list_empty(&pwq->inactive_works)) {
			idle = false;
			break;
		}
	}
	if (idle)  
		return;

	pr_info("workqueue %s: flags=0x%x\n", wq->name, wq->flags);

	for_each_pwq(pwq, wq) {
		raw_spin_lock_irqsave(&pwq->pool->lock, flags);
		if (pwq->nr_active || !list_empty(&pwq->inactive_works)) {
			 
			printk_deferred_enter();
			show_pwq(pwq);
			printk_deferred_exit();
		}
		raw_spin_unlock_irqrestore(&pwq->pool->lock, flags);
		 
		touch_nmi_watchdog();
	}

}

 
static void show_one_worker_pool(struct worker_pool *pool)
{
	struct worker *worker;
	bool first = true;
	unsigned long flags;
	unsigned long hung = 0;

	raw_spin_lock_irqsave(&pool->lock, flags);
	if (pool->nr_workers == pool->nr_idle)
		goto next_pool;

	 
	if (!list_empty(&pool->worklist))
		hung = jiffies_to_msecs(jiffies - pool->watchdog_ts) / 1000;

	 
	printk_deferred_enter();
	pr_info("pool %d:", pool->id);
	pr_cont_pool_info(pool);
	pr_cont(" hung=%lus workers=%d", hung, pool->nr_workers);
	if (pool->manager)
		pr_cont(" manager: %d",
			task_pid_nr(pool->manager->task));
	list_for_each_entry(worker, &pool->idle_list, entry) {
		pr_cont(" %s%d", first ? "idle: " : "",
			task_pid_nr(worker->task));
		first = false;
	}
	pr_cont("\n");
	printk_deferred_exit();
next_pool:
	raw_spin_unlock_irqrestore(&pool->lock, flags);
	 
	touch_nmi_watchdog();

}

 
void show_all_workqueues(void)
{
	struct workqueue_struct *wq;
	struct worker_pool *pool;
	int pi;

	rcu_read_lock();

	pr_info("Showing busy workqueues and worker pools:\n");

	list_for_each_entry_rcu(wq, &workqueues, list)
		show_one_workqueue(wq);

	for_each_pool(pool, pi)
		show_one_worker_pool(pool);

	rcu_read_unlock();
}

 
void show_freezable_workqueues(void)
{
	struct workqueue_struct *wq;

	rcu_read_lock();

	pr_info("Showing freezable workqueues that are still busy:\n");

	list_for_each_entry_rcu(wq, &workqueues, list) {
		if (!(wq->flags & WQ_FREEZABLE))
			continue;
		show_one_workqueue(wq);
	}

	rcu_read_unlock();
}

 
void wq_worker_comm(char *buf, size_t size, struct task_struct *task)
{
	int off;

	 
	off = strscpy(buf, task->comm, size);
	if (off < 0)
		return;

	 
	mutex_lock(&wq_pool_attach_mutex);

	if (task->flags & PF_WQ_WORKER) {
		struct worker *worker = kthread_data(task);
		struct worker_pool *pool = worker->pool;

		if (pool) {
			raw_spin_lock_irq(&pool->lock);
			 
			if (worker->desc[0] != '\0') {
				if (worker->current_work)
					scnprintf(buf + off, size - off, "+%s",
						  worker->desc);
				else
					scnprintf(buf + off, size - off, "-%s",
						  worker->desc);
			}
			raw_spin_unlock_irq(&pool->lock);
		}
	}

	mutex_unlock(&wq_pool_attach_mutex);
}

#ifdef CONFIG_SMP

 

static void unbind_workers(int cpu)
{
	struct worker_pool *pool;
	struct worker *worker;

	for_each_cpu_worker_pool(pool, cpu) {
		mutex_lock(&wq_pool_attach_mutex);
		raw_spin_lock_irq(&pool->lock);

		 
		for_each_pool_worker(worker, pool)
			worker->flags |= WORKER_UNBOUND;

		pool->flags |= POOL_DISASSOCIATED;

		 
		pool->nr_running = 0;

		 
		kick_pool(pool);

		raw_spin_unlock_irq(&pool->lock);

		for_each_pool_worker(worker, pool)
			unbind_worker(worker);

		mutex_unlock(&wq_pool_attach_mutex);
	}
}

 
static void rebind_workers(struct worker_pool *pool)
{
	struct worker *worker;

	lockdep_assert_held(&wq_pool_attach_mutex);

	 
	for_each_pool_worker(worker, pool) {
		kthread_set_per_cpu(worker->task, pool->cpu);
		WARN_ON_ONCE(set_cpus_allowed_ptr(worker->task,
						  pool_allowed_cpus(pool)) < 0);
	}

	raw_spin_lock_irq(&pool->lock);

	pool->flags &= ~POOL_DISASSOCIATED;

	for_each_pool_worker(worker, pool) {
		unsigned int worker_flags = worker->flags;

		 
		WARN_ON_ONCE(!(worker_flags & WORKER_UNBOUND));
		worker_flags |= WORKER_REBOUND;
		worker_flags &= ~WORKER_UNBOUND;
		WRITE_ONCE(worker->flags, worker_flags);
	}

	raw_spin_unlock_irq(&pool->lock);
}

 
static void restore_unbound_workers_cpumask(struct worker_pool *pool, int cpu)
{
	static cpumask_t cpumask;
	struct worker *worker;

	lockdep_assert_held(&wq_pool_attach_mutex);

	 
	if (!cpumask_test_cpu(cpu, pool->attrs->cpumask))
		return;

	cpumask_and(&cpumask, pool->attrs->cpumask, cpu_online_mask);

	 
	for_each_pool_worker(worker, pool)
		WARN_ON_ONCE(set_cpus_allowed_ptr(worker->task, &cpumask) < 0);
}

int workqueue_prepare_cpu(unsigned int cpu)
{
	struct worker_pool *pool;

	for_each_cpu_worker_pool(pool, cpu) {
		if (pool->nr_workers)
			continue;
		if (!create_worker(pool))
			return -ENOMEM;
	}
	return 0;
}

int workqueue_online_cpu(unsigned int cpu)
{
	struct worker_pool *pool;
	struct workqueue_struct *wq;
	int pi;

	mutex_lock(&wq_pool_mutex);

	for_each_pool(pool, pi) {
		mutex_lock(&wq_pool_attach_mutex);

		if (pool->cpu == cpu)
			rebind_workers(pool);
		else if (pool->cpu < 0)
			restore_unbound_workers_cpumask(pool, cpu);

		mutex_unlock(&wq_pool_attach_mutex);
	}

	 
	list_for_each_entry(wq, &workqueues, list) {
		struct workqueue_attrs *attrs = wq->unbound_attrs;

		if (attrs) {
			const struct wq_pod_type *pt = wqattrs_pod_type(attrs);
			int tcpu;

			for_each_cpu(tcpu, pt->pod_cpus[pt->cpu_pod[cpu]])
				wq_update_pod(wq, tcpu, cpu, true);
		}
	}

	mutex_unlock(&wq_pool_mutex);
	return 0;
}

int workqueue_offline_cpu(unsigned int cpu)
{
	struct workqueue_struct *wq;

	 
	if (WARN_ON(cpu != smp_processor_id()))
		return -1;

	unbind_workers(cpu);

	 
	mutex_lock(&wq_pool_mutex);
	list_for_each_entry(wq, &workqueues, list) {
		struct workqueue_attrs *attrs = wq->unbound_attrs;

		if (attrs) {
			const struct wq_pod_type *pt = wqattrs_pod_type(attrs);
			int tcpu;

			for_each_cpu(tcpu, pt->pod_cpus[pt->cpu_pod[cpu]])
				wq_update_pod(wq, tcpu, cpu, false);
		}
	}
	mutex_unlock(&wq_pool_mutex);

	return 0;
}

struct work_for_cpu {
	struct work_struct work;
	long (*fn)(void *);
	void *arg;
	long ret;
};

static void work_for_cpu_fn(struct work_struct *work)
{
	struct work_for_cpu *wfc = container_of(work, struct work_for_cpu, work);

	wfc->ret = wfc->fn(wfc->arg);
}

 
long work_on_cpu_key(int cpu, long (*fn)(void *),
		     void *arg, struct lock_class_key *key)
{
	struct work_for_cpu wfc = { .fn = fn, .arg = arg };

	INIT_WORK_ONSTACK_KEY(&wfc.work, work_for_cpu_fn, key);
	schedule_work_on(cpu, &wfc.work);
	flush_work(&wfc.work);
	destroy_work_on_stack(&wfc.work);
	return wfc.ret;
}
EXPORT_SYMBOL_GPL(work_on_cpu_key);

 
long work_on_cpu_safe_key(int cpu, long (*fn)(void *),
			  void *arg, struct lock_class_key *key)
{
	long ret = -ENODEV;

	cpus_read_lock();
	if (cpu_online(cpu))
		ret = work_on_cpu_key(cpu, fn, arg, key);
	cpus_read_unlock();
	return ret;
}
EXPORT_SYMBOL_GPL(work_on_cpu_safe_key);
#endif  

#ifdef CONFIG_FREEZER

 
void freeze_workqueues_begin(void)
{
	struct workqueue_struct *wq;
	struct pool_workqueue *pwq;

	mutex_lock(&wq_pool_mutex);

	WARN_ON_ONCE(workqueue_freezing);
	workqueue_freezing = true;

	list_for_each_entry(wq, &workqueues, list) {
		mutex_lock(&wq->mutex);
		for_each_pwq(pwq, wq)
			pwq_adjust_max_active(pwq);
		mutex_unlock(&wq->mutex);
	}

	mutex_unlock(&wq_pool_mutex);
}

 
bool freeze_workqueues_busy(void)
{
	bool busy = false;
	struct workqueue_struct *wq;
	struct pool_workqueue *pwq;

	mutex_lock(&wq_pool_mutex);

	WARN_ON_ONCE(!workqueue_freezing);

	list_for_each_entry(wq, &workqueues, list) {
		if (!(wq->flags & WQ_FREEZABLE))
			continue;
		 
		rcu_read_lock();
		for_each_pwq(pwq, wq) {
			WARN_ON_ONCE(pwq->nr_active < 0);
			if (pwq->nr_active) {
				busy = true;
				rcu_read_unlock();
				goto out_unlock;
			}
		}
		rcu_read_unlock();
	}
out_unlock:
	mutex_unlock(&wq_pool_mutex);
	return busy;
}

 
void thaw_workqueues(void)
{
	struct workqueue_struct *wq;
	struct pool_workqueue *pwq;

	mutex_lock(&wq_pool_mutex);

	if (!workqueue_freezing)
		goto out_unlock;

	workqueue_freezing = false;

	 
	list_for_each_entry(wq, &workqueues, list) {
		mutex_lock(&wq->mutex);
		for_each_pwq(pwq, wq)
			pwq_adjust_max_active(pwq);
		mutex_unlock(&wq->mutex);
	}

out_unlock:
	mutex_unlock(&wq_pool_mutex);
}
#endif  

static int workqueue_apply_unbound_cpumask(const cpumask_var_t unbound_cpumask)
{
	LIST_HEAD(ctxs);
	int ret = 0;
	struct workqueue_struct *wq;
	struct apply_wqattrs_ctx *ctx, *n;

	lockdep_assert_held(&wq_pool_mutex);

	list_for_each_entry(wq, &workqueues, list) {
		if (!(wq->flags & WQ_UNBOUND))
			continue;

		 
		if (!list_empty(&wq->pwqs)) {
			if (wq->flags & __WQ_ORDERED_EXPLICIT)
				continue;
			wq->flags &= ~__WQ_ORDERED;
		}

		ctx = apply_wqattrs_prepare(wq, wq->unbound_attrs, unbound_cpumask);
		if (IS_ERR(ctx)) {
			ret = PTR_ERR(ctx);
			break;
		}

		list_add_tail(&ctx->list, &ctxs);
	}

	list_for_each_entry_safe(ctx, n, &ctxs, list) {
		if (!ret)
			apply_wqattrs_commit(ctx);
		apply_wqattrs_cleanup(ctx);
	}

	if (!ret) {
		mutex_lock(&wq_pool_attach_mutex);
		cpumask_copy(wq_unbound_cpumask, unbound_cpumask);
		mutex_unlock(&wq_pool_attach_mutex);
	}
	return ret;
}

 
int workqueue_set_unbound_cpumask(cpumask_var_t cpumask)
{
	int ret = -EINVAL;

	 
	cpumask_and(cpumask, cpumask, cpu_possible_mask);
	if (!cpumask_empty(cpumask)) {
		apply_wqattrs_lock();
		if (cpumask_equal(cpumask, wq_unbound_cpumask)) {
			ret = 0;
			goto out_unlock;
		}

		ret = workqueue_apply_unbound_cpumask(cpumask);

out_unlock:
		apply_wqattrs_unlock();
	}

	return ret;
}

static int parse_affn_scope(const char *val)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(wq_affn_names); i++) {
		if (!strncasecmp(val, wq_affn_names[i], strlen(wq_affn_names[i])))
			return i;
	}
	return -EINVAL;
}

static int wq_affn_dfl_set(const char *val, const struct kernel_param *kp)
{
	struct workqueue_struct *wq;
	int affn, cpu;

	affn = parse_affn_scope(val);
	if (affn < 0)
		return affn;
	if (affn == WQ_AFFN_DFL)
		return -EINVAL;

	cpus_read_lock();
	mutex_lock(&wq_pool_mutex);

	wq_affn_dfl = affn;

	list_for_each_entry(wq, &workqueues, list) {
		for_each_online_cpu(cpu) {
			wq_update_pod(wq, cpu, cpu, true);
		}
	}

	mutex_unlock(&wq_pool_mutex);
	cpus_read_unlock();

	return 0;
}

static int wq_affn_dfl_get(char *buffer, const struct kernel_param *kp)
{
	return scnprintf(buffer, PAGE_SIZE, "%s\n", wq_affn_names[wq_affn_dfl]);
}

static const struct kernel_param_ops wq_affn_dfl_ops = {
	.set	= wq_affn_dfl_set,
	.get	= wq_affn_dfl_get,
};

module_param_cb(default_affinity_scope, &wq_affn_dfl_ops, NULL, 0644);

#ifdef CONFIG_SYSFS
 
struct wq_device {
	struct workqueue_struct		*wq;
	struct device			dev;
};

static struct workqueue_struct *dev_to_wq(struct device *dev)
{
	struct wq_device *wq_dev = container_of(dev, struct wq_device, dev);

	return wq_dev->wq;
}

static ssize_t per_cpu_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct workqueue_struct *wq = dev_to_wq(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", (bool)!(wq->flags & WQ_UNBOUND));
}
static DEVICE_ATTR_RO(per_cpu);

static ssize_t max_active_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct workqueue_struct *wq = dev_to_wq(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", wq->saved_max_active);
}

static ssize_t max_active_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct workqueue_struct *wq = dev_to_wq(dev);
	int val;

	if (sscanf(buf, "%d", &val) != 1 || val <= 0)
		return -EINVAL;

	workqueue_set_max_active(wq, val);
	return count;
}
static DEVICE_ATTR_RW(max_active);

static struct attribute *wq_sysfs_attrs[] = {
	&dev_attr_per_cpu.attr,
	&dev_attr_max_active.attr,
	NULL,
};
ATTRIBUTE_GROUPS(wq_sysfs);

static ssize_t wq_nice_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct workqueue_struct *wq = dev_to_wq(dev);
	int written;

	mutex_lock(&wq->mutex);
	written = scnprintf(buf, PAGE_SIZE, "%d\n", wq->unbound_attrs->nice);
	mutex_unlock(&wq->mutex);

	return written;
}

 
static struct workqueue_attrs *wq_sysfs_prep_attrs(struct workqueue_struct *wq)
{
	struct workqueue_attrs *attrs;

	lockdep_assert_held(&wq_pool_mutex);

	attrs = alloc_workqueue_attrs();
	if (!attrs)
		return NULL;

	copy_workqueue_attrs(attrs, wq->unbound_attrs);
	return attrs;
}

static ssize_t wq_nice_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t count)
{
	struct workqueue_struct *wq = dev_to_wq(dev);
	struct workqueue_attrs *attrs;
	int ret = -ENOMEM;

	apply_wqattrs_lock();

	attrs = wq_sysfs_prep_attrs(wq);
	if (!attrs)
		goto out_unlock;

	if (sscanf(buf, "%d", &attrs->nice) == 1 &&
	    attrs->nice >= MIN_NICE && attrs->nice <= MAX_NICE)
		ret = apply_workqueue_attrs_locked(wq, attrs);
	else
		ret = -EINVAL;

out_unlock:
	apply_wqattrs_unlock();
	free_workqueue_attrs(attrs);
	return ret ?: count;
}

static ssize_t wq_cpumask_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct workqueue_struct *wq = dev_to_wq(dev);
	int written;

	mutex_lock(&wq->mutex);
	written = scnprintf(buf, PAGE_SIZE, "%*pb\n",
			    cpumask_pr_args(wq->unbound_attrs->cpumask));
	mutex_unlock(&wq->mutex);
	return written;
}

static ssize_t wq_cpumask_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct workqueue_struct *wq = dev_to_wq(dev);
	struct workqueue_attrs *attrs;
	int ret = -ENOMEM;

	apply_wqattrs_lock();

	attrs = wq_sysfs_prep_attrs(wq);
	if (!attrs)
		goto out_unlock;

	ret = cpumask_parse(buf, attrs->cpumask);
	if (!ret)
		ret = apply_workqueue_attrs_locked(wq, attrs);

out_unlock:
	apply_wqattrs_unlock();
	free_workqueue_attrs(attrs);
	return ret ?: count;
}

static ssize_t wq_affn_scope_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct workqueue_struct *wq = dev_to_wq(dev);
	int written;

	mutex_lock(&wq->mutex);
	if (wq->unbound_attrs->affn_scope == WQ_AFFN_DFL)
		written = scnprintf(buf, PAGE_SIZE, "%s (%s)\n",
				    wq_affn_names[WQ_AFFN_DFL],
				    wq_affn_names[wq_affn_dfl]);
	else
		written = scnprintf(buf, PAGE_SIZE, "%s\n",
				    wq_affn_names[wq->unbound_attrs->affn_scope]);
	mutex_unlock(&wq->mutex);

	return written;
}

static ssize_t wq_affn_scope_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	struct workqueue_struct *wq = dev_to_wq(dev);
	struct workqueue_attrs *attrs;
	int affn, ret = -ENOMEM;

	affn = parse_affn_scope(buf);
	if (affn < 0)
		return affn;

	apply_wqattrs_lock();
	attrs = wq_sysfs_prep_attrs(wq);
	if (attrs) {
		attrs->affn_scope = affn;
		ret = apply_workqueue_attrs_locked(wq, attrs);
	}
	apply_wqattrs_unlock();
	free_workqueue_attrs(attrs);
	return ret ?: count;
}

static ssize_t wq_affinity_strict_show(struct device *dev,
				       struct device_attribute *attr, char *buf)
{
	struct workqueue_struct *wq = dev_to_wq(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n",
			 wq->unbound_attrs->affn_strict);
}

static ssize_t wq_affinity_strict_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct workqueue_struct *wq = dev_to_wq(dev);
	struct workqueue_attrs *attrs;
	int v, ret = -ENOMEM;

	if (sscanf(buf, "%d", &v) != 1)
		return -EINVAL;

	apply_wqattrs_lock();
	attrs = wq_sysfs_prep_attrs(wq);
	if (attrs) {
		attrs->affn_strict = (bool)v;
		ret = apply_workqueue_attrs_locked(wq, attrs);
	}
	apply_wqattrs_unlock();
	free_workqueue_attrs(attrs);
	return ret ?: count;
}

static struct device_attribute wq_sysfs_unbound_attrs[] = {
	__ATTR(nice, 0644, wq_nice_show, wq_nice_store),
	__ATTR(cpumask, 0644, wq_cpumask_show, wq_cpumask_store),
	__ATTR(affinity_scope, 0644, wq_affn_scope_show, wq_affn_scope_store),
	__ATTR(affinity_strict, 0644, wq_affinity_strict_show, wq_affinity_strict_store),
	__ATTR_NULL,
};

static struct bus_type wq_subsys = {
	.name				= "workqueue",
	.dev_groups			= wq_sysfs_groups,
};

static ssize_t wq_unbound_cpumask_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int written;

	mutex_lock(&wq_pool_mutex);
	written = scnprintf(buf, PAGE_SIZE, "%*pb\n",
			    cpumask_pr_args(wq_unbound_cpumask));
	mutex_unlock(&wq_pool_mutex);

	return written;
}

static ssize_t wq_unbound_cpumask_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	cpumask_var_t cpumask;
	int ret;

	if (!zalloc_cpumask_var(&cpumask, GFP_KERNEL))
		return -ENOMEM;

	ret = cpumask_parse(buf, cpumask);
	if (!ret)
		ret = workqueue_set_unbound_cpumask(cpumask);

	free_cpumask_var(cpumask);
	return ret ? ret : count;
}

static struct device_attribute wq_sysfs_cpumask_attr =
	__ATTR(cpumask, 0644, wq_unbound_cpumask_show,
	       wq_unbound_cpumask_store);

static int __init wq_sysfs_init(void)
{
	struct device *dev_root;
	int err;

	err = subsys_virtual_register(&wq_subsys, NULL);
	if (err)
		return err;

	dev_root = bus_get_dev_root(&wq_subsys);
	if (dev_root) {
		err = device_create_file(dev_root, &wq_sysfs_cpumask_attr);
		put_device(dev_root);
	}
	return err;
}
core_initcall(wq_sysfs_init);

static void wq_device_release(struct device *dev)
{
	struct wq_device *wq_dev = container_of(dev, struct wq_device, dev);

	kfree(wq_dev);
}

 
int workqueue_sysfs_register(struct workqueue_struct *wq)
{
	struct wq_device *wq_dev;
	int ret;

	 
	if (WARN_ON(wq->flags & __WQ_ORDERED_EXPLICIT))
		return -EINVAL;

	wq->wq_dev = wq_dev = kzalloc(sizeof(*wq_dev), GFP_KERNEL);
	if (!wq_dev)
		return -ENOMEM;

	wq_dev->wq = wq;
	wq_dev->dev.bus = &wq_subsys;
	wq_dev->dev.release = wq_device_release;
	dev_set_name(&wq_dev->dev, "%s", wq->name);

	 
	dev_set_uevent_suppress(&wq_dev->dev, true);

	ret = device_register(&wq_dev->dev);
	if (ret) {
		put_device(&wq_dev->dev);
		wq->wq_dev = NULL;
		return ret;
	}

	if (wq->flags & WQ_UNBOUND) {
		struct device_attribute *attr;

		for (attr = wq_sysfs_unbound_attrs; attr->attr.name; attr++) {
			ret = device_create_file(&wq_dev->dev, attr);
			if (ret) {
				device_unregister(&wq_dev->dev);
				wq->wq_dev = NULL;
				return ret;
			}
		}
	}

	dev_set_uevent_suppress(&wq_dev->dev, false);
	kobject_uevent(&wq_dev->dev.kobj, KOBJ_ADD);
	return 0;
}

 
static void workqueue_sysfs_unregister(struct workqueue_struct *wq)
{
	struct wq_device *wq_dev = wq->wq_dev;

	if (!wq->wq_dev)
		return;

	wq->wq_dev = NULL;
	device_unregister(&wq_dev->dev);
}
#else	 
static void workqueue_sysfs_unregister(struct workqueue_struct *wq)	{ }
#endif	 

 
#ifdef CONFIG_WQ_WATCHDOG

static unsigned long wq_watchdog_thresh = 30;
static struct timer_list wq_watchdog_timer;

static unsigned long wq_watchdog_touched = INITIAL_JIFFIES;
static DEFINE_PER_CPU(unsigned long, wq_watchdog_touched_cpu) = INITIAL_JIFFIES;

 
static void show_cpu_pool_hog(struct worker_pool *pool)
{
	struct worker *worker;
	unsigned long flags;
	int bkt;

	raw_spin_lock_irqsave(&pool->lock, flags);

	hash_for_each(pool->busy_hash, bkt, worker, hentry) {
		if (task_is_running(worker->task)) {
			 
			printk_deferred_enter();

			pr_info("pool %d:\n", pool->id);
			sched_show_task(worker->task);

			printk_deferred_exit();
		}
	}

	raw_spin_unlock_irqrestore(&pool->lock, flags);
}

static void show_cpu_pools_hogs(void)
{
	struct worker_pool *pool;
	int pi;

	pr_info("Showing backtraces of running workers in stalled CPU-bound worker pools:\n");

	rcu_read_lock();

	for_each_pool(pool, pi) {
		if (pool->cpu_stall)
			show_cpu_pool_hog(pool);

	}

	rcu_read_unlock();
}

static void wq_watchdog_reset_touched(void)
{
	int cpu;

	wq_watchdog_touched = jiffies;
	for_each_possible_cpu(cpu)
		per_cpu(wq_watchdog_touched_cpu, cpu) = jiffies;
}

static void wq_watchdog_timer_fn(struct timer_list *unused)
{
	unsigned long thresh = READ_ONCE(wq_watchdog_thresh) * HZ;
	bool lockup_detected = false;
	bool cpu_pool_stall = false;
	unsigned long now = jiffies;
	struct worker_pool *pool;
	int pi;

	if (!thresh)
		return;

	rcu_read_lock();

	for_each_pool(pool, pi) {
		unsigned long pool_ts, touched, ts;

		pool->cpu_stall = false;
		if (list_empty(&pool->worklist))
			continue;

		 
		kvm_check_and_clear_guest_paused();

		 
		if (pool->cpu >= 0)
			touched = READ_ONCE(per_cpu(wq_watchdog_touched_cpu, pool->cpu));
		else
			touched = READ_ONCE(wq_watchdog_touched);
		pool_ts = READ_ONCE(pool->watchdog_ts);

		if (time_after(pool_ts, touched))
			ts = pool_ts;
		else
			ts = touched;

		 
		if (time_after(now, ts + thresh)) {
			lockup_detected = true;
			if (pool->cpu >= 0) {
				pool->cpu_stall = true;
				cpu_pool_stall = true;
			}
			pr_emerg("BUG: workqueue lockup - pool");
			pr_cont_pool_info(pool);
			pr_cont(" stuck for %us!\n",
				jiffies_to_msecs(now - pool_ts) / 1000);
		}


	}

	rcu_read_unlock();

	if (lockup_detected)
		show_all_workqueues();

	if (cpu_pool_stall)
		show_cpu_pools_hogs();

	wq_watchdog_reset_touched();
	mod_timer(&wq_watchdog_timer, jiffies + thresh);
}

notrace void wq_watchdog_touch(int cpu)
{
	if (cpu >= 0)
		per_cpu(wq_watchdog_touched_cpu, cpu) = jiffies;

	wq_watchdog_touched = jiffies;
}

static void wq_watchdog_set_thresh(unsigned long thresh)
{
	wq_watchdog_thresh = 0;
	del_timer_sync(&wq_watchdog_timer);

	if (thresh) {
		wq_watchdog_thresh = thresh;
		wq_watchdog_reset_touched();
		mod_timer(&wq_watchdog_timer, jiffies + thresh * HZ);
	}
}

static int wq_watchdog_param_set_thresh(const char *val,
					const struct kernel_param *kp)
{
	unsigned long thresh;
	int ret;

	ret = kstrtoul(val, 0, &thresh);
	if (ret)
		return ret;

	if (system_wq)
		wq_watchdog_set_thresh(thresh);
	else
		wq_watchdog_thresh = thresh;

	return 0;
}

static const struct kernel_param_ops wq_watchdog_thresh_ops = {
	.set	= wq_watchdog_param_set_thresh,
	.get	= param_get_ulong,
};

module_param_cb(watchdog_thresh, &wq_watchdog_thresh_ops, &wq_watchdog_thresh,
		0644);

static void wq_watchdog_init(void)
{
	timer_setup(&wq_watchdog_timer, wq_watchdog_timer_fn, TIMER_DEFERRABLE);
	wq_watchdog_set_thresh(wq_watchdog_thresh);
}

#else	 

static inline void wq_watchdog_init(void) { }

#endif	 

static void __init restrict_unbound_cpumask(const char *name, const struct cpumask *mask)
{
	if (!cpumask_intersects(wq_unbound_cpumask, mask)) {
		pr_warn("workqueue: Restricting unbound_cpumask (%*pb) with %s (%*pb) leaves no CPU, ignoring\n",
			cpumask_pr_args(wq_unbound_cpumask), name, cpumask_pr_args(mask));
		return;
	}

	cpumask_and(wq_unbound_cpumask, wq_unbound_cpumask, mask);
}

 
void __init workqueue_init_early(void)
{
	struct wq_pod_type *pt = &wq_pod_types[WQ_AFFN_SYSTEM];
	int std_nice[NR_STD_WORKER_POOLS] = { 0, HIGHPRI_NICE_LEVEL };
	int i, cpu;

	BUILD_BUG_ON(__alignof__(struct pool_workqueue) < __alignof__(long long));

	BUG_ON(!alloc_cpumask_var(&wq_unbound_cpumask, GFP_KERNEL));
	cpumask_copy(wq_unbound_cpumask, cpu_possible_mask);
	restrict_unbound_cpumask("HK_TYPE_WQ", housekeeping_cpumask(HK_TYPE_WQ));
	restrict_unbound_cpumask("HK_TYPE_DOMAIN", housekeeping_cpumask(HK_TYPE_DOMAIN));
	if (!cpumask_empty(&wq_cmdline_cpumask))
		restrict_unbound_cpumask("workqueue.unbound_cpus", &wq_cmdline_cpumask);

	pwq_cache = KMEM_CACHE(pool_workqueue, SLAB_PANIC);

	wq_update_pod_attrs_buf = alloc_workqueue_attrs();
	BUG_ON(!wq_update_pod_attrs_buf);

	 
	pt->pod_cpus = kcalloc(1, sizeof(pt->pod_cpus[0]), GFP_KERNEL);
	pt->pod_node = kcalloc(1, sizeof(pt->pod_node[0]), GFP_KERNEL);
	pt->cpu_pod = kcalloc(nr_cpu_ids, sizeof(pt->cpu_pod[0]), GFP_KERNEL);
	BUG_ON(!pt->pod_cpus || !pt->pod_node || !pt->cpu_pod);

	BUG_ON(!zalloc_cpumask_var_node(&pt->pod_cpus[0], GFP_KERNEL, NUMA_NO_NODE));

	pt->nr_pods = 1;
	cpumask_copy(pt->pod_cpus[0], cpu_possible_mask);
	pt->pod_node[0] = NUMA_NO_NODE;
	pt->cpu_pod[0] = 0;

	 
	for_each_possible_cpu(cpu) {
		struct worker_pool *pool;

		i = 0;
		for_each_cpu_worker_pool(pool, cpu) {
			BUG_ON(init_worker_pool(pool));
			pool->cpu = cpu;
			cpumask_copy(pool->attrs->cpumask, cpumask_of(cpu));
			cpumask_copy(pool->attrs->__pod_cpumask, cpumask_of(cpu));
			pool->attrs->nice = std_nice[i++];
			pool->attrs->affn_strict = true;
			pool->node = cpu_to_node(cpu);

			 
			mutex_lock(&wq_pool_mutex);
			BUG_ON(worker_pool_assign_id(pool));
			mutex_unlock(&wq_pool_mutex);
		}
	}

	 
	for (i = 0; i < NR_STD_WORKER_POOLS; i++) {
		struct workqueue_attrs *attrs;

		BUG_ON(!(attrs = alloc_workqueue_attrs()));
		attrs->nice = std_nice[i];
		unbound_std_wq_attrs[i] = attrs;

		 
		BUG_ON(!(attrs = alloc_workqueue_attrs()));
		attrs->nice = std_nice[i];
		attrs->ordered = true;
		ordered_wq_attrs[i] = attrs;
	}

	system_wq = alloc_workqueue("events", 0, 0);
	system_highpri_wq = alloc_workqueue("events_highpri", WQ_HIGHPRI, 0);
	system_long_wq = alloc_workqueue("events_long", 0, 0);
	system_unbound_wq = alloc_workqueue("events_unbound", WQ_UNBOUND,
					    WQ_MAX_ACTIVE);
	system_freezable_wq = alloc_workqueue("events_freezable",
					      WQ_FREEZABLE, 0);
	system_power_efficient_wq = alloc_workqueue("events_power_efficient",
					      WQ_POWER_EFFICIENT, 0);
	system_freezable_power_efficient_wq = alloc_workqueue("events_freezable_power_efficient",
					      WQ_FREEZABLE | WQ_POWER_EFFICIENT,
					      0);
	BUG_ON(!system_wq || !system_highpri_wq || !system_long_wq ||
	       !system_unbound_wq || !system_freezable_wq ||
	       !system_power_efficient_wq ||
	       !system_freezable_power_efficient_wq);
}

static void __init wq_cpu_intensive_thresh_init(void)
{
	unsigned long thresh;
	unsigned long bogo;

	pwq_release_worker = kthread_create_worker(0, "pool_workqueue_release");
	BUG_ON(IS_ERR(pwq_release_worker));

	 
	if (wq_cpu_intensive_thresh_us != ULONG_MAX)
		return;

	 
	thresh = 10 * USEC_PER_MSEC;

	 
	bogo = max_t(unsigned long, loops_per_jiffy / 500000 * HZ, 1);
	if (bogo < 4000)
		thresh = min_t(unsigned long, thresh * 4000 / bogo, USEC_PER_SEC);

	pr_debug("wq_cpu_intensive_thresh: lpj=%lu BogoMIPS=%lu thresh_us=%lu\n",
		 loops_per_jiffy, bogo, thresh);

	wq_cpu_intensive_thresh_us = thresh;
}

 
void __init workqueue_init(void)
{
	struct workqueue_struct *wq;
	struct worker_pool *pool;
	int cpu, bkt;

	wq_cpu_intensive_thresh_init();

	mutex_lock(&wq_pool_mutex);

	 
	for_each_possible_cpu(cpu) {
		for_each_cpu_worker_pool(pool, cpu) {
			pool->node = cpu_to_node(cpu);
		}
	}

	list_for_each_entry(wq, &workqueues, list) {
		WARN(init_rescuer(wq),
		     "workqueue: failed to create early rescuer for %s",
		     wq->name);
	}

	mutex_unlock(&wq_pool_mutex);

	 
	for_each_online_cpu(cpu) {
		for_each_cpu_worker_pool(pool, cpu) {
			pool->flags &= ~POOL_DISASSOCIATED;
			BUG_ON(!create_worker(pool));
		}
	}

	hash_for_each(unbound_pool_hash, bkt, pool, hash_node)
		BUG_ON(!create_worker(pool));

	wq_online = true;
	wq_watchdog_init();
}

 
static void __init init_pod_type(struct wq_pod_type *pt,
				 bool (*cpus_share_pod)(int, int))
{
	int cur, pre, cpu, pod;

	pt->nr_pods = 0;

	 
	pt->cpu_pod = kcalloc(nr_cpu_ids, sizeof(pt->cpu_pod[0]), GFP_KERNEL);
	BUG_ON(!pt->cpu_pod);

	for_each_possible_cpu(cur) {
		for_each_possible_cpu(pre) {
			if (pre >= cur) {
				pt->cpu_pod[cur] = pt->nr_pods++;
				break;
			}
			if (cpus_share_pod(cur, pre)) {
				pt->cpu_pod[cur] = pt->cpu_pod[pre];
				break;
			}
		}
	}

	 
	pt->pod_cpus = kcalloc(pt->nr_pods, sizeof(pt->pod_cpus[0]), GFP_KERNEL);
	pt->pod_node = kcalloc(pt->nr_pods, sizeof(pt->pod_node[0]), GFP_KERNEL);
	BUG_ON(!pt->pod_cpus || !pt->pod_node);

	for (pod = 0; pod < pt->nr_pods; pod++)
		BUG_ON(!zalloc_cpumask_var(&pt->pod_cpus[pod], GFP_KERNEL));

	for_each_possible_cpu(cpu) {
		cpumask_set_cpu(cpu, pt->pod_cpus[pt->cpu_pod[cpu]]);
		pt->pod_node[pt->cpu_pod[cpu]] = cpu_to_node(cpu);
	}
}

static bool __init cpus_dont_share(int cpu0, int cpu1)
{
	return false;
}

static bool __init cpus_share_smt(int cpu0, int cpu1)
{
#ifdef CONFIG_SCHED_SMT
	return cpumask_test_cpu(cpu0, cpu_smt_mask(cpu1));
#else
	return false;
#endif
}

static bool __init cpus_share_numa(int cpu0, int cpu1)
{
	return cpu_to_node(cpu0) == cpu_to_node(cpu1);
}

 
void __init workqueue_init_topology(void)
{
	struct workqueue_struct *wq;
	int cpu;

	init_pod_type(&wq_pod_types[WQ_AFFN_CPU], cpus_dont_share);
	init_pod_type(&wq_pod_types[WQ_AFFN_SMT], cpus_share_smt);
	init_pod_type(&wq_pod_types[WQ_AFFN_CACHE], cpus_share_cache);
	init_pod_type(&wq_pod_types[WQ_AFFN_NUMA], cpus_share_numa);

	mutex_lock(&wq_pool_mutex);

	 
	list_for_each_entry(wq, &workqueues, list) {
		for_each_online_cpu(cpu) {
			wq_update_pod(wq, cpu, cpu, true);
		}
	}

	mutex_unlock(&wq_pool_mutex);
}

void __warn_flushing_systemwide_wq(void)
{
	pr_warn("WARNING: Flushing system-wide workqueues will be prohibited in near future.\n");
	dump_stack();
}
EXPORT_SYMBOL(__warn_flushing_systemwide_wq);

static int __init workqueue_unbound_cpus_setup(char *str)
{
	if (cpulist_parse(str, &wq_cmdline_cpumask) < 0) {
		cpumask_clear(&wq_cmdline_cpumask);
		pr_warn("workqueue.unbound_cpus: incorrect CPU range, using default\n");
	}

	return 1;
}
__setup("workqueue.unbound_cpus=", workqueue_unbound_cpus_setup);
