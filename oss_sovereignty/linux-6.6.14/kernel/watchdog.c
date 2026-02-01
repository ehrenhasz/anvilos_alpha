
 

#define pr_fmt(fmt) "watchdog: " fmt

#include <linux/mm.h>
#include <linux/cpu.h>
#include <linux/nmi.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sysctl.h>
#include <linux/tick.h>
#include <linux/sched/clock.h>
#include <linux/sched/debug.h>
#include <linux/sched/isolation.h>
#include <linux/stop_machine.h>

#include <asm/irq_regs.h>
#include <linux/kvm_para.h>

static DEFINE_MUTEX(watchdog_mutex);

#if defined(CONFIG_HARDLOCKUP_DETECTOR) || defined(CONFIG_HARDLOCKUP_DETECTOR_SPARC64)
# define WATCHDOG_HARDLOCKUP_DEFAULT	1
#else
# define WATCHDOG_HARDLOCKUP_DEFAULT	0
#endif

unsigned long __read_mostly watchdog_enabled;
int __read_mostly watchdog_user_enabled = 1;
static int __read_mostly watchdog_hardlockup_user_enabled = WATCHDOG_HARDLOCKUP_DEFAULT;
static int __read_mostly watchdog_softlockup_user_enabled = 1;
int __read_mostly watchdog_thresh = 10;
static int __read_mostly watchdog_hardlockup_available;

struct cpumask watchdog_cpumask __read_mostly;
unsigned long *watchdog_cpumask_bits = cpumask_bits(&watchdog_cpumask);

#ifdef CONFIG_HARDLOCKUP_DETECTOR

# ifdef CONFIG_SMP
int __read_mostly sysctl_hardlockup_all_cpu_backtrace;
# endif  

 
unsigned int __read_mostly hardlockup_panic =
			IS_ENABLED(CONFIG_BOOTPARAM_HARDLOCKUP_PANIC);
 
void __init hardlockup_detector_disable(void)
{
	watchdog_hardlockup_user_enabled = 0;
}

static int __init hardlockup_panic_setup(char *str)
{
	if (!strncmp(str, "panic", 5))
		hardlockup_panic = 1;
	else if (!strncmp(str, "nopanic", 7))
		hardlockup_panic = 0;
	else if (!strncmp(str, "0", 1))
		watchdog_hardlockup_user_enabled = 0;
	else if (!strncmp(str, "1", 1))
		watchdog_hardlockup_user_enabled = 1;
	return 1;
}
__setup("nmi_watchdog=", hardlockup_panic_setup);

#endif  

#if defined(CONFIG_HARDLOCKUP_DETECTOR_COUNTS_HRTIMER)

static DEFINE_PER_CPU(atomic_t, hrtimer_interrupts);
static DEFINE_PER_CPU(int, hrtimer_interrupts_saved);
static DEFINE_PER_CPU(bool, watchdog_hardlockup_warned);
static DEFINE_PER_CPU(bool, watchdog_hardlockup_touched);
static unsigned long watchdog_hardlockup_all_cpu_dumped;

notrace void arch_touch_nmi_watchdog(void)
{
	 
	raw_cpu_write(watchdog_hardlockup_touched, true);
}
EXPORT_SYMBOL(arch_touch_nmi_watchdog);

void watchdog_hardlockup_touch_cpu(unsigned int cpu)
{
	per_cpu(watchdog_hardlockup_touched, cpu) = true;
}

static bool is_hardlockup(unsigned int cpu)
{
	int hrint = atomic_read(&per_cpu(hrtimer_interrupts, cpu));

	if (per_cpu(hrtimer_interrupts_saved, cpu) == hrint)
		return true;

	 
	per_cpu(hrtimer_interrupts_saved, cpu) = hrint;

	return false;
}

static void watchdog_hardlockup_kick(void)
{
	int new_interrupts;

	new_interrupts = atomic_inc_return(this_cpu_ptr(&hrtimer_interrupts));
	watchdog_buddy_check_hardlockup(new_interrupts);
}

void watchdog_hardlockup_check(unsigned int cpu, struct pt_regs *regs)
{
	if (per_cpu(watchdog_hardlockup_touched, cpu)) {
		per_cpu(watchdog_hardlockup_touched, cpu) = false;
		return;
	}

	 
	if (is_hardlockup(cpu)) {
		unsigned int this_cpu = smp_processor_id();

		 
		if (per_cpu(watchdog_hardlockup_warned, cpu))
			return;

		pr_emerg("Watchdog detected hard LOCKUP on cpu %d\n", cpu);
		print_modules();
		print_irqtrace_events(current);
		if (cpu == this_cpu) {
			if (regs)
				show_regs(regs);
			else
				dump_stack();
		} else {
			trigger_single_cpu_backtrace(cpu);
		}

		 
		if (sysctl_hardlockup_all_cpu_backtrace &&
		    !test_and_set_bit(0, &watchdog_hardlockup_all_cpu_dumped))
			trigger_allbutcpu_cpu_backtrace(cpu);

		if (hardlockup_panic)
			nmi_panic(regs, "Hard LOCKUP");

		per_cpu(watchdog_hardlockup_warned, cpu) = true;
	} else {
		per_cpu(watchdog_hardlockup_warned, cpu) = false;
	}
}

#else  

static inline void watchdog_hardlockup_kick(void) { }

#endif  

 
void __weak watchdog_hardlockup_enable(unsigned int cpu) { }

void __weak watchdog_hardlockup_disable(unsigned int cpu) { }

 
int __weak __init watchdog_hardlockup_probe(void)
{
	return -ENODEV;
}

 
void __weak watchdog_hardlockup_stop(void) { }

 
void __weak watchdog_hardlockup_start(void) { }

 
static void lockup_detector_update_enable(void)
{
	watchdog_enabled = 0;
	if (!watchdog_user_enabled)
		return;
	if (watchdog_hardlockup_available && watchdog_hardlockup_user_enabled)
		watchdog_enabled |= WATCHDOG_HARDLOCKUP_ENABLED;
	if (watchdog_softlockup_user_enabled)
		watchdog_enabled |= WATCHDOG_SOFTOCKUP_ENABLED;
}

#ifdef CONFIG_SOFTLOCKUP_DETECTOR

 
#define SOFTLOCKUP_DELAY_REPORT	ULONG_MAX

#ifdef CONFIG_SMP
int __read_mostly sysctl_softlockup_all_cpu_backtrace;
#endif

static struct cpumask watchdog_allowed_mask __read_mostly;

 
unsigned int __read_mostly softlockup_panic =
			IS_ENABLED(CONFIG_BOOTPARAM_SOFTLOCKUP_PANIC);

static bool softlockup_initialized __read_mostly;
static u64 __read_mostly sample_period;

 
static DEFINE_PER_CPU(unsigned long, watchdog_touch_ts);
 
static DEFINE_PER_CPU(unsigned long, watchdog_report_ts);
static DEFINE_PER_CPU(struct hrtimer, watchdog_hrtimer);
static DEFINE_PER_CPU(bool, softlockup_touch_sync);
static unsigned long soft_lockup_nmi_warn;

static int __init softlockup_panic_setup(char *str)
{
	softlockup_panic = simple_strtoul(str, NULL, 0);
	return 1;
}
__setup("softlockup_panic=", softlockup_panic_setup);

static int __init nowatchdog_setup(char *str)
{
	watchdog_user_enabled = 0;
	return 1;
}
__setup("nowatchdog", nowatchdog_setup);

static int __init nosoftlockup_setup(char *str)
{
	watchdog_softlockup_user_enabled = 0;
	return 1;
}
__setup("nosoftlockup", nosoftlockup_setup);

static int __init watchdog_thresh_setup(char *str)
{
	get_option(&str, &watchdog_thresh);
	return 1;
}
__setup("watchdog_thresh=", watchdog_thresh_setup);

static void __lockup_detector_cleanup(void);

 
static int get_softlockup_thresh(void)
{
	return watchdog_thresh * 2;
}

 
static unsigned long get_timestamp(void)
{
	return running_clock() >> 30LL;   
}

static void set_sample_period(void)
{
	 
	sample_period = get_softlockup_thresh() * ((u64)NSEC_PER_SEC / 5);
	watchdog_update_hrtimer_threshold(sample_period);
}

static void update_report_ts(void)
{
	__this_cpu_write(watchdog_report_ts, get_timestamp());
}

 
static void update_touch_ts(void)
{
	__this_cpu_write(watchdog_touch_ts, get_timestamp());
	update_report_ts();
}

 
notrace void touch_softlockup_watchdog_sched(void)
{
	 
	raw_cpu_write(watchdog_report_ts, SOFTLOCKUP_DELAY_REPORT);
}

notrace void touch_softlockup_watchdog(void)
{
	touch_softlockup_watchdog_sched();
	wq_watchdog_touch(raw_smp_processor_id());
}
EXPORT_SYMBOL(touch_softlockup_watchdog);

void touch_all_softlockup_watchdogs(void)
{
	int cpu;

	 
	for_each_cpu(cpu, &watchdog_allowed_mask) {
		per_cpu(watchdog_report_ts, cpu) = SOFTLOCKUP_DELAY_REPORT;
		wq_watchdog_touch(cpu);
	}
}

void touch_softlockup_watchdog_sync(void)
{
	__this_cpu_write(softlockup_touch_sync, true);
	__this_cpu_write(watchdog_report_ts, SOFTLOCKUP_DELAY_REPORT);
}

static int is_softlockup(unsigned long touch_ts,
			 unsigned long period_ts,
			 unsigned long now)
{
	if ((watchdog_enabled & WATCHDOG_SOFTOCKUP_ENABLED) && watchdog_thresh) {
		 
		if (time_after(now, period_ts + get_softlockup_thresh()))
			return now - touch_ts;
	}
	return 0;
}

 
static DEFINE_PER_CPU(struct completion, softlockup_completion);
static DEFINE_PER_CPU(struct cpu_stop_work, softlockup_stop_work);

 
static int softlockup_fn(void *data)
{
	update_touch_ts();
	complete(this_cpu_ptr(&softlockup_completion));

	return 0;
}

 
static enum hrtimer_restart watchdog_timer_fn(struct hrtimer *hrtimer)
{
	unsigned long touch_ts, period_ts, now;
	struct pt_regs *regs = get_irq_regs();
	int duration;
	int softlockup_all_cpu_backtrace = sysctl_softlockup_all_cpu_backtrace;

	if (!watchdog_enabled)
		return HRTIMER_NORESTART;

	watchdog_hardlockup_kick();

	 
	if (completion_done(this_cpu_ptr(&softlockup_completion))) {
		reinit_completion(this_cpu_ptr(&softlockup_completion));
		stop_one_cpu_nowait(smp_processor_id(),
				softlockup_fn, NULL,
				this_cpu_ptr(&softlockup_stop_work));
	}

	 
	hrtimer_forward_now(hrtimer, ns_to_ktime(sample_period));

	 
	now = get_timestamp();
	 
	kvm_check_and_clear_guest_paused();
	 
	period_ts = READ_ONCE(*this_cpu_ptr(&watchdog_report_ts));

	 
	if (period_ts == SOFTLOCKUP_DELAY_REPORT) {
		if (unlikely(__this_cpu_read(softlockup_touch_sync))) {
			 
			__this_cpu_write(softlockup_touch_sync, false);
			sched_clock_tick();
		}

		update_report_ts();
		return HRTIMER_RESTART;
	}

	 
	touch_ts = __this_cpu_read(watchdog_touch_ts);
	duration = is_softlockup(touch_ts, period_ts, now);
	if (unlikely(duration)) {
		 
		if (softlockup_all_cpu_backtrace) {
			if (test_and_set_bit_lock(0, &soft_lockup_nmi_warn))
				return HRTIMER_RESTART;
		}

		 
		update_report_ts();

		pr_emerg("BUG: soft lockup - CPU#%d stuck for %us! [%s:%d]\n",
			smp_processor_id(), duration,
			current->comm, task_pid_nr(current));
		print_modules();
		print_irqtrace_events(current);
		if (regs)
			show_regs(regs);
		else
			dump_stack();

		if (softlockup_all_cpu_backtrace) {
			trigger_allbutcpu_cpu_backtrace(smp_processor_id());
			clear_bit_unlock(0, &soft_lockup_nmi_warn);
		}

		add_taint(TAINT_SOFTLOCKUP, LOCKDEP_STILL_OK);
		if (softlockup_panic)
			panic("softlockup: hung tasks");
	}

	return HRTIMER_RESTART;
}

static void watchdog_enable(unsigned int cpu)
{
	struct hrtimer *hrtimer = this_cpu_ptr(&watchdog_hrtimer);
	struct completion *done = this_cpu_ptr(&softlockup_completion);

	WARN_ON_ONCE(cpu != smp_processor_id());

	init_completion(done);
	complete(done);

	 
	hrtimer_init(hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_HARD);
	hrtimer->function = watchdog_timer_fn;
	hrtimer_start(hrtimer, ns_to_ktime(sample_period),
		      HRTIMER_MODE_REL_PINNED_HARD);

	 
	update_touch_ts();
	 
	if (watchdog_enabled & WATCHDOG_HARDLOCKUP_ENABLED)
		watchdog_hardlockup_enable(cpu);
}

static void watchdog_disable(unsigned int cpu)
{
	struct hrtimer *hrtimer = this_cpu_ptr(&watchdog_hrtimer);

	WARN_ON_ONCE(cpu != smp_processor_id());

	 
	watchdog_hardlockup_disable(cpu);
	hrtimer_cancel(hrtimer);
	wait_for_completion(this_cpu_ptr(&softlockup_completion));
}

static int softlockup_stop_fn(void *data)
{
	watchdog_disable(smp_processor_id());
	return 0;
}

static void softlockup_stop_all(void)
{
	int cpu;

	if (!softlockup_initialized)
		return;

	for_each_cpu(cpu, &watchdog_allowed_mask)
		smp_call_on_cpu(cpu, softlockup_stop_fn, NULL, false);

	cpumask_clear(&watchdog_allowed_mask);
}

static int softlockup_start_fn(void *data)
{
	watchdog_enable(smp_processor_id());
	return 0;
}

static void softlockup_start_all(void)
{
	int cpu;

	cpumask_copy(&watchdog_allowed_mask, &watchdog_cpumask);
	for_each_cpu(cpu, &watchdog_allowed_mask)
		smp_call_on_cpu(cpu, softlockup_start_fn, NULL, false);
}

int lockup_detector_online_cpu(unsigned int cpu)
{
	if (cpumask_test_cpu(cpu, &watchdog_allowed_mask))
		watchdog_enable(cpu);
	return 0;
}

int lockup_detector_offline_cpu(unsigned int cpu)
{
	if (cpumask_test_cpu(cpu, &watchdog_allowed_mask))
		watchdog_disable(cpu);
	return 0;
}

static void __lockup_detector_reconfigure(void)
{
	cpus_read_lock();
	watchdog_hardlockup_stop();

	softlockup_stop_all();
	set_sample_period();
	lockup_detector_update_enable();
	if (watchdog_enabled && watchdog_thresh)
		softlockup_start_all();

	watchdog_hardlockup_start();
	cpus_read_unlock();
	 
	__lockup_detector_cleanup();
}

void lockup_detector_reconfigure(void)
{
	mutex_lock(&watchdog_mutex);
	__lockup_detector_reconfigure();
	mutex_unlock(&watchdog_mutex);
}

 
static __init void lockup_detector_setup(void)
{
	 
	lockup_detector_update_enable();

	if (!IS_ENABLED(CONFIG_SYSCTL) &&
	    !(watchdog_enabled && watchdog_thresh))
		return;

	mutex_lock(&watchdog_mutex);
	__lockup_detector_reconfigure();
	softlockup_initialized = true;
	mutex_unlock(&watchdog_mutex);
}

#else  
static void __lockup_detector_reconfigure(void)
{
	cpus_read_lock();
	watchdog_hardlockup_stop();
	lockup_detector_update_enable();
	watchdog_hardlockup_start();
	cpus_read_unlock();
}
void lockup_detector_reconfigure(void)
{
	__lockup_detector_reconfigure();
}
static inline void lockup_detector_setup(void)
{
	__lockup_detector_reconfigure();
}
#endif  

static void __lockup_detector_cleanup(void)
{
	lockdep_assert_held(&watchdog_mutex);
	hardlockup_detector_perf_cleanup();
}

 
void lockup_detector_cleanup(void)
{
	mutex_lock(&watchdog_mutex);
	__lockup_detector_cleanup();
	mutex_unlock(&watchdog_mutex);
}

 
void lockup_detector_soft_poweroff(void)
{
	watchdog_enabled = 0;
}

#ifdef CONFIG_SYSCTL

 
static void proc_watchdog_update(void)
{
	 
	cpumask_and(&watchdog_cpumask, &watchdog_cpumask, cpu_possible_mask);
	__lockup_detector_reconfigure();
}

 
static int proc_watchdog_common(int which, struct ctl_table *table, int write,
				void *buffer, size_t *lenp, loff_t *ppos)
{
	int err, old, *param = table->data;

	mutex_lock(&watchdog_mutex);

	if (!write) {
		 
		*param = (watchdog_enabled & which) != 0;
		err = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
	} else {
		old = READ_ONCE(*param);
		err = proc_dointvec_minmax(table, write, buffer, lenp, ppos);
		if (!err && old != READ_ONCE(*param))
			proc_watchdog_update();
	}
	mutex_unlock(&watchdog_mutex);
	return err;
}

 
int proc_watchdog(struct ctl_table *table, int write,
		  void *buffer, size_t *lenp, loff_t *ppos)
{
	return proc_watchdog_common(WATCHDOG_HARDLOCKUP_ENABLED |
				    WATCHDOG_SOFTOCKUP_ENABLED,
				    table, write, buffer, lenp, ppos);
}

 
int proc_nmi_watchdog(struct ctl_table *table, int write,
		      void *buffer, size_t *lenp, loff_t *ppos)
{
	if (!watchdog_hardlockup_available && write)
		return -ENOTSUPP;
	return proc_watchdog_common(WATCHDOG_HARDLOCKUP_ENABLED,
				    table, write, buffer, lenp, ppos);
}

 
int proc_soft_watchdog(struct ctl_table *table, int write,
			void *buffer, size_t *lenp, loff_t *ppos)
{
	return proc_watchdog_common(WATCHDOG_SOFTOCKUP_ENABLED,
				    table, write, buffer, lenp, ppos);
}

 
int proc_watchdog_thresh(struct ctl_table *table, int write,
			 void *buffer, size_t *lenp, loff_t *ppos)
{
	int err, old;

	mutex_lock(&watchdog_mutex);

	old = READ_ONCE(watchdog_thresh);
	err = proc_dointvec_minmax(table, write, buffer, lenp, ppos);

	if (!err && write && old != READ_ONCE(watchdog_thresh))
		proc_watchdog_update();

	mutex_unlock(&watchdog_mutex);
	return err;
}

 
int proc_watchdog_cpumask(struct ctl_table *table, int write,
			  void *buffer, size_t *lenp, loff_t *ppos)
{
	int err;

	mutex_lock(&watchdog_mutex);

	err = proc_do_large_bitmap(table, write, buffer, lenp, ppos);
	if (!err && write)
		proc_watchdog_update();

	mutex_unlock(&watchdog_mutex);
	return err;
}

static const int sixty = 60;

static struct ctl_table watchdog_sysctls[] = {
	{
		.procname       = "watchdog",
		.data		= &watchdog_user_enabled,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler   = proc_watchdog,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "watchdog_thresh",
		.data		= &watchdog_thresh,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_watchdog_thresh,
		.extra1		= SYSCTL_ZERO,
		.extra2		= (void *)&sixty,
	},
	{
		.procname	= "watchdog_cpumask",
		.data		= &watchdog_cpumask_bits,
		.maxlen		= NR_CPUS,
		.mode		= 0644,
		.proc_handler	= proc_watchdog_cpumask,
	},
#ifdef CONFIG_SOFTLOCKUP_DETECTOR
	{
		.procname       = "soft_watchdog",
		.data		= &watchdog_softlockup_user_enabled,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler   = proc_soft_watchdog,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{
		.procname	= "softlockup_panic",
		.data		= &softlockup_panic,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#ifdef CONFIG_SMP
	{
		.procname	= "softlockup_all_cpu_backtrace",
		.data		= &sysctl_softlockup_all_cpu_backtrace,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#endif  
#endif
#ifdef CONFIG_HARDLOCKUP_DETECTOR
	{
		.procname	= "hardlockup_panic",
		.data		= &hardlockup_panic,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#ifdef CONFIG_SMP
	{
		.procname	= "hardlockup_all_cpu_backtrace",
		.data		= &sysctl_hardlockup_all_cpu_backtrace,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= proc_dointvec_minmax,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
#endif  
#endif
	{}
};

static struct ctl_table watchdog_hardlockup_sysctl[] = {
	{
		.procname       = "nmi_watchdog",
		.data		= &watchdog_hardlockup_user_enabled,
		.maxlen		= sizeof(int),
		.mode		= 0444,
		.proc_handler   = proc_nmi_watchdog,
		.extra1		= SYSCTL_ZERO,
		.extra2		= SYSCTL_ONE,
	},
	{}
};

static void __init watchdog_sysctl_init(void)
{
	register_sysctl_init("kernel", watchdog_sysctls);

	if (watchdog_hardlockup_available)
		watchdog_hardlockup_sysctl[0].mode = 0644;
	register_sysctl_init("kernel", watchdog_hardlockup_sysctl);
}

#else
#define watchdog_sysctl_init() do { } while (0)
#endif  

static void __init lockup_detector_delay_init(struct work_struct *work);
static bool allow_lockup_detector_init_retry __initdata;

static struct work_struct detector_work __initdata =
		__WORK_INITIALIZER(detector_work, lockup_detector_delay_init);

static void __init lockup_detector_delay_init(struct work_struct *work)
{
	int ret;

	ret = watchdog_hardlockup_probe();
	if (ret) {
		pr_info("Delayed init of the lockup detector failed: %d\n", ret);
		pr_info("Hard watchdog permanently disabled\n");
		return;
	}

	allow_lockup_detector_init_retry = false;

	watchdog_hardlockup_available = true;
	lockup_detector_setup();
}

 
void __init lockup_detector_retry_init(void)
{
	 
	if (!allow_lockup_detector_init_retry)
		return;

	schedule_work(&detector_work);
}

 
static int __init lockup_detector_check(void)
{
	 
	allow_lockup_detector_init_retry = false;

	 
	flush_work(&detector_work);

	watchdog_sysctl_init();

	return 0;

}
late_initcall_sync(lockup_detector_check);

void __init lockup_detector_init(void)
{
	if (tick_nohz_full_enabled())
		pr_info("Disabling watchdog on nohz_full cores by default\n");

	cpumask_copy(&watchdog_cpumask,
		     housekeeping_cpumask(HK_TYPE_TIMER));

	if (!watchdog_hardlockup_probe())
		watchdog_hardlockup_available = true;
	else
		allow_lockup_detector_init_retry = true;

	lockup_detector_setup();
}
