
 

static int psi_bug __read_mostly;

DEFINE_STATIC_KEY_FALSE(psi_disabled);
static DEFINE_STATIC_KEY_TRUE(psi_cgroups_enabled);

#ifdef CONFIG_PSI_DEFAULT_DISABLED
static bool psi_enable;
#else
static bool psi_enable = true;
#endif
static int __init setup_psi(char *str)
{
	return kstrtobool(str, &psi_enable) == 0;
}
__setup("psi=", setup_psi);

 
#define PSI_FREQ	(2*HZ+1)	 
#define EXP_10s		1677		 
#define EXP_60s		1981		 
#define EXP_300s	2034		 

 
#define WINDOW_MAX_US 10000000	 
#define UPDATES_PER_WINDOW 10	 

 
static u64 psi_period __read_mostly;

 
static DEFINE_PER_CPU(struct psi_group_cpu, system_group_pcpu);
struct psi_group psi_system = {
	.pcpu = &system_group_pcpu,
};

static void psi_avgs_work(struct work_struct *work);

static void poll_timer_fn(struct timer_list *t);

static void group_init(struct psi_group *group)
{
	int cpu;

	group->enabled = true;
	for_each_possible_cpu(cpu)
		seqcount_init(&per_cpu_ptr(group->pcpu, cpu)->seq);
	group->avg_last_update = sched_clock();
	group->avg_next_update = group->avg_last_update + psi_period;
	mutex_init(&group->avgs_lock);

	 
	INIT_LIST_HEAD(&group->avg_triggers);
	memset(group->avg_nr_triggers, 0, sizeof(group->avg_nr_triggers));
	INIT_DELAYED_WORK(&group->avgs_work, psi_avgs_work);

	 
	atomic_set(&group->rtpoll_scheduled, 0);
	mutex_init(&group->rtpoll_trigger_lock);
	INIT_LIST_HEAD(&group->rtpoll_triggers);
	group->rtpoll_min_period = U32_MAX;
	group->rtpoll_next_update = ULLONG_MAX;
	init_waitqueue_head(&group->rtpoll_wait);
	timer_setup(&group->rtpoll_timer, poll_timer_fn, 0);
	rcu_assign_pointer(group->rtpoll_task, NULL);
}

void __init psi_init(void)
{
	if (!psi_enable) {
		static_branch_enable(&psi_disabled);
		static_branch_disable(&psi_cgroups_enabled);
		return;
	}

	if (!cgroup_psi_enabled())
		static_branch_disable(&psi_cgroups_enabled);

	psi_period = jiffies_to_nsecs(PSI_FREQ);
	group_init(&psi_system);
}

static bool test_state(unsigned int *tasks, enum psi_states state, bool oncpu)
{
	switch (state) {
	case PSI_IO_SOME:
		return unlikely(tasks[NR_IOWAIT]);
	case PSI_IO_FULL:
		return unlikely(tasks[NR_IOWAIT] && !tasks[NR_RUNNING]);
	case PSI_MEM_SOME:
		return unlikely(tasks[NR_MEMSTALL]);
	case PSI_MEM_FULL:
		return unlikely(tasks[NR_MEMSTALL] &&
			tasks[NR_RUNNING] == tasks[NR_MEMSTALL_RUNNING]);
	case PSI_CPU_SOME:
		return unlikely(tasks[NR_RUNNING] > oncpu);
	case PSI_CPU_FULL:
		return unlikely(tasks[NR_RUNNING] && !oncpu);
	case PSI_NONIDLE:
		return tasks[NR_IOWAIT] || tasks[NR_MEMSTALL] ||
			tasks[NR_RUNNING];
	default:
		return false;
	}
}

static void get_recent_times(struct psi_group *group, int cpu,
			     enum psi_aggregators aggregator, u32 *times,
			     u32 *pchanged_states)
{
	struct psi_group_cpu *groupc = per_cpu_ptr(group->pcpu, cpu);
	int current_cpu = raw_smp_processor_id();
	unsigned int tasks[NR_PSI_TASK_COUNTS];
	u64 now, state_start;
	enum psi_states s;
	unsigned int seq;
	u32 state_mask;

	*pchanged_states = 0;

	 
	do {
		seq = read_seqcount_begin(&groupc->seq);
		now = cpu_clock(cpu);
		memcpy(times, groupc->times, sizeof(groupc->times));
		state_mask = groupc->state_mask;
		state_start = groupc->state_start;
		if (cpu == current_cpu)
			memcpy(tasks, groupc->tasks, sizeof(groupc->tasks));
	} while (read_seqcount_retry(&groupc->seq, seq));

	 
	for (s = 0; s < NR_PSI_STATES; s++) {
		u32 delta;
		 
		if (state_mask & (1 << s))
			times[s] += now - state_start;

		delta = times[s] - groupc->times_prev[aggregator][s];
		groupc->times_prev[aggregator][s] = times[s];

		times[s] = delta;
		if (delta)
			*pchanged_states |= (1 << s);
	}

	 
	if (current_work() == &group->avgs_work.work) {
		bool reschedule;

		if (cpu == current_cpu)
			reschedule = tasks[NR_RUNNING] +
				     tasks[NR_IOWAIT] +
				     tasks[NR_MEMSTALL] > 1;
		else
			reschedule = *pchanged_states & (1 << PSI_NONIDLE);

		if (reschedule)
			*pchanged_states |= PSI_STATE_RESCHEDULE;
	}
}

static void calc_avgs(unsigned long avg[3], int missed_periods,
		      u64 time, u64 period)
{
	unsigned long pct;

	 
	if (missed_periods) {
		avg[0] = calc_load_n(avg[0], EXP_10s, 0, missed_periods);
		avg[1] = calc_load_n(avg[1], EXP_60s, 0, missed_periods);
		avg[2] = calc_load_n(avg[2], EXP_300s, 0, missed_periods);
	}

	 
	pct = div_u64(time * 100, period);
	pct *= FIXED_1;
	avg[0] = calc_load(avg[0], EXP_10s, pct);
	avg[1] = calc_load(avg[1], EXP_60s, pct);
	avg[2] = calc_load(avg[2], EXP_300s, pct);
}

static void collect_percpu_times(struct psi_group *group,
				 enum psi_aggregators aggregator,
				 u32 *pchanged_states)
{
	u64 deltas[NR_PSI_STATES - 1] = { 0, };
	unsigned long nonidle_total = 0;
	u32 changed_states = 0;
	int cpu;
	int s;

	 
	for_each_possible_cpu(cpu) {
		u32 times[NR_PSI_STATES];
		u32 nonidle;
		u32 cpu_changed_states;

		get_recent_times(group, cpu, aggregator, times,
				&cpu_changed_states);
		changed_states |= cpu_changed_states;

		nonidle = nsecs_to_jiffies(times[PSI_NONIDLE]);
		nonidle_total += nonidle;

		for (s = 0; s < PSI_NONIDLE; s++)
			deltas[s] += (u64)times[s] * nonidle;
	}

	 

	 
	for (s = 0; s < NR_PSI_STATES - 1; s++)
		group->total[aggregator][s] +=
				div_u64(deltas[s], max(nonidle_total, 1UL));

	if (pchanged_states)
		*pchanged_states = changed_states;
}

 
static void window_reset(struct psi_window *win, u64 now, u64 value,
			 u64 prev_growth)
{
	win->start_time = now;
	win->start_value = value;
	win->prev_growth = prev_growth;
}

 
static u64 window_update(struct psi_window *win, u64 now, u64 value)
{
	u64 elapsed;
	u64 growth;

	elapsed = now - win->start_time;
	growth = value - win->start_value;
	 
	if (elapsed > win->size)
		window_reset(win, now, value, growth);
	else {
		u32 remaining;

		remaining = win->size - elapsed;
		growth += div64_u64(win->prev_growth * remaining, win->size);
	}

	return growth;
}

static u64 update_triggers(struct psi_group *group, u64 now, bool *update_total,
						   enum psi_aggregators aggregator)
{
	struct psi_trigger *t;
	u64 *total = group->total[aggregator];
	struct list_head *triggers;
	u64 *aggregator_total;
	*update_total = false;

	if (aggregator == PSI_AVGS) {
		triggers = &group->avg_triggers;
		aggregator_total = group->avg_total;
	} else {
		triggers = &group->rtpoll_triggers;
		aggregator_total = group->rtpoll_total;
	}

	 
	list_for_each_entry(t, triggers, node) {
		u64 growth;
		bool new_stall;

		new_stall = aggregator_total[t->state] != total[t->state];

		 
		if (!new_stall && !t->pending_event)
			continue;
		 
		if (new_stall) {
			 
			*update_total = true;

			 
			growth = window_update(&t->win, now, total[t->state]);
			if (!t->pending_event) {
				if (growth < t->threshold)
					continue;

				t->pending_event = true;
			}
		}
		 
		if (now < t->last_event_time + t->win.size)
			continue;

		 
		if (cmpxchg(&t->event, 0, 1) == 0) {
			if (t->of)
				kernfs_notify(t->of->kn);
			else
				wake_up_interruptible(&t->event_wait);
		}
		t->last_event_time = now;
		 
		t->pending_event = false;
	}

	return now + group->rtpoll_min_period;
}

static u64 update_averages(struct psi_group *group, u64 now)
{
	unsigned long missed_periods = 0;
	u64 expires, period;
	u64 avg_next_update;
	int s;

	 
	expires = group->avg_next_update;
	if (now - expires >= psi_period)
		missed_periods = div_u64(now - expires, psi_period);

	 
	avg_next_update = expires + ((1 + missed_periods) * psi_period);
	period = now - (group->avg_last_update + (missed_periods * psi_period));
	group->avg_last_update = now;

	for (s = 0; s < NR_PSI_STATES - 1; s++) {
		u32 sample;

		sample = group->total[PSI_AVGS][s] - group->avg_total[s];
		 
		if (sample > period)
			sample = period;
		group->avg_total[s] += sample;
		calc_avgs(group->avg[s], missed_periods, sample, period);
	}

	return avg_next_update;
}

static void psi_avgs_work(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct psi_group *group;
	u32 changed_states;
	bool update_total;
	u64 now;

	dwork = to_delayed_work(work);
	group = container_of(dwork, struct psi_group, avgs_work);

	mutex_lock(&group->avgs_lock);

	now = sched_clock();

	collect_percpu_times(group, PSI_AVGS, &changed_states);
	 
	if (now >= group->avg_next_update) {
		update_triggers(group, now, &update_total, PSI_AVGS);
		group->avg_next_update = update_averages(group, now);
	}

	if (changed_states & PSI_STATE_RESCHEDULE) {
		schedule_delayed_work(dwork, nsecs_to_jiffies(
				group->avg_next_update - now) + 1);
	}

	mutex_unlock(&group->avgs_lock);
}

static void init_rtpoll_triggers(struct psi_group *group, u64 now)
{
	struct psi_trigger *t;

	list_for_each_entry(t, &group->rtpoll_triggers, node)
		window_reset(&t->win, now,
				group->total[PSI_POLL][t->state], 0);
	memcpy(group->rtpoll_total, group->total[PSI_POLL],
		   sizeof(group->rtpoll_total));
	group->rtpoll_next_update = now + group->rtpoll_min_period;
}

 
static void psi_schedule_rtpoll_work(struct psi_group *group, unsigned long delay,
				   bool force)
{
	struct task_struct *task;

	 
	if (atomic_xchg(&group->rtpoll_scheduled, 1) && !force)
		return;

	rcu_read_lock();

	task = rcu_dereference(group->rtpoll_task);
	 
	if (likely(task))
		mod_timer(&group->rtpoll_timer, jiffies + delay);
	else
		atomic_set(&group->rtpoll_scheduled, 0);

	rcu_read_unlock();
}

static void psi_rtpoll_work(struct psi_group *group)
{
	bool force_reschedule = false;
	u32 changed_states;
	bool update_total;
	u64 now;

	mutex_lock(&group->rtpoll_trigger_lock);

	now = sched_clock();

	if (now > group->rtpoll_until) {
		 
		atomic_set(&group->rtpoll_scheduled, 0);
		 
		smp_mb();
	} else {
		 
		force_reschedule = true;
	}


	collect_percpu_times(group, PSI_POLL, &changed_states);

	if (changed_states & group->rtpoll_states) {
		 
		if (now > group->rtpoll_until)
			init_rtpoll_triggers(group, now);

		 
		group->rtpoll_until = now +
			group->rtpoll_min_period * UPDATES_PER_WINDOW;
	}

	if (now > group->rtpoll_until) {
		group->rtpoll_next_update = ULLONG_MAX;
		goto out;
	}

	if (now >= group->rtpoll_next_update) {
		group->rtpoll_next_update = update_triggers(group, now, &update_total, PSI_POLL);
		if (update_total)
			memcpy(group->rtpoll_total, group->total[PSI_POLL],
				   sizeof(group->rtpoll_total));
	}

	psi_schedule_rtpoll_work(group,
		nsecs_to_jiffies(group->rtpoll_next_update - now) + 1,
		force_reschedule);

out:
	mutex_unlock(&group->rtpoll_trigger_lock);
}

static int psi_rtpoll_worker(void *data)
{
	struct psi_group *group = (struct psi_group *)data;

	sched_set_fifo_low(current);

	while (true) {
		wait_event_interruptible(group->rtpoll_wait,
				atomic_cmpxchg(&group->rtpoll_wakeup, 1, 0) ||
				kthread_should_stop());
		if (kthread_should_stop())
			break;

		psi_rtpoll_work(group);
	}
	return 0;
}

static void poll_timer_fn(struct timer_list *t)
{
	struct psi_group *group = from_timer(group, t, rtpoll_timer);

	atomic_set(&group->rtpoll_wakeup, 1);
	wake_up_interruptible(&group->rtpoll_wait);
}

static void record_times(struct psi_group_cpu *groupc, u64 now)
{
	u32 delta;

	delta = now - groupc->state_start;
	groupc->state_start = now;

	if (groupc->state_mask & (1 << PSI_IO_SOME)) {
		groupc->times[PSI_IO_SOME] += delta;
		if (groupc->state_mask & (1 << PSI_IO_FULL))
			groupc->times[PSI_IO_FULL] += delta;
	}

	if (groupc->state_mask & (1 << PSI_MEM_SOME)) {
		groupc->times[PSI_MEM_SOME] += delta;
		if (groupc->state_mask & (1 << PSI_MEM_FULL))
			groupc->times[PSI_MEM_FULL] += delta;
	}

	if (groupc->state_mask & (1 << PSI_CPU_SOME)) {
		groupc->times[PSI_CPU_SOME] += delta;
		if (groupc->state_mask & (1 << PSI_CPU_FULL))
			groupc->times[PSI_CPU_FULL] += delta;
	}

	if (groupc->state_mask & (1 << PSI_NONIDLE))
		groupc->times[PSI_NONIDLE] += delta;
}

static void psi_group_change(struct psi_group *group, int cpu,
			     unsigned int clear, unsigned int set, u64 now,
			     bool wake_clock)
{
	struct psi_group_cpu *groupc;
	unsigned int t, m;
	enum psi_states s;
	u32 state_mask;

	groupc = per_cpu_ptr(group->pcpu, cpu);

	 
	write_seqcount_begin(&groupc->seq);

	 
	if (unlikely(clear & TSK_ONCPU)) {
		state_mask = 0;
		clear &= ~TSK_ONCPU;
	} else if (unlikely(set & TSK_ONCPU)) {
		state_mask = PSI_ONCPU;
		set &= ~TSK_ONCPU;
	} else {
		state_mask = groupc->state_mask & PSI_ONCPU;
	}

	 
	for (t = 0, m = clear; m; m &= ~(1 << t), t++) {
		if (!(m & (1 << t)))
			continue;
		if (groupc->tasks[t]) {
			groupc->tasks[t]--;
		} else if (!psi_bug) {
			printk_deferred(KERN_ERR "psi: task underflow! cpu=%d t=%d tasks=[%u %u %u %u] clear=%x set=%x\n",
					cpu, t, groupc->tasks[0],
					groupc->tasks[1], groupc->tasks[2],
					groupc->tasks[3], clear, set);
			psi_bug = 1;
		}
	}

	for (t = 0; set; set &= ~(1 << t), t++)
		if (set & (1 << t))
			groupc->tasks[t]++;

	if (!group->enabled) {
		 
		if (unlikely(groupc->state_mask & (1 << PSI_NONIDLE)))
			record_times(groupc, now);

		groupc->state_mask = state_mask;

		write_seqcount_end(&groupc->seq);
		return;
	}

	for (s = 0; s < NR_PSI_STATES; s++) {
		if (test_state(groupc->tasks, s, state_mask & PSI_ONCPU))
			state_mask |= (1 << s);
	}

	 
	if (unlikely((state_mask & PSI_ONCPU) && cpu_curr(cpu)->in_memstall))
		state_mask |= (1 << PSI_MEM_FULL);

	record_times(groupc, now);

	groupc->state_mask = state_mask;

	write_seqcount_end(&groupc->seq);

	if (state_mask & group->rtpoll_states)
		psi_schedule_rtpoll_work(group, 1, false);

	if (wake_clock && !delayed_work_pending(&group->avgs_work))
		schedule_delayed_work(&group->avgs_work, PSI_FREQ);
}

static inline struct psi_group *task_psi_group(struct task_struct *task)
{
#ifdef CONFIG_CGROUPS
	if (static_branch_likely(&psi_cgroups_enabled))
		return cgroup_psi(task_dfl_cgroup(task));
#endif
	return &psi_system;
}

static void psi_flags_change(struct task_struct *task, int clear, int set)
{
	if (((task->psi_flags & set) ||
	     (task->psi_flags & clear) != clear) &&
	    !psi_bug) {
		printk_deferred(KERN_ERR "psi: inconsistent task state! task=%d:%s cpu=%d psi_flags=%x clear=%x set=%x\n",
				task->pid, task->comm, task_cpu(task),
				task->psi_flags, clear, set);
		psi_bug = 1;
	}

	task->psi_flags &= ~clear;
	task->psi_flags |= set;
}

void psi_task_change(struct task_struct *task, int clear, int set)
{
	int cpu = task_cpu(task);
	struct psi_group *group;
	u64 now;

	if (!task->pid)
		return;

	psi_flags_change(task, clear, set);

	now = cpu_clock(cpu);

	group = task_psi_group(task);
	do {
		psi_group_change(group, cpu, clear, set, now, true);
	} while ((group = group->parent));
}

void psi_task_switch(struct task_struct *prev, struct task_struct *next,
		     bool sleep)
{
	struct psi_group *group, *common = NULL;
	int cpu = task_cpu(prev);
	u64 now = cpu_clock(cpu);

	if (next->pid) {
		psi_flags_change(next, 0, TSK_ONCPU);
		 
		group = task_psi_group(next);
		do {
			if (per_cpu_ptr(group->pcpu, cpu)->state_mask &
			    PSI_ONCPU) {
				common = group;
				break;
			}

			psi_group_change(group, cpu, 0, TSK_ONCPU, now, true);
		} while ((group = group->parent));
	}

	if (prev->pid) {
		int clear = TSK_ONCPU, set = 0;
		bool wake_clock = true;

		 
		if (sleep) {
			clear |= TSK_RUNNING;
			if (prev->in_memstall)
				clear |= TSK_MEMSTALL_RUNNING;
			if (prev->in_iowait)
				set |= TSK_IOWAIT;

			 
			if (unlikely((prev->flags & PF_WQ_WORKER) &&
				     wq_worker_last_func(prev) == psi_avgs_work))
				wake_clock = false;
		}

		psi_flags_change(prev, clear, set);

		group = task_psi_group(prev);
		do {
			if (group == common)
				break;
			psi_group_change(group, cpu, clear, set, now, wake_clock);
		} while ((group = group->parent));

		 
		if ((prev->psi_flags ^ next->psi_flags) & ~TSK_ONCPU) {
			clear &= ~TSK_ONCPU;
			for (; group; group = group->parent)
				psi_group_change(group, cpu, clear, set, now, wake_clock);
		}
	}
}

#ifdef CONFIG_IRQ_TIME_ACCOUNTING
void psi_account_irqtime(struct task_struct *task, u32 delta)
{
	int cpu = task_cpu(task);
	struct psi_group *group;
	struct psi_group_cpu *groupc;
	u64 now;

	if (!task->pid)
		return;

	now = cpu_clock(cpu);

	group = task_psi_group(task);
	do {
		if (!group->enabled)
			continue;

		groupc = per_cpu_ptr(group->pcpu, cpu);

		write_seqcount_begin(&groupc->seq);

		record_times(groupc, now);
		groupc->times[PSI_IRQ_FULL] += delta;

		write_seqcount_end(&groupc->seq);

		if (group->rtpoll_states & (1 << PSI_IRQ_FULL))
			psi_schedule_rtpoll_work(group, 1, false);
	} while ((group = group->parent));
}
#endif

 
void psi_memstall_enter(unsigned long *flags)
{
	struct rq_flags rf;
	struct rq *rq;

	if (static_branch_likely(&psi_disabled))
		return;

	*flags = current->in_memstall;
	if (*flags)
		return;
	 
	rq = this_rq_lock_irq(&rf);

	current->in_memstall = 1;
	psi_task_change(current, 0, TSK_MEMSTALL | TSK_MEMSTALL_RUNNING);

	rq_unlock_irq(rq, &rf);
}
EXPORT_SYMBOL_GPL(psi_memstall_enter);

 
void psi_memstall_leave(unsigned long *flags)
{
	struct rq_flags rf;
	struct rq *rq;

	if (static_branch_likely(&psi_disabled))
		return;

	if (*flags)
		return;
	 
	rq = this_rq_lock_irq(&rf);

	current->in_memstall = 0;
	psi_task_change(current, TSK_MEMSTALL | TSK_MEMSTALL_RUNNING, 0);

	rq_unlock_irq(rq, &rf);
}
EXPORT_SYMBOL_GPL(psi_memstall_leave);

#ifdef CONFIG_CGROUPS
int psi_cgroup_alloc(struct cgroup *cgroup)
{
	if (!static_branch_likely(&psi_cgroups_enabled))
		return 0;

	cgroup->psi = kzalloc(sizeof(struct psi_group), GFP_KERNEL);
	if (!cgroup->psi)
		return -ENOMEM;

	cgroup->psi->pcpu = alloc_percpu(struct psi_group_cpu);
	if (!cgroup->psi->pcpu) {
		kfree(cgroup->psi);
		return -ENOMEM;
	}
	group_init(cgroup->psi);
	cgroup->psi->parent = cgroup_psi(cgroup_parent(cgroup));
	return 0;
}

void psi_cgroup_free(struct cgroup *cgroup)
{
	if (!static_branch_likely(&psi_cgroups_enabled))
		return;

	cancel_delayed_work_sync(&cgroup->psi->avgs_work);
	free_percpu(cgroup->psi->pcpu);
	 
	WARN_ONCE(cgroup->psi->rtpoll_states, "psi: trigger leak\n");
	kfree(cgroup->psi);
}

 
void cgroup_move_task(struct task_struct *task, struct css_set *to)
{
	unsigned int task_flags;
	struct rq_flags rf;
	struct rq *rq;

	if (!static_branch_likely(&psi_cgroups_enabled)) {
		 
		rcu_assign_pointer(task->cgroups, to);
		return;
	}

	rq = task_rq_lock(task, &rf);

	 
	task_flags = task->psi_flags;

	if (task_flags)
		psi_task_change(task, task_flags, 0);

	 
	rcu_assign_pointer(task->cgroups, to);

	if (task_flags)
		psi_task_change(task, 0, task_flags);

	task_rq_unlock(rq, task, &rf);
}

void psi_cgroup_restart(struct psi_group *group)
{
	int cpu;

	 
	if (!group->enabled)
		return;

	for_each_possible_cpu(cpu) {
		struct rq *rq = cpu_rq(cpu);
		struct rq_flags rf;
		u64 now;

		rq_lock_irq(rq, &rf);
		now = cpu_clock(cpu);
		psi_group_change(group, cpu, 0, 0, now, true);
		rq_unlock_irq(rq, &rf);
	}
}
#endif  

int psi_show(struct seq_file *m, struct psi_group *group, enum psi_res res)
{
	bool only_full = false;
	int full;
	u64 now;

	if (static_branch_likely(&psi_disabled))
		return -EOPNOTSUPP;

	 
	mutex_lock(&group->avgs_lock);
	now = sched_clock();
	collect_percpu_times(group, PSI_AVGS, NULL);
	if (now >= group->avg_next_update)
		group->avg_next_update = update_averages(group, now);
	mutex_unlock(&group->avgs_lock);

#ifdef CONFIG_IRQ_TIME_ACCOUNTING
	only_full = res == PSI_IRQ;
#endif

	for (full = 0; full < 2 - only_full; full++) {
		unsigned long avg[3] = { 0, };
		u64 total = 0;
		int w;

		 
		if (!(group == &psi_system && res == PSI_CPU && full)) {
			for (w = 0; w < 3; w++)
				avg[w] = group->avg[res * 2 + full][w];
			total = div_u64(group->total[PSI_AVGS][res * 2 + full],
					NSEC_PER_USEC);
		}

		seq_printf(m, "%s avg10=%lu.%02lu avg60=%lu.%02lu avg300=%lu.%02lu total=%llu\n",
			   full || only_full ? "full" : "some",
			   LOAD_INT(avg[0]), LOAD_FRAC(avg[0]),
			   LOAD_INT(avg[1]), LOAD_FRAC(avg[1]),
			   LOAD_INT(avg[2]), LOAD_FRAC(avg[2]),
			   total);
	}

	return 0;
}

struct psi_trigger *psi_trigger_create(struct psi_group *group, char *buf,
				       enum psi_res res, struct file *file,
				       struct kernfs_open_file *of)
{
	struct psi_trigger *t;
	enum psi_states state;
	u32 threshold_us;
	bool privileged;
	u32 window_us;

	if (static_branch_likely(&psi_disabled))
		return ERR_PTR(-EOPNOTSUPP);

	 
	privileged = cap_raised(file->f_cred->cap_effective, CAP_SYS_RESOURCE);

	if (sscanf(buf, "some %u %u", &threshold_us, &window_us) == 2)
		state = PSI_IO_SOME + res * 2;
	else if (sscanf(buf, "full %u %u", &threshold_us, &window_us) == 2)
		state = PSI_IO_FULL + res * 2;
	else
		return ERR_PTR(-EINVAL);

#ifdef CONFIG_IRQ_TIME_ACCOUNTING
	if (res == PSI_IRQ && --state != PSI_IRQ_FULL)
		return ERR_PTR(-EINVAL);
#endif

	if (state >= PSI_NONIDLE)
		return ERR_PTR(-EINVAL);

	if (window_us == 0 || window_us > WINDOW_MAX_US)
		return ERR_PTR(-EINVAL);

	 
	if (!privileged && window_us % 2000000)
		return ERR_PTR(-EINVAL);

	 
	if (threshold_us == 0 || threshold_us > window_us)
		return ERR_PTR(-EINVAL);

	t = kmalloc(sizeof(*t), GFP_KERNEL);
	if (!t)
		return ERR_PTR(-ENOMEM);

	t->group = group;
	t->state = state;
	t->threshold = threshold_us * NSEC_PER_USEC;
	t->win.size = window_us * NSEC_PER_USEC;
	window_reset(&t->win, sched_clock(),
			group->total[PSI_POLL][t->state], 0);

	t->event = 0;
	t->last_event_time = 0;
	t->of = of;
	if (!of)
		init_waitqueue_head(&t->event_wait);
	t->pending_event = false;
	t->aggregator = privileged ? PSI_POLL : PSI_AVGS;

	if (privileged) {
		mutex_lock(&group->rtpoll_trigger_lock);

		if (!rcu_access_pointer(group->rtpoll_task)) {
			struct task_struct *task;

			task = kthread_create(psi_rtpoll_worker, group, "psimon");
			if (IS_ERR(task)) {
				kfree(t);
				mutex_unlock(&group->rtpoll_trigger_lock);
				return ERR_CAST(task);
			}
			atomic_set(&group->rtpoll_wakeup, 0);
			wake_up_process(task);
			rcu_assign_pointer(group->rtpoll_task, task);
		}

		list_add(&t->node, &group->rtpoll_triggers);
		group->rtpoll_min_period = min(group->rtpoll_min_period,
			div_u64(t->win.size, UPDATES_PER_WINDOW));
		group->rtpoll_nr_triggers[t->state]++;
		group->rtpoll_states |= (1 << t->state);

		mutex_unlock(&group->rtpoll_trigger_lock);
	} else {
		mutex_lock(&group->avgs_lock);

		list_add(&t->node, &group->avg_triggers);
		group->avg_nr_triggers[t->state]++;

		mutex_unlock(&group->avgs_lock);
	}
	return t;
}

void psi_trigger_destroy(struct psi_trigger *t)
{
	struct psi_group *group;
	struct task_struct *task_to_destroy = NULL;

	 
	if (!t)
		return;

	group = t->group;
	 
	if (t->of)
		kernfs_notify(t->of->kn);
	else
		wake_up_interruptible(&t->event_wait);

	if (t->aggregator == PSI_AVGS) {
		mutex_lock(&group->avgs_lock);
		if (!list_empty(&t->node)) {
			list_del(&t->node);
			group->avg_nr_triggers[t->state]--;
		}
		mutex_unlock(&group->avgs_lock);
	} else {
		mutex_lock(&group->rtpoll_trigger_lock);
		if (!list_empty(&t->node)) {
			struct psi_trigger *tmp;
			u64 period = ULLONG_MAX;

			list_del(&t->node);
			group->rtpoll_nr_triggers[t->state]--;
			if (!group->rtpoll_nr_triggers[t->state])
				group->rtpoll_states &= ~(1 << t->state);
			 
			if (group->rtpoll_min_period == div_u64(t->win.size, UPDATES_PER_WINDOW)) {
				list_for_each_entry(tmp, &group->rtpoll_triggers, node)
					period = min(period, div_u64(tmp->win.size,
							UPDATES_PER_WINDOW));
				group->rtpoll_min_period = period;
			}
			 
			if (group->rtpoll_states == 0) {
				group->rtpoll_until = 0;
				task_to_destroy = rcu_dereference_protected(
						group->rtpoll_task,
						lockdep_is_held(&group->rtpoll_trigger_lock));
				rcu_assign_pointer(group->rtpoll_task, NULL);
				del_timer(&group->rtpoll_timer);
			}
		}
		mutex_unlock(&group->rtpoll_trigger_lock);
	}

	 
	synchronize_rcu();
	 
	if (task_to_destroy) {
		 
		kthread_stop(task_to_destroy);
		atomic_set(&group->rtpoll_scheduled, 0);
	}
	kfree(t);
}

__poll_t psi_trigger_poll(void **trigger_ptr,
				struct file *file, poll_table *wait)
{
	__poll_t ret = DEFAULT_POLLMASK;
	struct psi_trigger *t;

	if (static_branch_likely(&psi_disabled))
		return DEFAULT_POLLMASK | EPOLLERR | EPOLLPRI;

	t = smp_load_acquire(trigger_ptr);
	if (!t)
		return DEFAULT_POLLMASK | EPOLLERR | EPOLLPRI;

	if (t->of)
		kernfs_generic_poll(t->of, wait);
	else
		poll_wait(file, &t->event_wait, wait);

	if (cmpxchg(&t->event, 1, 0) == 1)
		ret |= EPOLLPRI;

	return ret;
}

#ifdef CONFIG_PROC_FS
static int psi_io_show(struct seq_file *m, void *v)
{
	return psi_show(m, &psi_system, PSI_IO);
}

static int psi_memory_show(struct seq_file *m, void *v)
{
	return psi_show(m, &psi_system, PSI_MEM);
}

static int psi_cpu_show(struct seq_file *m, void *v)
{
	return psi_show(m, &psi_system, PSI_CPU);
}

static int psi_io_open(struct inode *inode, struct file *file)
{
	return single_open(file, psi_io_show, NULL);
}

static int psi_memory_open(struct inode *inode, struct file *file)
{
	return single_open(file, psi_memory_show, NULL);
}

static int psi_cpu_open(struct inode *inode, struct file *file)
{
	return single_open(file, psi_cpu_show, NULL);
}

static ssize_t psi_write(struct file *file, const char __user *user_buf,
			 size_t nbytes, enum psi_res res)
{
	char buf[32];
	size_t buf_size;
	struct seq_file *seq;
	struct psi_trigger *new;

	if (static_branch_likely(&psi_disabled))
		return -EOPNOTSUPP;

	if (!nbytes)
		return -EINVAL;

	buf_size = min(nbytes, sizeof(buf));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	buf[buf_size - 1] = '\0';

	seq = file->private_data;

	 
	mutex_lock(&seq->lock);

	 
	if (seq->private) {
		mutex_unlock(&seq->lock);
		return -EBUSY;
	}

	new = psi_trigger_create(&psi_system, buf, res, file, NULL);
	if (IS_ERR(new)) {
		mutex_unlock(&seq->lock);
		return PTR_ERR(new);
	}

	smp_store_release(&seq->private, new);
	mutex_unlock(&seq->lock);

	return nbytes;
}

static ssize_t psi_io_write(struct file *file, const char __user *user_buf,
			    size_t nbytes, loff_t *ppos)
{
	return psi_write(file, user_buf, nbytes, PSI_IO);
}

static ssize_t psi_memory_write(struct file *file, const char __user *user_buf,
				size_t nbytes, loff_t *ppos)
{
	return psi_write(file, user_buf, nbytes, PSI_MEM);
}

static ssize_t psi_cpu_write(struct file *file, const char __user *user_buf,
			     size_t nbytes, loff_t *ppos)
{
	return psi_write(file, user_buf, nbytes, PSI_CPU);
}

static __poll_t psi_fop_poll(struct file *file, poll_table *wait)
{
	struct seq_file *seq = file->private_data;

	return psi_trigger_poll(&seq->private, file, wait);
}

static int psi_fop_release(struct inode *inode, struct file *file)
{
	struct seq_file *seq = file->private_data;

	psi_trigger_destroy(seq->private);
	return single_release(inode, file);
}

static const struct proc_ops psi_io_proc_ops = {
	.proc_open	= psi_io_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_write	= psi_io_write,
	.proc_poll	= psi_fop_poll,
	.proc_release	= psi_fop_release,
};

static const struct proc_ops psi_memory_proc_ops = {
	.proc_open	= psi_memory_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_write	= psi_memory_write,
	.proc_poll	= psi_fop_poll,
	.proc_release	= psi_fop_release,
};

static const struct proc_ops psi_cpu_proc_ops = {
	.proc_open	= psi_cpu_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_write	= psi_cpu_write,
	.proc_poll	= psi_fop_poll,
	.proc_release	= psi_fop_release,
};

#ifdef CONFIG_IRQ_TIME_ACCOUNTING
static int psi_irq_show(struct seq_file *m, void *v)
{
	return psi_show(m, &psi_system, PSI_IRQ);
}

static int psi_irq_open(struct inode *inode, struct file *file)
{
	return single_open(file, psi_irq_show, NULL);
}

static ssize_t psi_irq_write(struct file *file, const char __user *user_buf,
			     size_t nbytes, loff_t *ppos)
{
	return psi_write(file, user_buf, nbytes, PSI_IRQ);
}

static const struct proc_ops psi_irq_proc_ops = {
	.proc_open	= psi_irq_open,
	.proc_read	= seq_read,
	.proc_lseek	= seq_lseek,
	.proc_write	= psi_irq_write,
	.proc_poll	= psi_fop_poll,
	.proc_release	= psi_fop_release,
};
#endif

static int __init psi_proc_init(void)
{
	if (psi_enable) {
		proc_mkdir("pressure", NULL);
		proc_create("pressure/io", 0666, NULL, &psi_io_proc_ops);
		proc_create("pressure/memory", 0666, NULL, &psi_memory_proc_ops);
		proc_create("pressure/cpu", 0666, NULL, &psi_cpu_proc_ops);
#ifdef CONFIG_IRQ_TIME_ACCOUNTING
		proc_create("pressure/irq", 0666, NULL, &psi_irq_proc_ops);
#endif
	}
	return 0;
}
module_init(psi_proc_init);

#endif  
