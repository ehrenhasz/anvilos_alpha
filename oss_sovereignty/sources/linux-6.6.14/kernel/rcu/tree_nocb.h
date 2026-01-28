


#ifdef CONFIG_RCU_NOCB_CPU
static cpumask_var_t rcu_nocb_mask; 
static bool __read_mostly rcu_nocb_poll;    
static inline int rcu_lockdep_is_held_nocb(struct rcu_data *rdp)
{
	return lockdep_is_held(&rdp->nocb_lock);
}

static inline bool rcu_current_is_nocb_kthread(struct rcu_data *rdp)
{
	
	if (!rdp->nocb_cb_kthread || !rdp->nocb_gp_kthread)
		return true;

	if (current == rdp->nocb_cb_kthread || current == rdp->nocb_gp_kthread)
		if (in_task())
			return true;
	return false;
}





static int __init rcu_nocb_setup(char *str)
{
	alloc_bootmem_cpumask_var(&rcu_nocb_mask);
	if (*str == '=') {
		if (cpulist_parse(++str, rcu_nocb_mask)) {
			pr_warn("rcu_nocbs= bad CPU range, all CPUs set\n");
			cpumask_setall(rcu_nocb_mask);
		}
	}
	rcu_state.nocb_is_setup = true;
	return 1;
}
__setup("rcu_nocbs", rcu_nocb_setup);

static int __init parse_rcu_nocb_poll(char *arg)
{
	rcu_nocb_poll = true;
	return 1;
}
__setup("rcu_nocb_poll", parse_rcu_nocb_poll);


static int nocb_nobypass_lim_per_jiffy = 16 * 1000 / HZ;
module_param(nocb_nobypass_lim_per_jiffy, int, 0);


static void rcu_nocb_bypass_lock(struct rcu_data *rdp)
	__acquires(&rdp->nocb_bypass_lock)
{
	lockdep_assert_irqs_disabled();
	if (raw_spin_trylock(&rdp->nocb_bypass_lock))
		return;
	atomic_inc(&rdp->nocb_lock_contended);
	WARN_ON_ONCE(smp_processor_id() != rdp->cpu);
	smp_mb__after_atomic(); 
	raw_spin_lock(&rdp->nocb_bypass_lock);
	smp_mb__before_atomic(); 
	atomic_dec(&rdp->nocb_lock_contended);
}


static void rcu_nocb_wait_contended(struct rcu_data *rdp)
{
	WARN_ON_ONCE(smp_processor_id() != rdp->cpu);
	while (WARN_ON_ONCE(atomic_read(&rdp->nocb_lock_contended)))
		cpu_relax();
}


static bool rcu_nocb_bypass_trylock(struct rcu_data *rdp)
{
	lockdep_assert_irqs_disabled();
	return raw_spin_trylock(&rdp->nocb_bypass_lock);
}


static void rcu_nocb_bypass_unlock(struct rcu_data *rdp)
	__releases(&rdp->nocb_bypass_lock)
{
	lockdep_assert_irqs_disabled();
	raw_spin_unlock(&rdp->nocb_bypass_lock);
}


static void rcu_nocb_lock(struct rcu_data *rdp)
{
	lockdep_assert_irqs_disabled();
	if (!rcu_rdp_is_offloaded(rdp))
		return;
	raw_spin_lock(&rdp->nocb_lock);
}


static void rcu_nocb_unlock(struct rcu_data *rdp)
{
	if (rcu_rdp_is_offloaded(rdp)) {
		lockdep_assert_irqs_disabled();
		raw_spin_unlock(&rdp->nocb_lock);
	}
}


static void rcu_nocb_unlock_irqrestore(struct rcu_data *rdp,
				       unsigned long flags)
{
	if (rcu_rdp_is_offloaded(rdp)) {
		lockdep_assert_irqs_disabled();
		raw_spin_unlock_irqrestore(&rdp->nocb_lock, flags);
	} else {
		local_irq_restore(flags);
	}
}


static void rcu_lockdep_assert_cblist_protected(struct rcu_data *rdp)
{
	lockdep_assert_irqs_disabled();
	if (rcu_rdp_is_offloaded(rdp))
		lockdep_assert_held(&rdp->nocb_lock);
}


static void rcu_nocb_gp_cleanup(struct swait_queue_head *sq)
{
	swake_up_all(sq);
}

static struct swait_queue_head *rcu_nocb_gp_get(struct rcu_node *rnp)
{
	return &rnp->nocb_gp_wq[rcu_seq_ctr(rnp->gp_seq) & 0x1];
}

static void rcu_init_one_nocb(struct rcu_node *rnp)
{
	init_swait_queue_head(&rnp->nocb_gp_wq[0]);
	init_swait_queue_head(&rnp->nocb_gp_wq[1]);
}

static bool __wake_nocb_gp(struct rcu_data *rdp_gp,
			   struct rcu_data *rdp,
			   bool force, unsigned long flags)
	__releases(rdp_gp->nocb_gp_lock)
{
	bool needwake = false;

	if (!READ_ONCE(rdp_gp->nocb_gp_kthread)) {
		raw_spin_unlock_irqrestore(&rdp_gp->nocb_gp_lock, flags);
		trace_rcu_nocb_wake(rcu_state.name, rdp->cpu,
				    TPS("AlreadyAwake"));
		return false;
	}

	if (rdp_gp->nocb_defer_wakeup > RCU_NOCB_WAKE_NOT) {
		WRITE_ONCE(rdp_gp->nocb_defer_wakeup, RCU_NOCB_WAKE_NOT);
		del_timer(&rdp_gp->nocb_timer);
	}

	if (force || READ_ONCE(rdp_gp->nocb_gp_sleep)) {
		WRITE_ONCE(rdp_gp->nocb_gp_sleep, false);
		needwake = true;
	}
	raw_spin_unlock_irqrestore(&rdp_gp->nocb_gp_lock, flags);
	if (needwake) {
		trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, TPS("DoWake"));
		wake_up_process(rdp_gp->nocb_gp_kthread);
	}

	return needwake;
}


static bool wake_nocb_gp(struct rcu_data *rdp, bool force)
{
	unsigned long flags;
	struct rcu_data *rdp_gp = rdp->nocb_gp_rdp;

	raw_spin_lock_irqsave(&rdp_gp->nocb_gp_lock, flags);
	return __wake_nocb_gp(rdp_gp, rdp, force, flags);
}


#define LAZY_FLUSH_JIFFIES (10 * HZ)
static unsigned long jiffies_till_flush = LAZY_FLUSH_JIFFIES;

#ifdef CONFIG_RCU_LAZY

void rcu_lazy_set_jiffies_till_flush(unsigned long jif)
{
	jiffies_till_flush = jif;
}
EXPORT_SYMBOL(rcu_lazy_set_jiffies_till_flush);

unsigned long rcu_lazy_get_jiffies_till_flush(void)
{
	return jiffies_till_flush;
}
EXPORT_SYMBOL(rcu_lazy_get_jiffies_till_flush);
#endif


static void wake_nocb_gp_defer(struct rcu_data *rdp, int waketype,
			       const char *reason)
{
	unsigned long flags;
	struct rcu_data *rdp_gp = rdp->nocb_gp_rdp;

	raw_spin_lock_irqsave(&rdp_gp->nocb_gp_lock, flags);

	
	if (waketype == RCU_NOCB_WAKE_LAZY &&
	    rdp->nocb_defer_wakeup == RCU_NOCB_WAKE_NOT) {
		mod_timer(&rdp_gp->nocb_timer, jiffies + jiffies_till_flush);
		WRITE_ONCE(rdp_gp->nocb_defer_wakeup, waketype);
	} else if (waketype == RCU_NOCB_WAKE_BYPASS) {
		mod_timer(&rdp_gp->nocb_timer, jiffies + 2);
		WRITE_ONCE(rdp_gp->nocb_defer_wakeup, waketype);
	} else {
		if (rdp_gp->nocb_defer_wakeup < RCU_NOCB_WAKE)
			mod_timer(&rdp_gp->nocb_timer, jiffies + 1);
		if (rdp_gp->nocb_defer_wakeup < waketype)
			WRITE_ONCE(rdp_gp->nocb_defer_wakeup, waketype);
	}

	raw_spin_unlock_irqrestore(&rdp_gp->nocb_gp_lock, flags);

	trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, reason);
}


static bool rcu_nocb_do_flush_bypass(struct rcu_data *rdp, struct rcu_head *rhp_in,
				     unsigned long j, bool lazy)
{
	struct rcu_cblist rcl;
	struct rcu_head *rhp = rhp_in;

	WARN_ON_ONCE(!rcu_rdp_is_offloaded(rdp));
	rcu_lockdep_assert_cblist_protected(rdp);
	lockdep_assert_held(&rdp->nocb_bypass_lock);
	if (rhp && !rcu_cblist_n_cbs(&rdp->nocb_bypass)) {
		raw_spin_unlock(&rdp->nocb_bypass_lock);
		return false;
	}
	
	if (rhp)
		rcu_segcblist_inc_len(&rdp->cblist); 

	
	if (lazy && rhp) {
		rcu_cblist_enqueue(&rdp->nocb_bypass, rhp);
		rhp = NULL;
	}
	rcu_cblist_flush_enqueue(&rcl, &rdp->nocb_bypass, rhp);
	WRITE_ONCE(rdp->lazy_len, 0);

	rcu_segcblist_insert_pend_cbs(&rdp->cblist, &rcl);
	WRITE_ONCE(rdp->nocb_bypass_first, j);
	rcu_nocb_bypass_unlock(rdp);
	return true;
}


static bool rcu_nocb_flush_bypass(struct rcu_data *rdp, struct rcu_head *rhp,
				  unsigned long j, bool lazy)
{
	if (!rcu_rdp_is_offloaded(rdp))
		return true;
	rcu_lockdep_assert_cblist_protected(rdp);
	rcu_nocb_bypass_lock(rdp);
	return rcu_nocb_do_flush_bypass(rdp, rhp, j, lazy);
}


static void rcu_nocb_try_flush_bypass(struct rcu_data *rdp, unsigned long j)
{
	rcu_lockdep_assert_cblist_protected(rdp);
	if (!rcu_rdp_is_offloaded(rdp) ||
	    !rcu_nocb_bypass_trylock(rdp))
		return;
	WARN_ON_ONCE(!rcu_nocb_do_flush_bypass(rdp, NULL, j, false));
}


static bool rcu_nocb_try_bypass(struct rcu_data *rdp, struct rcu_head *rhp,
				bool *was_alldone, unsigned long flags,
				bool lazy)
{
	unsigned long c;
	unsigned long cur_gp_seq;
	unsigned long j = jiffies;
	long ncbs = rcu_cblist_n_cbs(&rdp->nocb_bypass);
	bool bypass_is_lazy = (ncbs == READ_ONCE(rdp->lazy_len));

	lockdep_assert_irqs_disabled();

	
	
	if (!rcu_rdp_is_offloaded(rdp)) {
		*was_alldone = !rcu_segcblist_pend_cbs(&rdp->cblist);
		return false;
	}

	
	
	if (!rcu_segcblist_completely_offloaded(&rdp->cblist)) {
		rcu_nocb_lock(rdp);
		*was_alldone = !rcu_segcblist_pend_cbs(&rdp->cblist);
		return false; 
	}

	
	if (rcu_scheduler_active != RCU_SCHEDULER_RUNNING) {
		rcu_nocb_lock(rdp);
		WARN_ON_ONCE(rcu_cblist_n_cbs(&rdp->nocb_bypass));
		*was_alldone = !rcu_segcblist_pend_cbs(&rdp->cblist);
		return false;
	}

	
	
	if (j == rdp->nocb_nobypass_last) {
		c = rdp->nocb_nobypass_count + 1;
	} else {
		WRITE_ONCE(rdp->nocb_nobypass_last, j);
		c = rdp->nocb_nobypass_count - nocb_nobypass_lim_per_jiffy;
		if (ULONG_CMP_LT(rdp->nocb_nobypass_count,
				 nocb_nobypass_lim_per_jiffy))
			c = 0;
		else if (c > nocb_nobypass_lim_per_jiffy)
			c = nocb_nobypass_lim_per_jiffy;
	}
	WRITE_ONCE(rdp->nocb_nobypass_count, c);

	
	
	
	
	if (rdp->nocb_nobypass_count < nocb_nobypass_lim_per_jiffy && !lazy) {
		rcu_nocb_lock(rdp);
		*was_alldone = !rcu_segcblist_pend_cbs(&rdp->cblist);
		if (*was_alldone)
			trace_rcu_nocb_wake(rcu_state.name, rdp->cpu,
					    TPS("FirstQ"));

		WARN_ON_ONCE(!rcu_nocb_flush_bypass(rdp, NULL, j, false));
		WARN_ON_ONCE(rcu_cblist_n_cbs(&rdp->nocb_bypass));
		return false; 
	}

	
	
	if ((ncbs && !bypass_is_lazy && j != READ_ONCE(rdp->nocb_bypass_first)) ||
	    (ncbs &&  bypass_is_lazy &&
	     (time_after(j, READ_ONCE(rdp->nocb_bypass_first) + jiffies_till_flush))) ||
	    ncbs >= qhimark) {
		rcu_nocb_lock(rdp);
		*was_alldone = !rcu_segcblist_pend_cbs(&rdp->cblist);

		if (!rcu_nocb_flush_bypass(rdp, rhp, j, lazy)) {
			if (*was_alldone)
				trace_rcu_nocb_wake(rcu_state.name, rdp->cpu,
						    TPS("FirstQ"));
			WARN_ON_ONCE(rcu_cblist_n_cbs(&rdp->nocb_bypass));
			return false; 
		}
		if (j != rdp->nocb_gp_adv_time &&
		    rcu_segcblist_nextgp(&rdp->cblist, &cur_gp_seq) &&
		    rcu_seq_done(&rdp->mynode->gp_seq, cur_gp_seq)) {
			rcu_advance_cbs_nowake(rdp->mynode, rdp);
			rdp->nocb_gp_adv_time = j;
		}

		
		
		
		__call_rcu_nocb_wake(rdp, *was_alldone, flags);

		return true; 
	}

	
	rcu_nocb_wait_contended(rdp);
	rcu_nocb_bypass_lock(rdp);
	ncbs = rcu_cblist_n_cbs(&rdp->nocb_bypass);
	rcu_segcblist_inc_len(&rdp->cblist); 
	rcu_cblist_enqueue(&rdp->nocb_bypass, rhp);

	if (lazy)
		WRITE_ONCE(rdp->lazy_len, rdp->lazy_len + 1);

	if (!ncbs) {
		WRITE_ONCE(rdp->nocb_bypass_first, j);
		trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, TPS("FirstBQ"));
	}
	rcu_nocb_bypass_unlock(rdp);
	smp_mb(); 
	
	
	
	
	
	
	
	if (ncbs && (!bypass_is_lazy || lazy)) {
		local_irq_restore(flags);
	} else {
		
		rcu_nocb_lock(rdp); 
		if (!rcu_segcblist_pend_cbs(&rdp->cblist)) {
			trace_rcu_nocb_wake(rcu_state.name, rdp->cpu,
					    TPS("FirstBQwake"));
			__call_rcu_nocb_wake(rdp, true, flags);
		} else {
			trace_rcu_nocb_wake(rcu_state.name, rdp->cpu,
					    TPS("FirstBQnoWake"));
			rcu_nocb_unlock_irqrestore(rdp, flags);
		}
	}
	return true; 
}


static void __call_rcu_nocb_wake(struct rcu_data *rdp, bool was_alldone,
				 unsigned long flags)
				 __releases(rdp->nocb_lock)
{
	long bypass_len;
	unsigned long cur_gp_seq;
	unsigned long j;
	long lazy_len;
	long len;
	struct task_struct *t;

	
	t = READ_ONCE(rdp->nocb_gp_kthread);
	if (rcu_nocb_poll || !t) {
		rcu_nocb_unlock_irqrestore(rdp, flags);
		trace_rcu_nocb_wake(rcu_state.name, rdp->cpu,
				    TPS("WakeNotPoll"));
		return;
	}
	
	len = rcu_segcblist_n_cbs(&rdp->cblist);
	bypass_len = rcu_cblist_n_cbs(&rdp->nocb_bypass);
	lazy_len = READ_ONCE(rdp->lazy_len);
	if (was_alldone) {
		rdp->qlen_last_fqs_check = len;
		
		if (lazy_len && bypass_len == lazy_len) {
			rcu_nocb_unlock_irqrestore(rdp, flags);
			wake_nocb_gp_defer(rdp, RCU_NOCB_WAKE_LAZY,
					   TPS("WakeLazy"));
		} else if (!irqs_disabled_flags(flags)) {
			
			rcu_nocb_unlock_irqrestore(rdp, flags);
			wake_nocb_gp(rdp, false);
			trace_rcu_nocb_wake(rcu_state.name, rdp->cpu,
					    TPS("WakeEmpty"));
		} else {
			rcu_nocb_unlock_irqrestore(rdp, flags);
			wake_nocb_gp_defer(rdp, RCU_NOCB_WAKE,
					   TPS("WakeEmptyIsDeferred"));
		}
	} else if (len > rdp->qlen_last_fqs_check + qhimark) {
		
		rdp->qlen_last_fqs_check = len;
		j = jiffies;
		if (j != rdp->nocb_gp_adv_time &&
		    rcu_segcblist_nextgp(&rdp->cblist, &cur_gp_seq) &&
		    rcu_seq_done(&rdp->mynode->gp_seq, cur_gp_seq)) {
			rcu_advance_cbs_nowake(rdp->mynode, rdp);
			rdp->nocb_gp_adv_time = j;
		}
		smp_mb(); 
		if ((rdp->nocb_cb_sleep ||
		     !rcu_segcblist_ready_cbs(&rdp->cblist)) &&
		    !timer_pending(&rdp->nocb_timer)) {
			rcu_nocb_unlock_irqrestore(rdp, flags);
			wake_nocb_gp_defer(rdp, RCU_NOCB_WAKE_FORCE,
					   TPS("WakeOvfIsDeferred"));
		} else {
			rcu_nocb_unlock_irqrestore(rdp, flags);
			trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, TPS("WakeNot"));
		}
	} else {
		rcu_nocb_unlock_irqrestore(rdp, flags);
		trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, TPS("WakeNot"));
	}
}

static int nocb_gp_toggle_rdp(struct rcu_data *rdp,
			       bool *wake_state)
{
	struct rcu_segcblist *cblist = &rdp->cblist;
	unsigned long flags;
	int ret;

	rcu_nocb_lock_irqsave(rdp, flags);
	if (rcu_segcblist_test_flags(cblist, SEGCBLIST_OFFLOADED) &&
	    !rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_GP)) {
		
		rcu_segcblist_set_flags(cblist, SEGCBLIST_KTHREAD_GP);
		if (rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_CB))
			*wake_state = true;
		ret = 1;
	} else if (!rcu_segcblist_test_flags(cblist, SEGCBLIST_OFFLOADED) &&
		   rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_GP)) {
		
		rcu_segcblist_clear_flags(cblist, SEGCBLIST_KTHREAD_GP);
		if (!rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_CB))
			*wake_state = true;
		ret = 0;
	} else {
		WARN_ON_ONCE(1);
		ret = -1;
	}

	rcu_nocb_unlock_irqrestore(rdp, flags);

	return ret;
}

static void nocb_gp_sleep(struct rcu_data *my_rdp, int cpu)
{
	trace_rcu_nocb_wake(rcu_state.name, cpu, TPS("Sleep"));
	swait_event_interruptible_exclusive(my_rdp->nocb_gp_wq,
					!READ_ONCE(my_rdp->nocb_gp_sleep));
	trace_rcu_nocb_wake(rcu_state.name, cpu, TPS("EndSleep"));
}


static void nocb_gp_wait(struct rcu_data *my_rdp)
{
	bool bypass = false;
	int __maybe_unused cpu = my_rdp->cpu;
	unsigned long cur_gp_seq;
	unsigned long flags;
	bool gotcbs = false;
	unsigned long j = jiffies;
	bool lazy = false;
	bool needwait_gp = false; 
	bool needwake;
	bool needwake_gp;
	struct rcu_data *rdp, *rdp_toggling = NULL;
	struct rcu_node *rnp;
	unsigned long wait_gp_seq = 0; 
	bool wasempty = false;

	
	WARN_ON_ONCE(my_rdp->nocb_gp_rdp != my_rdp);
	
	list_for_each_entry(rdp, &my_rdp->nocb_head_rdp, nocb_entry_rdp) {
		long bypass_ncbs;
		bool flush_bypass = false;
		long lazy_ncbs;

		trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, TPS("Check"));
		rcu_nocb_lock_irqsave(rdp, flags);
		lockdep_assert_held(&rdp->nocb_lock);
		bypass_ncbs = rcu_cblist_n_cbs(&rdp->nocb_bypass);
		lazy_ncbs = READ_ONCE(rdp->lazy_len);

		if (bypass_ncbs && (lazy_ncbs == bypass_ncbs) &&
		    (time_after(j, READ_ONCE(rdp->nocb_bypass_first) + jiffies_till_flush) ||
		     bypass_ncbs > 2 * qhimark)) {
			flush_bypass = true;
		} else if (bypass_ncbs && (lazy_ncbs != bypass_ncbs) &&
		    (time_after(j, READ_ONCE(rdp->nocb_bypass_first) + 1) ||
		     bypass_ncbs > 2 * qhimark)) {
			flush_bypass = true;
		} else if (!bypass_ncbs && rcu_segcblist_empty(&rdp->cblist)) {
			rcu_nocb_unlock_irqrestore(rdp, flags);
			continue; 
		}

		if (flush_bypass) {
			
			(void)rcu_nocb_try_flush_bypass(rdp, j);
			bypass_ncbs = rcu_cblist_n_cbs(&rdp->nocb_bypass);
			lazy_ncbs = READ_ONCE(rdp->lazy_len);
		}

		if (bypass_ncbs) {
			trace_rcu_nocb_wake(rcu_state.name, rdp->cpu,
					    bypass_ncbs == lazy_ncbs ? TPS("Lazy") : TPS("Bypass"));
			if (bypass_ncbs == lazy_ncbs)
				lazy = true;
			else
				bypass = true;
		}
		rnp = rdp->mynode;

		
		needwake_gp = false;
		if (!rcu_segcblist_restempty(&rdp->cblist,
					     RCU_NEXT_READY_TAIL) ||
		    (rcu_segcblist_nextgp(&rdp->cblist, &cur_gp_seq) &&
		     rcu_seq_done(&rnp->gp_seq, cur_gp_seq))) {
			raw_spin_lock_rcu_node(rnp); 
			needwake_gp = rcu_advance_cbs(rnp, rdp);
			wasempty = rcu_segcblist_restempty(&rdp->cblist,
							   RCU_NEXT_READY_TAIL);
			raw_spin_unlock_rcu_node(rnp); 
		}
		
		WARN_ON_ONCE(wasempty &&
			     !rcu_segcblist_restempty(&rdp->cblist,
						      RCU_NEXT_READY_TAIL));
		if (rcu_segcblist_nextgp(&rdp->cblist, &cur_gp_seq)) {
			if (!needwait_gp ||
			    ULONG_CMP_LT(cur_gp_seq, wait_gp_seq))
				wait_gp_seq = cur_gp_seq;
			needwait_gp = true;
			trace_rcu_nocb_wake(rcu_state.name, rdp->cpu,
					    TPS("NeedWaitGP"));
		}
		if (rcu_segcblist_ready_cbs(&rdp->cblist)) {
			needwake = rdp->nocb_cb_sleep;
			WRITE_ONCE(rdp->nocb_cb_sleep, false);
			smp_mb(); 
		} else {
			needwake = false;
		}
		rcu_nocb_unlock_irqrestore(rdp, flags);
		if (needwake) {
			swake_up_one(&rdp->nocb_cb_wq);
			gotcbs = true;
		}
		if (needwake_gp)
			rcu_gp_kthread_wake();
	}

	my_rdp->nocb_gp_bypass = bypass;
	my_rdp->nocb_gp_gp = needwait_gp;
	my_rdp->nocb_gp_seq = needwait_gp ? wait_gp_seq : 0;

	
	
	if (!rcu_nocb_poll) {
		
		if (lazy && !bypass) {
			wake_nocb_gp_defer(my_rdp, RCU_NOCB_WAKE_LAZY,
					TPS("WakeLazyIsDeferred"));
		
		} else if (bypass) {
			wake_nocb_gp_defer(my_rdp, RCU_NOCB_WAKE_BYPASS,
					TPS("WakeBypassIsDeferred"));
		}
	}

	if (rcu_nocb_poll) {
		
		if (gotcbs)
			trace_rcu_nocb_wake(rcu_state.name, cpu, TPS("Poll"));
		if (list_empty(&my_rdp->nocb_head_rdp)) {
			raw_spin_lock_irqsave(&my_rdp->nocb_gp_lock, flags);
			if (!my_rdp->nocb_toggling_rdp)
				WRITE_ONCE(my_rdp->nocb_gp_sleep, true);
			raw_spin_unlock_irqrestore(&my_rdp->nocb_gp_lock, flags);
			
			nocb_gp_sleep(my_rdp, cpu);
		} else {
			schedule_timeout_idle(1);
		}
	} else if (!needwait_gp) {
		
		nocb_gp_sleep(my_rdp, cpu);
	} else {
		rnp = my_rdp->mynode;
		trace_rcu_this_gp(rnp, my_rdp, wait_gp_seq, TPS("StartWait"));
		swait_event_interruptible_exclusive(
			rnp->nocb_gp_wq[rcu_seq_ctr(wait_gp_seq) & 0x1],
			rcu_seq_done(&rnp->gp_seq, wait_gp_seq) ||
			!READ_ONCE(my_rdp->nocb_gp_sleep));
		trace_rcu_this_gp(rnp, my_rdp, wait_gp_seq, TPS("EndWait"));
	}

	if (!rcu_nocb_poll) {
		raw_spin_lock_irqsave(&my_rdp->nocb_gp_lock, flags);
		
		rdp_toggling = my_rdp->nocb_toggling_rdp;
		if (rdp_toggling)
			my_rdp->nocb_toggling_rdp = NULL;

		if (my_rdp->nocb_defer_wakeup > RCU_NOCB_WAKE_NOT) {
			WRITE_ONCE(my_rdp->nocb_defer_wakeup, RCU_NOCB_WAKE_NOT);
			del_timer(&my_rdp->nocb_timer);
		}
		WRITE_ONCE(my_rdp->nocb_gp_sleep, true);
		raw_spin_unlock_irqrestore(&my_rdp->nocb_gp_lock, flags);
	} else {
		rdp_toggling = READ_ONCE(my_rdp->nocb_toggling_rdp);
		if (rdp_toggling) {
			
			raw_spin_lock_irqsave(&my_rdp->nocb_gp_lock, flags);
			my_rdp->nocb_toggling_rdp = NULL;
			raw_spin_unlock_irqrestore(&my_rdp->nocb_gp_lock, flags);
		}
	}

	if (rdp_toggling) {
		bool wake_state = false;
		int ret;

		ret = nocb_gp_toggle_rdp(rdp_toggling, &wake_state);
		if (ret == 1)
			list_add_tail(&rdp_toggling->nocb_entry_rdp, &my_rdp->nocb_head_rdp);
		else if (ret == 0)
			list_del(&rdp_toggling->nocb_entry_rdp);
		if (wake_state)
			swake_up_one(&rdp_toggling->nocb_state_wq);
	}

	my_rdp->nocb_gp_seq = -1;
	WARN_ON(signal_pending(current));
}


static int rcu_nocb_gp_kthread(void *arg)
{
	struct rcu_data *rdp = arg;

	for (;;) {
		WRITE_ONCE(rdp->nocb_gp_loops, rdp->nocb_gp_loops + 1);
		nocb_gp_wait(rdp);
		cond_resched_tasks_rcu_qs();
	}
	return 0;
}

static inline bool nocb_cb_can_run(struct rcu_data *rdp)
{
	u8 flags = SEGCBLIST_OFFLOADED | SEGCBLIST_KTHREAD_CB;

	return rcu_segcblist_test_flags(&rdp->cblist, flags);
}

static inline bool nocb_cb_wait_cond(struct rcu_data *rdp)
{
	return nocb_cb_can_run(rdp) && !READ_ONCE(rdp->nocb_cb_sleep);
}


static void nocb_cb_wait(struct rcu_data *rdp)
{
	struct rcu_segcblist *cblist = &rdp->cblist;
	unsigned long cur_gp_seq;
	unsigned long flags;
	bool needwake_state = false;
	bool needwake_gp = false;
	bool can_sleep = true;
	struct rcu_node *rnp = rdp->mynode;

	do {
		swait_event_interruptible_exclusive(rdp->nocb_cb_wq,
						    nocb_cb_wait_cond(rdp));

		
		if (smp_load_acquire(&rdp->nocb_cb_sleep)) { 
			WARN_ON(signal_pending(current));
			trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, TPS("WokeEmpty"));
		}
	} while (!nocb_cb_can_run(rdp));


	local_irq_save(flags);
	rcu_momentary_dyntick_idle();
	local_irq_restore(flags);
	
	local_bh_disable();
	rcu_do_batch(rdp);
	local_bh_enable();
	lockdep_assert_irqs_enabled();
	rcu_nocb_lock_irqsave(rdp, flags);
	if (rcu_segcblist_nextgp(cblist, &cur_gp_seq) &&
	    rcu_seq_done(&rnp->gp_seq, cur_gp_seq) &&
	    raw_spin_trylock_rcu_node(rnp)) { 
		needwake_gp = rcu_advance_cbs(rdp->mynode, rdp);
		raw_spin_unlock_rcu_node(rnp); 
	}

	if (rcu_segcblist_test_flags(cblist, SEGCBLIST_OFFLOADED)) {
		if (!rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_CB)) {
			rcu_segcblist_set_flags(cblist, SEGCBLIST_KTHREAD_CB);
			if (rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_GP))
				needwake_state = true;
		}
		if (rcu_segcblist_ready_cbs(cblist))
			can_sleep = false;
	} else {
		
		WARN_ON_ONCE(!rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_CB));
		rcu_segcblist_clear_flags(cblist, SEGCBLIST_KTHREAD_CB);
		if (!rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_GP))
			needwake_state = true;
	}

	WRITE_ONCE(rdp->nocb_cb_sleep, can_sleep);

	if (rdp->nocb_cb_sleep)
		trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, TPS("CBSleep"));

	rcu_nocb_unlock_irqrestore(rdp, flags);
	if (needwake_gp)
		rcu_gp_kthread_wake();

	if (needwake_state)
		swake_up_one(&rdp->nocb_state_wq);
}


static int rcu_nocb_cb_kthread(void *arg)
{
	struct rcu_data *rdp = arg;

	
	
	for (;;) {
		nocb_cb_wait(rdp);
		cond_resched_tasks_rcu_qs();
	}
	return 0;
}


static int rcu_nocb_need_deferred_wakeup(struct rcu_data *rdp, int level)
{
	return READ_ONCE(rdp->nocb_defer_wakeup) >= level;
}


static bool do_nocb_deferred_wakeup_common(struct rcu_data *rdp_gp,
					   struct rcu_data *rdp, int level,
					   unsigned long flags)
	__releases(rdp_gp->nocb_gp_lock)
{
	int ndw;
	int ret;

	if (!rcu_nocb_need_deferred_wakeup(rdp_gp, level)) {
		raw_spin_unlock_irqrestore(&rdp_gp->nocb_gp_lock, flags);
		return false;
	}

	ndw = rdp_gp->nocb_defer_wakeup;
	ret = __wake_nocb_gp(rdp_gp, rdp, ndw == RCU_NOCB_WAKE_FORCE, flags);
	trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, TPS("DeferredWake"));

	return ret;
}


static void do_nocb_deferred_wakeup_timer(struct timer_list *t)
{
	unsigned long flags;
	struct rcu_data *rdp = from_timer(rdp, t, nocb_timer);

	WARN_ON_ONCE(rdp->nocb_gp_rdp != rdp);
	trace_rcu_nocb_wake(rcu_state.name, rdp->cpu, TPS("Timer"));

	raw_spin_lock_irqsave(&rdp->nocb_gp_lock, flags);
	smp_mb__after_spinlock(); 
	do_nocb_deferred_wakeup_common(rdp, rdp, RCU_NOCB_WAKE_BYPASS, flags);
}


static bool do_nocb_deferred_wakeup(struct rcu_data *rdp)
{
	unsigned long flags;
	struct rcu_data *rdp_gp = rdp->nocb_gp_rdp;

	if (!rdp_gp || !rcu_nocb_need_deferred_wakeup(rdp_gp, RCU_NOCB_WAKE))
		return false;

	raw_spin_lock_irqsave(&rdp_gp->nocb_gp_lock, flags);
	return do_nocb_deferred_wakeup_common(rdp_gp, rdp, RCU_NOCB_WAKE, flags);
}

void rcu_nocb_flush_deferred_wakeup(void)
{
	do_nocb_deferred_wakeup(this_cpu_ptr(&rcu_data));
}
EXPORT_SYMBOL_GPL(rcu_nocb_flush_deferred_wakeup);

static int rdp_offload_toggle(struct rcu_data *rdp,
			       bool offload, unsigned long flags)
	__releases(rdp->nocb_lock)
{
	struct rcu_segcblist *cblist = &rdp->cblist;
	struct rcu_data *rdp_gp = rdp->nocb_gp_rdp;
	bool wake_gp = false;

	rcu_segcblist_offload(cblist, offload);

	if (rdp->nocb_cb_sleep)
		rdp->nocb_cb_sleep = false;
	rcu_nocb_unlock_irqrestore(rdp, flags);

	
	swake_up_one(&rdp->nocb_cb_wq);

	raw_spin_lock_irqsave(&rdp_gp->nocb_gp_lock, flags);
	
	WRITE_ONCE(rdp_gp->nocb_toggling_rdp, rdp);
	if (rdp_gp->nocb_gp_sleep) {
		rdp_gp->nocb_gp_sleep = false;
		wake_gp = true;
	}
	raw_spin_unlock_irqrestore(&rdp_gp->nocb_gp_lock, flags);

	return wake_gp;
}

static long rcu_nocb_rdp_deoffload(void *arg)
{
	struct rcu_data *rdp = arg;
	struct rcu_segcblist *cblist = &rdp->cblist;
	unsigned long flags;
	int wake_gp;
	struct rcu_data *rdp_gp = rdp->nocb_gp_rdp;

	
	WARN_ON_ONCE((rdp->cpu != raw_smp_processor_id()) && cpu_online(rdp->cpu));

	pr_info("De-offloading %d\n", rdp->cpu);

	rcu_nocb_lock_irqsave(rdp, flags);
	
	WARN_ON_ONCE(!rcu_nocb_flush_bypass(rdp, NULL, jiffies, false));
	
	rcu_segcblist_set_flags(cblist, SEGCBLIST_RCU_CORE);
	invoke_rcu_core();
	wake_gp = rdp_offload_toggle(rdp, false, flags);

	mutex_lock(&rdp_gp->nocb_gp_kthread_mutex);
	if (rdp_gp->nocb_gp_kthread) {
		if (wake_gp)
			wake_up_process(rdp_gp->nocb_gp_kthread);

		
		if (!rdp->nocb_cb_kthread) {
			rcu_nocb_lock_irqsave(rdp, flags);
			rcu_segcblist_clear_flags(&rdp->cblist, SEGCBLIST_KTHREAD_CB);
			rcu_nocb_unlock_irqrestore(rdp, flags);
		}

		swait_event_exclusive(rdp->nocb_state_wq,
					!rcu_segcblist_test_flags(cblist,
					  SEGCBLIST_KTHREAD_CB | SEGCBLIST_KTHREAD_GP));
	} else {
		
		rcu_nocb_lock_irqsave(rdp, flags);
		rcu_segcblist_clear_flags(&rdp->cblist,
				SEGCBLIST_KTHREAD_CB | SEGCBLIST_KTHREAD_GP);
		rcu_nocb_unlock_irqrestore(rdp, flags);

		list_del(&rdp->nocb_entry_rdp);
	}
	mutex_unlock(&rdp_gp->nocb_gp_kthread_mutex);

	
	rcu_nocb_lock_irqsave(rdp, flags);
	
	rcu_segcblist_clear_flags(cblist, SEGCBLIST_LOCKING);
	
	raw_spin_unlock_irqrestore(&rdp->nocb_lock, flags);

	
	WARN_ON_ONCE(rcu_cblist_n_cbs(&rdp->nocb_bypass));


	return 0;
}

int rcu_nocb_cpu_deoffload(int cpu)
{
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);
	int ret = 0;

	cpus_read_lock();
	mutex_lock(&rcu_state.barrier_mutex);
	if (rcu_rdp_is_offloaded(rdp)) {
		if (cpu_online(cpu)) {
			ret = work_on_cpu(cpu, rcu_nocb_rdp_deoffload, rdp);
			if (!ret)
				cpumask_clear_cpu(cpu, rcu_nocb_mask);
		} else {
			pr_info("NOCB: Cannot CB-deoffload offline CPU %d\n", rdp->cpu);
			ret = -EINVAL;
		}
	}
	mutex_unlock(&rcu_state.barrier_mutex);
	cpus_read_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(rcu_nocb_cpu_deoffload);

static long rcu_nocb_rdp_offload(void *arg)
{
	struct rcu_data *rdp = arg;
	struct rcu_segcblist *cblist = &rdp->cblist;
	unsigned long flags;
	int wake_gp;
	struct rcu_data *rdp_gp = rdp->nocb_gp_rdp;

	WARN_ON_ONCE(rdp->cpu != raw_smp_processor_id());
	
	if (!rdp->nocb_gp_rdp)
		return -EINVAL;

	if (WARN_ON_ONCE(!rdp_gp->nocb_gp_kthread))
		return -EINVAL;

	pr_info("Offloading %d\n", rdp->cpu);

	
	raw_spin_lock_irqsave(&rdp->nocb_lock, flags);

	
	wake_gp = rdp_offload_toggle(rdp, true, flags);
	if (wake_gp)
		wake_up_process(rdp_gp->nocb_gp_kthread);
	swait_event_exclusive(rdp->nocb_state_wq,
			      rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_CB) &&
			      rcu_segcblist_test_flags(cblist, SEGCBLIST_KTHREAD_GP));

	
	rcu_nocb_lock_irqsave(rdp, flags);
	rcu_segcblist_clear_flags(cblist, SEGCBLIST_RCU_CORE);
	rcu_nocb_unlock_irqrestore(rdp, flags);

	return 0;
}

int rcu_nocb_cpu_offload(int cpu)
{
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);
	int ret = 0;

	cpus_read_lock();
	mutex_lock(&rcu_state.barrier_mutex);
	if (!rcu_rdp_is_offloaded(rdp)) {
		if (cpu_online(cpu)) {
			ret = work_on_cpu(cpu, rcu_nocb_rdp_offload, rdp);
			if (!ret)
				cpumask_set_cpu(cpu, rcu_nocb_mask);
		} else {
			pr_info("NOCB: Cannot CB-offload offline CPU %d\n", rdp->cpu);
			ret = -EINVAL;
		}
	}
	mutex_unlock(&rcu_state.barrier_mutex);
	cpus_read_unlock();

	return ret;
}
EXPORT_SYMBOL_GPL(rcu_nocb_cpu_offload);

#ifdef CONFIG_RCU_LAZY
static unsigned long
lazy_rcu_shrink_count(struct shrinker *shrink, struct shrink_control *sc)
{
	int cpu;
	unsigned long count = 0;

	if (WARN_ON_ONCE(!cpumask_available(rcu_nocb_mask)))
		return 0;

	
	if (!mutex_trylock(&rcu_state.barrier_mutex))
		return 0;

	
	for_each_cpu(cpu, rcu_nocb_mask) {
		struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);

		count +=  READ_ONCE(rdp->lazy_len);
	}

	mutex_unlock(&rcu_state.barrier_mutex);

	return count ? count : SHRINK_EMPTY;
}

static unsigned long
lazy_rcu_shrink_scan(struct shrinker *shrink, struct shrink_control *sc)
{
	int cpu;
	unsigned long flags;
	unsigned long count = 0;

	if (WARN_ON_ONCE(!cpumask_available(rcu_nocb_mask)))
		return 0;
	
	if (!mutex_trylock(&rcu_state.barrier_mutex)) {
		
		return 0;
	}

	
	for_each_cpu(cpu, rcu_nocb_mask) {
		struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);
		int _count;

		if (WARN_ON_ONCE(!rcu_rdp_is_offloaded(rdp)))
			continue;

		if (!READ_ONCE(rdp->lazy_len))
			continue;

		rcu_nocb_lock_irqsave(rdp, flags);
		
		_count = READ_ONCE(rdp->lazy_len);
		if (!_count) {
			rcu_nocb_unlock_irqrestore(rdp, flags);
			continue;
		}
		WARN_ON_ONCE(!rcu_nocb_flush_bypass(rdp, NULL, jiffies, false));
		rcu_nocb_unlock_irqrestore(rdp, flags);
		wake_nocb_gp(rdp, false);
		sc->nr_to_scan -= _count;
		count += _count;
		if (sc->nr_to_scan <= 0)
			break;
	}

	mutex_unlock(&rcu_state.barrier_mutex);

	return count ? count : SHRINK_STOP;
}

static struct shrinker lazy_rcu_shrinker = {
	.count_objects = lazy_rcu_shrink_count,
	.scan_objects = lazy_rcu_shrink_scan,
	.batch = 0,
	.seeks = DEFAULT_SEEKS,
};
#endif 

void __init rcu_init_nohz(void)
{
	int cpu;
	struct rcu_data *rdp;
	const struct cpumask *cpumask = NULL;

#if defined(CONFIG_NO_HZ_FULL)
	if (tick_nohz_full_running && !cpumask_empty(tick_nohz_full_mask))
		cpumask = tick_nohz_full_mask;
#endif

	if (IS_ENABLED(CONFIG_RCU_NOCB_CPU_DEFAULT_ALL) &&
	    !rcu_state.nocb_is_setup && !cpumask)
		cpumask = cpu_possible_mask;

	if (cpumask) {
		if (!cpumask_available(rcu_nocb_mask)) {
			if (!zalloc_cpumask_var(&rcu_nocb_mask, GFP_KERNEL)) {
				pr_info("rcu_nocb_mask allocation failed, callback offloading disabled.\n");
				return;
			}
		}

		cpumask_or(rcu_nocb_mask, rcu_nocb_mask, cpumask);
		rcu_state.nocb_is_setup = true;
	}

	if (!rcu_state.nocb_is_setup)
		return;

#ifdef CONFIG_RCU_LAZY
	if (register_shrinker(&lazy_rcu_shrinker, "rcu-lazy"))
		pr_err("Failed to register lazy_rcu shrinker!\n");
#endif 

	if (!cpumask_subset(rcu_nocb_mask, cpu_possible_mask)) {
		pr_info("\tNote: kernel parameter 'rcu_nocbs=', 'nohz_full', or 'isolcpus=' contains nonexistent CPUs.\n");
		cpumask_and(rcu_nocb_mask, cpu_possible_mask,
			    rcu_nocb_mask);
	}
	if (cpumask_empty(rcu_nocb_mask))
		pr_info("\tOffload RCU callbacks from CPUs: (none).\n");
	else
		pr_info("\tOffload RCU callbacks from CPUs: %*pbl.\n",
			cpumask_pr_args(rcu_nocb_mask));
	if (rcu_nocb_poll)
		pr_info("\tPoll for callbacks from no-CBs CPUs.\n");

	for_each_cpu(cpu, rcu_nocb_mask) {
		rdp = per_cpu_ptr(&rcu_data, cpu);
		if (rcu_segcblist_empty(&rdp->cblist))
			rcu_segcblist_init(&rdp->cblist);
		rcu_segcblist_offload(&rdp->cblist, true);
		rcu_segcblist_set_flags(&rdp->cblist, SEGCBLIST_KTHREAD_CB | SEGCBLIST_KTHREAD_GP);
		rcu_segcblist_clear_flags(&rdp->cblist, SEGCBLIST_RCU_CORE);
	}
	rcu_organize_nocb_kthreads();
}


static void __init rcu_boot_init_nocb_percpu_data(struct rcu_data *rdp)
{
	init_swait_queue_head(&rdp->nocb_cb_wq);
	init_swait_queue_head(&rdp->nocb_gp_wq);
	init_swait_queue_head(&rdp->nocb_state_wq);
	raw_spin_lock_init(&rdp->nocb_lock);
	raw_spin_lock_init(&rdp->nocb_bypass_lock);
	raw_spin_lock_init(&rdp->nocb_gp_lock);
	timer_setup(&rdp->nocb_timer, do_nocb_deferred_wakeup_timer, 0);
	rcu_cblist_init(&rdp->nocb_bypass);
	WRITE_ONCE(rdp->lazy_len, 0);
	mutex_init(&rdp->nocb_gp_kthread_mutex);
}


static void rcu_spawn_cpu_nocb_kthread(int cpu)
{
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);
	struct rcu_data *rdp_gp;
	struct task_struct *t;
	struct sched_param sp;

	if (!rcu_scheduler_fully_active || !rcu_state.nocb_is_setup)
		return;

	
	if (rdp->nocb_cb_kthread)
		return;

	
	sp.sched_priority = kthread_prio;
	rdp_gp = rdp->nocb_gp_rdp;
	mutex_lock(&rdp_gp->nocb_gp_kthread_mutex);
	if (!rdp_gp->nocb_gp_kthread) {
		t = kthread_run(rcu_nocb_gp_kthread, rdp_gp,
				"rcuog/%d", rdp_gp->cpu);
		if (WARN_ONCE(IS_ERR(t), "%s: Could not start rcuo GP kthread, OOM is now expected behavior\n", __func__)) {
			mutex_unlock(&rdp_gp->nocb_gp_kthread_mutex);
			goto end;
		}
		WRITE_ONCE(rdp_gp->nocb_gp_kthread, t);
		if (kthread_prio)
			sched_setscheduler_nocheck(t, SCHED_FIFO, &sp);
	}
	mutex_unlock(&rdp_gp->nocb_gp_kthread_mutex);

	
	t = kthread_run(rcu_nocb_cb_kthread, rdp,
			"rcuo%c/%d", rcu_state.abbr, cpu);
	if (WARN_ONCE(IS_ERR(t), "%s: Could not start rcuo CB kthread, OOM is now expected behavior\n", __func__))
		goto end;

	if (IS_ENABLED(CONFIG_RCU_NOCB_CPU_CB_BOOST) && kthread_prio)
		sched_setscheduler_nocheck(t, SCHED_FIFO, &sp);

	WRITE_ONCE(rdp->nocb_cb_kthread, t);
	WRITE_ONCE(rdp->nocb_gp_kthread, rdp_gp->nocb_gp_kthread);
	return;
end:
	mutex_lock(&rcu_state.barrier_mutex);
	if (rcu_rdp_is_offloaded(rdp)) {
		rcu_nocb_rdp_deoffload(rdp);
		cpumask_clear_cpu(cpu, rcu_nocb_mask);
	}
	mutex_unlock(&rcu_state.barrier_mutex);
}


static int rcu_nocb_gp_stride = -1;
module_param(rcu_nocb_gp_stride, int, 0444);


static void __init rcu_organize_nocb_kthreads(void)
{
	int cpu;
	bool firsttime = true;
	bool gotnocbs = false;
	bool gotnocbscbs = true;
	int ls = rcu_nocb_gp_stride;
	int nl = 0;  
	struct rcu_data *rdp;
	struct rcu_data *rdp_gp = NULL;  

	if (!cpumask_available(rcu_nocb_mask))
		return;
	if (ls == -1) {
		ls = nr_cpu_ids / int_sqrt(nr_cpu_ids);
		rcu_nocb_gp_stride = ls;
	}

	
	for_each_possible_cpu(cpu) {
		rdp = per_cpu_ptr(&rcu_data, cpu);
		if (rdp->cpu >= nl) {
			
			gotnocbs = true;
			nl = DIV_ROUND_UP(rdp->cpu + 1, ls) * ls;
			rdp_gp = rdp;
			INIT_LIST_HEAD(&rdp->nocb_head_rdp);
			if (dump_tree) {
				if (!firsttime)
					pr_cont("%s\n", gotnocbscbs
							? "" : " (self only)");
				gotnocbscbs = false;
				firsttime = false;
				pr_alert("%s: No-CB GP kthread CPU %d:",
					 __func__, cpu);
			}
		} else {
			
			gotnocbscbs = true;
			if (dump_tree)
				pr_cont(" %d", cpu);
		}
		rdp->nocb_gp_rdp = rdp_gp;
		if (cpumask_test_cpu(cpu, rcu_nocb_mask))
			list_add_tail(&rdp->nocb_entry_rdp, &rdp_gp->nocb_head_rdp);
	}
	if (gotnocbs && dump_tree)
		pr_cont("%s\n", gotnocbscbs ? "" : " (self only)");
}


void rcu_bind_current_to_nocb(void)
{
	if (cpumask_available(rcu_nocb_mask) && !cpumask_empty(rcu_nocb_mask))
		WARN_ON(sched_setaffinity(current->pid, rcu_nocb_mask));
}
EXPORT_SYMBOL_GPL(rcu_bind_current_to_nocb);


#ifdef CONFIG_SMP
static char *show_rcu_should_be_on_cpu(struct task_struct *tsp)
{
	return tsp && task_is_running(tsp) && !tsp->on_cpu ? "!" : "";
}
#else 
static char *show_rcu_should_be_on_cpu(struct task_struct *tsp)
{
	return "";
}
#endif 


static void show_rcu_nocb_gp_state(struct rcu_data *rdp)
{
	struct rcu_node *rnp = rdp->mynode;

	pr_info("nocb GP %d %c%c%c%c%c %c[%c%c] %c%c:%ld rnp %d:%d %lu %c CPU %d%s\n",
		rdp->cpu,
		"kK"[!!rdp->nocb_gp_kthread],
		"lL"[raw_spin_is_locked(&rdp->nocb_gp_lock)],
		"dD"[!!rdp->nocb_defer_wakeup],
		"tT"[timer_pending(&rdp->nocb_timer)],
		"sS"[!!rdp->nocb_gp_sleep],
		".W"[swait_active(&rdp->nocb_gp_wq)],
		".W"[swait_active(&rnp->nocb_gp_wq[0])],
		".W"[swait_active(&rnp->nocb_gp_wq[1])],
		".B"[!!rdp->nocb_gp_bypass],
		".G"[!!rdp->nocb_gp_gp],
		(long)rdp->nocb_gp_seq,
		rnp->grplo, rnp->grphi, READ_ONCE(rdp->nocb_gp_loops),
		rdp->nocb_gp_kthread ? task_state_to_char(rdp->nocb_gp_kthread) : '.',
		rdp->nocb_gp_kthread ? (int)task_cpu(rdp->nocb_gp_kthread) : -1,
		show_rcu_should_be_on_cpu(rdp->nocb_gp_kthread));
}


static void show_rcu_nocb_state(struct rcu_data *rdp)
{
	char bufw[20];
	char bufr[20];
	struct rcu_data *nocb_next_rdp;
	struct rcu_segcblist *rsclp = &rdp->cblist;
	bool waslocked;
	bool wassleep;

	if (rdp->nocb_gp_rdp == rdp)
		show_rcu_nocb_gp_state(rdp);

	nocb_next_rdp = list_next_or_null_rcu(&rdp->nocb_gp_rdp->nocb_head_rdp,
					      &rdp->nocb_entry_rdp,
					      typeof(*rdp),
					      nocb_entry_rdp);

	sprintf(bufw, "%ld", rsclp->gp_seq[RCU_WAIT_TAIL]);
	sprintf(bufr, "%ld", rsclp->gp_seq[RCU_NEXT_READY_TAIL]);
	pr_info("   CB %d^%d->%d %c%c%c%c%c%c F%ld L%ld C%d %c%c%s%c%s%c%c q%ld %c CPU %d%s\n",
		rdp->cpu, rdp->nocb_gp_rdp->cpu,
		nocb_next_rdp ? nocb_next_rdp->cpu : -1,
		"kK"[!!rdp->nocb_cb_kthread],
		"bB"[raw_spin_is_locked(&rdp->nocb_bypass_lock)],
		"cC"[!!atomic_read(&rdp->nocb_lock_contended)],
		"lL"[raw_spin_is_locked(&rdp->nocb_lock)],
		"sS"[!!rdp->nocb_cb_sleep],
		".W"[swait_active(&rdp->nocb_cb_wq)],
		jiffies - rdp->nocb_bypass_first,
		jiffies - rdp->nocb_nobypass_last,
		rdp->nocb_nobypass_count,
		".D"[rcu_segcblist_ready_cbs(rsclp)],
		".W"[!rcu_segcblist_segempty(rsclp, RCU_WAIT_TAIL)],
		rcu_segcblist_segempty(rsclp, RCU_WAIT_TAIL) ? "" : bufw,
		".R"[!rcu_segcblist_segempty(rsclp, RCU_NEXT_READY_TAIL)],
		rcu_segcblist_segempty(rsclp, RCU_NEXT_READY_TAIL) ? "" : bufr,
		".N"[!rcu_segcblist_segempty(rsclp, RCU_NEXT_TAIL)],
		".B"[!!rcu_cblist_n_cbs(&rdp->nocb_bypass)],
		rcu_segcblist_n_cbs(&rdp->cblist),
		rdp->nocb_cb_kthread ? task_state_to_char(rdp->nocb_cb_kthread) : '.',
		rdp->nocb_cb_kthread ? (int)task_cpu(rdp->nocb_cb_kthread) : -1,
		show_rcu_should_be_on_cpu(rdp->nocb_cb_kthread));

	
	if (rdp->nocb_gp_rdp == rdp)
		return;

	waslocked = raw_spin_is_locked(&rdp->nocb_gp_lock);
	wassleep = swait_active(&rdp->nocb_gp_wq);
	if (!rdp->nocb_gp_sleep && !waslocked && !wassleep)
		return;  

	pr_info("   nocb GP activity on CB-only CPU!!! %c%c%c %c\n",
		"lL"[waslocked],
		"dD"[!!rdp->nocb_defer_wakeup],
		"sS"[!!rdp->nocb_gp_sleep],
		".W"[wassleep]);
}

#else 

static inline int rcu_lockdep_is_held_nocb(struct rcu_data *rdp)
{
	return 0;
}

static inline bool rcu_current_is_nocb_kthread(struct rcu_data *rdp)
{
	return false;
}


static void rcu_nocb_lock(struct rcu_data *rdp)
{
}


static void rcu_nocb_unlock(struct rcu_data *rdp)
{
}


static void rcu_nocb_unlock_irqrestore(struct rcu_data *rdp,
				       unsigned long flags)
{
	local_irq_restore(flags);
}


static void rcu_lockdep_assert_cblist_protected(struct rcu_data *rdp)
{
	lockdep_assert_irqs_disabled();
}

static void rcu_nocb_gp_cleanup(struct swait_queue_head *sq)
{
}

static struct swait_queue_head *rcu_nocb_gp_get(struct rcu_node *rnp)
{
	return NULL;
}

static void rcu_init_one_nocb(struct rcu_node *rnp)
{
}

static bool wake_nocb_gp(struct rcu_data *rdp, bool force)
{
	return false;
}

static bool rcu_nocb_flush_bypass(struct rcu_data *rdp, struct rcu_head *rhp,
				  unsigned long j, bool lazy)
{
	return true;
}

static bool rcu_nocb_try_bypass(struct rcu_data *rdp, struct rcu_head *rhp,
				bool *was_alldone, unsigned long flags, bool lazy)
{
	return false;
}

static void __call_rcu_nocb_wake(struct rcu_data *rdp, bool was_empty,
				 unsigned long flags)
{
	WARN_ON_ONCE(1);  
}

static void __init rcu_boot_init_nocb_percpu_data(struct rcu_data *rdp)
{
}

static int rcu_nocb_need_deferred_wakeup(struct rcu_data *rdp, int level)
{
	return false;
}

static bool do_nocb_deferred_wakeup(struct rcu_data *rdp)
{
	return false;
}

static void rcu_spawn_cpu_nocb_kthread(int cpu)
{
}

static void show_rcu_nocb_state(struct rcu_data *rdp)
{
}

#endif 
