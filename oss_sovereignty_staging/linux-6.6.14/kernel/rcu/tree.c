
 

#define pr_fmt(fmt) "rcu: " fmt

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/smp.h>
#include <linux/rcupdate_wait.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <linux/nmi.h>
#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/export.h>
#include <linux/completion.h>
#include <linux/kmemleak.h>
#include <linux/moduleparam.h>
#include <linux/panic.h>
#include <linux/panic_notifier.h>
#include <linux/percpu.h>
#include <linux/notifier.h>
#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/time.h>
#include <linux/kernel_stat.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <linux/prefetch.h>
#include <linux/delay.h>
#include <linux/random.h>
#include <linux/trace_events.h>
#include <linux/suspend.h>
#include <linux/ftrace.h>
#include <linux/tick.h>
#include <linux/sysrq.h>
#include <linux/kprobes.h>
#include <linux/gfp.h>
#include <linux/oom.h>
#include <linux/smpboot.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/sched/isolation.h>
#include <linux/sched/clock.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/kasan.h>
#include <linux/context_tracking.h>
#include "../time/tick-internal.h"

#include "tree.h"
#include "rcu.h"

#ifdef MODULE_PARAM_PREFIX
#undef MODULE_PARAM_PREFIX
#endif
#define MODULE_PARAM_PREFIX "rcutree."

 

static DEFINE_PER_CPU_SHARED_ALIGNED(struct rcu_data, rcu_data) = {
	.gpwrap = true,
#ifdef CONFIG_RCU_NOCB_CPU
	.cblist.flags = SEGCBLIST_RCU_CORE,
#endif
};
static struct rcu_state rcu_state = {
	.level = { &rcu_state.node[0] },
	.gp_state = RCU_GP_IDLE,
	.gp_seq = (0UL - 300UL) << RCU_SEQ_CTR_SHIFT,
	.barrier_mutex = __MUTEX_INITIALIZER(rcu_state.barrier_mutex),
	.barrier_lock = __RAW_SPIN_LOCK_UNLOCKED(rcu_state.barrier_lock),
	.name = RCU_NAME,
	.abbr = RCU_ABBR,
	.exp_mutex = __MUTEX_INITIALIZER(rcu_state.exp_mutex),
	.exp_wake_mutex = __MUTEX_INITIALIZER(rcu_state.exp_wake_mutex),
	.ofl_lock = __ARCH_SPIN_LOCK_UNLOCKED,
};

 
static bool dump_tree;
module_param(dump_tree, bool, 0444);
 
static bool use_softirq = !IS_ENABLED(CONFIG_PREEMPT_RT);
#ifndef CONFIG_PREEMPT_RT
module_param(use_softirq, bool, 0444);
#endif
 
static bool rcu_fanout_exact;
module_param(rcu_fanout_exact, bool, 0444);
 
static int rcu_fanout_leaf = RCU_FANOUT_LEAF;
module_param(rcu_fanout_leaf, int, 0444);
int rcu_num_lvls __read_mostly = RCU_NUM_LVLS;
 
int num_rcu_lvl[] = NUM_RCU_LVL_INIT;
int rcu_num_nodes __read_mostly = NUM_RCU_NODES;  

 
int rcu_scheduler_active __read_mostly;
EXPORT_SYMBOL_GPL(rcu_scheduler_active);

 
static int rcu_scheduler_fully_active __read_mostly;

static void rcu_report_qs_rnp(unsigned long mask, struct rcu_node *rnp,
			      unsigned long gps, unsigned long flags);
static void rcu_boost_kthread_setaffinity(struct rcu_node *rnp, int outgoingcpu);
static void invoke_rcu_core(void);
static void rcu_report_exp_rdp(struct rcu_data *rdp);
static void sync_sched_exp_online_cleanup(int cpu);
static void check_cb_ovld_locked(struct rcu_data *rdp, struct rcu_node *rnp);
static bool rcu_rdp_is_offloaded(struct rcu_data *rdp);
static bool rcu_rdp_cpu_online(struct rcu_data *rdp);
static bool rcu_init_invoked(void);
static void rcu_cleanup_dead_rnp(struct rcu_node *rnp_leaf);
static void rcu_init_new_rnp(struct rcu_node *rnp_leaf);

 
static int kthread_prio = IS_ENABLED(CONFIG_RCU_BOOST) ? 1 : 0;
module_param(kthread_prio, int, 0444);

 

static int gp_preinit_delay;
module_param(gp_preinit_delay, int, 0444);
static int gp_init_delay;
module_param(gp_init_delay, int, 0444);
static int gp_cleanup_delay;
module_param(gp_cleanup_delay, int, 0444);


static int rcu_unlock_delay;
#ifdef CONFIG_RCU_STRICT_GRACE_PERIOD
module_param(rcu_unlock_delay, int, 0444);
#endif

 
static int rcu_min_cached_objs = 5;
module_param(rcu_min_cached_objs, int, 0444);









static int rcu_delay_page_cache_fill_msec = 5000;
module_param(rcu_delay_page_cache_fill_msec, int, 0444);

 
int rcu_get_gp_kthreads_prio(void)
{
	return kthread_prio;
}
EXPORT_SYMBOL_GPL(rcu_get_gp_kthreads_prio);

 
#define PER_RCU_NODE_PERIOD 3	 

 
static int rcu_gp_in_progress(void)
{
	return rcu_seq_state(rcu_seq_current(&rcu_state.gp_seq));
}

 
static long rcu_get_n_cbs_cpu(int cpu)
{
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);

	if (rcu_segcblist_is_enabled(&rdp->cblist))
		return rcu_segcblist_n_cbs(&rdp->cblist);
	return 0;
}

void rcu_softirq_qs(void)
{
	rcu_qs();
	rcu_preempt_deferred_qs(current);
	rcu_tasks_qs(current, false);
}

 
static void rcu_dynticks_eqs_online(void)
{
	if (ct_dynticks() & RCU_DYNTICKS_IDX)
		return;
	ct_state_inc(RCU_DYNTICKS_IDX);
}

 
static int rcu_dynticks_snap(int cpu)
{
	smp_mb();  
	return ct_dynticks_cpu_acquire(cpu);
}

 
static bool rcu_dynticks_in_eqs(int snap)
{
	return !(snap & RCU_DYNTICKS_IDX);
}

 
static bool rcu_dynticks_in_eqs_since(struct rcu_data *rdp, int snap)
{
	return snap != rcu_dynticks_snap(rdp->cpu);
}

 
bool rcu_dynticks_zero_in_eqs(int cpu, int *vp)
{
	int snap;

	
	snap = ct_dynticks_cpu(cpu) & ~RCU_DYNTICKS_IDX;
	smp_rmb(); 
	if (READ_ONCE(*vp))
		return false;  
	smp_rmb(); 

	
	return snap == ct_dynticks_cpu(cpu);
}

 
notrace void rcu_momentary_dyntick_idle(void)
{
	int seq;

	raw_cpu_write(rcu_data.rcu_need_heavy_qs, false);
	seq = ct_state_inc(2 * RCU_DYNTICKS_IDX);
	 
	WARN_ON_ONCE(!(seq & RCU_DYNTICKS_IDX));
	rcu_preempt_deferred_qs(current);
}
EXPORT_SYMBOL_GPL(rcu_momentary_dyntick_idle);

 
static int rcu_is_cpu_rrupt_from_idle(void)
{
	long nesting;

	 
	lockdep_assert_irqs_disabled();

	 
	RCU_LOCKDEP_WARN(ct_dynticks_nesting() < 0,
			 "RCU dynticks_nesting counter underflow!");
	RCU_LOCKDEP_WARN(ct_dynticks_nmi_nesting() <= 0,
			 "RCU dynticks_nmi_nesting counter underflow/zero!");

	 
	nesting = ct_dynticks_nmi_nesting();
	if (nesting > 1)
		return false;

	 
	WARN_ON_ONCE(!nesting && !is_idle_task(current));

	 
	return ct_dynticks_nesting() == 0;
}

#define DEFAULT_RCU_BLIMIT (IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD) ? 1000 : 10)
				 
#define DEFAULT_MAX_RCU_BLIMIT 10000  
static long blimit = DEFAULT_RCU_BLIMIT;
#define DEFAULT_RCU_QHIMARK 10000  
static long qhimark = DEFAULT_RCU_QHIMARK;
#define DEFAULT_RCU_QLOMARK 100    
static long qlowmark = DEFAULT_RCU_QLOMARK;
#define DEFAULT_RCU_QOVLD_MULT 2
#define DEFAULT_RCU_QOVLD (DEFAULT_RCU_QOVLD_MULT * DEFAULT_RCU_QHIMARK)
static long qovld = DEFAULT_RCU_QOVLD;  
static long qovld_calc = -1;	   

module_param(blimit, long, 0444);
module_param(qhimark, long, 0444);
module_param(qlowmark, long, 0444);
module_param(qovld, long, 0444);

static ulong jiffies_till_first_fqs = IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD) ? 0 : ULONG_MAX;
static ulong jiffies_till_next_fqs = ULONG_MAX;
static bool rcu_kick_kthreads;
static int rcu_divisor = 7;
module_param(rcu_divisor, int, 0644);

 
static long rcu_resched_ns = 3 * NSEC_PER_MSEC;
module_param(rcu_resched_ns, long, 0644);

 
static ulong jiffies_till_sched_qs = ULONG_MAX;
module_param(jiffies_till_sched_qs, ulong, 0444);
static ulong jiffies_to_sched_qs;  
module_param(jiffies_to_sched_qs, ulong, 0444);  

 
static void adjust_jiffies_till_sched_qs(void)
{
	unsigned long j;

	 
	if (jiffies_till_sched_qs != ULONG_MAX) {
		WRITE_ONCE(jiffies_to_sched_qs, jiffies_till_sched_qs);
		return;
	}
	 
	j = READ_ONCE(jiffies_till_first_fqs) +
		      2 * READ_ONCE(jiffies_till_next_fqs);
	if (j < HZ / 10 + nr_cpu_ids / RCU_JIFFIES_FQS_DIV)
		j = HZ / 10 + nr_cpu_ids / RCU_JIFFIES_FQS_DIV;
	pr_info("RCU calculated value of scheduler-enlistment delay is %ld jiffies.\n", j);
	WRITE_ONCE(jiffies_to_sched_qs, j);
}

static int param_set_first_fqs_jiffies(const char *val, const struct kernel_param *kp)
{
	ulong j;
	int ret = kstrtoul(val, 0, &j);

	if (!ret) {
		WRITE_ONCE(*(ulong *)kp->arg, (j > HZ) ? HZ : j);
		adjust_jiffies_till_sched_qs();
	}
	return ret;
}

static int param_set_next_fqs_jiffies(const char *val, const struct kernel_param *kp)
{
	ulong j;
	int ret = kstrtoul(val, 0, &j);

	if (!ret) {
		WRITE_ONCE(*(ulong *)kp->arg, (j > HZ) ? HZ : (j ?: 1));
		adjust_jiffies_till_sched_qs();
	}
	return ret;
}

static const struct kernel_param_ops first_fqs_jiffies_ops = {
	.set = param_set_first_fqs_jiffies,
	.get = param_get_ulong,
};

static const struct kernel_param_ops next_fqs_jiffies_ops = {
	.set = param_set_next_fqs_jiffies,
	.get = param_get_ulong,
};

module_param_cb(jiffies_till_first_fqs, &first_fqs_jiffies_ops, &jiffies_till_first_fqs, 0644);
module_param_cb(jiffies_till_next_fqs, &next_fqs_jiffies_ops, &jiffies_till_next_fqs, 0644);
module_param(rcu_kick_kthreads, bool, 0644);

static void force_qs_rnp(int (*f)(struct rcu_data *rdp));
static int rcu_pending(int user);

 
unsigned long rcu_get_gp_seq(void)
{
	return READ_ONCE(rcu_state.gp_seq);
}
EXPORT_SYMBOL_GPL(rcu_get_gp_seq);

 
unsigned long rcu_exp_batches_completed(void)
{
	return rcu_state.expedited_sequence;
}
EXPORT_SYMBOL_GPL(rcu_exp_batches_completed);

 
static struct rcu_node *rcu_get_root(void)
{
	return &rcu_state.node[0];
}

 
void rcutorture_get_gp_data(enum rcutorture_type test_type, int *flags,
			    unsigned long *gp_seq)
{
	switch (test_type) {
	case RCU_FLAVOR:
		*flags = READ_ONCE(rcu_state.gp_flags);
		*gp_seq = rcu_seq_current(&rcu_state.gp_seq);
		break;
	default:
		break;
	}
}
EXPORT_SYMBOL_GPL(rcutorture_get_gp_data);

#if defined(CONFIG_NO_HZ_FULL) && (!defined(CONFIG_GENERIC_ENTRY) || !defined(CONFIG_KVM_XFER_TO_GUEST_WORK))
 
static void late_wakeup_func(struct irq_work *work)
{
}

static DEFINE_PER_CPU(struct irq_work, late_wakeup_work) =
	IRQ_WORK_INIT(late_wakeup_func);

 
noinstr void rcu_irq_work_resched(void)
{
	struct rcu_data *rdp = this_cpu_ptr(&rcu_data);

	if (IS_ENABLED(CONFIG_GENERIC_ENTRY) && !(current->flags & PF_VCPU))
		return;

	if (IS_ENABLED(CONFIG_KVM_XFER_TO_GUEST_WORK) && (current->flags & PF_VCPU))
		return;

	instrumentation_begin();
	if (do_nocb_deferred_wakeup(rdp) && need_resched()) {
		irq_work_queue(this_cpu_ptr(&late_wakeup_work));
	}
	instrumentation_end();
}
#endif  

#ifdef CONFIG_PROVE_RCU
 
void rcu_irq_exit_check_preempt(void)
{
	lockdep_assert_irqs_disabled();

	RCU_LOCKDEP_WARN(ct_dynticks_nesting() <= 0,
			 "RCU dynticks_nesting counter underflow/zero!");
	RCU_LOCKDEP_WARN(ct_dynticks_nmi_nesting() !=
			 DYNTICK_IRQ_NONIDLE,
			 "Bad RCU  dynticks_nmi_nesting counter\n");
	RCU_LOCKDEP_WARN(rcu_dynticks_curr_cpu_in_eqs(),
			 "RCU in extended quiescent state!");
}
#endif  

#ifdef CONFIG_NO_HZ_FULL
 
void __rcu_irq_enter_check_tick(void)
{
	struct rcu_data *rdp = this_cpu_ptr(&rcu_data);

	
	if (in_nmi())
		return;

	RCU_LOCKDEP_WARN(rcu_dynticks_curr_cpu_in_eqs(),
			 "Illegal rcu_irq_enter_check_tick() from extended quiescent state");

	if (!tick_nohz_full_cpu(rdp->cpu) ||
	    !READ_ONCE(rdp->rcu_urgent_qs) ||
	    READ_ONCE(rdp->rcu_forced_tick)) {
		
		
		return;
	}

	
	
	
	
	
	
	raw_spin_lock_rcu_node(rdp->mynode);
	if (READ_ONCE(rdp->rcu_urgent_qs) && !rdp->rcu_forced_tick) {
		
		
		WRITE_ONCE(rdp->rcu_forced_tick, true);
		tick_dep_set_cpu(rdp->cpu, TICK_DEP_BIT_RCU);
	}
	raw_spin_unlock_rcu_node(rdp->mynode);
}
NOKPROBE_SYMBOL(__rcu_irq_enter_check_tick);
#endif  

 
int rcu_needs_cpu(void)
{
	return !rcu_segcblist_empty(&this_cpu_ptr(&rcu_data)->cblist) &&
		!rcu_rdp_is_offloaded(this_cpu_ptr(&rcu_data));
}

 
static void rcu_disable_urgency_upon_qs(struct rcu_data *rdp)
{
	raw_lockdep_assert_held_rcu_node(rdp->mynode);
	WRITE_ONCE(rdp->rcu_urgent_qs, false);
	WRITE_ONCE(rdp->rcu_need_heavy_qs, false);
	if (tick_nohz_full_cpu(rdp->cpu) && rdp->rcu_forced_tick) {
		tick_dep_clear_cpu(rdp->cpu, TICK_DEP_BIT_RCU);
		WRITE_ONCE(rdp->rcu_forced_tick, false);
	}
}

 
notrace bool rcu_is_watching(void)
{
	bool ret;

	preempt_disable_notrace();
	ret = !rcu_dynticks_curr_cpu_in_eqs();
	preempt_enable_notrace();
	return ret;
}
EXPORT_SYMBOL_GPL(rcu_is_watching);

 
void rcu_request_urgent_qs_task(struct task_struct *t)
{
	int cpu;

	barrier();
	cpu = task_cpu(t);
	if (!task_curr(t))
		return;  
	smp_store_release(per_cpu_ptr(&rcu_data.rcu_urgent_qs, cpu), true);
}

 
static void rcu_gpnum_ovf(struct rcu_node *rnp, struct rcu_data *rdp)
{
	raw_lockdep_assert_held_rcu_node(rnp);
	if (ULONG_CMP_LT(rcu_seq_current(&rdp->gp_seq) + ULONG_MAX / 4,
			 rnp->gp_seq))
		WRITE_ONCE(rdp->gpwrap, true);
	if (ULONG_CMP_LT(rdp->rcu_iw_gp_seq + ULONG_MAX / 4, rnp->gp_seq))
		rdp->rcu_iw_gp_seq = rnp->gp_seq + ULONG_MAX / 4;
}

 
static int dyntick_save_progress_counter(struct rcu_data *rdp)
{
	rdp->dynticks_snap = rcu_dynticks_snap(rdp->cpu);
	if (rcu_dynticks_in_eqs(rdp->dynticks_snap)) {
		trace_rcu_fqs(rcu_state.name, rdp->gp_seq, rdp->cpu, TPS("dti"));
		rcu_gpnum_ovf(rdp->mynode, rdp);
		return 1;
	}
	return 0;
}

 
static int rcu_implicit_dynticks_qs(struct rcu_data *rdp)
{
	unsigned long jtsq;
	int ret = 0;
	struct rcu_node *rnp = rdp->mynode;

	 
	if (rcu_dynticks_in_eqs_since(rdp, rdp->dynticks_snap)) {
		trace_rcu_fqs(rcu_state.name, rdp->gp_seq, rdp->cpu, TPS("dti"));
		rcu_gpnum_ovf(rnp, rdp);
		return 1;
	}

	 
	if (WARN_ON_ONCE(!rcu_rdp_cpu_online(rdp))) {
		struct rcu_node *rnp1;

		pr_info("%s: grp: %d-%d level: %d ->gp_seq %ld ->completedqs %ld\n",
			__func__, rnp->grplo, rnp->grphi, rnp->level,
			(long)rnp->gp_seq, (long)rnp->completedqs);
		for (rnp1 = rnp; rnp1; rnp1 = rnp1->parent)
			pr_info("%s: %d:%d ->qsmask %#lx ->qsmaskinit %#lx ->qsmaskinitnext %#lx ->rcu_gp_init_mask %#lx\n",
				__func__, rnp1->grplo, rnp1->grphi, rnp1->qsmask, rnp1->qsmaskinit, rnp1->qsmaskinitnext, rnp1->rcu_gp_init_mask);
		pr_info("%s %d: %c online: %ld(%d) offline: %ld(%d)\n",
			__func__, rdp->cpu, ".o"[rcu_rdp_cpu_online(rdp)],
			(long)rdp->rcu_onl_gp_seq, rdp->rcu_onl_gp_flags,
			(long)rdp->rcu_ofl_gp_seq, rdp->rcu_ofl_gp_flags);
		return 1;  
	}

	 
	jtsq = READ_ONCE(jiffies_to_sched_qs);
	if (!READ_ONCE(rdp->rcu_need_heavy_qs) &&
	    (time_after(jiffies, rcu_state.gp_start + jtsq * 2) ||
	     time_after(jiffies, rcu_state.jiffies_resched) ||
	     rcu_state.cbovld)) {
		WRITE_ONCE(rdp->rcu_need_heavy_qs, true);
		 
		smp_store_release(&rdp->rcu_urgent_qs, true);
	} else if (time_after(jiffies, rcu_state.gp_start + jtsq)) {
		WRITE_ONCE(rdp->rcu_urgent_qs, true);
	}

	 
	if (tick_nohz_full_cpu(rdp->cpu) &&
	    (time_after(jiffies, READ_ONCE(rdp->last_fqs_resched) + jtsq * 3) ||
	     rcu_state.cbovld)) {
		WRITE_ONCE(rdp->rcu_urgent_qs, true);
		WRITE_ONCE(rdp->last_fqs_resched, jiffies);
		ret = -1;
	}

	 
	if (time_after(jiffies, rcu_state.jiffies_resched)) {
		if (time_after(jiffies,
			       READ_ONCE(rdp->last_fqs_resched) + jtsq)) {
			WRITE_ONCE(rdp->last_fqs_resched, jiffies);
			ret = -1;
		}
		if (IS_ENABLED(CONFIG_IRQ_WORK) &&
		    !rdp->rcu_iw_pending && rdp->rcu_iw_gp_seq != rnp->gp_seq &&
		    (rnp->ffmask & rdp->grpmask)) {
			rdp->rcu_iw_pending = true;
			rdp->rcu_iw_gp_seq = rnp->gp_seq;
			irq_work_queue_on(&rdp->rcu_iw, rdp->cpu);
		}

		if (rcu_cpu_stall_cputime && rdp->snap_record.gp_seq != rdp->gp_seq) {
			int cpu = rdp->cpu;
			struct rcu_snap_record *rsrp;
			struct kernel_cpustat *kcsp;

			kcsp = &kcpustat_cpu(cpu);

			rsrp = &rdp->snap_record;
			rsrp->cputime_irq     = kcpustat_field(kcsp, CPUTIME_IRQ, cpu);
			rsrp->cputime_softirq = kcpustat_field(kcsp, CPUTIME_SOFTIRQ, cpu);
			rsrp->cputime_system  = kcpustat_field(kcsp, CPUTIME_SYSTEM, cpu);
			rsrp->nr_hardirqs = kstat_cpu_irqs_sum(rdp->cpu);
			rsrp->nr_softirqs = kstat_cpu_softirqs_sum(rdp->cpu);
			rsrp->nr_csw = nr_context_switches_cpu(rdp->cpu);
			rsrp->jiffies = jiffies;
			rsrp->gp_seq = rdp->gp_seq;
		}
	}

	return ret;
}

 
static void trace_rcu_this_gp(struct rcu_node *rnp, struct rcu_data *rdp,
			      unsigned long gp_seq_req, const char *s)
{
	trace_rcu_future_grace_period(rcu_state.name, READ_ONCE(rnp->gp_seq),
				      gp_seq_req, rnp->level,
				      rnp->grplo, rnp->grphi, s);
}

 
static bool rcu_start_this_gp(struct rcu_node *rnp_start, struct rcu_data *rdp,
			      unsigned long gp_seq_req)
{
	bool ret = false;
	struct rcu_node *rnp;

	 
	raw_lockdep_assert_held_rcu_node(rnp_start);
	trace_rcu_this_gp(rnp_start, rdp, gp_seq_req, TPS("Startleaf"));
	for (rnp = rnp_start; 1; rnp = rnp->parent) {
		if (rnp != rnp_start)
			raw_spin_lock_rcu_node(rnp);
		if (ULONG_CMP_GE(rnp->gp_seq_needed, gp_seq_req) ||
		    rcu_seq_started(&rnp->gp_seq, gp_seq_req) ||
		    (rnp != rnp_start &&
		     rcu_seq_state(rcu_seq_current(&rnp->gp_seq)))) {
			trace_rcu_this_gp(rnp, rdp, gp_seq_req,
					  TPS("Prestarted"));
			goto unlock_out;
		}
		WRITE_ONCE(rnp->gp_seq_needed, gp_seq_req);
		if (rcu_seq_state(rcu_seq_current(&rnp->gp_seq))) {
			 
			trace_rcu_this_gp(rnp_start, rdp, gp_seq_req,
					  TPS("Startedleaf"));
			goto unlock_out;
		}
		if (rnp != rnp_start && rnp->parent != NULL)
			raw_spin_unlock_rcu_node(rnp);
		if (!rnp->parent)
			break;   
	}

	 
	if (rcu_gp_in_progress()) {
		trace_rcu_this_gp(rnp, rdp, gp_seq_req, TPS("Startedleafroot"));
		goto unlock_out;
	}
	trace_rcu_this_gp(rnp, rdp, gp_seq_req, TPS("Startedroot"));
	WRITE_ONCE(rcu_state.gp_flags, rcu_state.gp_flags | RCU_GP_FLAG_INIT);
	WRITE_ONCE(rcu_state.gp_req_activity, jiffies);
	if (!READ_ONCE(rcu_state.gp_kthread)) {
		trace_rcu_this_gp(rnp, rdp, gp_seq_req, TPS("NoGPkthread"));
		goto unlock_out;
	}
	trace_rcu_grace_period(rcu_state.name, data_race(rcu_state.gp_seq), TPS("newreq"));
	ret = true;   
unlock_out:
	 
	if (ULONG_CMP_LT(gp_seq_req, rnp->gp_seq_needed)) {
		WRITE_ONCE(rnp_start->gp_seq_needed, rnp->gp_seq_needed);
		WRITE_ONCE(rdp->gp_seq_needed, rnp->gp_seq_needed);
	}
	if (rnp != rnp_start)
		raw_spin_unlock_rcu_node(rnp);
	return ret;
}

 
static bool rcu_future_gp_cleanup(struct rcu_node *rnp)
{
	bool needmore;
	struct rcu_data *rdp = this_cpu_ptr(&rcu_data);

	needmore = ULONG_CMP_LT(rnp->gp_seq, rnp->gp_seq_needed);
	if (!needmore)
		rnp->gp_seq_needed = rnp->gp_seq;  
	trace_rcu_this_gp(rnp, rdp, rnp->gp_seq,
			  needmore ? TPS("CleanupMore") : TPS("Cleanup"));
	return needmore;
}

 
static void rcu_gp_kthread_wake(void)
{
	struct task_struct *t = READ_ONCE(rcu_state.gp_kthread);

	if ((current == t && !in_hardirq() && !in_serving_softirq()) ||
	    !READ_ONCE(rcu_state.gp_flags) || !t)
		return;
	WRITE_ONCE(rcu_state.gp_wake_time, jiffies);
	WRITE_ONCE(rcu_state.gp_wake_seq, READ_ONCE(rcu_state.gp_seq));
	swake_up_one(&rcu_state.gp_wq);
}

 
static bool rcu_accelerate_cbs(struct rcu_node *rnp, struct rcu_data *rdp)
{
	unsigned long gp_seq_req;
	bool ret = false;

	rcu_lockdep_assert_cblist_protected(rdp);
	raw_lockdep_assert_held_rcu_node(rnp);

	 
	if (!rcu_segcblist_pend_cbs(&rdp->cblist))
		return false;

	trace_rcu_segcb_stats(&rdp->cblist, TPS("SegCbPreAcc"));

	 
	gp_seq_req = rcu_seq_snap(&rcu_state.gp_seq);
	if (rcu_segcblist_accelerate(&rdp->cblist, gp_seq_req))
		ret = rcu_start_this_gp(rnp, rdp, gp_seq_req);

	 
	if (rcu_segcblist_restempty(&rdp->cblist, RCU_WAIT_TAIL))
		trace_rcu_grace_period(rcu_state.name, gp_seq_req, TPS("AccWaitCB"));
	else
		trace_rcu_grace_period(rcu_state.name, gp_seq_req, TPS("AccReadyCB"));

	trace_rcu_segcb_stats(&rdp->cblist, TPS("SegCbPostAcc"));

	return ret;
}

 
static void rcu_accelerate_cbs_unlocked(struct rcu_node *rnp,
					struct rcu_data *rdp)
{
	unsigned long c;
	bool needwake;

	rcu_lockdep_assert_cblist_protected(rdp);
	c = rcu_seq_snap(&rcu_state.gp_seq);
	if (!READ_ONCE(rdp->gpwrap) && ULONG_CMP_GE(rdp->gp_seq_needed, c)) {
		 
		(void)rcu_segcblist_accelerate(&rdp->cblist, c);
		return;
	}
	raw_spin_lock_rcu_node(rnp);  
	needwake = rcu_accelerate_cbs(rnp, rdp);
	raw_spin_unlock_rcu_node(rnp);  
	if (needwake)
		rcu_gp_kthread_wake();
}

 
static bool rcu_advance_cbs(struct rcu_node *rnp, struct rcu_data *rdp)
{
	rcu_lockdep_assert_cblist_protected(rdp);
	raw_lockdep_assert_held_rcu_node(rnp);

	 
	if (!rcu_segcblist_pend_cbs(&rdp->cblist))
		return false;

	 
	rcu_segcblist_advance(&rdp->cblist, rnp->gp_seq);

	 
	return rcu_accelerate_cbs(rnp, rdp);
}

 
static void __maybe_unused rcu_advance_cbs_nowake(struct rcu_node *rnp,
						  struct rcu_data *rdp)
{
	rcu_lockdep_assert_cblist_protected(rdp);
	if (!rcu_seq_state(rcu_seq_current(&rnp->gp_seq)) || !raw_spin_trylock_rcu_node(rnp))
		return;
	 
	if (rcu_seq_state(rcu_seq_current(&rnp->gp_seq)))
		WARN_ON_ONCE(rcu_advance_cbs(rnp, rdp));
	raw_spin_unlock_rcu_node(rnp);
}

 
static void rcu_strict_gp_check_qs(void)
{
	if (IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD)) {
		rcu_read_lock();
		rcu_read_unlock();
	}
}

 
static bool __note_gp_changes(struct rcu_node *rnp, struct rcu_data *rdp)
{
	bool ret = false;
	bool need_qs;
	const bool offloaded = rcu_rdp_is_offloaded(rdp);

	raw_lockdep_assert_held_rcu_node(rnp);

	if (rdp->gp_seq == rnp->gp_seq)
		return false;  

	 
	if (rcu_seq_completed_gp(rdp->gp_seq, rnp->gp_seq) ||
	    unlikely(READ_ONCE(rdp->gpwrap))) {
		if (!offloaded)
			ret = rcu_advance_cbs(rnp, rdp);  
		rdp->core_needs_qs = false;
		trace_rcu_grace_period(rcu_state.name, rdp->gp_seq, TPS("cpuend"));
	} else {
		if (!offloaded)
			ret = rcu_accelerate_cbs(rnp, rdp);  
		if (rdp->core_needs_qs)
			rdp->core_needs_qs = !!(rnp->qsmask & rdp->grpmask);
	}

	 
	if (rcu_seq_new_gp(rdp->gp_seq, rnp->gp_seq) ||
	    unlikely(READ_ONCE(rdp->gpwrap))) {
		 
		trace_rcu_grace_period(rcu_state.name, rnp->gp_seq, TPS("cpustart"));
		need_qs = !!(rnp->qsmask & rdp->grpmask);
		rdp->cpu_no_qs.b.norm = need_qs;
		rdp->core_needs_qs = need_qs;
		zero_cpu_stall_ticks(rdp);
	}
	rdp->gp_seq = rnp->gp_seq;   
	if (ULONG_CMP_LT(rdp->gp_seq_needed, rnp->gp_seq_needed) || rdp->gpwrap)
		WRITE_ONCE(rdp->gp_seq_needed, rnp->gp_seq_needed);
	if (IS_ENABLED(CONFIG_PROVE_RCU) && READ_ONCE(rdp->gpwrap))
		WRITE_ONCE(rdp->last_sched_clock, jiffies);
	WRITE_ONCE(rdp->gpwrap, false);
	rcu_gpnum_ovf(rnp, rdp);
	return ret;
}

static void note_gp_changes(struct rcu_data *rdp)
{
	unsigned long flags;
	bool needwake;
	struct rcu_node *rnp;

	local_irq_save(flags);
	rnp = rdp->mynode;
	if ((rdp->gp_seq == rcu_seq_current(&rnp->gp_seq) &&
	     !unlikely(READ_ONCE(rdp->gpwrap))) ||  
	    !raw_spin_trylock_rcu_node(rnp)) {  
		local_irq_restore(flags);
		return;
	}
	needwake = __note_gp_changes(rnp, rdp);
	raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
	rcu_strict_gp_check_qs();
	if (needwake)
		rcu_gp_kthread_wake();
}

static atomic_t *rcu_gp_slow_suppress;

 
void rcu_gp_slow_register(atomic_t *rgssp)
{
	WARN_ON_ONCE(rcu_gp_slow_suppress);

	WRITE_ONCE(rcu_gp_slow_suppress, rgssp);
}
EXPORT_SYMBOL_GPL(rcu_gp_slow_register);

 
void rcu_gp_slow_unregister(atomic_t *rgssp)
{
	WARN_ON_ONCE(rgssp && rgssp != rcu_gp_slow_suppress);

	WRITE_ONCE(rcu_gp_slow_suppress, NULL);
}
EXPORT_SYMBOL_GPL(rcu_gp_slow_unregister);

static bool rcu_gp_slow_is_suppressed(void)
{
	atomic_t *rgssp = READ_ONCE(rcu_gp_slow_suppress);

	return rgssp && atomic_read(rgssp);
}

static void rcu_gp_slow(int delay)
{
	if (!rcu_gp_slow_is_suppressed() && delay > 0 &&
	    !(rcu_seq_ctr(rcu_state.gp_seq) % (rcu_num_nodes * PER_RCU_NODE_PERIOD * delay)))
		schedule_timeout_idle(delay);
}

static unsigned long sleep_duration;

 
void rcu_gp_set_torture_wait(int duration)
{
	if (IS_ENABLED(CONFIG_RCU_TORTURE_TEST) && duration > 0)
		WRITE_ONCE(sleep_duration, duration);
}
EXPORT_SYMBOL_GPL(rcu_gp_set_torture_wait);

 
static void rcu_gp_torture_wait(void)
{
	unsigned long duration;

	if (!IS_ENABLED(CONFIG_RCU_TORTURE_TEST))
		return;
	duration = xchg(&sleep_duration, 0UL);
	if (duration > 0) {
		pr_alert("%s: Waiting %lu jiffies\n", __func__, duration);
		schedule_timeout_idle(duration);
		pr_alert("%s: Wait complete\n", __func__);
	}
}

 
static void rcu_strict_gp_boundary(void *unused)
{
	invoke_rcu_core();
}

 
static void rcu_poll_gp_seq_start(unsigned long *snap)
{
	struct rcu_node *rnp = rcu_get_root();

	if (rcu_scheduler_active != RCU_SCHEDULER_INACTIVE)
		raw_lockdep_assert_held_rcu_node(rnp);

	 
	if (!rcu_seq_state(rcu_state.gp_seq_polled))
		rcu_seq_start(&rcu_state.gp_seq_polled);

	 
	*snap = rcu_state.gp_seq_polled;
}

 
static void rcu_poll_gp_seq_end(unsigned long *snap)
{
	struct rcu_node *rnp = rcu_get_root();

	if (rcu_scheduler_active != RCU_SCHEDULER_INACTIVE)
		raw_lockdep_assert_held_rcu_node(rnp);

	 
	 
	 
	if (*snap && *snap == rcu_state.gp_seq_polled) {
		rcu_seq_end(&rcu_state.gp_seq_polled);
		rcu_state.gp_seq_polled_snap = 0;
		rcu_state.gp_seq_polled_exp_snap = 0;
	} else {
		*snap = 0;
	}
}

 
 
static void rcu_poll_gp_seq_start_unlocked(unsigned long *snap)
{
	unsigned long flags;
	struct rcu_node *rnp = rcu_get_root();

	if (rcu_init_invoked()) {
		if (rcu_scheduler_active != RCU_SCHEDULER_INACTIVE)
			lockdep_assert_irqs_enabled();
		raw_spin_lock_irqsave_rcu_node(rnp, flags);
	}
	rcu_poll_gp_seq_start(snap);
	if (rcu_init_invoked())
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
}



static void rcu_poll_gp_seq_end_unlocked(unsigned long *snap)
{
	unsigned long flags;
	struct rcu_node *rnp = rcu_get_root();

	if (rcu_init_invoked()) {
		if (rcu_scheduler_active != RCU_SCHEDULER_INACTIVE)
			lockdep_assert_irqs_enabled();
		raw_spin_lock_irqsave_rcu_node(rnp, flags);
	}
	rcu_poll_gp_seq_end(snap);
	if (rcu_init_invoked())
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
}

 
static noinline_for_stack bool rcu_gp_init(void)
{
	unsigned long flags;
	unsigned long oldmask;
	unsigned long mask;
	struct rcu_data *rdp;
	struct rcu_node *rnp = rcu_get_root();

	WRITE_ONCE(rcu_state.gp_activity, jiffies);
	raw_spin_lock_irq_rcu_node(rnp);
	if (!READ_ONCE(rcu_state.gp_flags)) {
		 
		raw_spin_unlock_irq_rcu_node(rnp);
		return false;
	}
	WRITE_ONCE(rcu_state.gp_flags, 0);  

	if (WARN_ON_ONCE(rcu_gp_in_progress())) {
		 
		raw_spin_unlock_irq_rcu_node(rnp);
		return false;
	}

	 
	record_gp_stall_check_time();
	 
	rcu_seq_start(&rcu_state.gp_seq);
	ASSERT_EXCLUSIVE_WRITER(rcu_state.gp_seq);
	trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq, TPS("start"));
	rcu_poll_gp_seq_start(&rcu_state.gp_seq_polled_snap);
	raw_spin_unlock_irq_rcu_node(rnp);

	 
	WRITE_ONCE(rcu_state.gp_state, RCU_GP_ONOFF);
	 
	rcu_for_each_leaf_node(rnp) {
		local_irq_save(flags);
		arch_spin_lock(&rcu_state.ofl_lock);
		raw_spin_lock_rcu_node(rnp);
		if (rnp->qsmaskinit == rnp->qsmaskinitnext &&
		    !rnp->wait_blkd_tasks) {
			 
			raw_spin_unlock_rcu_node(rnp);
			arch_spin_unlock(&rcu_state.ofl_lock);
			local_irq_restore(flags);
			continue;
		}

		 
		oldmask = rnp->qsmaskinit;
		rnp->qsmaskinit = rnp->qsmaskinitnext;

		 
		if (!oldmask != !rnp->qsmaskinit) {
			if (!oldmask) {  
				if (!rnp->wait_blkd_tasks)  
					rcu_init_new_rnp(rnp);
			} else if (rcu_preempt_has_tasks(rnp)) {
				rnp->wait_blkd_tasks = true;  
			} else {  
				rcu_cleanup_dead_rnp(rnp);
			}
		}

		 
		if (rnp->wait_blkd_tasks &&
		    (!rcu_preempt_has_tasks(rnp) || rnp->qsmaskinit)) {
			rnp->wait_blkd_tasks = false;
			if (!rnp->qsmaskinit)
				rcu_cleanup_dead_rnp(rnp);
		}

		raw_spin_unlock_rcu_node(rnp);
		arch_spin_unlock(&rcu_state.ofl_lock);
		local_irq_restore(flags);
	}
	rcu_gp_slow(gp_preinit_delay);  

	 
	WRITE_ONCE(rcu_state.gp_state, RCU_GP_INIT);
	rcu_for_each_node_breadth_first(rnp) {
		rcu_gp_slow(gp_init_delay);
		raw_spin_lock_irqsave_rcu_node(rnp, flags);
		rdp = this_cpu_ptr(&rcu_data);
		rcu_preempt_check_blocked_tasks(rnp);
		rnp->qsmask = rnp->qsmaskinit;
		WRITE_ONCE(rnp->gp_seq, rcu_state.gp_seq);
		if (rnp == rdp->mynode)
			(void)__note_gp_changes(rnp, rdp);
		rcu_preempt_boost_start_gp(rnp);
		trace_rcu_grace_period_init(rcu_state.name, rnp->gp_seq,
					    rnp->level, rnp->grplo,
					    rnp->grphi, rnp->qsmask);
		 
		mask = rnp->qsmask & ~rnp->qsmaskinitnext;
		rnp->rcu_gp_init_mask = mask;
		if ((mask || rnp->wait_blkd_tasks) && rcu_is_leaf_node(rnp))
			rcu_report_qs_rnp(mask, rnp, rnp->gp_seq, flags);
		else
			raw_spin_unlock_irq_rcu_node(rnp);
		cond_resched_tasks_rcu_qs();
		WRITE_ONCE(rcu_state.gp_activity, jiffies);
	}

	
	if (IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD))
		on_each_cpu(rcu_strict_gp_boundary, NULL, 0);

	return true;
}

 
static bool rcu_gp_fqs_check_wake(int *gfp)
{
	struct rcu_node *rnp = rcu_get_root();

	
	if (*gfp & RCU_GP_FLAG_OVLD)
		return true;

	
	*gfp = READ_ONCE(rcu_state.gp_flags);
	if (*gfp & RCU_GP_FLAG_FQS)
		return true;

	
	if (!READ_ONCE(rnp->qsmask) && !rcu_preempt_blocked_readers_cgp(rnp))
		return true;

	return false;
}

 
static void rcu_gp_fqs(bool first_time)
{
	int nr_fqs = READ_ONCE(rcu_state.nr_fqs_jiffies_stall);
	struct rcu_node *rnp = rcu_get_root();

	WRITE_ONCE(rcu_state.gp_activity, jiffies);
	WRITE_ONCE(rcu_state.n_force_qs, rcu_state.n_force_qs + 1);

	WARN_ON_ONCE(nr_fqs > 3);
	 
	if (nr_fqs) {
		if (nr_fqs == 1) {
			WRITE_ONCE(rcu_state.jiffies_stall,
				   jiffies + rcu_jiffies_till_stall_check());
		}
		WRITE_ONCE(rcu_state.nr_fqs_jiffies_stall, --nr_fqs);
	}

	if (first_time) {
		 
		force_qs_rnp(dyntick_save_progress_counter);
	} else {
		 
		force_qs_rnp(rcu_implicit_dynticks_qs);
	}
	 
	if (READ_ONCE(rcu_state.gp_flags) & RCU_GP_FLAG_FQS) {
		raw_spin_lock_irq_rcu_node(rnp);
		WRITE_ONCE(rcu_state.gp_flags,
			   READ_ONCE(rcu_state.gp_flags) & ~RCU_GP_FLAG_FQS);
		raw_spin_unlock_irq_rcu_node(rnp);
	}
}

 
static noinline_for_stack void rcu_gp_fqs_loop(void)
{
	bool first_gp_fqs = true;
	int gf = 0;
	unsigned long j;
	int ret;
	struct rcu_node *rnp = rcu_get_root();

	j = READ_ONCE(jiffies_till_first_fqs);
	if (rcu_state.cbovld)
		gf = RCU_GP_FLAG_OVLD;
	ret = 0;
	for (;;) {
		if (rcu_state.cbovld) {
			j = (j + 2) / 3;
			if (j <= 0)
				j = 1;
		}
		if (!ret || time_before(jiffies + j, rcu_state.jiffies_force_qs)) {
			WRITE_ONCE(rcu_state.jiffies_force_qs, jiffies + j);
			 
			smp_wmb();
			WRITE_ONCE(rcu_state.jiffies_kick_kthreads,
				   jiffies + (j ? 3 * j : 2));
		}
		trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq,
				       TPS("fqswait"));
		WRITE_ONCE(rcu_state.gp_state, RCU_GP_WAIT_FQS);
		(void)swait_event_idle_timeout_exclusive(rcu_state.gp_wq,
				 rcu_gp_fqs_check_wake(&gf), j);
		rcu_gp_torture_wait();
		WRITE_ONCE(rcu_state.gp_state, RCU_GP_DOING_FQS);
		 
		 
		if (!READ_ONCE(rnp->qsmask) &&
		    !rcu_preempt_blocked_readers_cgp(rnp))
			break;
		 
		if (!time_after(rcu_state.jiffies_force_qs, jiffies) ||
		    (gf & (RCU_GP_FLAG_FQS | RCU_GP_FLAG_OVLD))) {
			trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq,
					       TPS("fqsstart"));
			rcu_gp_fqs(first_gp_fqs);
			gf = 0;
			if (first_gp_fqs) {
				first_gp_fqs = false;
				gf = rcu_state.cbovld ? RCU_GP_FLAG_OVLD : 0;
			}
			trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq,
					       TPS("fqsend"));
			cond_resched_tasks_rcu_qs();
			WRITE_ONCE(rcu_state.gp_activity, jiffies);
			ret = 0;  
			j = READ_ONCE(jiffies_till_next_fqs);
		} else {
			 
			cond_resched_tasks_rcu_qs();
			WRITE_ONCE(rcu_state.gp_activity, jiffies);
			WARN_ON(signal_pending(current));
			trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq,
					       TPS("fqswaitsig"));
			ret = 1;  
			j = jiffies;
			if (time_after(jiffies, rcu_state.jiffies_force_qs))
				j = 1;
			else
				j = rcu_state.jiffies_force_qs - j;
			gf = 0;
		}
	}
}

 
static noinline void rcu_gp_cleanup(void)
{
	int cpu;
	bool needgp = false;
	unsigned long gp_duration;
	unsigned long new_gp_seq;
	bool offloaded;
	struct rcu_data *rdp;
	struct rcu_node *rnp = rcu_get_root();
	struct swait_queue_head *sq;

	WRITE_ONCE(rcu_state.gp_activity, jiffies);
	raw_spin_lock_irq_rcu_node(rnp);
	rcu_state.gp_end = jiffies;
	gp_duration = rcu_state.gp_end - rcu_state.gp_start;
	if (gp_duration > rcu_state.gp_max)
		rcu_state.gp_max = gp_duration;

	 
	rcu_poll_gp_seq_end(&rcu_state.gp_seq_polled_snap);
	raw_spin_unlock_irq_rcu_node(rnp);

	 
	new_gp_seq = rcu_state.gp_seq;
	rcu_seq_end(&new_gp_seq);
	rcu_for_each_node_breadth_first(rnp) {
		raw_spin_lock_irq_rcu_node(rnp);
		if (WARN_ON_ONCE(rcu_preempt_blocked_readers_cgp(rnp)))
			dump_blkd_tasks(rnp, 10);
		WARN_ON_ONCE(rnp->qsmask);
		WRITE_ONCE(rnp->gp_seq, new_gp_seq);
		if (!rnp->parent)
			smp_mb();  
		rdp = this_cpu_ptr(&rcu_data);
		if (rnp == rdp->mynode)
			needgp = __note_gp_changes(rnp, rdp) || needgp;
		 
		needgp = rcu_future_gp_cleanup(rnp) || needgp;
		 
		if (rcu_is_leaf_node(rnp))
			for_each_leaf_node_cpu_mask(rnp, cpu, rnp->cbovldmask) {
				rdp = per_cpu_ptr(&rcu_data, cpu);
				check_cb_ovld_locked(rdp, rnp);
			}
		sq = rcu_nocb_gp_get(rnp);
		raw_spin_unlock_irq_rcu_node(rnp);
		rcu_nocb_gp_cleanup(sq);
		cond_resched_tasks_rcu_qs();
		WRITE_ONCE(rcu_state.gp_activity, jiffies);
		rcu_gp_slow(gp_cleanup_delay);
	}
	rnp = rcu_get_root();
	raw_spin_lock_irq_rcu_node(rnp);  

	 
	trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq, TPS("end"));
	rcu_seq_end(&rcu_state.gp_seq);
	ASSERT_EXCLUSIVE_WRITER(rcu_state.gp_seq);
	WRITE_ONCE(rcu_state.gp_state, RCU_GP_IDLE);
	 
	rdp = this_cpu_ptr(&rcu_data);
	if (!needgp && ULONG_CMP_LT(rnp->gp_seq, rnp->gp_seq_needed)) {
		trace_rcu_this_gp(rnp, rdp, rnp->gp_seq_needed,
				  TPS("CleanupMore"));
		needgp = true;
	}
	 
	offloaded = rcu_rdp_is_offloaded(rdp);
	if ((offloaded || !rcu_accelerate_cbs(rnp, rdp)) && needgp) {

		 
		 
		 
		 
		 
		 
		 
		 
		 

		WRITE_ONCE(rcu_state.gp_flags, RCU_GP_FLAG_INIT);
		WRITE_ONCE(rcu_state.gp_req_activity, jiffies);
		trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq, TPS("newreq"));
	} else {

		 
		 
		 
		 
		 

		WRITE_ONCE(rcu_state.gp_flags, rcu_state.gp_flags & RCU_GP_FLAG_INIT);
	}
	raw_spin_unlock_irq_rcu_node(rnp);

	 
	if (IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD))
		on_each_cpu(rcu_strict_gp_boundary, NULL, 0);
}

 
static int __noreturn rcu_gp_kthread(void *unused)
{
	rcu_bind_gp_kthread();
	for (;;) {

		 
		for (;;) {
			trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq,
					       TPS("reqwait"));
			WRITE_ONCE(rcu_state.gp_state, RCU_GP_WAIT_GPS);
			swait_event_idle_exclusive(rcu_state.gp_wq,
					 READ_ONCE(rcu_state.gp_flags) &
					 RCU_GP_FLAG_INIT);
			rcu_gp_torture_wait();
			WRITE_ONCE(rcu_state.gp_state, RCU_GP_DONE_GPS);
			 
			if (rcu_gp_init())
				break;
			cond_resched_tasks_rcu_qs();
			WRITE_ONCE(rcu_state.gp_activity, jiffies);
			WARN_ON(signal_pending(current));
			trace_rcu_grace_period(rcu_state.name, rcu_state.gp_seq,
					       TPS("reqwaitsig"));
		}

		 
		rcu_gp_fqs_loop();

		 
		WRITE_ONCE(rcu_state.gp_state, RCU_GP_CLEANUP);
		rcu_gp_cleanup();
		WRITE_ONCE(rcu_state.gp_state, RCU_GP_CLEANED);
	}
}

 
static void rcu_report_qs_rsp(unsigned long flags)
	__releases(rcu_get_root()->lock)
{
	raw_lockdep_assert_held_rcu_node(rcu_get_root());
	WARN_ON_ONCE(!rcu_gp_in_progress());
	WRITE_ONCE(rcu_state.gp_flags,
		   READ_ONCE(rcu_state.gp_flags) | RCU_GP_FLAG_FQS);
	raw_spin_unlock_irqrestore_rcu_node(rcu_get_root(), flags);
	rcu_gp_kthread_wake();
}

 
static void rcu_report_qs_rnp(unsigned long mask, struct rcu_node *rnp,
			      unsigned long gps, unsigned long flags)
	__releases(rnp->lock)
{
	unsigned long oldmask = 0;
	struct rcu_node *rnp_c;

	raw_lockdep_assert_held_rcu_node(rnp);

	 
	for (;;) {
		if ((!(rnp->qsmask & mask) && mask) || rnp->gp_seq != gps) {

			 
			raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
			return;
		}
		WARN_ON_ONCE(oldmask);  
		WARN_ON_ONCE(!rcu_is_leaf_node(rnp) &&
			     rcu_preempt_blocked_readers_cgp(rnp));
		WRITE_ONCE(rnp->qsmask, rnp->qsmask & ~mask);
		trace_rcu_quiescent_state_report(rcu_state.name, rnp->gp_seq,
						 mask, rnp->qsmask, rnp->level,
						 rnp->grplo, rnp->grphi,
						 !!rnp->gp_tasks);
		if (rnp->qsmask != 0 || rcu_preempt_blocked_readers_cgp(rnp)) {

			 
			raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
			return;
		}
		rnp->completedqs = rnp->gp_seq;
		mask = rnp->grpmask;
		if (rnp->parent == NULL) {

			 

			break;
		}
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
		rnp_c = rnp;
		rnp = rnp->parent;
		raw_spin_lock_irqsave_rcu_node(rnp, flags);
		oldmask = READ_ONCE(rnp_c->qsmask);
	}

	 
	rcu_report_qs_rsp(flags);  
}

 
static void __maybe_unused
rcu_report_unblock_qs_rnp(struct rcu_node *rnp, unsigned long flags)
	__releases(rnp->lock)
{
	unsigned long gps;
	unsigned long mask;
	struct rcu_node *rnp_p;

	raw_lockdep_assert_held_rcu_node(rnp);
	if (WARN_ON_ONCE(!IS_ENABLED(CONFIG_PREEMPT_RCU)) ||
	    WARN_ON_ONCE(rcu_preempt_blocked_readers_cgp(rnp)) ||
	    rnp->qsmask != 0) {
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
		return;   
	}

	rnp->completedqs = rnp->gp_seq;
	rnp_p = rnp->parent;
	if (rnp_p == NULL) {
		 
		rcu_report_qs_rsp(flags);
		return;
	}

	 
	gps = rnp->gp_seq;
	mask = rnp->grpmask;
	raw_spin_unlock_rcu_node(rnp);	 
	raw_spin_lock_rcu_node(rnp_p);	 
	rcu_report_qs_rnp(mask, rnp_p, gps, flags);
}

 
static void
rcu_report_qs_rdp(struct rcu_data *rdp)
{
	unsigned long flags;
	unsigned long mask;
	bool needacc = false;
	struct rcu_node *rnp;

	WARN_ON_ONCE(rdp->cpu != smp_processor_id());
	rnp = rdp->mynode;
	raw_spin_lock_irqsave_rcu_node(rnp, flags);
	if (rdp->cpu_no_qs.b.norm || rdp->gp_seq != rnp->gp_seq ||
	    rdp->gpwrap) {

		 
		rdp->cpu_no_qs.b.norm = true;	 
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
		return;
	}
	mask = rdp->grpmask;
	rdp->core_needs_qs = false;
	if ((rnp->qsmask & mask) == 0) {
		raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
	} else {
		 
		if (!rcu_rdp_is_offloaded(rdp)) {
			 
			WARN_ON_ONCE(rcu_accelerate_cbs(rnp, rdp));
		} else if (!rcu_segcblist_completely_offloaded(&rdp->cblist)) {
			 
			needacc = true;
		}

		rcu_disable_urgency_upon_qs(rdp);
		rcu_report_qs_rnp(mask, rnp, rnp->gp_seq, flags);
		 

		if (needacc) {
			rcu_nocb_lock_irqsave(rdp, flags);
			rcu_accelerate_cbs_unlocked(rnp, rdp);
			rcu_nocb_unlock_irqrestore(rdp, flags);
		}
	}
}

 
static void
rcu_check_quiescent_state(struct rcu_data *rdp)
{
	 
	note_gp_changes(rdp);

	 
	if (!rdp->core_needs_qs)
		return;

	 
	if (rdp->cpu_no_qs.b.norm)
		return;

	 
	rcu_report_qs_rdp(rdp);
}

 
static bool rcu_do_batch_check_time(long count, long tlimit,
				    bool jlimit_check, unsigned long jlimit)
{
	 
	return unlikely(tlimit) &&
	       (!likely(count & 31) ||
		(IS_ENABLED(CONFIG_RCU_DOUBLE_CHECK_CB_TIME) &&
		 jlimit_check && time_after(jiffies, jlimit))) &&
	       local_clock() >= tlimit;
}

 
static void rcu_do_batch(struct rcu_data *rdp)
{
	long bl;
	long count = 0;
	int div;
	bool __maybe_unused empty;
	unsigned long flags;
	unsigned long jlimit;
	bool jlimit_check = false;
	long pending;
	struct rcu_cblist rcl = RCU_CBLIST_INITIALIZER(rcl);
	struct rcu_head *rhp;
	long tlimit = 0;

	 
	if (!rcu_segcblist_ready_cbs(&rdp->cblist)) {
		trace_rcu_batch_start(rcu_state.name,
				      rcu_segcblist_n_cbs(&rdp->cblist), 0);
		trace_rcu_batch_end(rcu_state.name, 0,
				    !rcu_segcblist_empty(&rdp->cblist),
				    need_resched(), is_idle_task(current),
				    rcu_is_callbacks_kthread(rdp));
		return;
	}

	 
	rcu_nocb_lock_irqsave(rdp, flags);
	WARN_ON_ONCE(cpu_is_offline(smp_processor_id()));
	pending = rcu_segcblist_get_seglen(&rdp->cblist, RCU_DONE_TAIL);
	div = READ_ONCE(rcu_divisor);
	div = div < 0 ? 7 : div > sizeof(long) * 8 - 2 ? sizeof(long) * 8 - 2 : div;
	bl = max(rdp->blimit, pending >> div);
	if ((in_serving_softirq() || rdp->rcu_cpu_kthread_status == RCU_KTHREAD_RUNNING) &&
	    (IS_ENABLED(CONFIG_RCU_DOUBLE_CHECK_CB_TIME) || unlikely(bl > 100))) {
		const long npj = NSEC_PER_SEC / HZ;
		long rrn = READ_ONCE(rcu_resched_ns);

		rrn = rrn < NSEC_PER_MSEC ? NSEC_PER_MSEC : rrn > NSEC_PER_SEC ? NSEC_PER_SEC : rrn;
		tlimit = local_clock() + rrn;
		jlimit = jiffies + (rrn + npj + 1) / npj;
		jlimit_check = true;
	}
	trace_rcu_batch_start(rcu_state.name,
			      rcu_segcblist_n_cbs(&rdp->cblist), bl);
	rcu_segcblist_extract_done_cbs(&rdp->cblist, &rcl);
	if (rcu_rdp_is_offloaded(rdp))
		rdp->qlen_last_fqs_check = rcu_segcblist_n_cbs(&rdp->cblist);

	trace_rcu_segcb_stats(&rdp->cblist, TPS("SegCbDequeued"));
	rcu_nocb_unlock_irqrestore(rdp, flags);

	 
	tick_dep_set_task(current, TICK_DEP_BIT_RCU);
	rhp = rcu_cblist_dequeue(&rcl);

	for (; rhp; rhp = rcu_cblist_dequeue(&rcl)) {
		rcu_callback_t f;

		count++;
		debug_rcu_head_unqueue(rhp);

		rcu_lock_acquire(&rcu_callback_map);
		trace_rcu_invoke_callback(rcu_state.name, rhp);

		f = rhp->func;
		WRITE_ONCE(rhp->func, (rcu_callback_t)0L);
		f(rhp);

		rcu_lock_release(&rcu_callback_map);

		 
		if (in_serving_softirq()) {
			if (count >= bl && (need_resched() || !is_idle_task(current)))
				break;
			 
			if (rcu_do_batch_check_time(count, tlimit, jlimit_check, jlimit))
				break;
		} else {
			
			
			local_bh_enable();
			lockdep_assert_irqs_enabled();
			cond_resched_tasks_rcu_qs();
			lockdep_assert_irqs_enabled();
			local_bh_disable();
			
			
			if (rdp->rcu_cpu_kthread_status == RCU_KTHREAD_RUNNING &&
			    rcu_do_batch_check_time(count, tlimit, jlimit_check, jlimit)) {
				rdp->rcu_cpu_has_work = 1;
				break;
			}
		}
	}

	rcu_nocb_lock_irqsave(rdp, flags);
	rdp->n_cbs_invoked += count;
	trace_rcu_batch_end(rcu_state.name, count, !!rcl.head, need_resched(),
			    is_idle_task(current), rcu_is_callbacks_kthread(rdp));

	 
	rcu_segcblist_insert_done_cbs(&rdp->cblist, &rcl);
	rcu_segcblist_add_len(&rdp->cblist, -count);

	 
	count = rcu_segcblist_n_cbs(&rdp->cblist);
	if (rdp->blimit >= DEFAULT_MAX_RCU_BLIMIT && count <= qlowmark)
		rdp->blimit = blimit;

	 
	if (count == 0 && rdp->qlen_last_fqs_check != 0) {
		rdp->qlen_last_fqs_check = 0;
		rdp->n_force_qs_snap = READ_ONCE(rcu_state.n_force_qs);
	} else if (count < rdp->qlen_last_fqs_check - qhimark)
		rdp->qlen_last_fqs_check = count;

	 
	empty = rcu_segcblist_empty(&rdp->cblist);
	WARN_ON_ONCE(count == 0 && !empty);
	WARN_ON_ONCE(!IS_ENABLED(CONFIG_RCU_NOCB_CPU) &&
		     count != 0 && empty);
	WARN_ON_ONCE(count == 0 && rcu_segcblist_n_segment_cbs(&rdp->cblist) != 0);
	WARN_ON_ONCE(!empty && rcu_segcblist_n_segment_cbs(&rdp->cblist) == 0);

	rcu_nocb_unlock_irqrestore(rdp, flags);

	tick_dep_clear_task(current, TICK_DEP_BIT_RCU);
}

 
void rcu_sched_clock_irq(int user)
{
	unsigned long j;

	if (IS_ENABLED(CONFIG_PROVE_RCU)) {
		j = jiffies;
		WARN_ON_ONCE(time_before(j, __this_cpu_read(rcu_data.last_sched_clock)));
		__this_cpu_write(rcu_data.last_sched_clock, j);
	}
	trace_rcu_utilization(TPS("Start scheduler-tick"));
	lockdep_assert_irqs_disabled();
	raw_cpu_inc(rcu_data.ticks_this_gp);
	 
	if (smp_load_acquire(this_cpu_ptr(&rcu_data.rcu_urgent_qs))) {
		 
		if (!rcu_is_cpu_rrupt_from_idle() && !user) {
			set_tsk_need_resched(current);
			set_preempt_need_resched();
		}
		__this_cpu_write(rcu_data.rcu_urgent_qs, false);
	}
	rcu_flavor_sched_clock_irq(user);
	if (rcu_pending(user))
		invoke_rcu_core();
	if (user || rcu_is_cpu_rrupt_from_idle())
		rcu_note_voluntary_context_switch(current);
	lockdep_assert_irqs_disabled();

	trace_rcu_utilization(TPS("End scheduler-tick"));
}

 
static void force_qs_rnp(int (*f)(struct rcu_data *rdp))
{
	int cpu;
	unsigned long flags;
	struct rcu_node *rnp;

	rcu_state.cbovld = rcu_state.cbovldnext;
	rcu_state.cbovldnext = false;
	rcu_for_each_leaf_node(rnp) {
		unsigned long mask = 0;
		unsigned long rsmask = 0;

		cond_resched_tasks_rcu_qs();
		raw_spin_lock_irqsave_rcu_node(rnp, flags);
		rcu_state.cbovldnext |= !!rnp->cbovldmask;
		if (rnp->qsmask == 0) {
			if (rcu_preempt_blocked_readers_cgp(rnp)) {
				 
				rcu_initiate_boost(rnp, flags);
				 
				continue;
			}
			raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
			continue;
		}
		for_each_leaf_node_cpu_mask(rnp, cpu, rnp->qsmask) {
			struct rcu_data *rdp;
			int ret;

			rdp = per_cpu_ptr(&rcu_data, cpu);
			ret = f(rdp);
			if (ret > 0) {
				mask |= rdp->grpmask;
				rcu_disable_urgency_upon_qs(rdp);
			}
			if (ret < 0)
				rsmask |= rdp->grpmask;
		}
		if (mask != 0) {
			 
			rcu_report_qs_rnp(mask, rnp, rnp->gp_seq, flags);
		} else {
			 
			raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
		}

		for_each_leaf_node_cpu_mask(rnp, cpu, rsmask)
			resched_cpu(cpu);
	}
}

 
void rcu_force_quiescent_state(void)
{
	unsigned long flags;
	bool ret;
	struct rcu_node *rnp;
	struct rcu_node *rnp_old = NULL;

	 
	rnp = raw_cpu_read(rcu_data.mynode);
	for (; rnp != NULL; rnp = rnp->parent) {
		ret = (READ_ONCE(rcu_state.gp_flags) & RCU_GP_FLAG_FQS) ||
		       !raw_spin_trylock(&rnp->fqslock);
		if (rnp_old != NULL)
			raw_spin_unlock(&rnp_old->fqslock);
		if (ret)
			return;
		rnp_old = rnp;
	}
	 

	 
	raw_spin_lock_irqsave_rcu_node(rnp_old, flags);
	raw_spin_unlock(&rnp_old->fqslock);
	if (READ_ONCE(rcu_state.gp_flags) & RCU_GP_FLAG_FQS) {
		raw_spin_unlock_irqrestore_rcu_node(rnp_old, flags);
		return;   
	}
	WRITE_ONCE(rcu_state.gp_flags,
		   READ_ONCE(rcu_state.gp_flags) | RCU_GP_FLAG_FQS);
	raw_spin_unlock_irqrestore_rcu_node(rnp_old, flags);
	rcu_gp_kthread_wake();
}
EXPORT_SYMBOL_GPL(rcu_force_quiescent_state);



static void strict_work_handler(struct work_struct *work)
{
	rcu_read_lock();
	rcu_read_unlock();
}

 
static __latent_entropy void rcu_core(void)
{
	unsigned long flags;
	struct rcu_data *rdp = raw_cpu_ptr(&rcu_data);
	struct rcu_node *rnp = rdp->mynode;
	 
	const bool do_batch = !rcu_segcblist_completely_offloaded(&rdp->cblist);

	if (cpu_is_offline(smp_processor_id()))
		return;
	trace_rcu_utilization(TPS("Start RCU core"));
	WARN_ON_ONCE(!rdp->beenonline);

	 
	if (IS_ENABLED(CONFIG_PREEMPT_COUNT) && (!(preempt_count() & PREEMPT_MASK))) {
		rcu_preempt_deferred_qs(current);
	} else if (rcu_preempt_need_deferred_qs(current)) {
		set_tsk_need_resched(current);
		set_preempt_need_resched();
	}

	 
	rcu_check_quiescent_state(rdp);

	 
	if (!rcu_gp_in_progress() &&
	    rcu_segcblist_is_enabled(&rdp->cblist) && do_batch) {
		rcu_nocb_lock_irqsave(rdp, flags);
		if (!rcu_segcblist_restempty(&rdp->cblist, RCU_NEXT_READY_TAIL))
			rcu_accelerate_cbs_unlocked(rnp, rdp);
		rcu_nocb_unlock_irqrestore(rdp, flags);
	}

	rcu_check_gp_start_stall(rnp, rdp, rcu_jiffies_till_stall_check());

	 
	if (do_batch && rcu_segcblist_ready_cbs(&rdp->cblist) &&
	    likely(READ_ONCE(rcu_scheduler_fully_active))) {
		rcu_do_batch(rdp);
		 
		if (rcu_segcblist_ready_cbs(&rdp->cblist))
			invoke_rcu_core();
	}

	 
	do_nocb_deferred_wakeup(rdp);
	trace_rcu_utilization(TPS("End RCU core"));

	
	if (IS_ENABLED(CONFIG_RCU_STRICT_GRACE_PERIOD))
		queue_work_on(rdp->cpu, rcu_gp_wq, &rdp->strict_work);
}

static void rcu_core_si(struct softirq_action *h)
{
	rcu_core();
}

static void rcu_wake_cond(struct task_struct *t, int status)
{
	 
	if (t && (status != RCU_KTHREAD_YIELDING || is_idle_task(current)))
		wake_up_process(t);
}

static void invoke_rcu_core_kthread(void)
{
	struct task_struct *t;
	unsigned long flags;

	local_irq_save(flags);
	__this_cpu_write(rcu_data.rcu_cpu_has_work, 1);
	t = __this_cpu_read(rcu_data.rcu_cpu_kthread_task);
	if (t != NULL && t != current)
		rcu_wake_cond(t, __this_cpu_read(rcu_data.rcu_cpu_kthread_status));
	local_irq_restore(flags);
}

 
static void invoke_rcu_core(void)
{
	if (!cpu_online(smp_processor_id()))
		return;
	if (use_softirq)
		raise_softirq(RCU_SOFTIRQ);
	else
		invoke_rcu_core_kthread();
}

static void rcu_cpu_kthread_park(unsigned int cpu)
{
	per_cpu(rcu_data.rcu_cpu_kthread_status, cpu) = RCU_KTHREAD_OFFCPU;
}

static int rcu_cpu_kthread_should_run(unsigned int cpu)
{
	return __this_cpu_read(rcu_data.rcu_cpu_has_work);
}

 
static void rcu_cpu_kthread(unsigned int cpu)
{
	unsigned int *statusp = this_cpu_ptr(&rcu_data.rcu_cpu_kthread_status);
	char work, *workp = this_cpu_ptr(&rcu_data.rcu_cpu_has_work);
	unsigned long *j = this_cpu_ptr(&rcu_data.rcuc_activity);
	int spincnt;

	trace_rcu_utilization(TPS("Start CPU kthread@rcu_run"));
	for (spincnt = 0; spincnt < 10; spincnt++) {
		WRITE_ONCE(*j, jiffies);
		local_bh_disable();
		*statusp = RCU_KTHREAD_RUNNING;
		local_irq_disable();
		work = *workp;
		WRITE_ONCE(*workp, 0);
		local_irq_enable();
		if (work)
			rcu_core();
		local_bh_enable();
		if (!READ_ONCE(*workp)) {
			trace_rcu_utilization(TPS("End CPU kthread@rcu_wait"));
			*statusp = RCU_KTHREAD_WAITING;
			return;
		}
	}
	*statusp = RCU_KTHREAD_YIELDING;
	trace_rcu_utilization(TPS("Start CPU kthread@rcu_yield"));
	schedule_timeout_idle(2);
	trace_rcu_utilization(TPS("End CPU kthread@rcu_yield"));
	*statusp = RCU_KTHREAD_WAITING;
	WRITE_ONCE(*j, jiffies);
}

static struct smp_hotplug_thread rcu_cpu_thread_spec = {
	.store			= &rcu_data.rcu_cpu_kthread_task,
	.thread_should_run	= rcu_cpu_kthread_should_run,
	.thread_fn		= rcu_cpu_kthread,
	.thread_comm		= "rcuc/%u",
	.setup			= rcu_cpu_kthread_setup,
	.park			= rcu_cpu_kthread_park,
};

 
static int __init rcu_spawn_core_kthreads(void)
{
	int cpu;

	for_each_possible_cpu(cpu)
		per_cpu(rcu_data.rcu_cpu_has_work, cpu) = 0;
	if (use_softirq)
		return 0;
	WARN_ONCE(smpboot_register_percpu_thread(&rcu_cpu_thread_spec),
		  "%s: Could not start rcuc kthread, OOM is now expected behavior\n", __func__);
	return 0;
}

 
static void __call_rcu_core(struct rcu_data *rdp, struct rcu_head *head,
			    unsigned long flags)
{
	 
	if (!rcu_is_watching())
		invoke_rcu_core();

	 
	if (irqs_disabled_flags(flags) || cpu_is_offline(smp_processor_id()))
		return;

	 
	if (unlikely(rcu_segcblist_n_cbs(&rdp->cblist) >
		     rdp->qlen_last_fqs_check + qhimark)) {

		 
		note_gp_changes(rdp);

		 
		if (!rcu_gp_in_progress()) {
			rcu_accelerate_cbs_unlocked(rdp->mynode, rdp);
		} else {
			 
			rdp->blimit = DEFAULT_MAX_RCU_BLIMIT;
			if (READ_ONCE(rcu_state.n_force_qs) == rdp->n_force_qs_snap &&
			    rcu_segcblist_first_pend_cb(&rdp->cblist) != head)
				rcu_force_quiescent_state();
			rdp->n_force_qs_snap = READ_ONCE(rcu_state.n_force_qs);
			rdp->qlen_last_fqs_check = rcu_segcblist_n_cbs(&rdp->cblist);
		}
	}
}

 
static void rcu_leak_callback(struct rcu_head *rhp)
{
}

 
static void check_cb_ovld_locked(struct rcu_data *rdp, struct rcu_node *rnp)
{
	raw_lockdep_assert_held_rcu_node(rnp);
	if (qovld_calc <= 0)
		return; 
	if (rcu_segcblist_n_cbs(&rdp->cblist) >= qovld_calc)
		WRITE_ONCE(rnp->cbovldmask, rnp->cbovldmask | rdp->grpmask);
	else
		WRITE_ONCE(rnp->cbovldmask, rnp->cbovldmask & ~rdp->grpmask);
}

 
static void check_cb_ovld(struct rcu_data *rdp)
{
	struct rcu_node *const rnp = rdp->mynode;

	if (qovld_calc <= 0 ||
	    ((rcu_segcblist_n_cbs(&rdp->cblist) >= qovld_calc) ==
	     !!(READ_ONCE(rnp->cbovldmask) & rdp->grpmask)))
		return; 
	raw_spin_lock_rcu_node(rnp);
	check_cb_ovld_locked(rdp, rnp);
	raw_spin_unlock_rcu_node(rnp);
}

static void
__call_rcu_common(struct rcu_head *head, rcu_callback_t func, bool lazy_in)
{
	static atomic_t doublefrees;
	unsigned long flags;
	bool lazy;
	struct rcu_data *rdp;
	bool was_alldone;

	 
	WARN_ON_ONCE((unsigned long)head & (sizeof(void *) - 1));

	if (debug_rcu_head_queue(head)) {
		 
		if (atomic_inc_return(&doublefrees) < 4) {
			pr_err("%s(): Double-freed CB %p->%pS()!!!  ", __func__, head, head->func);
			mem_dump_obj(head);
		}
		WRITE_ONCE(head->func, rcu_leak_callback);
		return;
	}
	head->func = func;
	head->next = NULL;
	kasan_record_aux_stack_noalloc(head);
	local_irq_save(flags);
	rdp = this_cpu_ptr(&rcu_data);
	lazy = lazy_in && !rcu_async_should_hurry();

	 
	if (unlikely(!rcu_segcblist_is_enabled(&rdp->cblist))) {
		
		WARN_ON_ONCE(rcu_scheduler_active != RCU_SCHEDULER_INACTIVE);
		WARN_ON_ONCE(!rcu_is_watching());
		
		
		if (rcu_segcblist_empty(&rdp->cblist))
			rcu_segcblist_init(&rdp->cblist);
	}

	check_cb_ovld(rdp);
	if (rcu_nocb_try_bypass(rdp, head, &was_alldone, flags, lazy))
		return; 
	
	rcu_segcblist_enqueue(&rdp->cblist, head);
	if (__is_kvfree_rcu_offset((unsigned long)func))
		trace_rcu_kvfree_callback(rcu_state.name, head,
					 (unsigned long)func,
					 rcu_segcblist_n_cbs(&rdp->cblist));
	else
		trace_rcu_callback(rcu_state.name, head,
				   rcu_segcblist_n_cbs(&rdp->cblist));

	trace_rcu_segcb_stats(&rdp->cblist, TPS("SegCBQueued"));

	 
	if (unlikely(rcu_rdp_is_offloaded(rdp))) {
		__call_rcu_nocb_wake(rdp, was_alldone, flags);  
	} else {
		__call_rcu_core(rdp, head, flags);
		local_irq_restore(flags);
	}
}

#ifdef CONFIG_RCU_LAZY
 
void call_rcu_hurry(struct rcu_head *head, rcu_callback_t func)
{
	return __call_rcu_common(head, func, false);
}
EXPORT_SYMBOL_GPL(call_rcu_hurry);
#endif

 
void call_rcu(struct rcu_head *head, rcu_callback_t func)
{
	return __call_rcu_common(head, func, IS_ENABLED(CONFIG_RCU_LAZY));
}
EXPORT_SYMBOL_GPL(call_rcu);

 
#define KFREE_DRAIN_JIFFIES (5 * HZ)
#define KFREE_N_BATCHES 2
#define FREE_N_CHANNELS 2

 
struct kvfree_rcu_bulk_data {
	struct list_head list;
	struct rcu_gp_oldstate gp_snap;
	unsigned long nr_records;
	void *records[];
};

 
#define KVFREE_BULK_MAX_ENTR \
	((PAGE_SIZE - sizeof(struct kvfree_rcu_bulk_data)) / sizeof(void *))

 

struct kfree_rcu_cpu_work {
	struct rcu_work rcu_work;
	struct rcu_head *head_free;
	struct rcu_gp_oldstate head_free_gp_snap;
	struct list_head bulk_head_free[FREE_N_CHANNELS];
	struct kfree_rcu_cpu *krcp;
};

 
struct kfree_rcu_cpu {
	
	
	struct rcu_head *head;
	unsigned long head_gp_snap;
	atomic_t head_count;

	
	struct list_head bulk_head[FREE_N_CHANNELS];
	atomic_t bulk_count[FREE_N_CHANNELS];

	struct kfree_rcu_cpu_work krw_arr[KFREE_N_BATCHES];
	raw_spinlock_t lock;
	struct delayed_work monitor_work;
	bool initialized;

	struct delayed_work page_cache_work;
	atomic_t backoff_page_cache_fill;
	atomic_t work_in_progress;
	struct hrtimer hrtimer;

	struct llist_head bkvcache;
	int nr_bkv_objs;
};

static DEFINE_PER_CPU(struct kfree_rcu_cpu, krc) = {
	.lock = __RAW_SPIN_LOCK_UNLOCKED(krc.lock),
};

static __always_inline void
debug_rcu_bhead_unqueue(struct kvfree_rcu_bulk_data *bhead)
{
#ifdef CONFIG_DEBUG_OBJECTS_RCU_HEAD
	int i;

	for (i = 0; i < bhead->nr_records; i++)
		debug_rcu_head_unqueue((struct rcu_head *)(bhead->records[i]));
#endif
}

static inline struct kfree_rcu_cpu *
krc_this_cpu_lock(unsigned long *flags)
{
	struct kfree_rcu_cpu *krcp;

	local_irq_save(*flags);	
	krcp = this_cpu_ptr(&krc);
	raw_spin_lock(&krcp->lock);

	return krcp;
}

static inline void
krc_this_cpu_unlock(struct kfree_rcu_cpu *krcp, unsigned long flags)
{
	raw_spin_unlock_irqrestore(&krcp->lock, flags);
}

static inline struct kvfree_rcu_bulk_data *
get_cached_bnode(struct kfree_rcu_cpu *krcp)
{
	if (!krcp->nr_bkv_objs)
		return NULL;

	WRITE_ONCE(krcp->nr_bkv_objs, krcp->nr_bkv_objs - 1);
	return (struct kvfree_rcu_bulk_data *)
		llist_del_first(&krcp->bkvcache);
}

static inline bool
put_cached_bnode(struct kfree_rcu_cpu *krcp,
	struct kvfree_rcu_bulk_data *bnode)
{
	
	if (krcp->nr_bkv_objs >= rcu_min_cached_objs)
		return false;

	llist_add((struct llist_node *) bnode, &krcp->bkvcache);
	WRITE_ONCE(krcp->nr_bkv_objs, krcp->nr_bkv_objs + 1);
	return true;
}

static int
drain_page_cache(struct kfree_rcu_cpu *krcp)
{
	unsigned long flags;
	struct llist_node *page_list, *pos, *n;
	int freed = 0;

	if (!rcu_min_cached_objs)
		return 0;

	raw_spin_lock_irqsave(&krcp->lock, flags);
	page_list = llist_del_all(&krcp->bkvcache);
	WRITE_ONCE(krcp->nr_bkv_objs, 0);
	raw_spin_unlock_irqrestore(&krcp->lock, flags);

	llist_for_each_safe(pos, n, page_list) {
		free_page((unsigned long)pos);
		freed++;
	}

	return freed;
}

static void
kvfree_rcu_bulk(struct kfree_rcu_cpu *krcp,
	struct kvfree_rcu_bulk_data *bnode, int idx)
{
	unsigned long flags;
	int i;

	if (!WARN_ON_ONCE(!poll_state_synchronize_rcu_full(&bnode->gp_snap))) {
		debug_rcu_bhead_unqueue(bnode);
		rcu_lock_acquire(&rcu_callback_map);
		if (idx == 0) { 
			trace_rcu_invoke_kfree_bulk_callback(
				rcu_state.name, bnode->nr_records,
				bnode->records);

			kfree_bulk(bnode->nr_records, bnode->records);
		} else { 
			for (i = 0; i < bnode->nr_records; i++) {
				trace_rcu_invoke_kvfree_callback(
					rcu_state.name, bnode->records[i], 0);

				vfree(bnode->records[i]);
			}
		}
		rcu_lock_release(&rcu_callback_map);
	}

	raw_spin_lock_irqsave(&krcp->lock, flags);
	if (put_cached_bnode(krcp, bnode))
		bnode = NULL;
	raw_spin_unlock_irqrestore(&krcp->lock, flags);

	if (bnode)
		free_page((unsigned long) bnode);

	cond_resched_tasks_rcu_qs();
}

static void
kvfree_rcu_list(struct rcu_head *head)
{
	struct rcu_head *next;

	for (; head; head = next) {
		void *ptr = (void *) head->func;
		unsigned long offset = (void *) head - ptr;

		next = head->next;
		debug_rcu_head_unqueue((struct rcu_head *)ptr);
		rcu_lock_acquire(&rcu_callback_map);
		trace_rcu_invoke_kvfree_callback(rcu_state.name, head, offset);

		if (!WARN_ON_ONCE(!__is_kvfree_rcu_offset(offset)))
			kvfree(ptr);

		rcu_lock_release(&rcu_callback_map);
		cond_resched_tasks_rcu_qs();
	}
}

 
static void kfree_rcu_work(struct work_struct *work)
{
	unsigned long flags;
	struct kvfree_rcu_bulk_data *bnode, *n;
	struct list_head bulk_head[FREE_N_CHANNELS];
	struct rcu_head *head;
	struct kfree_rcu_cpu *krcp;
	struct kfree_rcu_cpu_work *krwp;
	struct rcu_gp_oldstate head_gp_snap;
	int i;

	krwp = container_of(to_rcu_work(work),
		struct kfree_rcu_cpu_work, rcu_work);
	krcp = krwp->krcp;

	raw_spin_lock_irqsave(&krcp->lock, flags);
	
	for (i = 0; i < FREE_N_CHANNELS; i++)
		list_replace_init(&krwp->bulk_head_free[i], &bulk_head[i]);

	
	head = krwp->head_free;
	krwp->head_free = NULL;
	head_gp_snap = krwp->head_free_gp_snap;
	raw_spin_unlock_irqrestore(&krcp->lock, flags);

	
	for (i = 0; i < FREE_N_CHANNELS; i++) {
		
		list_for_each_entry_safe(bnode, n, &bulk_head[i], list)
			kvfree_rcu_bulk(krcp, bnode, i);
	}

	 
	if (head && !WARN_ON_ONCE(!poll_state_synchronize_rcu_full(&head_gp_snap)))
		kvfree_rcu_list(head);
}

static bool
need_offload_krc(struct kfree_rcu_cpu *krcp)
{
	int i;

	for (i = 0; i < FREE_N_CHANNELS; i++)
		if (!list_empty(&krcp->bulk_head[i]))
			return true;

	return !!READ_ONCE(krcp->head);
}

static bool
need_wait_for_krwp_work(struct kfree_rcu_cpu_work *krwp)
{
	int i;

	for (i = 0; i < FREE_N_CHANNELS; i++)
		if (!list_empty(&krwp->bulk_head_free[i]))
			return true;

	return !!krwp->head_free;
}

static int krc_count(struct kfree_rcu_cpu *krcp)
{
	int sum = atomic_read(&krcp->head_count);
	int i;

	for (i = 0; i < FREE_N_CHANNELS; i++)
		sum += atomic_read(&krcp->bulk_count[i]);

	return sum;
}

static void
schedule_delayed_monitor_work(struct kfree_rcu_cpu *krcp)
{
	long delay, delay_left;

	delay = krc_count(krcp) >= KVFREE_BULK_MAX_ENTR ? 1:KFREE_DRAIN_JIFFIES;
	if (delayed_work_pending(&krcp->monitor_work)) {
		delay_left = krcp->monitor_work.timer.expires - jiffies;
		if (delay < delay_left)
			mod_delayed_work(system_wq, &krcp->monitor_work, delay);
		return;
	}
	queue_delayed_work(system_wq, &krcp->monitor_work, delay);
}

static void
kvfree_rcu_drain_ready(struct kfree_rcu_cpu *krcp)
{
	struct list_head bulk_ready[FREE_N_CHANNELS];
	struct kvfree_rcu_bulk_data *bnode, *n;
	struct rcu_head *head_ready = NULL;
	unsigned long flags;
	int i;

	raw_spin_lock_irqsave(&krcp->lock, flags);
	for (i = 0; i < FREE_N_CHANNELS; i++) {
		INIT_LIST_HEAD(&bulk_ready[i]);

		list_for_each_entry_safe_reverse(bnode, n, &krcp->bulk_head[i], list) {
			if (!poll_state_synchronize_rcu_full(&bnode->gp_snap))
				break;

			atomic_sub(bnode->nr_records, &krcp->bulk_count[i]);
			list_move(&bnode->list, &bulk_ready[i]);
		}
	}

	if (krcp->head && poll_state_synchronize_rcu(krcp->head_gp_snap)) {
		head_ready = krcp->head;
		atomic_set(&krcp->head_count, 0);
		WRITE_ONCE(krcp->head, NULL);
	}
	raw_spin_unlock_irqrestore(&krcp->lock, flags);

	for (i = 0; i < FREE_N_CHANNELS; i++) {
		list_for_each_entry_safe(bnode, n, &bulk_ready[i], list)
			kvfree_rcu_bulk(krcp, bnode, i);
	}

	if (head_ready)
		kvfree_rcu_list(head_ready);
}

 
static void kfree_rcu_monitor(struct work_struct *work)
{
	struct kfree_rcu_cpu *krcp = container_of(work,
		struct kfree_rcu_cpu, monitor_work.work);
	unsigned long flags;
	int i, j;

	
	kvfree_rcu_drain_ready(krcp);

	raw_spin_lock_irqsave(&krcp->lock, flags);

	
	for (i = 0; i < KFREE_N_BATCHES; i++) {
		struct kfree_rcu_cpu_work *krwp = &(krcp->krw_arr[i]);

		
		
		
		if (need_wait_for_krwp_work(krwp))
			continue;

		
		if (need_offload_krc(krcp)) {
			
			
			for (j = 0; j < FREE_N_CHANNELS; j++) {
				if (list_empty(&krwp->bulk_head_free[j])) {
					atomic_set(&krcp->bulk_count[j], 0);
					list_replace_init(&krcp->bulk_head[j],
						&krwp->bulk_head_free[j]);
				}
			}

			
			
			if (!krwp->head_free) {
				krwp->head_free = krcp->head;
				get_state_synchronize_rcu_full(&krwp->head_free_gp_snap);
				atomic_set(&krcp->head_count, 0);
				WRITE_ONCE(krcp->head, NULL);
			}

			
			
			
			
			
			queue_rcu_work(system_wq, &krwp->rcu_work);
		}
	}

	raw_spin_unlock_irqrestore(&krcp->lock, flags);

	
	
	
	
	
	if (need_offload_krc(krcp))
		schedule_delayed_monitor_work(krcp);
}

static enum hrtimer_restart
schedule_page_work_fn(struct hrtimer *t)
{
	struct kfree_rcu_cpu *krcp =
		container_of(t, struct kfree_rcu_cpu, hrtimer);

	queue_delayed_work(system_highpri_wq, &krcp->page_cache_work, 0);
	return HRTIMER_NORESTART;
}

static void fill_page_cache_func(struct work_struct *work)
{
	struct kvfree_rcu_bulk_data *bnode;
	struct kfree_rcu_cpu *krcp =
		container_of(work, struct kfree_rcu_cpu,
			page_cache_work.work);
	unsigned long flags;
	int nr_pages;
	bool pushed;
	int i;

	nr_pages = atomic_read(&krcp->backoff_page_cache_fill) ?
		1 : rcu_min_cached_objs;

	for (i = READ_ONCE(krcp->nr_bkv_objs); i < nr_pages; i++) {
		bnode = (struct kvfree_rcu_bulk_data *)
			__get_free_page(GFP_KERNEL | __GFP_NORETRY | __GFP_NOMEMALLOC | __GFP_NOWARN);

		if (!bnode)
			break;

		raw_spin_lock_irqsave(&krcp->lock, flags);
		pushed = put_cached_bnode(krcp, bnode);
		raw_spin_unlock_irqrestore(&krcp->lock, flags);

		if (!pushed) {
			free_page((unsigned long) bnode);
			break;
		}
	}

	atomic_set(&krcp->work_in_progress, 0);
	atomic_set(&krcp->backoff_page_cache_fill, 0);
}

static void
run_page_cache_worker(struct kfree_rcu_cpu *krcp)
{
	
	if (!rcu_min_cached_objs)
		return;

	if (rcu_scheduler_active == RCU_SCHEDULER_RUNNING &&
			!atomic_xchg(&krcp->work_in_progress, 1)) {
		if (atomic_read(&krcp->backoff_page_cache_fill)) {
			queue_delayed_work(system_wq,
				&krcp->page_cache_work,
					msecs_to_jiffies(rcu_delay_page_cache_fill_msec));
		} else {
			hrtimer_init(&krcp->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
			krcp->hrtimer.function = schedule_page_work_fn;
			hrtimer_start(&krcp->hrtimer, 0, HRTIMER_MODE_REL);
		}
	}
}







static inline bool
add_ptr_to_bulk_krc_lock(struct kfree_rcu_cpu **krcp,
	unsigned long *flags, void *ptr, bool can_alloc)
{
	struct kvfree_rcu_bulk_data *bnode;
	int idx;

	*krcp = krc_this_cpu_lock(flags);
	if (unlikely(!(*krcp)->initialized))
		return false;

	idx = !!is_vmalloc_addr(ptr);
	bnode = list_first_entry_or_null(&(*krcp)->bulk_head[idx],
		struct kvfree_rcu_bulk_data, list);

	 
	if (!bnode || bnode->nr_records == KVFREE_BULK_MAX_ENTR) {
		bnode = get_cached_bnode(*krcp);
		if (!bnode && can_alloc) {
			krc_this_cpu_unlock(*krcp, *flags);

			
			
			
			
			
			
			
			
			
			
			
			bnode = (struct kvfree_rcu_bulk_data *)
				__get_free_page(GFP_KERNEL | __GFP_NORETRY | __GFP_NOMEMALLOC | __GFP_NOWARN);
			raw_spin_lock_irqsave(&(*krcp)->lock, *flags);
		}

		if (!bnode)
			return false;

		
		bnode->nr_records = 0;
		list_add(&bnode->list, &(*krcp)->bulk_head[idx]);
	}

	
	bnode->records[bnode->nr_records++] = ptr;
	get_state_synchronize_rcu_full(&bnode->gp_snap);
	atomic_inc(&(*krcp)->bulk_count[idx]);

	return true;
}

 
void kvfree_call_rcu(struct rcu_head *head, void *ptr)
{
	unsigned long flags;
	struct kfree_rcu_cpu *krcp;
	bool success;

	 
	if (!head)
		might_sleep();

	 
	if (debug_rcu_head_queue(ptr)) {
		
		WARN_ONCE(1, "%s(): Double-freed call. rcu_head %p\n",
			  __func__, head);

		
		return;
	}

	kasan_record_aux_stack_noalloc(ptr);
	success = add_ptr_to_bulk_krc_lock(&krcp, &flags, ptr, !head);
	if (!success) {
		run_page_cache_worker(krcp);

		if (head == NULL)
			
			goto unlock_return;

		head->func = ptr;
		head->next = krcp->head;
		WRITE_ONCE(krcp->head, head);
		atomic_inc(&krcp->head_count);

		
		krcp->head_gp_snap = get_state_synchronize_rcu();
		success = true;
	}

	 
	kmemleak_ignore(ptr);

	
	if (rcu_scheduler_active == RCU_SCHEDULER_RUNNING)
		schedule_delayed_monitor_work(krcp);

unlock_return:
	krc_this_cpu_unlock(krcp, flags);

	 
	if (!success) {
		debug_rcu_head_unqueue((struct rcu_head *) ptr);
		synchronize_rcu();
		kvfree(ptr);
	}
}
EXPORT_SYMBOL_GPL(kvfree_call_rcu);

static unsigned long
kfree_rcu_shrink_count(struct shrinker *shrink, struct shrink_control *sc)
{
	int cpu;
	unsigned long count = 0;

	 
	for_each_possible_cpu(cpu) {
		struct kfree_rcu_cpu *krcp = per_cpu_ptr(&krc, cpu);

		count += krc_count(krcp);
		count += READ_ONCE(krcp->nr_bkv_objs);
		atomic_set(&krcp->backoff_page_cache_fill, 1);
	}

	return count == 0 ? SHRINK_EMPTY : count;
}

static unsigned long
kfree_rcu_shrink_scan(struct shrinker *shrink, struct shrink_control *sc)
{
	int cpu, freed = 0;

	for_each_possible_cpu(cpu) {
		int count;
		struct kfree_rcu_cpu *krcp = per_cpu_ptr(&krc, cpu);

		count = krc_count(krcp);
		count += drain_page_cache(krcp);
		kfree_rcu_monitor(&krcp->monitor_work.work);

		sc->nr_to_scan -= count;
		freed += count;

		if (sc->nr_to_scan <= 0)
			break;
	}

	return freed == 0 ? SHRINK_STOP : freed;
}

static struct shrinker kfree_rcu_shrinker = {
	.count_objects = kfree_rcu_shrink_count,
	.scan_objects = kfree_rcu_shrink_scan,
	.batch = 0,
	.seeks = DEFAULT_SEEKS,
};

void __init kfree_rcu_scheduler_running(void)
{
	int cpu;

	for_each_possible_cpu(cpu) {
		struct kfree_rcu_cpu *krcp = per_cpu_ptr(&krc, cpu);

		if (need_offload_krc(krcp))
			schedule_delayed_monitor_work(krcp);
	}
}

 
static int rcu_blocking_is_gp(void)
{
	if (rcu_scheduler_active != RCU_SCHEDULER_INACTIVE) {
		might_sleep();
		return false;
	}
	return true;
}

 
void synchronize_rcu(void)
{
	unsigned long flags;
	struct rcu_node *rnp;

	RCU_LOCKDEP_WARN(lock_is_held(&rcu_bh_lock_map) ||
			 lock_is_held(&rcu_lock_map) ||
			 lock_is_held(&rcu_sched_lock_map),
			 "Illegal synchronize_rcu() in RCU read-side critical section");
	if (!rcu_blocking_is_gp()) {
		if (rcu_gp_is_expedited())
			synchronize_rcu_expedited();
		else
			wait_rcu_gp(call_rcu_hurry);
		return;
	}

	
	
	
	
	
	
	rcu_poll_gp_seq_start_unlocked(&rcu_state.gp_seq_polled_snap);
	rcu_poll_gp_seq_end_unlocked(&rcu_state.gp_seq_polled_snap);

	
	
	
	
	local_irq_save(flags);
	WARN_ON_ONCE(num_online_cpus() > 1);
	rcu_state.gp_seq += (1 << RCU_SEQ_CTR_SHIFT);
	for (rnp = this_cpu_ptr(&rcu_data)->mynode; rnp; rnp = rnp->parent)
		rnp->gp_seq_needed = rnp->gp_seq = rcu_state.gp_seq;
	local_irq_restore(flags);
}
EXPORT_SYMBOL_GPL(synchronize_rcu);

 
void get_completed_synchronize_rcu_full(struct rcu_gp_oldstate *rgosp)
{
	rgosp->rgos_norm = RCU_GET_STATE_COMPLETED;
	rgosp->rgos_exp = RCU_GET_STATE_COMPLETED;
}
EXPORT_SYMBOL_GPL(get_completed_synchronize_rcu_full);

 
unsigned long get_state_synchronize_rcu(void)
{
	 
	smp_mb();   
	return rcu_seq_snap(&rcu_state.gp_seq_polled);
}
EXPORT_SYMBOL_GPL(get_state_synchronize_rcu);

 
void get_state_synchronize_rcu_full(struct rcu_gp_oldstate *rgosp)
{
	struct rcu_node *rnp = rcu_get_root();

	 
	smp_mb();   
	rgosp->rgos_norm = rcu_seq_snap(&rnp->gp_seq);
	rgosp->rgos_exp = rcu_seq_snap(&rcu_state.expedited_sequence);
}
EXPORT_SYMBOL_GPL(get_state_synchronize_rcu_full);

 
static void start_poll_synchronize_rcu_common(void)
{
	unsigned long flags;
	bool needwake;
	struct rcu_data *rdp;
	struct rcu_node *rnp;

	lockdep_assert_irqs_enabled();
	local_irq_save(flags);
	rdp = this_cpu_ptr(&rcu_data);
	rnp = rdp->mynode;
	raw_spin_lock_rcu_node(rnp); 
	
	
	
	
	
	
	needwake = rcu_start_this_gp(rnp, rdp, rcu_seq_snap(&rcu_state.gp_seq));
	raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
	if (needwake)
		rcu_gp_kthread_wake();
}

 
unsigned long start_poll_synchronize_rcu(void)
{
	unsigned long gp_seq = get_state_synchronize_rcu();

	start_poll_synchronize_rcu_common();
	return gp_seq;
}
EXPORT_SYMBOL_GPL(start_poll_synchronize_rcu);

 
void start_poll_synchronize_rcu_full(struct rcu_gp_oldstate *rgosp)
{
	get_state_synchronize_rcu_full(rgosp);

	start_poll_synchronize_rcu_common();
}
EXPORT_SYMBOL_GPL(start_poll_synchronize_rcu_full);

 
bool poll_state_synchronize_rcu(unsigned long oldstate)
{
	if (oldstate == RCU_GET_STATE_COMPLETED ||
	    rcu_seq_done_exact(&rcu_state.gp_seq_polled, oldstate)) {
		smp_mb();  
		return true;
	}
	return false;
}
EXPORT_SYMBOL_GPL(poll_state_synchronize_rcu);

 
bool poll_state_synchronize_rcu_full(struct rcu_gp_oldstate *rgosp)
{
	struct rcu_node *rnp = rcu_get_root();

	smp_mb(); 
	if (rgosp->rgos_norm == RCU_GET_STATE_COMPLETED ||
	    rcu_seq_done_exact(&rnp->gp_seq, rgosp->rgos_norm) ||
	    rgosp->rgos_exp == RCU_GET_STATE_COMPLETED ||
	    rcu_seq_done_exact(&rcu_state.expedited_sequence, rgosp->rgos_exp)) {
		smp_mb();  
		return true;
	}
	return false;
}
EXPORT_SYMBOL_GPL(poll_state_synchronize_rcu_full);

 
void cond_synchronize_rcu(unsigned long oldstate)
{
	if (!poll_state_synchronize_rcu(oldstate))
		synchronize_rcu();
}
EXPORT_SYMBOL_GPL(cond_synchronize_rcu);

 
void cond_synchronize_rcu_full(struct rcu_gp_oldstate *rgosp)
{
	if (!poll_state_synchronize_rcu_full(rgosp))
		synchronize_rcu();
}
EXPORT_SYMBOL_GPL(cond_synchronize_rcu_full);

 
static int rcu_pending(int user)
{
	bool gp_in_progress;
	struct rcu_data *rdp = this_cpu_ptr(&rcu_data);
	struct rcu_node *rnp = rdp->mynode;

	lockdep_assert_irqs_disabled();

	 
	check_cpu_stall(rdp);

	 
	if (rcu_nocb_need_deferred_wakeup(rdp, RCU_NOCB_WAKE))
		return 1;

	 
	if ((user || rcu_is_cpu_rrupt_from_idle()) && rcu_nohz_full_cpu())
		return 0;

	 
	gp_in_progress = rcu_gp_in_progress();
	if (rdp->core_needs_qs && !rdp->cpu_no_qs.b.norm && gp_in_progress)
		return 1;

	 
	if (!rcu_rdp_is_offloaded(rdp) &&
	    rcu_segcblist_ready_cbs(&rdp->cblist))
		return 1;

	 
	if (!gp_in_progress && rcu_segcblist_is_enabled(&rdp->cblist) &&
	    !rcu_rdp_is_offloaded(rdp) &&
	    !rcu_segcblist_restempty(&rdp->cblist, RCU_NEXT_READY_TAIL))
		return 1;

	 
	if (rcu_seq_current(&rnp->gp_seq) != rdp->gp_seq ||
	    unlikely(READ_ONCE(rdp->gpwrap)))  
		return 1;

	 
	return 0;
}

 
static void rcu_barrier_trace(const char *s, int cpu, unsigned long done)
{
	trace_rcu_barrier(rcu_state.name, s, cpu,
			  atomic_read(&rcu_state.barrier_cpu_count), done);
}

 
static void rcu_barrier_callback(struct rcu_head *rhp)
{
	unsigned long __maybe_unused s = rcu_state.barrier_sequence;

	if (atomic_dec_and_test(&rcu_state.barrier_cpu_count)) {
		rcu_barrier_trace(TPS("LastCB"), -1, s);
		complete(&rcu_state.barrier_completion);
	} else {
		rcu_barrier_trace(TPS("CB"), -1, s);
	}
}

 
static void rcu_barrier_entrain(struct rcu_data *rdp)
{
	unsigned long gseq = READ_ONCE(rcu_state.barrier_sequence);
	unsigned long lseq = READ_ONCE(rdp->barrier_seq_snap);
	bool wake_nocb = false;
	bool was_alldone = false;

	lockdep_assert_held(&rcu_state.barrier_lock);
	if (rcu_seq_state(lseq) || !rcu_seq_state(gseq) || rcu_seq_ctr(lseq) != rcu_seq_ctr(gseq))
		return;
	rcu_barrier_trace(TPS("IRQ"), -1, rcu_state.barrier_sequence);
	rdp->barrier_head.func = rcu_barrier_callback;
	debug_rcu_head_queue(&rdp->barrier_head);
	rcu_nocb_lock(rdp);
	 
	was_alldone = rcu_rdp_is_offloaded(rdp) && !rcu_segcblist_pend_cbs(&rdp->cblist);
	WARN_ON_ONCE(!rcu_nocb_flush_bypass(rdp, NULL, jiffies, false));
	wake_nocb = was_alldone && rcu_segcblist_pend_cbs(&rdp->cblist);
	if (rcu_segcblist_entrain(&rdp->cblist, &rdp->barrier_head)) {
		atomic_inc(&rcu_state.barrier_cpu_count);
	} else {
		debug_rcu_head_unqueue(&rdp->barrier_head);
		rcu_barrier_trace(TPS("IRQNQ"), -1, rcu_state.barrier_sequence);
	}
	rcu_nocb_unlock(rdp);
	if (wake_nocb)
		wake_nocb_gp(rdp, false);
	smp_store_release(&rdp->barrier_seq_snap, gseq);
}

 
static void rcu_barrier_handler(void *cpu_in)
{
	uintptr_t cpu = (uintptr_t)cpu_in;
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);

	lockdep_assert_irqs_disabled();
	WARN_ON_ONCE(cpu != rdp->cpu);
	WARN_ON_ONCE(cpu != smp_processor_id());
	raw_spin_lock(&rcu_state.barrier_lock);
	rcu_barrier_entrain(rdp);
	raw_spin_unlock(&rcu_state.barrier_lock);
}

 
void rcu_barrier(void)
{
	uintptr_t cpu;
	unsigned long flags;
	unsigned long gseq;
	struct rcu_data *rdp;
	unsigned long s = rcu_seq_snap(&rcu_state.barrier_sequence);

	rcu_barrier_trace(TPS("Begin"), -1, s);

	 
	mutex_lock(&rcu_state.barrier_mutex);

	 
	if (rcu_seq_done(&rcu_state.barrier_sequence, s)) {
		rcu_barrier_trace(TPS("EarlyExit"), -1, rcu_state.barrier_sequence);
		smp_mb();  
		mutex_unlock(&rcu_state.barrier_mutex);
		return;
	}

	 
	raw_spin_lock_irqsave(&rcu_state.barrier_lock, flags);
	rcu_seq_start(&rcu_state.barrier_sequence);
	gseq = rcu_state.barrier_sequence;
	rcu_barrier_trace(TPS("Inc1"), -1, rcu_state.barrier_sequence);

	 
	init_completion(&rcu_state.barrier_completion);
	atomic_set(&rcu_state.barrier_cpu_count, 2);
	raw_spin_unlock_irqrestore(&rcu_state.barrier_lock, flags);

	 
	for_each_possible_cpu(cpu) {
		rdp = per_cpu_ptr(&rcu_data, cpu);
retry:
		if (smp_load_acquire(&rdp->barrier_seq_snap) == gseq)
			continue;
		raw_spin_lock_irqsave(&rcu_state.barrier_lock, flags);
		if (!rcu_segcblist_n_cbs(&rdp->cblist)) {
			WRITE_ONCE(rdp->barrier_seq_snap, gseq);
			raw_spin_unlock_irqrestore(&rcu_state.barrier_lock, flags);
			rcu_barrier_trace(TPS("NQ"), cpu, rcu_state.barrier_sequence);
			continue;
		}
		if (!rcu_rdp_cpu_online(rdp)) {
			rcu_barrier_entrain(rdp);
			WARN_ON_ONCE(READ_ONCE(rdp->barrier_seq_snap) != gseq);
			raw_spin_unlock_irqrestore(&rcu_state.barrier_lock, flags);
			rcu_barrier_trace(TPS("OfflineNoCBQ"), cpu, rcu_state.barrier_sequence);
			continue;
		}
		raw_spin_unlock_irqrestore(&rcu_state.barrier_lock, flags);
		if (smp_call_function_single(cpu, rcu_barrier_handler, (void *)cpu, 1)) {
			schedule_timeout_uninterruptible(1);
			goto retry;
		}
		WARN_ON_ONCE(READ_ONCE(rdp->barrier_seq_snap) != gseq);
		rcu_barrier_trace(TPS("OnlineQ"), cpu, rcu_state.barrier_sequence);
	}

	 
	if (atomic_sub_and_test(2, &rcu_state.barrier_cpu_count))
		complete(&rcu_state.barrier_completion);

	 
	wait_for_completion(&rcu_state.barrier_completion);

	 
	rcu_barrier_trace(TPS("Inc2"), -1, rcu_state.barrier_sequence);
	rcu_seq_end(&rcu_state.barrier_sequence);
	gseq = rcu_state.barrier_sequence;
	for_each_possible_cpu(cpu) {
		rdp = per_cpu_ptr(&rcu_data, cpu);

		WRITE_ONCE(rdp->barrier_seq_snap, gseq);
	}

	 
	mutex_unlock(&rcu_state.barrier_mutex);
}
EXPORT_SYMBOL_GPL(rcu_barrier);

 
static unsigned long rcu_rnp_online_cpus(struct rcu_node *rnp)
{
	return READ_ONCE(rnp->qsmaskinitnext);
}

 
static bool rcu_rdp_cpu_online(struct rcu_data *rdp)
{
	return !!(rdp->grpmask & rcu_rnp_online_cpus(rdp->mynode));
}

bool rcu_cpu_online(int cpu)
{
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);

	return rcu_rdp_cpu_online(rdp);
}

#if defined(CONFIG_PROVE_RCU) && defined(CONFIG_HOTPLUG_CPU)

 
bool rcu_lockdep_current_cpu_online(void)
{
	struct rcu_data *rdp;
	bool ret = false;

	if (in_nmi() || !rcu_scheduler_fully_active)
		return true;
	preempt_disable_notrace();
	rdp = this_cpu_ptr(&rcu_data);
	 
	if (rcu_rdp_cpu_online(rdp) || arch_spin_is_locked(&rcu_state.ofl_lock))
		ret = true;
	preempt_enable_notrace();
	return ret;
}
EXPORT_SYMBOL_GPL(rcu_lockdep_current_cpu_online);

#endif  

 
 
static bool rcu_init_invoked(void)
{
	return !!rcu_state.n_online_cpus;
}

 
int rcutree_dying_cpu(unsigned int cpu)
{
	bool blkd;
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);
	struct rcu_node *rnp = rdp->mynode;

	if (!IS_ENABLED(CONFIG_HOTPLUG_CPU))
		return 0;

	blkd = !!(READ_ONCE(rnp->qsmask) & rdp->grpmask);
	trace_rcu_grace_period(rcu_state.name, READ_ONCE(rnp->gp_seq),
			       blkd ? TPS("cpuofl-bgp") : TPS("cpuofl"));
	return 0;
}

 
static void rcu_cleanup_dead_rnp(struct rcu_node *rnp_leaf)
{
	long mask;
	struct rcu_node *rnp = rnp_leaf;

	raw_lockdep_assert_held_rcu_node(rnp_leaf);
	if (!IS_ENABLED(CONFIG_HOTPLUG_CPU) ||
	    WARN_ON_ONCE(rnp_leaf->qsmaskinit) ||
	    WARN_ON_ONCE(rcu_preempt_has_tasks(rnp_leaf)))
		return;
	for (;;) {
		mask = rnp->grpmask;
		rnp = rnp->parent;
		if (!rnp)
			break;
		raw_spin_lock_rcu_node(rnp);  
		rnp->qsmaskinit &= ~mask;
		 
		WARN_ON_ONCE(rnp->qsmask);
		if (rnp->qsmaskinit) {
			raw_spin_unlock_rcu_node(rnp);
			 
			return;
		}
		raw_spin_unlock_rcu_node(rnp);  
	}
}

 
int rcutree_dead_cpu(unsigned int cpu)
{
	if (!IS_ENABLED(CONFIG_HOTPLUG_CPU))
		return 0;

	WRITE_ONCE(rcu_state.n_online_cpus, rcu_state.n_online_cpus - 1);
	
	tick_dep_clear(TICK_DEP_BIT_RCU);
	return 0;
}

 
static void rcu_init_new_rnp(struct rcu_node *rnp_leaf)
{
	long mask;
	long oldmask;
	struct rcu_node *rnp = rnp_leaf;

	raw_lockdep_assert_held_rcu_node(rnp_leaf);
	WARN_ON_ONCE(rnp->wait_blkd_tasks);
	for (;;) {
		mask = rnp->grpmask;
		rnp = rnp->parent;
		if (rnp == NULL)
			return;
		raw_spin_lock_rcu_node(rnp);  
		oldmask = rnp->qsmaskinit;
		rnp->qsmaskinit |= mask;
		raw_spin_unlock_rcu_node(rnp);  
		if (oldmask)
			return;
	}
}

 
static void __init
rcu_boot_init_percpu_data(int cpu)
{
	struct context_tracking *ct = this_cpu_ptr(&context_tracking);
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);

	 
	rdp->grpmask = leaf_node_cpu_bit(rdp->mynode, cpu);
	INIT_WORK(&rdp->strict_work, strict_work_handler);
	WARN_ON_ONCE(ct->dynticks_nesting != 1);
	WARN_ON_ONCE(rcu_dynticks_in_eqs(rcu_dynticks_snap(cpu)));
	rdp->barrier_seq_snap = rcu_state.barrier_sequence;
	rdp->rcu_ofl_gp_seq = rcu_state.gp_seq;
	rdp->rcu_ofl_gp_flags = RCU_GP_CLEANED;
	rdp->rcu_onl_gp_seq = rcu_state.gp_seq;
	rdp->rcu_onl_gp_flags = RCU_GP_CLEANED;
	rdp->last_sched_clock = jiffies;
	rdp->cpu = cpu;
	rcu_boot_init_nocb_percpu_data(rdp);
}

 
int rcutree_prepare_cpu(unsigned int cpu)
{
	unsigned long flags;
	struct context_tracking *ct = per_cpu_ptr(&context_tracking, cpu);
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);
	struct rcu_node *rnp = rcu_get_root();

	 
	raw_spin_lock_irqsave_rcu_node(rnp, flags);
	rdp->qlen_last_fqs_check = 0;
	rdp->n_force_qs_snap = READ_ONCE(rcu_state.n_force_qs);
	rdp->blimit = blimit;
	ct->dynticks_nesting = 1;	 
	raw_spin_unlock_rcu_node(rnp);		 

	 
	if (!rcu_segcblist_is_enabled(&rdp->cblist))
		rcu_segcblist_init(&rdp->cblist);   

	 
	rnp = rdp->mynode;
	raw_spin_lock_rcu_node(rnp);		 
	rdp->gp_seq = READ_ONCE(rnp->gp_seq);
	rdp->gp_seq_needed = rdp->gp_seq;
	rdp->cpu_no_qs.b.norm = true;
	rdp->core_needs_qs = false;
	rdp->rcu_iw_pending = false;
	rdp->rcu_iw = IRQ_WORK_INIT_HARD(rcu_iw_handler);
	rdp->rcu_iw_gp_seq = rdp->gp_seq - 1;
	trace_rcu_grace_period(rcu_state.name, rdp->gp_seq, TPS("cpuonl"));
	raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
	rcu_spawn_one_boost_kthread(rnp);
	rcu_spawn_cpu_nocb_kthread(cpu);
	WRITE_ONCE(rcu_state.n_online_cpus, rcu_state.n_online_cpus + 1);

	return 0;
}

 
static void rcutree_affinity_setting(unsigned int cpu, int outgoing)
{
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);

	rcu_boost_kthread_setaffinity(rdp->mynode, outgoing);
}

 
bool rcu_cpu_beenfullyonline(int cpu)
{
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);

	return smp_load_acquire(&rdp->beenonline);
}

 
int rcutree_online_cpu(unsigned int cpu)
{
	unsigned long flags;
	struct rcu_data *rdp;
	struct rcu_node *rnp;

	rdp = per_cpu_ptr(&rcu_data, cpu);
	rnp = rdp->mynode;
	raw_spin_lock_irqsave_rcu_node(rnp, flags);
	rnp->ffmask |= rdp->grpmask;
	raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
	if (rcu_scheduler_active == RCU_SCHEDULER_INACTIVE)
		return 0;  
	sync_sched_exp_online_cleanup(cpu);
	rcutree_affinity_setting(cpu, -1);

	 
	tick_dep_clear(TICK_DEP_BIT_RCU);
	return 0;
}

 
int rcutree_offline_cpu(unsigned int cpu)
{
	unsigned long flags;
	struct rcu_data *rdp;
	struct rcu_node *rnp;

	rdp = per_cpu_ptr(&rcu_data, cpu);
	rnp = rdp->mynode;
	raw_spin_lock_irqsave_rcu_node(rnp, flags);
	rnp->ffmask &= ~rdp->grpmask;
	raw_spin_unlock_irqrestore_rcu_node(rnp, flags);

	rcutree_affinity_setting(cpu, cpu);

	 
	tick_dep_set(TICK_DEP_BIT_RCU);
	return 0;
}

 
void rcu_cpu_starting(unsigned int cpu)
{
	unsigned long mask;
	struct rcu_data *rdp;
	struct rcu_node *rnp;
	bool newcpu;

	lockdep_assert_irqs_disabled();
	rdp = per_cpu_ptr(&rcu_data, cpu);
	if (rdp->cpu_started)
		return;
	rdp->cpu_started = true;

	rnp = rdp->mynode;
	mask = rdp->grpmask;
	arch_spin_lock(&rcu_state.ofl_lock);
	rcu_dynticks_eqs_online();
	raw_spin_lock(&rcu_state.barrier_lock);
	raw_spin_lock_rcu_node(rnp);
	WRITE_ONCE(rnp->qsmaskinitnext, rnp->qsmaskinitnext | mask);
	raw_spin_unlock(&rcu_state.barrier_lock);
	newcpu = !(rnp->expmaskinitnext & mask);
	rnp->expmaskinitnext |= mask;
	 
	smp_store_release(&rcu_state.ncpus, rcu_state.ncpus + newcpu);  
	ASSERT_EXCLUSIVE_WRITER(rcu_state.ncpus);
	rcu_gpnum_ovf(rnp, rdp);  
	rdp->rcu_onl_gp_seq = READ_ONCE(rcu_state.gp_seq);
	rdp->rcu_onl_gp_flags = READ_ONCE(rcu_state.gp_flags);

	 
	if (WARN_ON_ONCE(rnp->qsmask & mask)) {  
		 
		unsigned long flags;

		local_irq_save(flags);
		rcu_disable_urgency_upon_qs(rdp);
		 
		rcu_report_qs_rnp(mask, rnp, rnp->gp_seq, flags);
	} else {
		raw_spin_unlock_rcu_node(rnp);
	}
	arch_spin_unlock(&rcu_state.ofl_lock);
	smp_store_release(&rdp->beenonline, true);
	smp_mb();  
}

 
void rcu_report_dead(unsigned int cpu)
{
	unsigned long flags, seq_flags;
	unsigned long mask;
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);
	struct rcu_node *rnp = rdp->mynode;   

	 
	do_nocb_deferred_wakeup(rdp);

	rcu_preempt_deferred_qs(current);

	 
	mask = rdp->grpmask;
	local_irq_save(seq_flags);
	arch_spin_lock(&rcu_state.ofl_lock);
	raw_spin_lock_irqsave_rcu_node(rnp, flags);  
	rdp->rcu_ofl_gp_seq = READ_ONCE(rcu_state.gp_seq);
	rdp->rcu_ofl_gp_flags = READ_ONCE(rcu_state.gp_flags);
	if (rnp->qsmask & mask) {  
		 
		rcu_disable_urgency_upon_qs(rdp);
		rcu_report_qs_rnp(mask, rnp, rnp->gp_seq, flags);
		raw_spin_lock_irqsave_rcu_node(rnp, flags);
	}
	WRITE_ONCE(rnp->qsmaskinitnext, rnp->qsmaskinitnext & ~mask);
	raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
	arch_spin_unlock(&rcu_state.ofl_lock);
	local_irq_restore(seq_flags);

	rdp->cpu_started = false;
}

#ifdef CONFIG_HOTPLUG_CPU
 
void rcutree_migrate_callbacks(int cpu)
{
	unsigned long flags;
	struct rcu_data *my_rdp;
	struct rcu_node *my_rnp;
	struct rcu_data *rdp = per_cpu_ptr(&rcu_data, cpu);
	bool needwake;

	if (rcu_rdp_is_offloaded(rdp) ||
	    rcu_segcblist_empty(&rdp->cblist))
		return;   

	raw_spin_lock_irqsave(&rcu_state.barrier_lock, flags);
	WARN_ON_ONCE(rcu_rdp_cpu_online(rdp));
	rcu_barrier_entrain(rdp);
	my_rdp = this_cpu_ptr(&rcu_data);
	my_rnp = my_rdp->mynode;
	rcu_nocb_lock(my_rdp);  
	WARN_ON_ONCE(!rcu_nocb_flush_bypass(my_rdp, NULL, jiffies, false));
	raw_spin_lock_rcu_node(my_rnp);  
	 
	needwake = rcu_advance_cbs(my_rnp, rdp) ||
		   rcu_advance_cbs(my_rnp, my_rdp);
	rcu_segcblist_merge(&my_rdp->cblist, &rdp->cblist);
	raw_spin_unlock(&rcu_state.barrier_lock);  
	needwake = needwake || rcu_advance_cbs(my_rnp, my_rdp);
	rcu_segcblist_disable(&rdp->cblist);
	WARN_ON_ONCE(rcu_segcblist_empty(&my_rdp->cblist) != !rcu_segcblist_n_cbs(&my_rdp->cblist));
	check_cb_ovld_locked(my_rdp, my_rnp);
	if (rcu_rdp_is_offloaded(my_rdp)) {
		raw_spin_unlock_rcu_node(my_rnp);  
		__call_rcu_nocb_wake(my_rdp, true, flags);
	} else {
		rcu_nocb_unlock(my_rdp);  
		raw_spin_unlock_irqrestore_rcu_node(my_rnp, flags);
	}
	if (needwake)
		rcu_gp_kthread_wake();
	lockdep_assert_irqs_enabled();
	WARN_ONCE(rcu_segcblist_n_cbs(&rdp->cblist) != 0 ||
		  !rcu_segcblist_empty(&rdp->cblist),
		  "rcu_cleanup_dead_cpu: Callbacks on offline CPU %d: qlen=%lu, 1stCB=%p\n",
		  cpu, rcu_segcblist_n_cbs(&rdp->cblist),
		  rcu_segcblist_first_cb(&rdp->cblist));
}
#endif

 
static int rcu_pm_notify(struct notifier_block *self,
			 unsigned long action, void *hcpu)
{
	switch (action) {
	case PM_HIBERNATION_PREPARE:
	case PM_SUSPEND_PREPARE:
		rcu_async_hurry();
		rcu_expedite_gp();
		break;
	case PM_POST_HIBERNATION:
	case PM_POST_SUSPEND:
		rcu_unexpedite_gp();
		rcu_async_relax();
		break;
	default:
		break;
	}
	return NOTIFY_OK;
}

#ifdef CONFIG_RCU_EXP_KTHREAD
struct kthread_worker *rcu_exp_gp_kworker;
struct kthread_worker *rcu_exp_par_gp_kworker;

static void __init rcu_start_exp_gp_kworkers(void)
{
	const char *par_gp_kworker_name = "rcu_exp_par_gp_kthread_worker";
	const char *gp_kworker_name = "rcu_exp_gp_kthread_worker";
	struct sched_param param = { .sched_priority = kthread_prio };

	rcu_exp_gp_kworker = kthread_create_worker(0, gp_kworker_name);
	if (IS_ERR_OR_NULL(rcu_exp_gp_kworker)) {
		pr_err("Failed to create %s!\n", gp_kworker_name);
		return;
	}

	rcu_exp_par_gp_kworker = kthread_create_worker(0, par_gp_kworker_name);
	if (IS_ERR_OR_NULL(rcu_exp_par_gp_kworker)) {
		pr_err("Failed to create %s!\n", par_gp_kworker_name);
		kthread_destroy_worker(rcu_exp_gp_kworker);
		return;
	}

	sched_setscheduler_nocheck(rcu_exp_gp_kworker->task, SCHED_FIFO, &param);
	sched_setscheduler_nocheck(rcu_exp_par_gp_kworker->task, SCHED_FIFO,
				   &param);
}

static inline void rcu_alloc_par_gp_wq(void)
{
}
#else  
struct workqueue_struct *rcu_par_gp_wq;

static void __init rcu_start_exp_gp_kworkers(void)
{
}

static inline void rcu_alloc_par_gp_wq(void)
{
	rcu_par_gp_wq = alloc_workqueue("rcu_par_gp", WQ_MEM_RECLAIM, 0);
	WARN_ON(!rcu_par_gp_wq);
}
#endif  

 
static int __init rcu_spawn_gp_kthread(void)
{
	unsigned long flags;
	struct rcu_node *rnp;
	struct sched_param sp;
	struct task_struct *t;
	struct rcu_data *rdp = this_cpu_ptr(&rcu_data);

	rcu_scheduler_fully_active = 1;
	t = kthread_create(rcu_gp_kthread, NULL, "%s", rcu_state.name);
	if (WARN_ONCE(IS_ERR(t), "%s: Could not start grace-period kthread, OOM is now expected behavior\n", __func__))
		return 0;
	if (kthread_prio) {
		sp.sched_priority = kthread_prio;
		sched_setscheduler_nocheck(t, SCHED_FIFO, &sp);
	}
	rnp = rcu_get_root();
	raw_spin_lock_irqsave_rcu_node(rnp, flags);
	WRITE_ONCE(rcu_state.gp_activity, jiffies);
	WRITE_ONCE(rcu_state.gp_req_activity, jiffies);
	 
	smp_store_release(&rcu_state.gp_kthread, t);   
	raw_spin_unlock_irqrestore_rcu_node(rnp, flags);
	wake_up_process(t);
	 
	WARN_ON(num_online_cpus() > 1);
	 
	rcu_spawn_cpu_nocb_kthread(smp_processor_id());
	rcu_spawn_one_boost_kthread(rdp->mynode);
	rcu_spawn_core_kthreads();
	 
	rcu_start_exp_gp_kworkers();
	return 0;
}
early_initcall(rcu_spawn_gp_kthread);

 
void rcu_scheduler_starting(void)
{
	unsigned long flags;
	struct rcu_node *rnp;

	WARN_ON(num_online_cpus() != 1);
	WARN_ON(nr_context_switches() > 0);
	rcu_test_sync_prims();

	 
	local_irq_save(flags);
	rcu_for_each_node_breadth_first(rnp)
		rnp->gp_seq_needed = rnp->gp_seq = rcu_state.gp_seq;
	local_irq_restore(flags);

	 
	rcu_scheduler_active = RCU_SCHEDULER_INIT;
	rcu_test_sync_prims();
}

 
static void __init rcu_init_one(void)
{
	static const char * const buf[] = RCU_NODE_NAME_INIT;
	static const char * const fqs[] = RCU_FQS_NAME_INIT;
	static struct lock_class_key rcu_node_class[RCU_NUM_LVLS];
	static struct lock_class_key rcu_fqs_class[RCU_NUM_LVLS];

	int levelspread[RCU_NUM_LVLS];		 
	int cpustride = 1;
	int i;
	int j;
	struct rcu_node *rnp;

	BUILD_BUG_ON(RCU_NUM_LVLS > ARRAY_SIZE(buf));   

	 
	if (rcu_num_lvls <= 0 || rcu_num_lvls > RCU_NUM_LVLS)
		panic("rcu_init_one: rcu_num_lvls out of range");

	 

	for (i = 1; i < rcu_num_lvls; i++)
		rcu_state.level[i] =
			rcu_state.level[i - 1] + num_rcu_lvl[i - 1];
	rcu_init_levelspread(levelspread, num_rcu_lvl);

	 

	for (i = rcu_num_lvls - 1; i >= 0; i--) {
		cpustride *= levelspread[i];
		rnp = rcu_state.level[i];
		for (j = 0; j < num_rcu_lvl[i]; j++, rnp++) {
			raw_spin_lock_init(&ACCESS_PRIVATE(rnp, lock));
			lockdep_set_class_and_name(&ACCESS_PRIVATE(rnp, lock),
						   &rcu_node_class[i], buf[i]);
			raw_spin_lock_init(&rnp->fqslock);
			lockdep_set_class_and_name(&rnp->fqslock,
						   &rcu_fqs_class[i], fqs[i]);
			rnp->gp_seq = rcu_state.gp_seq;
			rnp->gp_seq_needed = rcu_state.gp_seq;
			rnp->completedqs = rcu_state.gp_seq;
			rnp->qsmask = 0;
			rnp->qsmaskinit = 0;
			rnp->grplo = j * cpustride;
			rnp->grphi = (j + 1) * cpustride - 1;
			if (rnp->grphi >= nr_cpu_ids)
				rnp->grphi = nr_cpu_ids - 1;
			if (i == 0) {
				rnp->grpnum = 0;
				rnp->grpmask = 0;
				rnp->parent = NULL;
			} else {
				rnp->grpnum = j % levelspread[i - 1];
				rnp->grpmask = BIT(rnp->grpnum);
				rnp->parent = rcu_state.level[i - 1] +
					      j / levelspread[i - 1];
			}
			rnp->level = i;
			INIT_LIST_HEAD(&rnp->blkd_tasks);
			rcu_init_one_nocb(rnp);
			init_waitqueue_head(&rnp->exp_wq[0]);
			init_waitqueue_head(&rnp->exp_wq[1]);
			init_waitqueue_head(&rnp->exp_wq[2]);
			init_waitqueue_head(&rnp->exp_wq[3]);
			spin_lock_init(&rnp->exp_lock);
			mutex_init(&rnp->boost_kthread_mutex);
			raw_spin_lock_init(&rnp->exp_poll_lock);
			rnp->exp_seq_poll_rq = RCU_GET_STATE_COMPLETED;
			INIT_WORK(&rnp->exp_poll_wq, sync_rcu_do_polled_gp);
		}
	}

	init_swait_queue_head(&rcu_state.gp_wq);
	init_swait_queue_head(&rcu_state.expedited_wq);
	rnp = rcu_first_leaf_node();
	for_each_possible_cpu(i) {
		while (i > rnp->grphi)
			rnp++;
		per_cpu_ptr(&rcu_data, i)->mynode = rnp;
		rcu_boot_init_percpu_data(i);
	}
}

 
static void __init sanitize_kthread_prio(void)
{
	int kthread_prio_in = kthread_prio;

	if (IS_ENABLED(CONFIG_RCU_BOOST) && kthread_prio < 2
	    && IS_BUILTIN(CONFIG_RCU_TORTURE_TEST))
		kthread_prio = 2;
	else if (IS_ENABLED(CONFIG_RCU_BOOST) && kthread_prio < 1)
		kthread_prio = 1;
	else if (kthread_prio < 0)
		kthread_prio = 0;
	else if (kthread_prio > 99)
		kthread_prio = 99;

	if (kthread_prio != kthread_prio_in)
		pr_alert("%s: Limited prio to %d from %d\n",
			 __func__, kthread_prio, kthread_prio_in);
}

 
void rcu_init_geometry(void)
{
	ulong d;
	int i;
	static unsigned long old_nr_cpu_ids;
	int rcu_capacity[RCU_NUM_LVLS];
	static bool initialized;

	if (initialized) {
		 
		WARN_ON_ONCE(old_nr_cpu_ids != nr_cpu_ids);
		return;
	}

	old_nr_cpu_ids = nr_cpu_ids;
	initialized = true;

	 
	d = RCU_JIFFIES_TILL_FORCE_QS + nr_cpu_ids / RCU_JIFFIES_FQS_DIV;
	if (jiffies_till_first_fqs == ULONG_MAX)
		jiffies_till_first_fqs = d;
	if (jiffies_till_next_fqs == ULONG_MAX)
		jiffies_till_next_fqs = d;
	adjust_jiffies_till_sched_qs();

	 
	if (rcu_fanout_leaf == RCU_FANOUT_LEAF &&
	    nr_cpu_ids == NR_CPUS)
		return;
	pr_info("Adjusting geometry for rcu_fanout_leaf=%d, nr_cpu_ids=%u\n",
		rcu_fanout_leaf, nr_cpu_ids);

	 
	if (rcu_fanout_leaf < 2 ||
	    rcu_fanout_leaf > sizeof(unsigned long) * 8) {
		rcu_fanout_leaf = RCU_FANOUT_LEAF;
		WARN_ON(1);
		return;
	}

	 
	rcu_capacity[0] = rcu_fanout_leaf;
	for (i = 1; i < RCU_NUM_LVLS; i++)
		rcu_capacity[i] = rcu_capacity[i - 1] * RCU_FANOUT;

	 
	if (nr_cpu_ids > rcu_capacity[RCU_NUM_LVLS - 1]) {
		rcu_fanout_leaf = RCU_FANOUT_LEAF;
		WARN_ON(1);
		return;
	}

	 
	for (i = 0; nr_cpu_ids > rcu_capacity[i]; i++) {
	}
	rcu_num_lvls = i + 1;

	 
	for (i = 0; i < rcu_num_lvls; i++) {
		int cap = rcu_capacity[(rcu_num_lvls - 1) - i];
		num_rcu_lvl[i] = DIV_ROUND_UP(nr_cpu_ids, cap);
	}

	 
	rcu_num_nodes = 0;
	for (i = 0; i < rcu_num_lvls; i++)
		rcu_num_nodes += num_rcu_lvl[i];
}

 
static void __init rcu_dump_rcu_node_tree(void)
{
	int level = 0;
	struct rcu_node *rnp;

	pr_info("rcu_node tree layout dump\n");
	pr_info(" ");
	rcu_for_each_node_breadth_first(rnp) {
		if (rnp->level != level) {
			pr_cont("\n");
			pr_info(" ");
			level = rnp->level;
		}
		pr_cont("%d:%d ^%d  ", rnp->grplo, rnp->grphi, rnp->grpnum);
	}
	pr_cont("\n");
}

struct workqueue_struct *rcu_gp_wq;

static void __init kfree_rcu_batch_init(void)
{
	int cpu;
	int i, j;

	 
	if (rcu_delay_page_cache_fill_msec < 0 ||
		rcu_delay_page_cache_fill_msec > 100 * MSEC_PER_SEC) {

		rcu_delay_page_cache_fill_msec =
			clamp(rcu_delay_page_cache_fill_msec, 0,
				(int) (100 * MSEC_PER_SEC));

		pr_info("Adjusting rcutree.rcu_delay_page_cache_fill_msec to %d ms.\n",
			rcu_delay_page_cache_fill_msec);
	}

	for_each_possible_cpu(cpu) {
		struct kfree_rcu_cpu *krcp = per_cpu_ptr(&krc, cpu);

		for (i = 0; i < KFREE_N_BATCHES; i++) {
			INIT_RCU_WORK(&krcp->krw_arr[i].rcu_work, kfree_rcu_work);
			krcp->krw_arr[i].krcp = krcp;

			for (j = 0; j < FREE_N_CHANNELS; j++)
				INIT_LIST_HEAD(&krcp->krw_arr[i].bulk_head_free[j]);
		}

		for (i = 0; i < FREE_N_CHANNELS; i++)
			INIT_LIST_HEAD(&krcp->bulk_head[i]);

		INIT_DELAYED_WORK(&krcp->monitor_work, kfree_rcu_monitor);
		INIT_DELAYED_WORK(&krcp->page_cache_work, fill_page_cache_func);
		krcp->initialized = true;
	}
	if (register_shrinker(&kfree_rcu_shrinker, "rcu-kfree"))
		pr_err("Failed to register kfree_rcu() shrinker!\n");
}

void __init rcu_init(void)
{
	int cpu = smp_processor_id();

	rcu_early_boot_tests();

	kfree_rcu_batch_init();
	rcu_bootup_announce();
	sanitize_kthread_prio();
	rcu_init_geometry();
	rcu_init_one();
	if (dump_tree)
		rcu_dump_rcu_node_tree();
	if (use_softirq)
		open_softirq(RCU_SOFTIRQ, rcu_core_si);

	 
	pm_notifier(rcu_pm_notify, 0);
	WARN_ON(num_online_cpus() > 1); 
	rcutree_prepare_cpu(cpu);
	rcu_cpu_starting(cpu);
	rcutree_online_cpu(cpu);

	 
	rcu_gp_wq = alloc_workqueue("rcu_gp", WQ_MEM_RECLAIM, 0);
	WARN_ON(!rcu_gp_wq);
	rcu_alloc_par_gp_wq();

	 
	 
	if (qovld < 0)
		qovld_calc = DEFAULT_RCU_QOVLD_MULT * qhimark;
	else
		qovld_calc = qovld;

	
	(void)start_poll_synchronize_rcu_expedited();

	rcu_test_sync_prims();
}

#include "tree_stall.h"
#include "tree_exp.h"
#include "tree_nocb.h"
#include "tree_plugin.h"
