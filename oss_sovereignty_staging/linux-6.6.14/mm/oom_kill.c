
 

#include <linux/oom.h>
#include <linux/mm.h>
#include <linux/err.h>
#include <linux/gfp.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/coredump.h>
#include <linux/sched/task.h>
#include <linux/sched/debug.h>
#include <linux/swap.h>
#include <linux/syscalls.h>
#include <linux/timex.h>
#include <linux/jiffies.h>
#include <linux/cpuset.h>
#include <linux/export.h>
#include <linux/notifier.h>
#include <linux/memcontrol.h>
#include <linux/mempolicy.h>
#include <linux/security.h>
#include <linux/ptrace.h>
#include <linux/freezer.h>
#include <linux/ftrace.h>
#include <linux/ratelimit.h>
#include <linux/kthread.h>
#include <linux/init.h>
#include <linux/mmu_notifier.h>

#include <asm/tlb.h>
#include "internal.h"
#include "slab.h"

#define CREATE_TRACE_POINTS
#include <trace/events/oom.h>

static int sysctl_panic_on_oom;
static int sysctl_oom_kill_allocating_task;
static int sysctl_oom_dump_tasks = 1;

 
DEFINE_MUTEX(oom_lock);
 
DEFINE_MUTEX(oom_adj_mutex);

static inline bool is_memcg_oom(struct oom_control *oc)
{
	return oc->memcg != NULL;
}

#ifdef CONFIG_NUMA
 
static bool oom_cpuset_eligible(struct task_struct *start,
				struct oom_control *oc)
{
	struct task_struct *tsk;
	bool ret = false;
	const nodemask_t *mask = oc->nodemask;

	rcu_read_lock();
	for_each_thread(start, tsk) {
		if (mask) {
			 
			ret = mempolicy_in_oom_domain(tsk, mask);
		} else {
			 
			ret = cpuset_mems_allowed_intersects(current, tsk);
		}
		if (ret)
			break;
	}
	rcu_read_unlock();

	return ret;
}
#else
static bool oom_cpuset_eligible(struct task_struct *tsk, struct oom_control *oc)
{
	return true;
}
#endif  

 
struct task_struct *find_lock_task_mm(struct task_struct *p)
{
	struct task_struct *t;

	rcu_read_lock();

	for_each_thread(p, t) {
		task_lock(t);
		if (likely(t->mm))
			goto found;
		task_unlock(t);
	}
	t = NULL;
found:
	rcu_read_unlock();

	return t;
}

 
static inline bool is_sysrq_oom(struct oom_control *oc)
{
	return oc->order == -1;
}

 
static bool oom_unkillable_task(struct task_struct *p)
{
	if (is_global_init(p))
		return true;
	if (p->flags & PF_KTHREAD)
		return true;
	return false;
}

 
static bool should_dump_unreclaim_slab(void)
{
	unsigned long nr_lru;

	nr_lru = global_node_page_state(NR_ACTIVE_ANON) +
		 global_node_page_state(NR_INACTIVE_ANON) +
		 global_node_page_state(NR_ACTIVE_FILE) +
		 global_node_page_state(NR_INACTIVE_FILE) +
		 global_node_page_state(NR_ISOLATED_ANON) +
		 global_node_page_state(NR_ISOLATED_FILE) +
		 global_node_page_state(NR_UNEVICTABLE);

	return (global_node_page_state_pages(NR_SLAB_UNRECLAIMABLE_B) > nr_lru);
}

 
long oom_badness(struct task_struct *p, unsigned long totalpages)
{
	long points;
	long adj;

	if (oom_unkillable_task(p))
		return LONG_MIN;

	p = find_lock_task_mm(p);
	if (!p)
		return LONG_MIN;

	 
	adj = (long)p->signal->oom_score_adj;
	if (adj == OOM_SCORE_ADJ_MIN ||
			test_bit(MMF_OOM_SKIP, &p->mm->flags) ||
			in_vfork(p)) {
		task_unlock(p);
		return LONG_MIN;
	}

	 
	points = get_mm_rss(p->mm) + get_mm_counter(p->mm, MM_SWAPENTS) +
		mm_pgtables_bytes(p->mm) / PAGE_SIZE;
	task_unlock(p);

	 
	adj *= totalpages / 1000;
	points += adj;

	return points;
}

static const char * const oom_constraint_text[] = {
	[CONSTRAINT_NONE] = "CONSTRAINT_NONE",
	[CONSTRAINT_CPUSET] = "CONSTRAINT_CPUSET",
	[CONSTRAINT_MEMORY_POLICY] = "CONSTRAINT_MEMORY_POLICY",
	[CONSTRAINT_MEMCG] = "CONSTRAINT_MEMCG",
};

 
static enum oom_constraint constrained_alloc(struct oom_control *oc)
{
	struct zone *zone;
	struct zoneref *z;
	enum zone_type highest_zoneidx = gfp_zone(oc->gfp_mask);
	bool cpuset_limited = false;
	int nid;

	if (is_memcg_oom(oc)) {
		oc->totalpages = mem_cgroup_get_max(oc->memcg) ?: 1;
		return CONSTRAINT_MEMCG;
	}

	 
	oc->totalpages = totalram_pages() + total_swap_pages;

	if (!IS_ENABLED(CONFIG_NUMA))
		return CONSTRAINT_NONE;

	if (!oc->zonelist)
		return CONSTRAINT_NONE;
	 
	if (oc->gfp_mask & __GFP_THISNODE)
		return CONSTRAINT_NONE;

	 
	if (oc->nodemask &&
	    !nodes_subset(node_states[N_MEMORY], *oc->nodemask)) {
		oc->totalpages = total_swap_pages;
		for_each_node_mask(nid, *oc->nodemask)
			oc->totalpages += node_present_pages(nid);
		return CONSTRAINT_MEMORY_POLICY;
	}

	 
	for_each_zone_zonelist_nodemask(zone, z, oc->zonelist,
			highest_zoneidx, oc->nodemask)
		if (!cpuset_zone_allowed(zone, oc->gfp_mask))
			cpuset_limited = true;

	if (cpuset_limited) {
		oc->totalpages = total_swap_pages;
		for_each_node_mask(nid, cpuset_current_mems_allowed)
			oc->totalpages += node_present_pages(nid);
		return CONSTRAINT_CPUSET;
	}
	return CONSTRAINT_NONE;
}

static int oom_evaluate_task(struct task_struct *task, void *arg)
{
	struct oom_control *oc = arg;
	long points;

	if (oom_unkillable_task(task))
		goto next;

	 
	if (!is_memcg_oom(oc) && !oom_cpuset_eligible(task, oc))
		goto next;

	 
	if (!is_sysrq_oom(oc) && tsk_is_oom_victim(task)) {
		if (test_bit(MMF_OOM_SKIP, &task->signal->oom_mm->flags))
			goto next;
		goto abort;
	}

	 
	if (oom_task_origin(task)) {
		points = LONG_MAX;
		goto select;
	}

	points = oom_badness(task, oc->totalpages);
	if (points == LONG_MIN || points < oc->chosen_points)
		goto next;

select:
	if (oc->chosen)
		put_task_struct(oc->chosen);
	get_task_struct(task);
	oc->chosen = task;
	oc->chosen_points = points;
next:
	return 0;
abort:
	if (oc->chosen)
		put_task_struct(oc->chosen);
	oc->chosen = (void *)-1UL;
	return 1;
}

 
static void select_bad_process(struct oom_control *oc)
{
	oc->chosen_points = LONG_MIN;

	if (is_memcg_oom(oc))
		mem_cgroup_scan_tasks(oc->memcg, oom_evaluate_task, oc);
	else {
		struct task_struct *p;

		rcu_read_lock();
		for_each_process(p)
			if (oom_evaluate_task(p, oc))
				break;
		rcu_read_unlock();
	}
}

static int dump_task(struct task_struct *p, void *arg)
{
	struct oom_control *oc = arg;
	struct task_struct *task;

	if (oom_unkillable_task(p))
		return 0;

	 
	if (!is_memcg_oom(oc) && !oom_cpuset_eligible(p, oc))
		return 0;

	task = find_lock_task_mm(p);
	if (!task) {
		 
		return 0;
	}

	pr_info("[%7d] %5d %5d %8lu %8lu %8ld %8lu         %5hd %s\n",
		task->pid, from_kuid(&init_user_ns, task_uid(task)),
		task->tgid, task->mm->total_vm, get_mm_rss(task->mm),
		mm_pgtables_bytes(task->mm),
		get_mm_counter(task->mm, MM_SWAPENTS),
		task->signal->oom_score_adj, task->comm);
	task_unlock(task);

	return 0;
}

 
static void dump_tasks(struct oom_control *oc)
{
	pr_info("Tasks state (memory values in pages):\n");
	pr_info("[  pid  ]   uid  tgid total_vm      rss pgtables_bytes swapents oom_score_adj name\n");

	if (is_memcg_oom(oc))
		mem_cgroup_scan_tasks(oc->memcg, dump_task, oc);
	else {
		struct task_struct *p;

		rcu_read_lock();
		for_each_process(p)
			dump_task(p, oc);
		rcu_read_unlock();
	}
}

static void dump_oom_summary(struct oom_control *oc, struct task_struct *victim)
{
	 
	pr_info("oom-kill:constraint=%s,nodemask=%*pbl",
			oom_constraint_text[oc->constraint],
			nodemask_pr_args(oc->nodemask));
	cpuset_print_current_mems_allowed();
	mem_cgroup_print_oom_context(oc->memcg, victim);
	pr_cont(",task=%s,pid=%d,uid=%d\n", victim->comm, victim->pid,
		from_kuid(&init_user_ns, task_uid(victim)));
}

static void dump_header(struct oom_control *oc, struct task_struct *p)
{
	pr_warn("%s invoked oom-killer: gfp_mask=%#x(%pGg), order=%d, oom_score_adj=%hd\n",
		current->comm, oc->gfp_mask, &oc->gfp_mask, oc->order,
			current->signal->oom_score_adj);
	if (!IS_ENABLED(CONFIG_COMPACTION) && oc->order)
		pr_warn("COMPACTION is disabled!!!\n");

	dump_stack();
	if (is_memcg_oom(oc))
		mem_cgroup_print_oom_meminfo(oc->memcg);
	else {
		__show_mem(SHOW_MEM_FILTER_NODES, oc->nodemask, gfp_zone(oc->gfp_mask));
		if (should_dump_unreclaim_slab())
			dump_unreclaimable_slab();
	}
	if (sysctl_oom_dump_tasks)
		dump_tasks(oc);
	if (p)
		dump_oom_summary(oc, p);
}

 
static atomic_t oom_victims = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(oom_victims_wait);

static bool oom_killer_disabled __read_mostly;

 
bool process_shares_mm(struct task_struct *p, struct mm_struct *mm)
{
	struct task_struct *t;

	for_each_thread(p, t) {
		struct mm_struct *t_mm = READ_ONCE(t->mm);
		if (t_mm)
			return t_mm == mm;
	}
	return false;
}

#ifdef CONFIG_MMU
 
static struct task_struct *oom_reaper_th;
static DECLARE_WAIT_QUEUE_HEAD(oom_reaper_wait);
static struct task_struct *oom_reaper_list;
static DEFINE_SPINLOCK(oom_reaper_lock);

static bool __oom_reap_task_mm(struct mm_struct *mm)
{
	struct vm_area_struct *vma;
	bool ret = true;
	VMA_ITERATOR(vmi, mm, 0);

	 
	set_bit(MMF_UNSTABLE, &mm->flags);

	for_each_vma(vmi, vma) {
		if (vma->vm_flags & (VM_HUGETLB|VM_PFNMAP))
			continue;

		 
		if (vma_is_anonymous(vma) || !(vma->vm_flags & VM_SHARED)) {
			struct mmu_notifier_range range;
			struct mmu_gather tlb;

			mmu_notifier_range_init(&range, MMU_NOTIFY_UNMAP, 0,
						mm, vma->vm_start,
						vma->vm_end);
			tlb_gather_mmu(&tlb, mm);
			if (mmu_notifier_invalidate_range_start_nonblock(&range)) {
				tlb_finish_mmu(&tlb);
				ret = false;
				continue;
			}
			unmap_page_range(&tlb, vma, range.start, range.end, NULL);
			mmu_notifier_invalidate_range_end(&range);
			tlb_finish_mmu(&tlb);
		}
	}

	return ret;
}

 
static bool oom_reap_task_mm(struct task_struct *tsk, struct mm_struct *mm)
{
	bool ret = true;

	if (!mmap_read_trylock(mm)) {
		trace_skip_task_reaping(tsk->pid);
		return false;
	}

	 
	if (test_bit(MMF_OOM_SKIP, &mm->flags)) {
		trace_skip_task_reaping(tsk->pid);
		goto out_unlock;
	}

	trace_start_task_reaping(tsk->pid);

	 
	ret = __oom_reap_task_mm(mm);
	if (!ret)
		goto out_finish;

	pr_info("oom_reaper: reaped process %d (%s), now anon-rss:%lukB, file-rss:%lukB, shmem-rss:%lukB\n",
			task_pid_nr(tsk), tsk->comm,
			K(get_mm_counter(mm, MM_ANONPAGES)),
			K(get_mm_counter(mm, MM_FILEPAGES)),
			K(get_mm_counter(mm, MM_SHMEMPAGES)));
out_finish:
	trace_finish_task_reaping(tsk->pid);
out_unlock:
	mmap_read_unlock(mm);

	return ret;
}

#define MAX_OOM_REAP_RETRIES 10
static void oom_reap_task(struct task_struct *tsk)
{
	int attempts = 0;
	struct mm_struct *mm = tsk->signal->oom_mm;

	 
	while (attempts++ < MAX_OOM_REAP_RETRIES && !oom_reap_task_mm(tsk, mm))
		schedule_timeout_idle(HZ/10);

	if (attempts <= MAX_OOM_REAP_RETRIES ||
	    test_bit(MMF_OOM_SKIP, &mm->flags))
		goto done;

	pr_info("oom_reaper: unable to reap pid:%d (%s)\n",
		task_pid_nr(tsk), tsk->comm);
	sched_show_task(tsk);
	debug_show_all_locks();

done:
	tsk->oom_reaper_list = NULL;

	 
	set_bit(MMF_OOM_SKIP, &mm->flags);

	 
	put_task_struct(tsk);
}

static int oom_reaper(void *unused)
{
	set_freezable();

	while (true) {
		struct task_struct *tsk = NULL;

		wait_event_freezable(oom_reaper_wait, oom_reaper_list != NULL);
		spin_lock_irq(&oom_reaper_lock);
		if (oom_reaper_list != NULL) {
			tsk = oom_reaper_list;
			oom_reaper_list = tsk->oom_reaper_list;
		}
		spin_unlock_irq(&oom_reaper_lock);

		if (tsk)
			oom_reap_task(tsk);
	}

	return 0;
}

static void wake_oom_reaper(struct timer_list *timer)
{
	struct task_struct *tsk = container_of(timer, struct task_struct,
			oom_reaper_timer);
	struct mm_struct *mm = tsk->signal->oom_mm;
	unsigned long flags;

	 
	if (test_bit(MMF_OOM_SKIP, &mm->flags)) {
		put_task_struct(tsk);
		return;
	}

	spin_lock_irqsave(&oom_reaper_lock, flags);
	tsk->oom_reaper_list = oom_reaper_list;
	oom_reaper_list = tsk;
	spin_unlock_irqrestore(&oom_reaper_lock, flags);
	trace_wake_reaper(tsk->pid);
	wake_up(&oom_reaper_wait);
}

 
#define OOM_REAPER_DELAY (2*HZ)
static void queue_oom_reaper(struct task_struct *tsk)
{
	 
	if (test_and_set_bit(MMF_OOM_REAP_QUEUED, &tsk->signal->oom_mm->flags))
		return;

	get_task_struct(tsk);
	timer_setup(&tsk->oom_reaper_timer, wake_oom_reaper, 0);
	tsk->oom_reaper_timer.expires = jiffies + OOM_REAPER_DELAY;
	add_timer(&tsk->oom_reaper_timer);
}

#ifdef CONFIG_SYSCTL
static struct ctl_table vm_oom_kill_table[] = {
	{
		.procname	= "panic_on_oom",
		.data		= &sysctl_panic_on_oom,
		.maxlen		= sizeof(sysctl_panic_on_oom),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_TWO,
	},
	{
		.procname	= "oom_kill_allocating_task",
		.data		= &sysctl_oom_kill_allocating_task,
		.maxlen		= sizeof(sysctl_oom_kill_allocating_task),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{
		.procname	= "oom_dump_tasks",
		.data		= &sysctl_oom_dump_tasks,
		.maxlen		= sizeof(sysctl_oom_dump_tasks),
		.mode		= 0644,
		.proc_handler	= proc_dointvec,
	},
	{}
};
#endif

static int __init oom_init(void)
{
	oom_reaper_th = kthread_run(oom_reaper, NULL, "oom_reaper");
#ifdef CONFIG_SYSCTL
	register_sysctl_init("vm", vm_oom_kill_table);
#endif
	return 0;
}
subsys_initcall(oom_init)
#else
static inline void queue_oom_reaper(struct task_struct *tsk)
{
}
#endif  

 
static void mark_oom_victim(struct task_struct *tsk)
{
	struct mm_struct *mm = tsk->mm;

	WARN_ON(oom_killer_disabled);
	 
	if (test_and_set_tsk_thread_flag(tsk, TIF_MEMDIE))
		return;

	 
	if (!cmpxchg(&tsk->signal->oom_mm, NULL, mm))
		mmgrab(tsk->signal->oom_mm);

	 
	__thaw_task(tsk);
	atomic_inc(&oom_victims);
	trace_mark_victim(tsk->pid);
}

 
void exit_oom_victim(void)
{
	clear_thread_flag(TIF_MEMDIE);

	if (!atomic_dec_return(&oom_victims))
		wake_up_all(&oom_victims_wait);
}

 
void oom_killer_enable(void)
{
	oom_killer_disabled = false;
	pr_info("OOM killer enabled.\n");
}

 
bool oom_killer_disable(signed long timeout)
{
	signed long ret;

	 
	if (mutex_lock_killable(&oom_lock))
		return false;
	oom_killer_disabled = true;
	mutex_unlock(&oom_lock);

	ret = wait_event_interruptible_timeout(oom_victims_wait,
			!atomic_read(&oom_victims), timeout);
	if (ret <= 0) {
		oom_killer_enable();
		return false;
	}
	pr_info("OOM killer disabled.\n");

	return true;
}

static inline bool __task_will_free_mem(struct task_struct *task)
{
	struct signal_struct *sig = task->signal;

	 
	if (sig->core_state)
		return false;

	if (sig->flags & SIGNAL_GROUP_EXIT)
		return true;

	if (thread_group_empty(task) && (task->flags & PF_EXITING))
		return true;

	return false;
}

 
static bool task_will_free_mem(struct task_struct *task)
{
	struct mm_struct *mm = task->mm;
	struct task_struct *p;
	bool ret = true;

	 
	if (!mm)
		return false;

	if (!__task_will_free_mem(task))
		return false;

	 
	if (test_bit(MMF_OOM_SKIP, &mm->flags))
		return false;

	if (atomic_read(&mm->mm_users) <= 1)
		return true;

	 
	rcu_read_lock();
	for_each_process(p) {
		if (!process_shares_mm(p, mm))
			continue;
		if (same_thread_group(task, p))
			continue;
		ret = __task_will_free_mem(p);
		if (!ret)
			break;
	}
	rcu_read_unlock();

	return ret;
}

static void __oom_kill_process(struct task_struct *victim, const char *message)
{
	struct task_struct *p;
	struct mm_struct *mm;
	bool can_oom_reap = true;

	p = find_lock_task_mm(victim);
	if (!p) {
		pr_info("%s: OOM victim %d (%s) is already exiting. Skip killing the task\n",
			message, task_pid_nr(victim), victim->comm);
		put_task_struct(victim);
		return;
	} else if (victim != p) {
		get_task_struct(p);
		put_task_struct(victim);
		victim = p;
	}

	 
	mm = victim->mm;
	mmgrab(mm);

	 
	count_vm_event(OOM_KILL);
	memcg_memory_event_mm(mm, MEMCG_OOM_KILL);

	 
	do_send_sig_info(SIGKILL, SEND_SIG_PRIV, victim, PIDTYPE_TGID);
	mark_oom_victim(victim);
	pr_err("%s: Killed process %d (%s) total-vm:%lukB, anon-rss:%lukB, file-rss:%lukB, shmem-rss:%lukB, UID:%u pgtables:%lukB oom_score_adj:%hd\n",
		message, task_pid_nr(victim), victim->comm, K(mm->total_vm),
		K(get_mm_counter(mm, MM_ANONPAGES)),
		K(get_mm_counter(mm, MM_FILEPAGES)),
		K(get_mm_counter(mm, MM_SHMEMPAGES)),
		from_kuid(&init_user_ns, task_uid(victim)),
		mm_pgtables_bytes(mm) >> 10, victim->signal->oom_score_adj);
	task_unlock(victim);

	 
	rcu_read_lock();
	for_each_process(p) {
		if (!process_shares_mm(p, mm))
			continue;
		if (same_thread_group(p, victim))
			continue;
		if (is_global_init(p)) {
			can_oom_reap = false;
			set_bit(MMF_OOM_SKIP, &mm->flags);
			pr_info("oom killer %d (%s) has mm pinned by %d (%s)\n",
					task_pid_nr(victim), victim->comm,
					task_pid_nr(p), p->comm);
			continue;
		}
		 
		if (unlikely(p->flags & PF_KTHREAD))
			continue;
		do_send_sig_info(SIGKILL, SEND_SIG_PRIV, p, PIDTYPE_TGID);
	}
	rcu_read_unlock();

	if (can_oom_reap)
		queue_oom_reaper(victim);

	mmdrop(mm);
	put_task_struct(victim);
}

 
static int oom_kill_memcg_member(struct task_struct *task, void *message)
{
	if (task->signal->oom_score_adj != OOM_SCORE_ADJ_MIN &&
	    !is_global_init(task)) {
		get_task_struct(task);
		__oom_kill_process(task, message);
	}
	return 0;
}

static void oom_kill_process(struct oom_control *oc, const char *message)
{
	struct task_struct *victim = oc->chosen;
	struct mem_cgroup *oom_group;
	static DEFINE_RATELIMIT_STATE(oom_rs, DEFAULT_RATELIMIT_INTERVAL,
					      DEFAULT_RATELIMIT_BURST);

	 
	task_lock(victim);
	if (task_will_free_mem(victim)) {
		mark_oom_victim(victim);
		queue_oom_reaper(victim);
		task_unlock(victim);
		put_task_struct(victim);
		return;
	}
	task_unlock(victim);

	if (__ratelimit(&oom_rs))
		dump_header(oc, victim);

	 
	oom_group = mem_cgroup_get_oom_group(victim, oc->memcg);

	__oom_kill_process(victim, message);

	 
	if (oom_group) {
		memcg_memory_event(oom_group, MEMCG_OOM_GROUP_KILL);
		mem_cgroup_print_oom_group(oom_group);
		mem_cgroup_scan_tasks(oom_group, oom_kill_memcg_member,
				      (void *)message);
		mem_cgroup_put(oom_group);
	}
}

 
static void check_panic_on_oom(struct oom_control *oc)
{
	if (likely(!sysctl_panic_on_oom))
		return;
	if (sysctl_panic_on_oom != 2) {
		 
		if (oc->constraint != CONSTRAINT_NONE)
			return;
	}
	 
	if (is_sysrq_oom(oc))
		return;
	dump_header(oc, NULL);
	panic("Out of memory: %s panic_on_oom is enabled\n",
		sysctl_panic_on_oom == 2 ? "compulsory" : "system-wide");
}

static BLOCKING_NOTIFIER_HEAD(oom_notify_list);

int register_oom_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&oom_notify_list, nb);
}
EXPORT_SYMBOL_GPL(register_oom_notifier);

int unregister_oom_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&oom_notify_list, nb);
}
EXPORT_SYMBOL_GPL(unregister_oom_notifier);

 
bool out_of_memory(struct oom_control *oc)
{
	unsigned long freed = 0;

	if (oom_killer_disabled)
		return false;

	if (!is_memcg_oom(oc)) {
		blocking_notifier_call_chain(&oom_notify_list, 0, &freed);
		if (freed > 0 && !is_sysrq_oom(oc))
			 
			return true;
	}

	 
	if (task_will_free_mem(current)) {
		mark_oom_victim(current);
		queue_oom_reaper(current);
		return true;
	}

	 
	if (!(oc->gfp_mask & __GFP_FS) && !is_memcg_oom(oc))
		return true;

	 
	oc->constraint = constrained_alloc(oc);
	if (oc->constraint != CONSTRAINT_MEMORY_POLICY)
		oc->nodemask = NULL;
	check_panic_on_oom(oc);

	if (!is_memcg_oom(oc) && sysctl_oom_kill_allocating_task &&
	    current->mm && !oom_unkillable_task(current) &&
	    oom_cpuset_eligible(current, oc) &&
	    current->signal->oom_score_adj != OOM_SCORE_ADJ_MIN) {
		get_task_struct(current);
		oc->chosen = current;
		oom_kill_process(oc, "Out of memory (oom_kill_allocating_task)");
		return true;
	}

	select_bad_process(oc);
	 
	if (!oc->chosen) {
		dump_header(oc, NULL);
		pr_warn("Out of memory and no killable processes...\n");
		 
		if (!is_sysrq_oom(oc) && !is_memcg_oom(oc))
			panic("System is deadlocked on memory\n");
	}
	if (oc->chosen && oc->chosen != (void *)-1UL)
		oom_kill_process(oc, !is_memcg_oom(oc) ? "Out of memory" :
				 "Memory cgroup out of memory");
	return !!oc->chosen;
}

 
void pagefault_out_of_memory(void)
{
	static DEFINE_RATELIMIT_STATE(pfoom_rs, DEFAULT_RATELIMIT_INTERVAL,
				      DEFAULT_RATELIMIT_BURST);

	if (mem_cgroup_oom_synchronize(true))
		return;

	if (fatal_signal_pending(current))
		return;

	if (__ratelimit(&pfoom_rs))
		pr_warn("Huh VM_FAULT_OOM leaked out to the #PF handler. Retrying PF\n");
}

SYSCALL_DEFINE2(process_mrelease, int, pidfd, unsigned int, flags)
{
#ifdef CONFIG_MMU
	struct mm_struct *mm = NULL;
	struct task_struct *task;
	struct task_struct *p;
	unsigned int f_flags;
	bool reap = false;
	long ret = 0;

	if (flags)
		return -EINVAL;

	task = pidfd_get_task(pidfd, &f_flags);
	if (IS_ERR(task))
		return PTR_ERR(task);

	 
	p = find_lock_task_mm(task);
	if (!p) {
		ret = -ESRCH;
		goto put_task;
	}

	mm = p->mm;
	mmgrab(mm);

	if (task_will_free_mem(p))
		reap = true;
	else {
		 
		if (!test_bit(MMF_OOM_SKIP, &mm->flags))
			ret = -EINVAL;
	}
	task_unlock(p);

	if (!reap)
		goto drop_mm;

	if (mmap_read_lock_killable(mm)) {
		ret = -EINTR;
		goto drop_mm;
	}
	 
	if (!test_bit(MMF_OOM_SKIP, &mm->flags) && !__oom_reap_task_mm(mm))
		ret = -EAGAIN;
	mmap_read_unlock(mm);

drop_mm:
	mmdrop(mm);
put_task:
	put_task_struct(task);
	return ret;
#else
	return -ENOSYS;
#endif  
}
