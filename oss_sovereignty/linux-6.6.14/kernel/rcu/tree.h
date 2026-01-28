#include <linux/cache.h>
#include <linux/kthread.h>
#include <linux/spinlock.h>
#include <linux/rtmutex.h>
#include <linux/threads.h>
#include <linux/cpumask.h>
#include <linux/seqlock.h>
#include <linux/swait.h>
#include <linux/rcu_node_tree.h>
#include "rcu_segcblist.h"
struct rcu_exp_work {
	unsigned long rew_s;
#ifdef CONFIG_RCU_EXP_KTHREAD
	struct kthread_work rew_work;
#else
	struct work_struct rew_work;
#endif  
};
#define RCU_KTHREAD_STOPPED  0
#define RCU_KTHREAD_RUNNING  1
#define RCU_KTHREAD_WAITING  2
#define RCU_KTHREAD_OFFCPU   3
#define RCU_KTHREAD_YIELDING 4
#define RCU_KTHREAD_MAX      4
struct rcu_node {
	raw_spinlock_t __private lock;	 
	unsigned long gp_seq;	 
	unsigned long gp_seq_needed;  
	unsigned long completedqs;  
	unsigned long qsmask;	 
	unsigned long rcu_gp_init_mask;	 
	unsigned long qsmaskinit;
	unsigned long qsmaskinitnext;
	unsigned long expmask;	 
	unsigned long expmaskinit;
	unsigned long expmaskinitnext;
	unsigned long cbovldmask;
	unsigned long ffmask;	 
	unsigned long grpmask;	 
	int	grplo;		 
	int	grphi;		 
	u8	grpnum;		 
	u8	level;		 
	bool	wait_blkd_tasks; 
	struct rcu_node *parent;
	struct list_head blkd_tasks;
	struct list_head *gp_tasks;
	struct list_head *exp_tasks;
	struct list_head *boost_tasks;
	struct rt_mutex boost_mtx;
	unsigned long boost_time;
	struct mutex boost_kthread_mutex;
	struct task_struct *boost_kthread_task;
	unsigned int boost_kthread_status;
	unsigned long n_boosts;	 
#ifdef CONFIG_RCU_NOCB_CPU
	struct swait_queue_head nocb_gp_wq[2];
#endif  
	raw_spinlock_t fqslock ____cacheline_internodealigned_in_smp;
	spinlock_t exp_lock ____cacheline_internodealigned_in_smp;
	unsigned long exp_seq_rq;
	wait_queue_head_t exp_wq[4];
	struct rcu_exp_work rew;
	bool exp_need_flush;	 
	raw_spinlock_t exp_poll_lock;
	unsigned long exp_seq_poll_rq;
	struct work_struct exp_poll_wq;
} ____cacheline_internodealigned_in_smp;
#define leaf_node_cpu_bit(rnp, cpu) (BIT((cpu) - (rnp)->grplo))
union rcu_noqs {
	struct {
		u8 norm;
		u8 exp;
	} b;  
	u16 s;  
};
struct rcu_snap_record {
	unsigned long	gp_seq;		 
	u64		cputime_irq;	 
	u64		cputime_softirq; 
	u64		cputime_system;  
	unsigned long	nr_hardirqs;	 
	unsigned int	nr_softirqs;	 
	unsigned long long nr_csw;	 
	unsigned long   jiffies;	 
};
struct rcu_data {
	unsigned long	gp_seq;		 
	unsigned long	gp_seq_needed;	 
	union rcu_noqs	cpu_no_qs;	 
	bool		core_needs_qs;	 
	bool		beenonline;	 
	bool		gpwrap;		 
	bool		cpu_started;	 
	struct rcu_node *mynode;	 
	unsigned long grpmask;		 
	unsigned long	ticks_this_gp;	 
	struct irq_work defer_qs_iw;	 
	bool defer_qs_iw_pending;	 
	struct work_struct strict_work;	 
	struct rcu_segcblist cblist;	 
	long		qlen_last_fqs_check;
	unsigned long	n_cbs_invoked;	 
	unsigned long	n_force_qs_snap;
	long		blimit;		 
	int dynticks_snap;		 
	bool rcu_need_heavy_qs;		 
	bool rcu_urgent_qs;		 
	bool rcu_forced_tick;		 
	bool rcu_forced_tick_exp;	 
	unsigned long barrier_seq_snap;	 
	struct rcu_head barrier_head;
	int exp_dynticks_snap;		 
#ifdef CONFIG_RCU_NOCB_CPU
	struct swait_queue_head nocb_cb_wq;  
	struct swait_queue_head nocb_state_wq;  
	struct task_struct *nocb_gp_kthread;
	raw_spinlock_t nocb_lock;	 
	atomic_t nocb_lock_contended;	 
	int nocb_defer_wakeup;		 
	struct timer_list nocb_timer;	 
	unsigned long nocb_gp_adv_time;	 
	struct mutex nocb_gp_kthread_mutex;  
	raw_spinlock_t nocb_bypass_lock ____cacheline_internodealigned_in_smp;
	struct rcu_cblist nocb_bypass;	 
	unsigned long nocb_bypass_first;  
	unsigned long nocb_nobypass_last;  
	int nocb_nobypass_count;	 
	raw_spinlock_t nocb_gp_lock ____cacheline_internodealigned_in_smp;
	u8 nocb_gp_sleep;		 
	u8 nocb_gp_bypass;		 
	u8 nocb_gp_gp;			 
	unsigned long nocb_gp_seq;	 
	unsigned long nocb_gp_loops;	 
	struct swait_queue_head nocb_gp_wq;  
	bool nocb_cb_sleep;		 
	struct task_struct *nocb_cb_kthread;
	struct list_head nocb_head_rdp;  
	struct list_head nocb_entry_rdp;  
	struct rcu_data *nocb_toggling_rdp;  
	struct rcu_data *nocb_gp_rdp ____cacheline_internodealigned_in_smp;
#endif  
	struct task_struct *rcu_cpu_kthread_task;
	unsigned int rcu_cpu_kthread_status;
	char rcu_cpu_has_work;
	unsigned long rcuc_activity;
	unsigned int softirq_snap;	 
	struct irq_work rcu_iw;		 
	bool rcu_iw_pending;		 
	unsigned long rcu_iw_gp_seq;	 
	unsigned long rcu_ofl_gp_seq;	 
	short rcu_ofl_gp_flags;		 
	unsigned long rcu_onl_gp_seq;	 
	short rcu_onl_gp_flags;		 
	unsigned long last_fqs_resched;	 
	unsigned long last_sched_clock;	 
	struct rcu_snap_record snap_record;  
	long lazy_len;			 
	int cpu;
};
#define RCU_NOCB_WAKE_NOT	0
#define RCU_NOCB_WAKE_BYPASS	1
#define RCU_NOCB_WAKE_LAZY	2
#define RCU_NOCB_WAKE		3
#define RCU_NOCB_WAKE_FORCE	4
#define RCU_JIFFIES_TILL_FORCE_QS (1 + (HZ > 250) + (HZ > 500))
#define RCU_JIFFIES_FQS_DIV	256	 
#define RCU_STALL_RAT_DELAY	2	 
#define rcu_wait(cond)							\
do {									\
	for (;;) {							\
		set_current_state(TASK_INTERRUPTIBLE);			\
		if (cond)						\
			break;						\
		schedule();						\
	}								\
	__set_current_state(TASK_RUNNING);				\
} while (0)
struct rcu_state {
	struct rcu_node node[NUM_RCU_NODES];	 
	struct rcu_node *level[RCU_NUM_LVLS + 1];
	int ncpus;				 
	int n_online_cpus;			 
	unsigned long gp_seq ____cacheline_internodealigned_in_smp;
	unsigned long gp_max;			 
	struct task_struct *gp_kthread;		 
	struct swait_queue_head gp_wq;		 
	short gp_flags;				 
	short gp_state;				 
	unsigned long gp_wake_time;		 
	unsigned long gp_wake_seq;		 
	unsigned long gp_seq_polled;		 
	unsigned long gp_seq_polled_snap;	 
	unsigned long gp_seq_polled_exp_snap;	 
	struct mutex barrier_mutex;		 
	atomic_t barrier_cpu_count;		 
	struct completion barrier_completion;	 
	unsigned long barrier_sequence;		 
	raw_spinlock_t barrier_lock;		 
	struct mutex exp_mutex;			 
	struct mutex exp_wake_mutex;		 
	unsigned long expedited_sequence;	 
	atomic_t expedited_need_qs;		 
	struct swait_queue_head expedited_wq;	 
	int ncpus_snap;				 
	u8 cbovld;				 
	u8 cbovldnext;				 
	unsigned long jiffies_force_qs;		 
	unsigned long jiffies_kick_kthreads;	 
	unsigned long n_force_qs;		 
	unsigned long gp_start;			 
	unsigned long gp_end;			 
	unsigned long gp_activity;		 
	unsigned long gp_req_activity;		 
	unsigned long jiffies_stall;		 
	int nr_fqs_jiffies_stall;		 
	unsigned long jiffies_resched;		 
	unsigned long n_force_qs_gpstart;	 
	const char *name;			 
	char abbr;				 
	arch_spinlock_t ofl_lock ____cacheline_internodealigned_in_smp;
	int nocb_is_setup;			 
};
#define RCU_GP_FLAG_INIT 0x1	 
#define RCU_GP_FLAG_FQS  0x2	 
#define RCU_GP_FLAG_OVLD 0x4	 
#define RCU_GP_IDLE	 0	 
#define RCU_GP_WAIT_GPS  1	 
#define RCU_GP_DONE_GPS  2	 
#define RCU_GP_ONOFF     3	 
#define RCU_GP_INIT      4	 
#define RCU_GP_WAIT_FQS  5	 
#define RCU_GP_DOING_FQS 6	 
#define RCU_GP_CLEANUP   7	 
#define RCU_GP_CLEANED   8	 
#ifdef CONFIG_PREEMPT_RCU
#define RCU_ABBR 'p'
#define RCU_NAME_RAW "rcu_preempt"
#else  
#define RCU_ABBR 's'
#define RCU_NAME_RAW "rcu_sched"
#endif  
#ifndef CONFIG_TRACING
#define RCU_NAME RCU_NAME_RAW
#else  
static char rcu_name[] = RCU_NAME_RAW;
static const char *tp_rcu_varname __used __tracepoint_string = rcu_name;
#define RCU_NAME rcu_name
#endif  
static void rcu_bootup_announce(void);
static void rcu_qs(void);
static int rcu_preempt_blocked_readers_cgp(struct rcu_node *rnp);
#ifdef CONFIG_HOTPLUG_CPU
static bool rcu_preempt_has_tasks(struct rcu_node *rnp);
#endif  
static int rcu_print_task_exp_stall(struct rcu_node *rnp);
static void rcu_preempt_check_blocked_tasks(struct rcu_node *rnp);
static void rcu_flavor_sched_clock_irq(int user);
static void dump_blkd_tasks(struct rcu_node *rnp, int ncheck);
static void rcu_initiate_boost(struct rcu_node *rnp, unsigned long flags);
static void rcu_preempt_boost_start_gp(struct rcu_node *rnp);
static bool rcu_is_callbacks_kthread(struct rcu_data *rdp);
static void rcu_cpu_kthread_setup(unsigned int cpu);
static void rcu_spawn_one_boost_kthread(struct rcu_node *rnp);
static bool rcu_preempt_has_tasks(struct rcu_node *rnp);
static bool rcu_preempt_need_deferred_qs(struct task_struct *t);
static void zero_cpu_stall_ticks(struct rcu_data *rdp);
static struct swait_queue_head *rcu_nocb_gp_get(struct rcu_node *rnp);
static void rcu_nocb_gp_cleanup(struct swait_queue_head *sq);
static void rcu_init_one_nocb(struct rcu_node *rnp);
static bool wake_nocb_gp(struct rcu_data *rdp, bool force);
static bool rcu_nocb_flush_bypass(struct rcu_data *rdp, struct rcu_head *rhp,
				  unsigned long j, bool lazy);
static bool rcu_nocb_try_bypass(struct rcu_data *rdp, struct rcu_head *rhp,
				bool *was_alldone, unsigned long flags,
				bool lazy);
static void __call_rcu_nocb_wake(struct rcu_data *rdp, bool was_empty,
				 unsigned long flags);
static int rcu_nocb_need_deferred_wakeup(struct rcu_data *rdp, int level);
static bool do_nocb_deferred_wakeup(struct rcu_data *rdp);
static void rcu_boot_init_nocb_percpu_data(struct rcu_data *rdp);
static void rcu_spawn_cpu_nocb_kthread(int cpu);
static void show_rcu_nocb_state(struct rcu_data *rdp);
static void rcu_nocb_lock(struct rcu_data *rdp);
static void rcu_nocb_unlock(struct rcu_data *rdp);
static void rcu_nocb_unlock_irqrestore(struct rcu_data *rdp,
				       unsigned long flags);
static void rcu_lockdep_assert_cblist_protected(struct rcu_data *rdp);
#ifdef CONFIG_RCU_NOCB_CPU
static void __init rcu_organize_nocb_kthreads(void);
#define rcu_nocb_lock_irqsave(rdp, flags)			\
do {								\
	local_irq_save(flags);					\
	if (rcu_segcblist_is_offloaded(&(rdp)->cblist))	\
		raw_spin_lock(&(rdp)->nocb_lock);		\
} while (0)
#else  
#define rcu_nocb_lock_irqsave(rdp, flags) local_irq_save(flags)
#endif  
static void rcu_bind_gp_kthread(void);
static bool rcu_nohz_full_cpu(void);
static void record_gp_stall_check_time(void);
static void rcu_iw_handler(struct irq_work *iwp);
static void check_cpu_stall(struct rcu_data *rdp);
static void rcu_check_gp_start_stall(struct rcu_node *rnp, struct rcu_data *rdp,
				     const unsigned long gpssdelay);
static void sync_rcu_do_polled_gp(struct work_struct *wp);
