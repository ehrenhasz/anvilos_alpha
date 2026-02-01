
 

#define pr_fmt(fmt) "rcu: " fmt

#include <linux/export.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/preempt.h>
#include <linux/rcupdate_wait.h>
#include <linux/sched.h>
#include <linux/smp.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/srcu.h>

#include "rcu.h"
#include "rcu_segcblist.h"

 
#define DEFAULT_SRCU_EXP_HOLDOFF (25 * 1000)
static ulong exp_holdoff = DEFAULT_SRCU_EXP_HOLDOFF;
module_param(exp_holdoff, ulong, 0444);

 
static ulong counter_wrap_check = (ULONG_MAX >> 2);
module_param(counter_wrap_check, ulong, 0444);

 
#define SRCU_SIZING_NONE	0
#define SRCU_SIZING_INIT	1
#define SRCU_SIZING_TORTURE	2
#define SRCU_SIZING_AUTO	3
#define SRCU_SIZING_CONTEND	0x10
#define SRCU_SIZING_IS(x) ((convert_to_big & ~SRCU_SIZING_CONTEND) == x)
#define SRCU_SIZING_IS_NONE() (SRCU_SIZING_IS(SRCU_SIZING_NONE))
#define SRCU_SIZING_IS_INIT() (SRCU_SIZING_IS(SRCU_SIZING_INIT))
#define SRCU_SIZING_IS_TORTURE() (SRCU_SIZING_IS(SRCU_SIZING_TORTURE))
#define SRCU_SIZING_IS_CONTEND() (convert_to_big & SRCU_SIZING_CONTEND)
static int convert_to_big = SRCU_SIZING_AUTO;
module_param(convert_to_big, int, 0444);

 
static int big_cpu_lim __read_mostly = 128;
module_param(big_cpu_lim, int, 0444);

 
static int small_contention_lim __read_mostly = 100;
module_param(small_contention_lim, int, 0444);

 
static LIST_HEAD(srcu_boot_list);
static bool __read_mostly srcu_init_done;

static void srcu_invoke_callbacks(struct work_struct *work);
static void srcu_reschedule(struct srcu_struct *ssp, unsigned long delay);
static void process_srcu(struct work_struct *work);
static void srcu_delay_timer(struct timer_list *t);

 
#define spin_lock_rcu_node(p)							\
do {										\
	spin_lock(&ACCESS_PRIVATE(p, lock));					\
	smp_mb__after_unlock_lock();						\
} while (0)

#define spin_unlock_rcu_node(p) spin_unlock(&ACCESS_PRIVATE(p, lock))

#define spin_lock_irq_rcu_node(p)						\
do {										\
	spin_lock_irq(&ACCESS_PRIVATE(p, lock));				\
	smp_mb__after_unlock_lock();						\
} while (0)

#define spin_unlock_irq_rcu_node(p)						\
	spin_unlock_irq(&ACCESS_PRIVATE(p, lock))

#define spin_lock_irqsave_rcu_node(p, flags)					\
do {										\
	spin_lock_irqsave(&ACCESS_PRIVATE(p, lock), flags);			\
	smp_mb__after_unlock_lock();						\
} while (0)

#define spin_trylock_irqsave_rcu_node(p, flags)					\
({										\
	bool ___locked = spin_trylock_irqsave(&ACCESS_PRIVATE(p, lock), flags); \
										\
	if (___locked)								\
		smp_mb__after_unlock_lock();					\
	___locked;								\
})

#define spin_unlock_irqrestore_rcu_node(p, flags)				\
	spin_unlock_irqrestore(&ACCESS_PRIVATE(p, lock), flags)			\

 
static void init_srcu_struct_data(struct srcu_struct *ssp)
{
	int cpu;
	struct srcu_data *sdp;

	 
	WARN_ON_ONCE(ARRAY_SIZE(sdp->srcu_lock_count) !=
		     ARRAY_SIZE(sdp->srcu_unlock_count));
	for_each_possible_cpu(cpu) {
		sdp = per_cpu_ptr(ssp->sda, cpu);
		spin_lock_init(&ACCESS_PRIVATE(sdp, lock));
		rcu_segcblist_init(&sdp->srcu_cblist);
		sdp->srcu_cblist_invoking = false;
		sdp->srcu_gp_seq_needed = ssp->srcu_sup->srcu_gp_seq;
		sdp->srcu_gp_seq_needed_exp = ssp->srcu_sup->srcu_gp_seq;
		sdp->mynode = NULL;
		sdp->cpu = cpu;
		INIT_WORK(&sdp->work, srcu_invoke_callbacks);
		timer_setup(&sdp->delay_work, srcu_delay_timer, 0);
		sdp->ssp = ssp;
	}
}

 
#define SRCU_SNP_INIT_SEQ		0x2

 
static inline bool srcu_invl_snp_seq(unsigned long s)
{
	return s == SRCU_SNP_INIT_SEQ;
}

 
static bool init_srcu_struct_nodes(struct srcu_struct *ssp, gfp_t gfp_flags)
{
	int cpu;
	int i;
	int level = 0;
	int levelspread[RCU_NUM_LVLS];
	struct srcu_data *sdp;
	struct srcu_node *snp;
	struct srcu_node *snp_first;

	 
	rcu_init_geometry();
	ssp->srcu_sup->node = kcalloc(rcu_num_nodes, sizeof(*ssp->srcu_sup->node), gfp_flags);
	if (!ssp->srcu_sup->node)
		return false;

	 
	ssp->srcu_sup->level[0] = &ssp->srcu_sup->node[0];
	for (i = 1; i < rcu_num_lvls; i++)
		ssp->srcu_sup->level[i] = ssp->srcu_sup->level[i - 1] + num_rcu_lvl[i - 1];
	rcu_init_levelspread(levelspread, num_rcu_lvl);

	 
	srcu_for_each_node_breadth_first(ssp, snp) {
		spin_lock_init(&ACCESS_PRIVATE(snp, lock));
		WARN_ON_ONCE(ARRAY_SIZE(snp->srcu_have_cbs) !=
			     ARRAY_SIZE(snp->srcu_data_have_cbs));
		for (i = 0; i < ARRAY_SIZE(snp->srcu_have_cbs); i++) {
			snp->srcu_have_cbs[i] = SRCU_SNP_INIT_SEQ;
			snp->srcu_data_have_cbs[i] = 0;
		}
		snp->srcu_gp_seq_needed_exp = SRCU_SNP_INIT_SEQ;
		snp->grplo = -1;
		snp->grphi = -1;
		if (snp == &ssp->srcu_sup->node[0]) {
			 
			snp->srcu_parent = NULL;
			continue;
		}

		 
		if (snp == ssp->srcu_sup->level[level + 1])
			level++;
		snp->srcu_parent = ssp->srcu_sup->level[level - 1] +
				   (snp - ssp->srcu_sup->level[level]) /
				   levelspread[level - 1];
	}

	 
	level = rcu_num_lvls - 1;
	snp_first = ssp->srcu_sup->level[level];
	for_each_possible_cpu(cpu) {
		sdp = per_cpu_ptr(ssp->sda, cpu);
		sdp->mynode = &snp_first[cpu / levelspread[level]];
		for (snp = sdp->mynode; snp != NULL; snp = snp->srcu_parent) {
			if (snp->grplo < 0)
				snp->grplo = cpu;
			snp->grphi = cpu;
		}
		sdp->grpmask = 1UL << (cpu - sdp->mynode->grplo);
	}
	smp_store_release(&ssp->srcu_sup->srcu_size_state, SRCU_SIZE_WAIT_BARRIER);
	return true;
}

 
static int init_srcu_struct_fields(struct srcu_struct *ssp, bool is_static)
{
	if (!is_static)
		ssp->srcu_sup = kzalloc(sizeof(*ssp->srcu_sup), GFP_KERNEL);
	if (!ssp->srcu_sup)
		return -ENOMEM;
	if (!is_static)
		spin_lock_init(&ACCESS_PRIVATE(ssp->srcu_sup, lock));
	ssp->srcu_sup->srcu_size_state = SRCU_SIZE_SMALL;
	ssp->srcu_sup->node = NULL;
	mutex_init(&ssp->srcu_sup->srcu_cb_mutex);
	mutex_init(&ssp->srcu_sup->srcu_gp_mutex);
	ssp->srcu_idx = 0;
	ssp->srcu_sup->srcu_gp_seq = 0;
	ssp->srcu_sup->srcu_barrier_seq = 0;
	mutex_init(&ssp->srcu_sup->srcu_barrier_mutex);
	atomic_set(&ssp->srcu_sup->srcu_barrier_cpu_cnt, 0);
	INIT_DELAYED_WORK(&ssp->srcu_sup->work, process_srcu);
	ssp->srcu_sup->sda_is_static = is_static;
	if (!is_static)
		ssp->sda = alloc_percpu(struct srcu_data);
	if (!ssp->sda) {
		if (!is_static)
			kfree(ssp->srcu_sup);
		return -ENOMEM;
	}
	init_srcu_struct_data(ssp);
	ssp->srcu_sup->srcu_gp_seq_needed_exp = 0;
	ssp->srcu_sup->srcu_last_gp_end = ktime_get_mono_fast_ns();
	if (READ_ONCE(ssp->srcu_sup->srcu_size_state) == SRCU_SIZE_SMALL && SRCU_SIZING_IS_INIT()) {
		if (!init_srcu_struct_nodes(ssp, GFP_ATOMIC)) {
			if (!ssp->srcu_sup->sda_is_static) {
				free_percpu(ssp->sda);
				ssp->sda = NULL;
				kfree(ssp->srcu_sup);
				return -ENOMEM;
			}
		} else {
			WRITE_ONCE(ssp->srcu_sup->srcu_size_state, SRCU_SIZE_BIG);
		}
	}
	ssp->srcu_sup->srcu_ssp = ssp;
	smp_store_release(&ssp->srcu_sup->srcu_gp_seq_needed, 0);  
	return 0;
}

#ifdef CONFIG_DEBUG_LOCK_ALLOC

int __init_srcu_struct(struct srcu_struct *ssp, const char *name,
		       struct lock_class_key *key)
{
	 
	debug_check_no_locks_freed((void *)ssp, sizeof(*ssp));
	lockdep_init_map(&ssp->dep_map, name, key, 0);
	return init_srcu_struct_fields(ssp, false);
}
EXPORT_SYMBOL_GPL(__init_srcu_struct);

#else  

 
int init_srcu_struct(struct srcu_struct *ssp)
{
	return init_srcu_struct_fields(ssp, false);
}
EXPORT_SYMBOL_GPL(init_srcu_struct);

#endif  

 
static void __srcu_transition_to_big(struct srcu_struct *ssp)
{
	lockdep_assert_held(&ACCESS_PRIVATE(ssp->srcu_sup, lock));
	smp_store_release(&ssp->srcu_sup->srcu_size_state, SRCU_SIZE_ALLOC);
}

 
static void srcu_transition_to_big(struct srcu_struct *ssp)
{
	unsigned long flags;

	 
	if (smp_load_acquire(&ssp->srcu_sup->srcu_size_state) != SRCU_SIZE_SMALL)
		return;
	spin_lock_irqsave_rcu_node(ssp->srcu_sup, flags);
	if (smp_load_acquire(&ssp->srcu_sup->srcu_size_state) != SRCU_SIZE_SMALL) {
		spin_unlock_irqrestore_rcu_node(ssp->srcu_sup, flags);
		return;
	}
	__srcu_transition_to_big(ssp);
	spin_unlock_irqrestore_rcu_node(ssp->srcu_sup, flags);
}

 
static void spin_lock_irqsave_check_contention(struct srcu_struct *ssp)
{
	unsigned long j;

	if (!SRCU_SIZING_IS_CONTEND() || ssp->srcu_sup->srcu_size_state)
		return;
	j = jiffies;
	if (ssp->srcu_sup->srcu_size_jiffies != j) {
		ssp->srcu_sup->srcu_size_jiffies = j;
		ssp->srcu_sup->srcu_n_lock_retries = 0;
	}
	if (++ssp->srcu_sup->srcu_n_lock_retries <= small_contention_lim)
		return;
	__srcu_transition_to_big(ssp);
}

 
static void spin_lock_irqsave_sdp_contention(struct srcu_data *sdp, unsigned long *flags)
{
	struct srcu_struct *ssp = sdp->ssp;

	if (spin_trylock_irqsave_rcu_node(sdp, *flags))
		return;
	spin_lock_irqsave_rcu_node(ssp->srcu_sup, *flags);
	spin_lock_irqsave_check_contention(ssp);
	spin_unlock_irqrestore_rcu_node(ssp->srcu_sup, *flags);
	spin_lock_irqsave_rcu_node(sdp, *flags);
}

 
static void spin_lock_irqsave_ssp_contention(struct srcu_struct *ssp, unsigned long *flags)
{
	if (spin_trylock_irqsave_rcu_node(ssp->srcu_sup, *flags))
		return;
	spin_lock_irqsave_rcu_node(ssp->srcu_sup, *flags);
	spin_lock_irqsave_check_contention(ssp);
}

 
static void check_init_srcu_struct(struct srcu_struct *ssp)
{
	unsigned long flags;

	 
	if (!rcu_seq_state(smp_load_acquire(&ssp->srcu_sup->srcu_gp_seq_needed)))  
		return;  
	spin_lock_irqsave_rcu_node(ssp->srcu_sup, flags);
	if (!rcu_seq_state(ssp->srcu_sup->srcu_gp_seq_needed)) {
		spin_unlock_irqrestore_rcu_node(ssp->srcu_sup, flags);
		return;
	}
	init_srcu_struct_fields(ssp, true);
	spin_unlock_irqrestore_rcu_node(ssp->srcu_sup, flags);
}

 
static unsigned long srcu_readers_lock_idx(struct srcu_struct *ssp, int idx)
{
	int cpu;
	unsigned long sum = 0;

	for_each_possible_cpu(cpu) {
		struct srcu_data *cpuc = per_cpu_ptr(ssp->sda, cpu);

		sum += atomic_long_read(&cpuc->srcu_lock_count[idx]);
	}
	return sum;
}

 
static unsigned long srcu_readers_unlock_idx(struct srcu_struct *ssp, int idx)
{
	int cpu;
	unsigned long mask = 0;
	unsigned long sum = 0;

	for_each_possible_cpu(cpu) {
		struct srcu_data *cpuc = per_cpu_ptr(ssp->sda, cpu);

		sum += atomic_long_read(&cpuc->srcu_unlock_count[idx]);
		if (IS_ENABLED(CONFIG_PROVE_RCU))
			mask = mask | READ_ONCE(cpuc->srcu_nmi_safety);
	}
	WARN_ONCE(IS_ENABLED(CONFIG_PROVE_RCU) && (mask & (mask >> 1)),
		  "Mixed NMI-safe readers for srcu_struct at %ps.\n", ssp);
	return sum;
}

 
static bool srcu_readers_active_idx_check(struct srcu_struct *ssp, int idx)
{
	unsigned long unlocks;

	unlocks = srcu_readers_unlock_idx(ssp, idx);

	 
	smp_mb();  

	 
	return srcu_readers_lock_idx(ssp, idx) == unlocks;
}

 
static bool srcu_readers_active(struct srcu_struct *ssp)
{
	int cpu;
	unsigned long sum = 0;

	for_each_possible_cpu(cpu) {
		struct srcu_data *cpuc = per_cpu_ptr(ssp->sda, cpu);

		sum += atomic_long_read(&cpuc->srcu_lock_count[0]);
		sum += atomic_long_read(&cpuc->srcu_lock_count[1]);
		sum -= atomic_long_read(&cpuc->srcu_unlock_count[0]);
		sum -= atomic_long_read(&cpuc->srcu_unlock_count[1]);
	}
	return sum;
}

 
#define SRCU_DEFAULT_RETRY_CHECK_DELAY		5

static ulong srcu_retry_check_delay = SRCU_DEFAULT_RETRY_CHECK_DELAY;
module_param(srcu_retry_check_delay, ulong, 0444);

#define SRCU_INTERVAL		1		
#define SRCU_MAX_INTERVAL	10		

#define SRCU_DEFAULT_MAX_NODELAY_PHASE_LO	3UL	
							
#define SRCU_DEFAULT_MAX_NODELAY_PHASE_HI	1000UL	
							

#define SRCU_UL_CLAMP_LO(val, low)	((val) > (low) ? (val) : (low))
#define SRCU_UL_CLAMP_HI(val, high)	((val) < (high) ? (val) : (high))
#define SRCU_UL_CLAMP(val, low, high)	SRCU_UL_CLAMP_HI(SRCU_UL_CLAMP_LO((val), (low)), (high))



#define SRCU_DEFAULT_MAX_NODELAY_PHASE_ADJUSTED	\
	(2UL * USEC_PER_SEC / HZ / SRCU_DEFAULT_RETRY_CHECK_DELAY)


#define SRCU_DEFAULT_MAX_NODELAY_PHASE	\
	SRCU_UL_CLAMP(SRCU_DEFAULT_MAX_NODELAY_PHASE_ADJUSTED,	\
		      SRCU_DEFAULT_MAX_NODELAY_PHASE_LO,	\
		      SRCU_DEFAULT_MAX_NODELAY_PHASE_HI)

static ulong srcu_max_nodelay_phase = SRCU_DEFAULT_MAX_NODELAY_PHASE;
module_param(srcu_max_nodelay_phase, ulong, 0444);


#define SRCU_DEFAULT_MAX_NODELAY	(SRCU_DEFAULT_MAX_NODELAY_PHASE > 100 ?	\
					 SRCU_DEFAULT_MAX_NODELAY_PHASE : 100)

static ulong srcu_max_nodelay = SRCU_DEFAULT_MAX_NODELAY;
module_param(srcu_max_nodelay, ulong, 0444);

 
static unsigned long srcu_get_delay(struct srcu_struct *ssp)
{
	unsigned long gpstart;
	unsigned long j;
	unsigned long jbase = SRCU_INTERVAL;
	struct srcu_usage *sup = ssp->srcu_sup;

	if (ULONG_CMP_LT(READ_ONCE(sup->srcu_gp_seq), READ_ONCE(sup->srcu_gp_seq_needed_exp)))
		jbase = 0;
	if (rcu_seq_state(READ_ONCE(sup->srcu_gp_seq))) {
		j = jiffies - 1;
		gpstart = READ_ONCE(sup->srcu_gp_start);
		if (time_after(j, gpstart))
			jbase += j - gpstart;
		if (!jbase) {
			WRITE_ONCE(sup->srcu_n_exp_nodelay, READ_ONCE(sup->srcu_n_exp_nodelay) + 1);
			if (READ_ONCE(sup->srcu_n_exp_nodelay) > srcu_max_nodelay_phase)
				jbase = 1;
		}
	}
	return jbase > SRCU_MAX_INTERVAL ? SRCU_MAX_INTERVAL : jbase;
}

 
void cleanup_srcu_struct(struct srcu_struct *ssp)
{
	int cpu;
	struct srcu_usage *sup = ssp->srcu_sup;

	if (WARN_ON(!srcu_get_delay(ssp)))
		return;  
	if (WARN_ON(srcu_readers_active(ssp)))
		return;  
	flush_delayed_work(&sup->work);
	for_each_possible_cpu(cpu) {
		struct srcu_data *sdp = per_cpu_ptr(ssp->sda, cpu);

		del_timer_sync(&sdp->delay_work);
		flush_work(&sdp->work);
		if (WARN_ON(rcu_segcblist_n_cbs(&sdp->srcu_cblist)))
			return;  
	}
	if (WARN_ON(rcu_seq_state(READ_ONCE(sup->srcu_gp_seq)) != SRCU_STATE_IDLE) ||
	    WARN_ON(rcu_seq_current(&sup->srcu_gp_seq) != sup->srcu_gp_seq_needed) ||
	    WARN_ON(srcu_readers_active(ssp))) {
		pr_info("%s: Active srcu_struct %p read state: %d gp state: %lu/%lu\n",
			__func__, ssp, rcu_seq_state(READ_ONCE(sup->srcu_gp_seq)),
			rcu_seq_current(&sup->srcu_gp_seq), sup->srcu_gp_seq_needed);
		return;  
	}
	kfree(sup->node);
	sup->node = NULL;
	sup->srcu_size_state = SRCU_SIZE_SMALL;
	if (!sup->sda_is_static) {
		free_percpu(ssp->sda);
		ssp->sda = NULL;
		kfree(sup);
		ssp->srcu_sup = NULL;
	}
}
EXPORT_SYMBOL_GPL(cleanup_srcu_struct);

#ifdef CONFIG_PROVE_RCU
 
void srcu_check_nmi_safety(struct srcu_struct *ssp, bool nmi_safe)
{
	int nmi_safe_mask = 1 << nmi_safe;
	int old_nmi_safe_mask;
	struct srcu_data *sdp;

	 
	WARN_ON_ONCE(!nmi_safe && in_nmi());
	sdp = raw_cpu_ptr(ssp->sda);
	old_nmi_safe_mask = READ_ONCE(sdp->srcu_nmi_safety);
	if (!old_nmi_safe_mask) {
		WRITE_ONCE(sdp->srcu_nmi_safety, nmi_safe_mask);
		return;
	}
	WARN_ONCE(old_nmi_safe_mask != nmi_safe_mask, "CPU %d old state %d new state %d\n", sdp->cpu, old_nmi_safe_mask, nmi_safe_mask);
}
EXPORT_SYMBOL_GPL(srcu_check_nmi_safety);
#endif  

 
int __srcu_read_lock(struct srcu_struct *ssp)
{
	int idx;

	idx = READ_ONCE(ssp->srcu_idx) & 0x1;
	this_cpu_inc(ssp->sda->srcu_lock_count[idx].counter);
	smp_mb();     
	return idx;
}
EXPORT_SYMBOL_GPL(__srcu_read_lock);

 
void __srcu_read_unlock(struct srcu_struct *ssp, int idx)
{
	smp_mb();     
	this_cpu_inc(ssp->sda->srcu_unlock_count[idx].counter);
}
EXPORT_SYMBOL_GPL(__srcu_read_unlock);

#ifdef CONFIG_NEED_SRCU_NMI_SAFE

 
int __srcu_read_lock_nmisafe(struct srcu_struct *ssp)
{
	int idx;
	struct srcu_data *sdp = raw_cpu_ptr(ssp->sda);

	idx = READ_ONCE(ssp->srcu_idx) & 0x1;
	atomic_long_inc(&sdp->srcu_lock_count[idx]);
	smp_mb__after_atomic();     
	return idx;
}
EXPORT_SYMBOL_GPL(__srcu_read_lock_nmisafe);

 
void __srcu_read_unlock_nmisafe(struct srcu_struct *ssp, int idx)
{
	struct srcu_data *sdp = raw_cpu_ptr(ssp->sda);

	smp_mb__before_atomic();     
	atomic_long_inc(&sdp->srcu_unlock_count[idx]);
}
EXPORT_SYMBOL_GPL(__srcu_read_unlock_nmisafe);

#endif 

 
static void srcu_gp_start(struct srcu_struct *ssp)
{
	struct srcu_data *sdp;
	int state;

	if (smp_load_acquire(&ssp->srcu_sup->srcu_size_state) < SRCU_SIZE_WAIT_BARRIER)
		sdp = per_cpu_ptr(ssp->sda, get_boot_cpu_id());
	else
		sdp = this_cpu_ptr(ssp->sda);
	lockdep_assert_held(&ACCESS_PRIVATE(ssp->srcu_sup, lock));
	WARN_ON_ONCE(ULONG_CMP_GE(ssp->srcu_sup->srcu_gp_seq, ssp->srcu_sup->srcu_gp_seq_needed));
	spin_lock_rcu_node(sdp);   
	rcu_segcblist_advance(&sdp->srcu_cblist,
			      rcu_seq_current(&ssp->srcu_sup->srcu_gp_seq));
	WARN_ON_ONCE(!rcu_segcblist_segempty(&sdp->srcu_cblist, RCU_NEXT_TAIL));
	spin_unlock_rcu_node(sdp);   
	WRITE_ONCE(ssp->srcu_sup->srcu_gp_start, jiffies);
	WRITE_ONCE(ssp->srcu_sup->srcu_n_exp_nodelay, 0);
	smp_mb();  
	rcu_seq_start(&ssp->srcu_sup->srcu_gp_seq);
	state = rcu_seq_state(ssp->srcu_sup->srcu_gp_seq);
	WARN_ON_ONCE(state != SRCU_STATE_SCAN1);
}


static void srcu_delay_timer(struct timer_list *t)
{
	struct srcu_data *sdp = container_of(t, struct srcu_data, delay_work);

	queue_work_on(sdp->cpu, rcu_gp_wq, &sdp->work);
}

static void srcu_queue_delayed_work_on(struct srcu_data *sdp,
				       unsigned long delay)
{
	if (!delay) {
		queue_work_on(sdp->cpu, rcu_gp_wq, &sdp->work);
		return;
	}

	timer_reduce(&sdp->delay_work, jiffies + delay);
}

 
static void srcu_schedule_cbs_sdp(struct srcu_data *sdp, unsigned long delay)
{
	srcu_queue_delayed_work_on(sdp, delay);
}

 
static void srcu_schedule_cbs_snp(struct srcu_struct *ssp, struct srcu_node *snp,
				  unsigned long mask, unsigned long delay)
{
	int cpu;

	for (cpu = snp->grplo; cpu <= snp->grphi; cpu++) {
		if (!(mask & (1UL << (cpu - snp->grplo))))
			continue;
		srcu_schedule_cbs_sdp(per_cpu_ptr(ssp->sda, cpu), delay);
	}
}

 
static void srcu_gp_end(struct srcu_struct *ssp)
{
	unsigned long cbdelay = 1;
	bool cbs;
	bool last_lvl;
	int cpu;
	unsigned long flags;
	unsigned long gpseq;
	int idx;
	unsigned long mask;
	struct srcu_data *sdp;
	unsigned long sgsne;
	struct srcu_node *snp;
	int ss_state;
	struct srcu_usage *sup = ssp->srcu_sup;

	 
	mutex_lock(&sup->srcu_cb_mutex);

	 
	spin_lock_irq_rcu_node(sup);
	idx = rcu_seq_state(sup->srcu_gp_seq);
	WARN_ON_ONCE(idx != SRCU_STATE_SCAN2);
	if (ULONG_CMP_LT(READ_ONCE(sup->srcu_gp_seq), READ_ONCE(sup->srcu_gp_seq_needed_exp)))
		cbdelay = 0;

	WRITE_ONCE(sup->srcu_last_gp_end, ktime_get_mono_fast_ns());
	rcu_seq_end(&sup->srcu_gp_seq);
	gpseq = rcu_seq_current(&sup->srcu_gp_seq);
	if (ULONG_CMP_LT(sup->srcu_gp_seq_needed_exp, gpseq))
		WRITE_ONCE(sup->srcu_gp_seq_needed_exp, gpseq);
	spin_unlock_irq_rcu_node(sup);
	mutex_unlock(&sup->srcu_gp_mutex);
	 

	 
	ss_state = smp_load_acquire(&sup->srcu_size_state);
	if (ss_state < SRCU_SIZE_WAIT_BARRIER) {
		srcu_schedule_cbs_sdp(per_cpu_ptr(ssp->sda, get_boot_cpu_id()),
					cbdelay);
	} else {
		idx = rcu_seq_ctr(gpseq) % ARRAY_SIZE(snp->srcu_have_cbs);
		srcu_for_each_node_breadth_first(ssp, snp) {
			spin_lock_irq_rcu_node(snp);
			cbs = false;
			last_lvl = snp >= sup->level[rcu_num_lvls - 1];
			if (last_lvl)
				cbs = ss_state < SRCU_SIZE_BIG || snp->srcu_have_cbs[idx] == gpseq;
			snp->srcu_have_cbs[idx] = gpseq;
			rcu_seq_set_state(&snp->srcu_have_cbs[idx], 1);
			sgsne = snp->srcu_gp_seq_needed_exp;
			if (srcu_invl_snp_seq(sgsne) || ULONG_CMP_LT(sgsne, gpseq))
				WRITE_ONCE(snp->srcu_gp_seq_needed_exp, gpseq);
			if (ss_state < SRCU_SIZE_BIG)
				mask = ~0;
			else
				mask = snp->srcu_data_have_cbs[idx];
			snp->srcu_data_have_cbs[idx] = 0;
			spin_unlock_irq_rcu_node(snp);
			if (cbs)
				srcu_schedule_cbs_snp(ssp, snp, mask, cbdelay);
		}
	}

	 
	if (!(gpseq & counter_wrap_check))
		for_each_possible_cpu(cpu) {
			sdp = per_cpu_ptr(ssp->sda, cpu);
			spin_lock_irqsave_rcu_node(sdp, flags);
			if (ULONG_CMP_GE(gpseq, sdp->srcu_gp_seq_needed + 100))
				sdp->srcu_gp_seq_needed = gpseq;
			if (ULONG_CMP_GE(gpseq, sdp->srcu_gp_seq_needed_exp + 100))
				sdp->srcu_gp_seq_needed_exp = gpseq;
			spin_unlock_irqrestore_rcu_node(sdp, flags);
		}

	 
	mutex_unlock(&sup->srcu_cb_mutex);

	 
	spin_lock_irq_rcu_node(sup);
	gpseq = rcu_seq_current(&sup->srcu_gp_seq);
	if (!rcu_seq_state(gpseq) &&
	    ULONG_CMP_LT(gpseq, sup->srcu_gp_seq_needed)) {
		srcu_gp_start(ssp);
		spin_unlock_irq_rcu_node(sup);
		srcu_reschedule(ssp, 0);
	} else {
		spin_unlock_irq_rcu_node(sup);
	}

	 
	if (ss_state != SRCU_SIZE_SMALL && ss_state != SRCU_SIZE_BIG) {
		if (ss_state == SRCU_SIZE_ALLOC)
			init_srcu_struct_nodes(ssp, GFP_KERNEL);
		else
			smp_store_release(&sup->srcu_size_state, ss_state + 1);
	}
}

 
static void srcu_funnel_exp_start(struct srcu_struct *ssp, struct srcu_node *snp,
				  unsigned long s)
{
	unsigned long flags;
	unsigned long sgsne;

	if (snp)
		for (; snp != NULL; snp = snp->srcu_parent) {
			sgsne = READ_ONCE(snp->srcu_gp_seq_needed_exp);
			if (WARN_ON_ONCE(rcu_seq_done(&ssp->srcu_sup->srcu_gp_seq, s)) ||
			    (!srcu_invl_snp_seq(sgsne) && ULONG_CMP_GE(sgsne, s)))
				return;
			spin_lock_irqsave_rcu_node(snp, flags);
			sgsne = snp->srcu_gp_seq_needed_exp;
			if (!srcu_invl_snp_seq(sgsne) && ULONG_CMP_GE(sgsne, s)) {
				spin_unlock_irqrestore_rcu_node(snp, flags);
				return;
			}
			WRITE_ONCE(snp->srcu_gp_seq_needed_exp, s);
			spin_unlock_irqrestore_rcu_node(snp, flags);
		}
	spin_lock_irqsave_ssp_contention(ssp, &flags);
	if (ULONG_CMP_LT(ssp->srcu_sup->srcu_gp_seq_needed_exp, s))
		WRITE_ONCE(ssp->srcu_sup->srcu_gp_seq_needed_exp, s);
	spin_unlock_irqrestore_rcu_node(ssp->srcu_sup, flags);
}

 
static void srcu_funnel_gp_start(struct srcu_struct *ssp, struct srcu_data *sdp,
				 unsigned long s, bool do_norm)
{
	unsigned long flags;
	int idx = rcu_seq_ctr(s) % ARRAY_SIZE(sdp->mynode->srcu_have_cbs);
	unsigned long sgsne;
	struct srcu_node *snp;
	struct srcu_node *snp_leaf;
	unsigned long snp_seq;
	struct srcu_usage *sup = ssp->srcu_sup;

	 
	if (smp_load_acquire(&sup->srcu_size_state) < SRCU_SIZE_WAIT_BARRIER)
		snp_leaf = NULL;
	else
		snp_leaf = sdp->mynode;

	if (snp_leaf)
		 
		for (snp = snp_leaf; snp != NULL; snp = snp->srcu_parent) {
			if (WARN_ON_ONCE(rcu_seq_done(&sup->srcu_gp_seq, s)) && snp != snp_leaf)
				return;  
			spin_lock_irqsave_rcu_node(snp, flags);
			snp_seq = snp->srcu_have_cbs[idx];
			if (!srcu_invl_snp_seq(snp_seq) && ULONG_CMP_GE(snp_seq, s)) {
				if (snp == snp_leaf && snp_seq == s)
					snp->srcu_data_have_cbs[idx] |= sdp->grpmask;
				spin_unlock_irqrestore_rcu_node(snp, flags);
				if (snp == snp_leaf && snp_seq != s) {
					srcu_schedule_cbs_sdp(sdp, do_norm ? SRCU_INTERVAL : 0);
					return;
				}
				if (!do_norm)
					srcu_funnel_exp_start(ssp, snp, s);
				return;
			}
			snp->srcu_have_cbs[idx] = s;
			if (snp == snp_leaf)
				snp->srcu_data_have_cbs[idx] |= sdp->grpmask;
			sgsne = snp->srcu_gp_seq_needed_exp;
			if (!do_norm && (srcu_invl_snp_seq(sgsne) || ULONG_CMP_LT(sgsne, s)))
				WRITE_ONCE(snp->srcu_gp_seq_needed_exp, s);
			spin_unlock_irqrestore_rcu_node(snp, flags);
		}

	 
	spin_lock_irqsave_ssp_contention(ssp, &flags);
	if (ULONG_CMP_LT(sup->srcu_gp_seq_needed, s)) {
		 
		smp_store_release(&sup->srcu_gp_seq_needed, s);  
	}
	if (!do_norm && ULONG_CMP_LT(sup->srcu_gp_seq_needed_exp, s))
		WRITE_ONCE(sup->srcu_gp_seq_needed_exp, s);

	 
	if (!WARN_ON_ONCE(rcu_seq_done(&sup->srcu_gp_seq, s)) &&
	    rcu_seq_state(sup->srcu_gp_seq) == SRCU_STATE_IDLE) {
		WARN_ON_ONCE(ULONG_CMP_GE(sup->srcu_gp_seq, sup->srcu_gp_seq_needed));
		srcu_gp_start(ssp);

		
		
		
		
		
		if (likely(srcu_init_done))
			queue_delayed_work(rcu_gp_wq, &sup->work,
					   !!srcu_get_delay(ssp));
		else if (list_empty(&sup->work.work.entry))
			list_add(&sup->work.work.entry, &srcu_boot_list);
	}
	spin_unlock_irqrestore_rcu_node(sup, flags);
}

 
static bool try_check_zero(struct srcu_struct *ssp, int idx, int trycount)
{
	unsigned long curdelay;

	curdelay = !srcu_get_delay(ssp);

	for (;;) {
		if (srcu_readers_active_idx_check(ssp, idx))
			return true;
		if ((--trycount + curdelay) <= 0)
			return false;
		udelay(srcu_retry_check_delay);
	}
}

 
static void srcu_flip(struct srcu_struct *ssp)
{
	 
	smp_mb();     

	WRITE_ONCE(ssp->srcu_idx, ssp->srcu_idx + 1);  

	 
	smp_mb();     
}

 
static bool srcu_might_be_idle(struct srcu_struct *ssp)
{
	unsigned long curseq;
	unsigned long flags;
	struct srcu_data *sdp;
	unsigned long t;
	unsigned long tlast;

	check_init_srcu_struct(ssp);
	 
	sdp = raw_cpu_ptr(ssp->sda);
	spin_lock_irqsave_rcu_node(sdp, flags);
	if (rcu_segcblist_pend_cbs(&sdp->srcu_cblist)) {
		spin_unlock_irqrestore_rcu_node(sdp, flags);
		return false;  
	}
	spin_unlock_irqrestore_rcu_node(sdp, flags);

	 

	 
	t = ktime_get_mono_fast_ns();
	tlast = READ_ONCE(ssp->srcu_sup->srcu_last_gp_end);
	if (exp_holdoff == 0 ||
	    time_in_range_open(t, tlast, tlast + exp_holdoff))
		return false;  

	 
	curseq = rcu_seq_current(&ssp->srcu_sup->srcu_gp_seq);
	smp_mb();  
	if (ULONG_CMP_LT(curseq, READ_ONCE(ssp->srcu_sup->srcu_gp_seq_needed)))
		return false;  
	smp_mb();  
	if (curseq != rcu_seq_current(&ssp->srcu_sup->srcu_gp_seq))
		return false;  
	return true;  
}

 
static void srcu_leak_callback(struct rcu_head *rhp)
{
}

 
static unsigned long srcu_gp_start_if_needed(struct srcu_struct *ssp,
					     struct rcu_head *rhp, bool do_norm)
{
	unsigned long flags;
	int idx;
	bool needexp = false;
	bool needgp = false;
	unsigned long s;
	struct srcu_data *sdp;
	struct srcu_node *sdp_mynode;
	int ss_state;

	check_init_srcu_struct(ssp);
	 
	idx = __srcu_read_lock_nmisafe(ssp);
	ss_state = smp_load_acquire(&ssp->srcu_sup->srcu_size_state);
	if (ss_state < SRCU_SIZE_WAIT_CALL)
		sdp = per_cpu_ptr(ssp->sda, get_boot_cpu_id());
	else
		sdp = raw_cpu_ptr(ssp->sda);
	spin_lock_irqsave_sdp_contention(sdp, &flags);
	if (rhp)
		rcu_segcblist_enqueue(&sdp->srcu_cblist, rhp);
	 
	s = rcu_seq_snap(&ssp->srcu_sup->srcu_gp_seq);
	rcu_segcblist_advance(&sdp->srcu_cblist,
			      rcu_seq_current(&ssp->srcu_sup->srcu_gp_seq));
	WARN_ON_ONCE(!rcu_segcblist_accelerate(&sdp->srcu_cblist, s) && rhp);
	if (ULONG_CMP_LT(sdp->srcu_gp_seq_needed, s)) {
		sdp->srcu_gp_seq_needed = s;
		needgp = true;
	}
	if (!do_norm && ULONG_CMP_LT(sdp->srcu_gp_seq_needed_exp, s)) {
		sdp->srcu_gp_seq_needed_exp = s;
		needexp = true;
	}
	spin_unlock_irqrestore_rcu_node(sdp, flags);

	 
	if (ss_state < SRCU_SIZE_WAIT_BARRIER)
		sdp_mynode = NULL;
	else
		sdp_mynode = sdp->mynode;

	if (needgp)
		srcu_funnel_gp_start(ssp, sdp, s, do_norm);
	else if (needexp)
		srcu_funnel_exp_start(ssp, sdp_mynode, s);
	__srcu_read_unlock_nmisafe(ssp, idx);
	return s;
}

 
static void __call_srcu(struct srcu_struct *ssp, struct rcu_head *rhp,
			rcu_callback_t func, bool do_norm)
{
	if (debug_rcu_head_queue(rhp)) {
		 
		WRITE_ONCE(rhp->func, srcu_leak_callback);
		WARN_ONCE(1, "call_srcu(): Leaked duplicate callback\n");
		return;
	}
	rhp->func = func;
	(void)srcu_gp_start_if_needed(ssp, rhp, do_norm);
}

 
void call_srcu(struct srcu_struct *ssp, struct rcu_head *rhp,
	       rcu_callback_t func)
{
	__call_srcu(ssp, rhp, func, true);
}
EXPORT_SYMBOL_GPL(call_srcu);

 
static void __synchronize_srcu(struct srcu_struct *ssp, bool do_norm)
{
	struct rcu_synchronize rcu;

	srcu_lock_sync(&ssp->dep_map);

	RCU_LOCKDEP_WARN(lockdep_is_held(ssp) ||
			 lock_is_held(&rcu_bh_lock_map) ||
			 lock_is_held(&rcu_lock_map) ||
			 lock_is_held(&rcu_sched_lock_map),
			 "Illegal synchronize_srcu() in same-type SRCU (or in RCU) read-side critical section");

	if (rcu_scheduler_active == RCU_SCHEDULER_INACTIVE)
		return;
	might_sleep();
	check_init_srcu_struct(ssp);
	init_completion(&rcu.completion);
	init_rcu_head_on_stack(&rcu.head);
	__call_srcu(ssp, &rcu.head, wakeme_after_rcu, do_norm);
	wait_for_completion(&rcu.completion);
	destroy_rcu_head_on_stack(&rcu.head);

	 
	smp_mb();
}

 
void synchronize_srcu_expedited(struct srcu_struct *ssp)
{
	__synchronize_srcu(ssp, rcu_gp_is_normal());
}
EXPORT_SYMBOL_GPL(synchronize_srcu_expedited);

 
void synchronize_srcu(struct srcu_struct *ssp)
{
	if (srcu_might_be_idle(ssp) || rcu_gp_is_expedited())
		synchronize_srcu_expedited(ssp);
	else
		__synchronize_srcu(ssp, true);
}
EXPORT_SYMBOL_GPL(synchronize_srcu);

 
unsigned long get_state_synchronize_srcu(struct srcu_struct *ssp)
{
	 
	 
	smp_mb();
	return rcu_seq_snap(&ssp->srcu_sup->srcu_gp_seq);
}
EXPORT_SYMBOL_GPL(get_state_synchronize_srcu);

 
unsigned long start_poll_synchronize_srcu(struct srcu_struct *ssp)
{
	return srcu_gp_start_if_needed(ssp, NULL, true);
}
EXPORT_SYMBOL_GPL(start_poll_synchronize_srcu);

 
bool poll_state_synchronize_srcu(struct srcu_struct *ssp, unsigned long cookie)
{
	if (!rcu_seq_done(&ssp->srcu_sup->srcu_gp_seq, cookie))
		return false;
	
	
	smp_mb(); 
	return true;
}
EXPORT_SYMBOL_GPL(poll_state_synchronize_srcu);

 
static void srcu_barrier_cb(struct rcu_head *rhp)
{
	struct srcu_data *sdp;
	struct srcu_struct *ssp;

	sdp = container_of(rhp, struct srcu_data, srcu_barrier_head);
	ssp = sdp->ssp;
	if (atomic_dec_and_test(&ssp->srcu_sup->srcu_barrier_cpu_cnt))
		complete(&ssp->srcu_sup->srcu_barrier_completion);
}

 
static void srcu_barrier_one_cpu(struct srcu_struct *ssp, struct srcu_data *sdp)
{
	spin_lock_irq_rcu_node(sdp);
	atomic_inc(&ssp->srcu_sup->srcu_barrier_cpu_cnt);
	sdp->srcu_barrier_head.func = srcu_barrier_cb;
	debug_rcu_head_queue(&sdp->srcu_barrier_head);
	if (!rcu_segcblist_entrain(&sdp->srcu_cblist,
				   &sdp->srcu_barrier_head)) {
		debug_rcu_head_unqueue(&sdp->srcu_barrier_head);
		atomic_dec(&ssp->srcu_sup->srcu_barrier_cpu_cnt);
	}
	spin_unlock_irq_rcu_node(sdp);
}

 
void srcu_barrier(struct srcu_struct *ssp)
{
	int cpu;
	int idx;
	unsigned long s = rcu_seq_snap(&ssp->srcu_sup->srcu_barrier_seq);

	check_init_srcu_struct(ssp);
	mutex_lock(&ssp->srcu_sup->srcu_barrier_mutex);
	if (rcu_seq_done(&ssp->srcu_sup->srcu_barrier_seq, s)) {
		smp_mb();  
		mutex_unlock(&ssp->srcu_sup->srcu_barrier_mutex);
		return;  
	}
	rcu_seq_start(&ssp->srcu_sup->srcu_barrier_seq);
	init_completion(&ssp->srcu_sup->srcu_barrier_completion);

	 
	atomic_set(&ssp->srcu_sup->srcu_barrier_cpu_cnt, 1);

	idx = __srcu_read_lock_nmisafe(ssp);
	if (smp_load_acquire(&ssp->srcu_sup->srcu_size_state) < SRCU_SIZE_WAIT_BARRIER)
		srcu_barrier_one_cpu(ssp, per_cpu_ptr(ssp->sda,	get_boot_cpu_id()));
	else
		for_each_possible_cpu(cpu)
			srcu_barrier_one_cpu(ssp, per_cpu_ptr(ssp->sda, cpu));
	__srcu_read_unlock_nmisafe(ssp, idx);

	 
	if (atomic_dec_and_test(&ssp->srcu_sup->srcu_barrier_cpu_cnt))
		complete(&ssp->srcu_sup->srcu_barrier_completion);
	wait_for_completion(&ssp->srcu_sup->srcu_barrier_completion);

	rcu_seq_end(&ssp->srcu_sup->srcu_barrier_seq);
	mutex_unlock(&ssp->srcu_sup->srcu_barrier_mutex);
}
EXPORT_SYMBOL_GPL(srcu_barrier);

 
unsigned long srcu_batches_completed(struct srcu_struct *ssp)
{
	return READ_ONCE(ssp->srcu_idx);
}
EXPORT_SYMBOL_GPL(srcu_batches_completed);

 
static void srcu_advance_state(struct srcu_struct *ssp)
{
	int idx;

	mutex_lock(&ssp->srcu_sup->srcu_gp_mutex);

	 
	idx = rcu_seq_state(smp_load_acquire(&ssp->srcu_sup->srcu_gp_seq));  
	if (idx == SRCU_STATE_IDLE) {
		spin_lock_irq_rcu_node(ssp->srcu_sup);
		if (ULONG_CMP_GE(ssp->srcu_sup->srcu_gp_seq, ssp->srcu_sup->srcu_gp_seq_needed)) {
			WARN_ON_ONCE(rcu_seq_state(ssp->srcu_sup->srcu_gp_seq));
			spin_unlock_irq_rcu_node(ssp->srcu_sup);
			mutex_unlock(&ssp->srcu_sup->srcu_gp_mutex);
			return;
		}
		idx = rcu_seq_state(READ_ONCE(ssp->srcu_sup->srcu_gp_seq));
		if (idx == SRCU_STATE_IDLE)
			srcu_gp_start(ssp);
		spin_unlock_irq_rcu_node(ssp->srcu_sup);
		if (idx != SRCU_STATE_IDLE) {
			mutex_unlock(&ssp->srcu_sup->srcu_gp_mutex);
			return;  
		}
	}

	if (rcu_seq_state(READ_ONCE(ssp->srcu_sup->srcu_gp_seq)) == SRCU_STATE_SCAN1) {
		idx = 1 ^ (ssp->srcu_idx & 1);
		if (!try_check_zero(ssp, idx, 1)) {
			mutex_unlock(&ssp->srcu_sup->srcu_gp_mutex);
			return;  
		}
		srcu_flip(ssp);
		spin_lock_irq_rcu_node(ssp->srcu_sup);
		rcu_seq_set_state(&ssp->srcu_sup->srcu_gp_seq, SRCU_STATE_SCAN2);
		ssp->srcu_sup->srcu_n_exp_nodelay = 0;
		spin_unlock_irq_rcu_node(ssp->srcu_sup);
	}

	if (rcu_seq_state(READ_ONCE(ssp->srcu_sup->srcu_gp_seq)) == SRCU_STATE_SCAN2) {

		 
		idx = 1 ^ (ssp->srcu_idx & 1);
		if (!try_check_zero(ssp, idx, 2)) {
			mutex_unlock(&ssp->srcu_sup->srcu_gp_mutex);
			return;  
		}
		ssp->srcu_sup->srcu_n_exp_nodelay = 0;
		srcu_gp_end(ssp);   
	}
}

 
static void srcu_invoke_callbacks(struct work_struct *work)
{
	long len;
	bool more;
	struct rcu_cblist ready_cbs;
	struct rcu_head *rhp;
	struct srcu_data *sdp;
	struct srcu_struct *ssp;

	sdp = container_of(work, struct srcu_data, work);

	ssp = sdp->ssp;
	rcu_cblist_init(&ready_cbs);
	spin_lock_irq_rcu_node(sdp);
	WARN_ON_ONCE(!rcu_segcblist_segempty(&sdp->srcu_cblist, RCU_NEXT_TAIL));
	rcu_segcblist_advance(&sdp->srcu_cblist,
			      rcu_seq_current(&ssp->srcu_sup->srcu_gp_seq));
	if (sdp->srcu_cblist_invoking ||
	    !rcu_segcblist_ready_cbs(&sdp->srcu_cblist)) {
		spin_unlock_irq_rcu_node(sdp);
		return;   
	}

	 
	sdp->srcu_cblist_invoking = true;
	rcu_segcblist_extract_done_cbs(&sdp->srcu_cblist, &ready_cbs);
	len = ready_cbs.len;
	spin_unlock_irq_rcu_node(sdp);
	rhp = rcu_cblist_dequeue(&ready_cbs);
	for (; rhp != NULL; rhp = rcu_cblist_dequeue(&ready_cbs)) {
		debug_rcu_head_unqueue(rhp);
		local_bh_disable();
		rhp->func(rhp);
		local_bh_enable();
	}
	WARN_ON_ONCE(ready_cbs.len);

	 
	spin_lock_irq_rcu_node(sdp);
	rcu_segcblist_add_len(&sdp->srcu_cblist, -len);
	sdp->srcu_cblist_invoking = false;
	more = rcu_segcblist_ready_cbs(&sdp->srcu_cblist);
	spin_unlock_irq_rcu_node(sdp);
	if (more)
		srcu_schedule_cbs_sdp(sdp, 0);
}

 
static void srcu_reschedule(struct srcu_struct *ssp, unsigned long delay)
{
	bool pushgp = true;

	spin_lock_irq_rcu_node(ssp->srcu_sup);
	if (ULONG_CMP_GE(ssp->srcu_sup->srcu_gp_seq, ssp->srcu_sup->srcu_gp_seq_needed)) {
		if (!WARN_ON_ONCE(rcu_seq_state(ssp->srcu_sup->srcu_gp_seq))) {
			 
			pushgp = false;
		}
	} else if (!rcu_seq_state(ssp->srcu_sup->srcu_gp_seq)) {
		 
		srcu_gp_start(ssp);
	}
	spin_unlock_irq_rcu_node(ssp->srcu_sup);

	if (pushgp)
		queue_delayed_work(rcu_gp_wq, &ssp->srcu_sup->work, delay);
}

 
static void process_srcu(struct work_struct *work)
{
	unsigned long curdelay;
	unsigned long j;
	struct srcu_struct *ssp;
	struct srcu_usage *sup;

	sup = container_of(work, struct srcu_usage, work.work);
	ssp = sup->srcu_ssp;

	srcu_advance_state(ssp);
	curdelay = srcu_get_delay(ssp);
	if (curdelay) {
		WRITE_ONCE(sup->reschedule_count, 0);
	} else {
		j = jiffies;
		if (READ_ONCE(sup->reschedule_jiffies) == j) {
			WRITE_ONCE(sup->reschedule_count, READ_ONCE(sup->reschedule_count) + 1);
			if (READ_ONCE(sup->reschedule_count) > srcu_max_nodelay)
				curdelay = 1;
		} else {
			WRITE_ONCE(sup->reschedule_count, 1);
			WRITE_ONCE(sup->reschedule_jiffies, j);
		}
	}
	srcu_reschedule(ssp, curdelay);
}

void srcutorture_get_gp_data(enum rcutorture_type test_type,
			     struct srcu_struct *ssp, int *flags,
			     unsigned long *gp_seq)
{
	if (test_type != SRCU_FLAVOR)
		return;
	*flags = 0;
	*gp_seq = rcu_seq_current(&ssp->srcu_sup->srcu_gp_seq);
}
EXPORT_SYMBOL_GPL(srcutorture_get_gp_data);

static const char * const srcu_size_state_name[] = {
	"SRCU_SIZE_SMALL",
	"SRCU_SIZE_ALLOC",
	"SRCU_SIZE_WAIT_BARRIER",
	"SRCU_SIZE_WAIT_CALL",
	"SRCU_SIZE_WAIT_CBS1",
	"SRCU_SIZE_WAIT_CBS2",
	"SRCU_SIZE_WAIT_CBS3",
	"SRCU_SIZE_WAIT_CBS4",
	"SRCU_SIZE_BIG",
	"SRCU_SIZE_???",
};

void srcu_torture_stats_print(struct srcu_struct *ssp, char *tt, char *tf)
{
	int cpu;
	int idx;
	unsigned long s0 = 0, s1 = 0;
	int ss_state = READ_ONCE(ssp->srcu_sup->srcu_size_state);
	int ss_state_idx = ss_state;

	idx = ssp->srcu_idx & 0x1;
	if (ss_state < 0 || ss_state >= ARRAY_SIZE(srcu_size_state_name))
		ss_state_idx = ARRAY_SIZE(srcu_size_state_name) - 1;
	pr_alert("%s%s Tree SRCU g%ld state %d (%s)",
		 tt, tf, rcu_seq_current(&ssp->srcu_sup->srcu_gp_seq), ss_state,
		 srcu_size_state_name[ss_state_idx]);
	if (!ssp->sda) {
		
		pr_cont(" No per-CPU srcu_data structures (->sda == NULL).\n");
	} else {
		pr_cont(" per-CPU(idx=%d):", idx);
		for_each_possible_cpu(cpu) {
			unsigned long l0, l1;
			unsigned long u0, u1;
			long c0, c1;
			struct srcu_data *sdp;

			sdp = per_cpu_ptr(ssp->sda, cpu);
			u0 = data_race(atomic_long_read(&sdp->srcu_unlock_count[!idx]));
			u1 = data_race(atomic_long_read(&sdp->srcu_unlock_count[idx]));

			 
			smp_rmb();

			l0 = data_race(atomic_long_read(&sdp->srcu_lock_count[!idx]));
			l1 = data_race(atomic_long_read(&sdp->srcu_lock_count[idx]));

			c0 = l0 - u0;
			c1 = l1 - u1;
			pr_cont(" %d(%ld,%ld %c)",
				cpu, c0, c1,
				"C."[rcu_segcblist_empty(&sdp->srcu_cblist)]);
			s0 += c0;
			s1 += c1;
		}
		pr_cont(" T(%ld,%ld)\n", s0, s1);
	}
	if (SRCU_SIZING_IS_TORTURE())
		srcu_transition_to_big(ssp);
}
EXPORT_SYMBOL_GPL(srcu_torture_stats_print);

static int __init srcu_bootup_announce(void)
{
	pr_info("Hierarchical SRCU implementation.\n");
	if (exp_holdoff != DEFAULT_SRCU_EXP_HOLDOFF)
		pr_info("\tNon-default auto-expedite holdoff of %lu ns.\n", exp_holdoff);
	if (srcu_retry_check_delay != SRCU_DEFAULT_RETRY_CHECK_DELAY)
		pr_info("\tNon-default retry check delay of %lu us.\n", srcu_retry_check_delay);
	if (srcu_max_nodelay != SRCU_DEFAULT_MAX_NODELAY)
		pr_info("\tNon-default max no-delay of %lu.\n", srcu_max_nodelay);
	pr_info("\tMax phase no-delay instances is %lu.\n", srcu_max_nodelay_phase);
	return 0;
}
early_initcall(srcu_bootup_announce);

void __init srcu_init(void)
{
	struct srcu_usage *sup;

	 
	if (SRCU_SIZING_IS(SRCU_SIZING_AUTO)) {
		if (nr_cpu_ids >= big_cpu_lim) {
			convert_to_big = SRCU_SIZING_INIT; 
			pr_info("%s: Setting srcu_struct sizes to big.\n", __func__);
		} else {
			convert_to_big = SRCU_SIZING_NONE | SRCU_SIZING_CONTEND;
			pr_info("%s: Setting srcu_struct sizes based on contention.\n", __func__);
		}
	}

	 
	srcu_init_done = true;
	while (!list_empty(&srcu_boot_list)) {
		sup = list_first_entry(&srcu_boot_list, struct srcu_usage,
				      work.work.entry);
		list_del_init(&sup->work.work.entry);
		if (SRCU_SIZING_IS(SRCU_SIZING_INIT) &&
		    sup->srcu_size_state == SRCU_SIZE_SMALL)
			sup->srcu_size_state = SRCU_SIZE_ALLOC;
		queue_work(rcu_gp_wq, &sup->work.work);
	}
}

#ifdef CONFIG_MODULES

 
static int srcu_module_coming(struct module *mod)
{
	int i;
	struct srcu_struct *ssp;
	struct srcu_struct **sspp = mod->srcu_struct_ptrs;

	for (i = 0; i < mod->num_srcu_structs; i++) {
		ssp = *(sspp++);
		ssp->sda = alloc_percpu(struct srcu_data);
		if (WARN_ON_ONCE(!ssp->sda))
			return -ENOMEM;
	}
	return 0;
}

 
static void srcu_module_going(struct module *mod)
{
	int i;
	struct srcu_struct *ssp;
	struct srcu_struct **sspp = mod->srcu_struct_ptrs;

	for (i = 0; i < mod->num_srcu_structs; i++) {
		ssp = *(sspp++);
		if (!rcu_seq_state(smp_load_acquire(&ssp->srcu_sup->srcu_gp_seq_needed)) &&
		    !WARN_ON_ONCE(!ssp->srcu_sup->sda_is_static))
			cleanup_srcu_struct(ssp);
		if (!WARN_ON(srcu_readers_active(ssp)))
			free_percpu(ssp->sda);
	}
}

 
static int srcu_module_notify(struct notifier_block *self,
			      unsigned long val, void *data)
{
	struct module *mod = data;
	int ret = 0;

	switch (val) {
	case MODULE_STATE_COMING:
		ret = srcu_module_coming(mod);
		break;
	case MODULE_STATE_GOING:
		srcu_module_going(mod);
		break;
	default:
		break;
	}
	return ret;
}

static struct notifier_block srcu_module_nb = {
	.notifier_call = srcu_module_notify,
	.priority = 0,
};

static __init int init_srcu_module_notifier(void)
{
	int ret;

	ret = register_module_notifier(&srcu_module_nb);
	if (ret)
		pr_warn("Failed to register srcu module notifier\n");
	return ret;
}
late_initcall(init_srcu_module_notifier);

#endif  
