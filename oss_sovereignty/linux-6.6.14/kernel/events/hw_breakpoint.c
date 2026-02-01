
 

 

#include <linux/hw_breakpoint.h>

#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/cpu.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/irqflags.h>
#include <linux/kdebug.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/notifier.h>
#include <linux/percpu-rwsem.h>
#include <linux/percpu.h>
#include <linux/rhashtable.h>
#include <linux/sched.h>
#include <linux/slab.h>

 
struct bp_slots_histogram {
#ifdef hw_breakpoint_slots
	atomic_t count[hw_breakpoint_slots(0)];
#else
	atomic_t *count;
#endif
};

 
struct bp_cpuinfo {
	 
	unsigned int			cpu_pinned;
	 
	struct bp_slots_histogram	tsk_pinned;
};

static DEFINE_PER_CPU(struct bp_cpuinfo, bp_cpuinfo[TYPE_MAX]);

static struct bp_cpuinfo *get_bp_info(int cpu, enum bp_type_idx type)
{
	return per_cpu_ptr(bp_cpuinfo + type, cpu);
}

 
static struct bp_slots_histogram cpu_pinned[TYPE_MAX];
 
static struct bp_slots_histogram tsk_pinned_all[TYPE_MAX];

 
static struct rhltable task_bps_ht;
static const struct rhashtable_params task_bps_ht_params = {
	.head_offset = offsetof(struct hw_perf_event, bp_list),
	.key_offset = offsetof(struct hw_perf_event, target),
	.key_len = sizeof_field(struct hw_perf_event, target),
	.automatic_shrinking = true,
};

static bool constraints_initialized __ro_after_init;

 
DEFINE_STATIC_PERCPU_RWSEM(bp_cpuinfo_sem);

 
static inline struct mutex *get_task_bps_mutex(struct perf_event *bp)
{
	struct task_struct *tsk = bp->hw.target;

	return tsk ? &tsk->perf_event_mutex : NULL;
}

static struct mutex *bp_constraints_lock(struct perf_event *bp)
{
	struct mutex *tsk_mtx = get_task_bps_mutex(bp);

	if (tsk_mtx) {
		 
		mutex_lock_nested(tsk_mtx, SINGLE_DEPTH_NESTING);
		percpu_down_read(&bp_cpuinfo_sem);
	} else {
		percpu_down_write(&bp_cpuinfo_sem);
	}

	return tsk_mtx;
}

static void bp_constraints_unlock(struct mutex *tsk_mtx)
{
	if (tsk_mtx) {
		percpu_up_read(&bp_cpuinfo_sem);
		mutex_unlock(tsk_mtx);
	} else {
		percpu_up_write(&bp_cpuinfo_sem);
	}
}

static bool bp_constraints_is_locked(struct perf_event *bp)
{
	struct mutex *tsk_mtx = get_task_bps_mutex(bp);

	return percpu_is_write_locked(&bp_cpuinfo_sem) ||
	       (tsk_mtx ? mutex_is_locked(tsk_mtx) :
			  percpu_is_read_locked(&bp_cpuinfo_sem));
}

static inline void assert_bp_constraints_lock_held(struct perf_event *bp)
{
	struct mutex *tsk_mtx = get_task_bps_mutex(bp);

	if (tsk_mtx)
		lockdep_assert_held(tsk_mtx);
	lockdep_assert_held(&bp_cpuinfo_sem);
}

#ifdef hw_breakpoint_slots
 
static_assert(hw_breakpoint_slots(TYPE_INST) == hw_breakpoint_slots(TYPE_DATA));
static inline int hw_breakpoint_slots_cached(int type)	{ return hw_breakpoint_slots(type); }
static inline int init_breakpoint_slots(void)		{ return 0; }
#else
 
static int __nr_bp_slots[TYPE_MAX] __ro_after_init;

static inline int hw_breakpoint_slots_cached(int type)
{
	return __nr_bp_slots[type];
}

static __init bool
bp_slots_histogram_alloc(struct bp_slots_histogram *hist, enum bp_type_idx type)
{
	hist->count = kcalloc(hw_breakpoint_slots_cached(type), sizeof(*hist->count), GFP_KERNEL);
	return hist->count;
}

static __init void bp_slots_histogram_free(struct bp_slots_histogram *hist)
{
	kfree(hist->count);
}

static __init int init_breakpoint_slots(void)
{
	int i, cpu, err_cpu;

	for (i = 0; i < TYPE_MAX; i++)
		__nr_bp_slots[i] = hw_breakpoint_slots(i);

	for_each_possible_cpu(cpu) {
		for (i = 0; i < TYPE_MAX; i++) {
			struct bp_cpuinfo *info = get_bp_info(cpu, i);

			if (!bp_slots_histogram_alloc(&info->tsk_pinned, i))
				goto err;
		}
	}
	for (i = 0; i < TYPE_MAX; i++) {
		if (!bp_slots_histogram_alloc(&cpu_pinned[i], i))
			goto err;
		if (!bp_slots_histogram_alloc(&tsk_pinned_all[i], i))
			goto err;
	}

	return 0;
err:
	for_each_possible_cpu(err_cpu) {
		for (i = 0; i < TYPE_MAX; i++)
			bp_slots_histogram_free(&get_bp_info(err_cpu, i)->tsk_pinned);
		if (err_cpu == cpu)
			break;
	}
	for (i = 0; i < TYPE_MAX; i++) {
		bp_slots_histogram_free(&cpu_pinned[i]);
		bp_slots_histogram_free(&tsk_pinned_all[i]);
	}

	return -ENOMEM;
}
#endif

static inline void
bp_slots_histogram_add(struct bp_slots_histogram *hist, int old, int val)
{
	const int old_idx = old - 1;
	const int new_idx = old_idx + val;

	if (old_idx >= 0)
		WARN_ON(atomic_dec_return_relaxed(&hist->count[old_idx]) < 0);
	if (new_idx >= 0)
		WARN_ON(atomic_inc_return_relaxed(&hist->count[new_idx]) < 0);
}

static int
bp_slots_histogram_max(struct bp_slots_histogram *hist, enum bp_type_idx type)
{
	for (int i = hw_breakpoint_slots_cached(type) - 1; i >= 0; i--) {
		const int count = atomic_read(&hist->count[i]);

		 
		ASSERT_EXCLUSIVE_WRITER(hist->count[i]);
		if (count > 0)
			return i + 1;
		WARN(count < 0, "inconsistent breakpoint slots histogram");
	}

	return 0;
}

static int
bp_slots_histogram_max_merge(struct bp_slots_histogram *hist1, struct bp_slots_histogram *hist2,
			     enum bp_type_idx type)
{
	for (int i = hw_breakpoint_slots_cached(type) - 1; i >= 0; i--) {
		const int count1 = atomic_read(&hist1->count[i]);
		const int count2 = atomic_read(&hist2->count[i]);

		 
		ASSERT_EXCLUSIVE_WRITER(hist1->count[i]);
		ASSERT_EXCLUSIVE_WRITER(hist2->count[i]);
		if (count1 + count2 > 0)
			return i + 1;
		WARN(count1 < 0, "inconsistent breakpoint slots histogram");
		WARN(count2 < 0, "inconsistent breakpoint slots histogram");
	}

	return 0;
}

#ifndef hw_breakpoint_weight
static inline int hw_breakpoint_weight(struct perf_event *bp)
{
	return 1;
}
#endif

static inline enum bp_type_idx find_slot_idx(u64 bp_type)
{
	if (bp_type & HW_BREAKPOINT_RW)
		return TYPE_DATA;

	return TYPE_INST;
}

 
static unsigned int max_task_bp_pinned(int cpu, enum bp_type_idx type)
{
	struct bp_slots_histogram *tsk_pinned = &get_bp_info(cpu, type)->tsk_pinned;

	 
	lockdep_assert_held_write(&bp_cpuinfo_sem);
	return bp_slots_histogram_max_merge(tsk_pinned, &tsk_pinned_all[type], type);
}

 
static int task_bp_pinned(int cpu, struct perf_event *bp, enum bp_type_idx type)
{
	struct rhlist_head *head, *pos;
	struct perf_event *iter;
	int count = 0;

	 
	assert_bp_constraints_lock_held(bp);

	rcu_read_lock();
	head = rhltable_lookup(&task_bps_ht, &bp->hw.target, task_bps_ht_params);
	if (!head)
		goto out;

	rhl_for_each_entry_rcu(iter, pos, head, hw.bp_list) {
		if (find_slot_idx(iter->attr.bp_type) != type)
			continue;

		if (iter->cpu >= 0) {
			if (cpu == -1) {
				count = -1;
				goto out;
			} else if (cpu != iter->cpu)
				continue;
		}

		count += hw_breakpoint_weight(iter);
	}

out:
	rcu_read_unlock();
	return count;
}

static const struct cpumask *cpumask_of_bp(struct perf_event *bp)
{
	if (bp->cpu >= 0)
		return cpumask_of(bp->cpu);
	return cpu_possible_mask;
}

 
static int
max_bp_pinned_slots(struct perf_event *bp, enum bp_type_idx type)
{
	const struct cpumask *cpumask = cpumask_of_bp(bp);
	int pinned_slots = 0;
	int cpu;

	if (bp->hw.target && bp->cpu < 0) {
		int max_pinned = task_bp_pinned(-1, bp, type);

		if (max_pinned >= 0) {
			 
			max_pinned += bp_slots_histogram_max(&cpu_pinned[type], type);
			return max_pinned;
		}
	}

	for_each_cpu(cpu, cpumask) {
		struct bp_cpuinfo *info = get_bp_info(cpu, type);
		int nr;

		nr = info->cpu_pinned;
		if (!bp->hw.target)
			nr += max_task_bp_pinned(cpu, type);
		else
			nr += task_bp_pinned(cpu, bp, type);

		pinned_slots = max(nr, pinned_slots);
	}

	return pinned_slots;
}

 
static int
toggle_bp_slot(struct perf_event *bp, bool enable, enum bp_type_idx type, int weight)
{
	int cpu, next_tsk_pinned;

	if (!enable)
		weight = -weight;

	if (!bp->hw.target) {
		 
		struct bp_cpuinfo *info = get_bp_info(bp->cpu, type);

		lockdep_assert_held_write(&bp_cpuinfo_sem);
		bp_slots_histogram_add(&cpu_pinned[type], info->cpu_pinned, weight);
		info->cpu_pinned += weight;
		return 0;
	}

	 
	lockdep_assert_held_read(&bp_cpuinfo_sem);

	 

	if (!enable) {
		 
		int ret = rhltable_remove(&task_bps_ht, &bp->hw.bp_list, task_bps_ht_params);

		if (ret)
			return ret;
	}
	 
	next_tsk_pinned = task_bp_pinned(-1, bp, type);

	if (next_tsk_pinned >= 0) {
		if (bp->cpu < 0) {  
			if (!enable)
				next_tsk_pinned += hw_breakpoint_weight(bp);
			bp_slots_histogram_add(&tsk_pinned_all[type], next_tsk_pinned, weight);
		} else if (enable) {  
			 
			for_each_possible_cpu(cpu) {
				bp_slots_histogram_add(&get_bp_info(cpu, type)->tsk_pinned,
						       0, next_tsk_pinned);
			}
			 
			bp_slots_histogram_add(&get_bp_info(bp->cpu, type)->tsk_pinned,
					       next_tsk_pinned, weight);
			 
			bp_slots_histogram_add(&tsk_pinned_all[type], next_tsk_pinned,
					       -next_tsk_pinned);
		} else {  
			 
			bp_slots_histogram_add(&get_bp_info(bp->cpu, type)->tsk_pinned,
					       next_tsk_pinned + hw_breakpoint_weight(bp), weight);
			 
			for_each_possible_cpu(cpu) {
				bp_slots_histogram_add(&get_bp_info(cpu, type)->tsk_pinned,
						       next_tsk_pinned, -next_tsk_pinned);
			}
			 
			bp_slots_histogram_add(&tsk_pinned_all[type], 0, next_tsk_pinned);
		}
	} else {  
		const struct cpumask *cpumask = cpumask_of_bp(bp);

		for_each_cpu(cpu, cpumask) {
			next_tsk_pinned = task_bp_pinned(cpu, bp, type);
			if (!enable)
				next_tsk_pinned += hw_breakpoint_weight(bp);
			bp_slots_histogram_add(&get_bp_info(cpu, type)->tsk_pinned,
					       next_tsk_pinned, weight);
		}
	}

	 
	assert_bp_constraints_lock_held(bp);

	if (enable)
		return rhltable_insert(&task_bps_ht, &bp->hw.bp_list, task_bps_ht_params);

	return 0;
}

 
static int __reserve_bp_slot(struct perf_event *bp, u64 bp_type)
{
	enum bp_type_idx type;
	int max_pinned_slots;
	int weight;

	 
	if (!constraints_initialized)
		return -ENOMEM;

	 
	if (bp_type == HW_BREAKPOINT_EMPTY ||
	    bp_type == HW_BREAKPOINT_INVALID)
		return -EINVAL;

	type = find_slot_idx(bp_type);
	weight = hw_breakpoint_weight(bp);

	 
	max_pinned_slots = max_bp_pinned_slots(bp, type) + weight;
	if (max_pinned_slots > hw_breakpoint_slots_cached(type))
		return -ENOSPC;

	return toggle_bp_slot(bp, true, type, weight);
}

int reserve_bp_slot(struct perf_event *bp)
{
	struct mutex *mtx = bp_constraints_lock(bp);
	int ret = __reserve_bp_slot(bp, bp->attr.bp_type);

	bp_constraints_unlock(mtx);
	return ret;
}

static void __release_bp_slot(struct perf_event *bp, u64 bp_type)
{
	enum bp_type_idx type;
	int weight;

	type = find_slot_idx(bp_type);
	weight = hw_breakpoint_weight(bp);
	WARN_ON(toggle_bp_slot(bp, false, type, weight));
}

void release_bp_slot(struct perf_event *bp)
{
	struct mutex *mtx = bp_constraints_lock(bp);

	__release_bp_slot(bp, bp->attr.bp_type);
	bp_constraints_unlock(mtx);
}

static int __modify_bp_slot(struct perf_event *bp, u64 old_type, u64 new_type)
{
	int err;

	__release_bp_slot(bp, old_type);

	err = __reserve_bp_slot(bp, new_type);
	if (err) {
		 
		WARN_ON(__reserve_bp_slot(bp, old_type));
	}

	return err;
}

static int modify_bp_slot(struct perf_event *bp, u64 old_type, u64 new_type)
{
	struct mutex *mtx = bp_constraints_lock(bp);
	int ret = __modify_bp_slot(bp, old_type, new_type);

	bp_constraints_unlock(mtx);
	return ret;
}

 
int dbg_reserve_bp_slot(struct perf_event *bp)
{
	int ret;

	if (bp_constraints_is_locked(bp))
		return -1;

	 
	lockdep_off();
	ret = __reserve_bp_slot(bp, bp->attr.bp_type);
	lockdep_on();

	return ret;
}

int dbg_release_bp_slot(struct perf_event *bp)
{
	if (bp_constraints_is_locked(bp))
		return -1;

	 
	lockdep_off();
	__release_bp_slot(bp, bp->attr.bp_type);
	lockdep_on();

	return 0;
}

static int hw_breakpoint_parse(struct perf_event *bp,
			       const struct perf_event_attr *attr,
			       struct arch_hw_breakpoint *hw)
{
	int err;

	err = hw_breakpoint_arch_parse(bp, attr, hw);
	if (err)
		return err;

	if (arch_check_bp_in_kernelspace(hw)) {
		if (attr->exclude_kernel)
			return -EINVAL;
		 
		if (!capable(CAP_SYS_ADMIN))
			return -EPERM;
	}

	return 0;
}

int register_perf_hw_breakpoint(struct perf_event *bp)
{
	struct arch_hw_breakpoint hw = { };
	int err;

	err = reserve_bp_slot(bp);
	if (err)
		return err;

	err = hw_breakpoint_parse(bp, &bp->attr, &hw);
	if (err) {
		release_bp_slot(bp);
		return err;
	}

	bp->hw.info = hw;

	return 0;
}

 
struct perf_event *
register_user_hw_breakpoint(struct perf_event_attr *attr,
			    perf_overflow_handler_t triggered,
			    void *context,
			    struct task_struct *tsk)
{
	return perf_event_create_kernel_counter(attr, -1, tsk, triggered,
						context);
}
EXPORT_SYMBOL_GPL(register_user_hw_breakpoint);

static void hw_breakpoint_copy_attr(struct perf_event_attr *to,
				    struct perf_event_attr *from)
{
	to->bp_addr = from->bp_addr;
	to->bp_type = from->bp_type;
	to->bp_len  = from->bp_len;
	to->disabled = from->disabled;
}

int
modify_user_hw_breakpoint_check(struct perf_event *bp, struct perf_event_attr *attr,
			        bool check)
{
	struct arch_hw_breakpoint hw = { };
	int err;

	err = hw_breakpoint_parse(bp, attr, &hw);
	if (err)
		return err;

	if (check) {
		struct perf_event_attr old_attr;

		old_attr = bp->attr;
		hw_breakpoint_copy_attr(&old_attr, attr);
		if (memcmp(&old_attr, attr, sizeof(*attr)))
			return -EINVAL;
	}

	if (bp->attr.bp_type != attr->bp_type) {
		err = modify_bp_slot(bp, bp->attr.bp_type, attr->bp_type);
		if (err)
			return err;
	}

	hw_breakpoint_copy_attr(&bp->attr, attr);
	bp->hw.info = hw;

	return 0;
}

 
int modify_user_hw_breakpoint(struct perf_event *bp, struct perf_event_attr *attr)
{
	int err;

	 
	if (irqs_disabled() && bp->ctx && bp->ctx->task == current)
		perf_event_disable_local(bp);
	else
		perf_event_disable(bp);

	err = modify_user_hw_breakpoint_check(bp, attr, false);

	if (!bp->attr.disabled)
		perf_event_enable(bp);

	return err;
}
EXPORT_SYMBOL_GPL(modify_user_hw_breakpoint);

 
void unregister_hw_breakpoint(struct perf_event *bp)
{
	if (!bp)
		return;
	perf_event_release_kernel(bp);
}
EXPORT_SYMBOL_GPL(unregister_hw_breakpoint);

 
struct perf_event * __percpu *
register_wide_hw_breakpoint(struct perf_event_attr *attr,
			    perf_overflow_handler_t triggered,
			    void *context)
{
	struct perf_event * __percpu *cpu_events, *bp;
	long err = 0;
	int cpu;

	cpu_events = alloc_percpu(typeof(*cpu_events));
	if (!cpu_events)
		return (void __percpu __force *)ERR_PTR(-ENOMEM);

	cpus_read_lock();
	for_each_online_cpu(cpu) {
		bp = perf_event_create_kernel_counter(attr, cpu, NULL,
						      triggered, context);
		if (IS_ERR(bp)) {
			err = PTR_ERR(bp);
			break;
		}

		per_cpu(*cpu_events, cpu) = bp;
	}
	cpus_read_unlock();

	if (likely(!err))
		return cpu_events;

	unregister_wide_hw_breakpoint(cpu_events);
	return (void __percpu __force *)ERR_PTR(err);
}
EXPORT_SYMBOL_GPL(register_wide_hw_breakpoint);

 
void unregister_wide_hw_breakpoint(struct perf_event * __percpu *cpu_events)
{
	int cpu;

	for_each_possible_cpu(cpu)
		unregister_hw_breakpoint(per_cpu(*cpu_events, cpu));

	free_percpu(cpu_events);
}
EXPORT_SYMBOL_GPL(unregister_wide_hw_breakpoint);

 
bool hw_breakpoint_is_used(void)
{
	int cpu;

	if (!constraints_initialized)
		return false;

	for_each_possible_cpu(cpu) {
		for (int type = 0; type < TYPE_MAX; ++type) {
			struct bp_cpuinfo *info = get_bp_info(cpu, type);

			if (info->cpu_pinned)
				return true;

			for (int slot = 0; slot < hw_breakpoint_slots_cached(type); ++slot) {
				if (atomic_read(&info->tsk_pinned.count[slot]))
					return true;
			}
		}
	}

	for (int type = 0; type < TYPE_MAX; ++type) {
		for (int slot = 0; slot < hw_breakpoint_slots_cached(type); ++slot) {
			 
			if (WARN_ON(atomic_read(&cpu_pinned[type].count[slot])))
				return true;

			if (atomic_read(&tsk_pinned_all[type].count[slot]))
				return true;
		}
	}

	return false;
}

static struct notifier_block hw_breakpoint_exceptions_nb = {
	.notifier_call = hw_breakpoint_exceptions_notify,
	 
	.priority = 0x7fffffff
};

static void bp_perf_event_destroy(struct perf_event *event)
{
	release_bp_slot(event);
}

static int hw_breakpoint_event_init(struct perf_event *bp)
{
	int err;

	if (bp->attr.type != PERF_TYPE_BREAKPOINT)
		return -ENOENT;

	 
	if (has_branch_stack(bp))
		return -EOPNOTSUPP;

	err = register_perf_hw_breakpoint(bp);
	if (err)
		return err;

	bp->destroy = bp_perf_event_destroy;

	return 0;
}

static int hw_breakpoint_add(struct perf_event *bp, int flags)
{
	if (!(flags & PERF_EF_START))
		bp->hw.state = PERF_HES_STOPPED;

	if (is_sampling_event(bp)) {
		bp->hw.last_period = bp->hw.sample_period;
		perf_swevent_set_period(bp);
	}

	return arch_install_hw_breakpoint(bp);
}

static void hw_breakpoint_del(struct perf_event *bp, int flags)
{
	arch_uninstall_hw_breakpoint(bp);
}

static void hw_breakpoint_start(struct perf_event *bp, int flags)
{
	bp->hw.state = 0;
}

static void hw_breakpoint_stop(struct perf_event *bp, int flags)
{
	bp->hw.state = PERF_HES_STOPPED;
}

static struct pmu perf_breakpoint = {
	.task_ctx_nr	= perf_sw_context,  

	.event_init	= hw_breakpoint_event_init,
	.add		= hw_breakpoint_add,
	.del		= hw_breakpoint_del,
	.start		= hw_breakpoint_start,
	.stop		= hw_breakpoint_stop,
	.read		= hw_breakpoint_pmu_read,
};

int __init init_hw_breakpoint(void)
{
	int ret;

	ret = rhltable_init(&task_bps_ht, &task_bps_ht_params);
	if (ret)
		return ret;

	ret = init_breakpoint_slots();
	if (ret)
		return ret;

	constraints_initialized = true;

	perf_pmu_register(&perf_breakpoint, "breakpoint", PERF_TYPE_BREAKPOINT);

	return register_die_notifier(&hw_breakpoint_exceptions_nb);
}
