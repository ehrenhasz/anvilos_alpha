
 

#ifdef CONFIG_SMP
static int
select_task_rq_stop(struct task_struct *p, int cpu, int flags)
{
	return task_cpu(p);  
}

static int
balance_stop(struct rq *rq, struct task_struct *prev, struct rq_flags *rf)
{
	return sched_stop_runnable(rq);
}
#endif  

static void
check_preempt_curr_stop(struct rq *rq, struct task_struct *p, int flags)
{
	 
}

static void set_next_task_stop(struct rq *rq, struct task_struct *stop, bool first)
{
	stop->se.exec_start = rq_clock_task(rq);
}

static struct task_struct *pick_task_stop(struct rq *rq)
{
	if (!sched_stop_runnable(rq))
		return NULL;

	return rq->stop;
}

static struct task_struct *pick_next_task_stop(struct rq *rq)
{
	struct task_struct *p = pick_task_stop(rq);

	if (p)
		set_next_task_stop(rq, p, true);

	return p;
}

static void
enqueue_task_stop(struct rq *rq, struct task_struct *p, int flags)
{
	add_nr_running(rq, 1);
}

static void
dequeue_task_stop(struct rq *rq, struct task_struct *p, int flags)
{
	sub_nr_running(rq, 1);
}

static void yield_task_stop(struct rq *rq)
{
	BUG();  
}

static void put_prev_task_stop(struct rq *rq, struct task_struct *prev)
{
	struct task_struct *curr = rq->curr;
	u64 now, delta_exec;

	now = rq_clock_task(rq);
	delta_exec = now - curr->se.exec_start;
	if (unlikely((s64)delta_exec < 0))
		delta_exec = 0;

	schedstat_set(curr->stats.exec_max,
		      max(curr->stats.exec_max, delta_exec));

	update_current_exec_runtime(curr, now, delta_exec);
}

 
static void task_tick_stop(struct rq *rq, struct task_struct *curr, int queued)
{
}

static void switched_to_stop(struct rq *rq, struct task_struct *p)
{
	BUG();  
}

static void
prio_changed_stop(struct rq *rq, struct task_struct *p, int oldprio)
{
	BUG();  
}

static void update_curr_stop(struct rq *rq)
{
}

 
DEFINE_SCHED_CLASS(stop) = {

	.enqueue_task		= enqueue_task_stop,
	.dequeue_task		= dequeue_task_stop,
	.yield_task		= yield_task_stop,

	.check_preempt_curr	= check_preempt_curr_stop,

	.pick_next_task		= pick_next_task_stop,
	.put_prev_task		= put_prev_task_stop,
	.set_next_task          = set_next_task_stop,

#ifdef CONFIG_SMP
	.balance		= balance_stop,
	.pick_task		= pick_task_stop,
	.select_task_rq		= select_task_rq_stop,
	.set_cpus_allowed	= set_cpus_allowed_common,
#endif

	.task_tick		= task_tick_stop,

	.prio_changed		= prio_changed_stop,
	.switched_to		= switched_to_stop,
	.update_curr		= update_curr_stop,
};
