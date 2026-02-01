
 
#define pr_fmt(fmt) "ii_dev: " fmt

#include <linux/cpu.h>
#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/smpboot.h>
#include <linux/idle_inject.h>

#include <uapi/linux/sched/types.h>

 
struct idle_inject_thread {
	struct task_struct *tsk;
	int should_run;
};

 
struct idle_inject_device {
	struct hrtimer timer;
	unsigned int idle_duration_us;
	unsigned int run_duration_us;
	unsigned int latency_us;
	bool (*update)(void);
	unsigned long cpumask[];
};

static DEFINE_PER_CPU(struct idle_inject_thread, idle_inject_thread);
static DEFINE_PER_CPU(struct idle_inject_device *, idle_inject_device);

 
static void idle_inject_wakeup(struct idle_inject_device *ii_dev)
{
	struct idle_inject_thread *iit;
	unsigned int cpu;

	for_each_cpu_and(cpu, to_cpumask(ii_dev->cpumask), cpu_online_mask) {
		iit = per_cpu_ptr(&idle_inject_thread, cpu);
		iit->should_run = 1;
		wake_up_process(iit->tsk);
	}
}

 
static enum hrtimer_restart idle_inject_timer_fn(struct hrtimer *timer)
{
	unsigned int duration_us;
	struct idle_inject_device *ii_dev =
		container_of(timer, struct idle_inject_device, timer);

	if (!ii_dev->update || (ii_dev->update && ii_dev->update()))
		idle_inject_wakeup(ii_dev);

	duration_us = READ_ONCE(ii_dev->run_duration_us);
	duration_us += READ_ONCE(ii_dev->idle_duration_us);

	hrtimer_forward_now(timer, ns_to_ktime(duration_us * NSEC_PER_USEC));

	return HRTIMER_RESTART;
}

 
static void idle_inject_fn(unsigned int cpu)
{
	struct idle_inject_device *ii_dev;
	struct idle_inject_thread *iit;

	ii_dev = per_cpu(idle_inject_device, cpu);
	iit = per_cpu_ptr(&idle_inject_thread, cpu);

	 
	iit->should_run = 0;

	play_idle_precise(READ_ONCE(ii_dev->idle_duration_us) * NSEC_PER_USEC,
			  READ_ONCE(ii_dev->latency_us) * NSEC_PER_USEC);
}

 
void idle_inject_set_duration(struct idle_inject_device *ii_dev,
			      unsigned int run_duration_us,
			      unsigned int idle_duration_us)
{
	if (run_duration_us + idle_duration_us) {
		WRITE_ONCE(ii_dev->run_duration_us, run_duration_us);
		WRITE_ONCE(ii_dev->idle_duration_us, idle_duration_us);
	}
	if (!run_duration_us)
		pr_debug("CPU is forced to 100 percent idle\n");
}
EXPORT_SYMBOL_NS_GPL(idle_inject_set_duration, IDLE_INJECT);

 
void idle_inject_get_duration(struct idle_inject_device *ii_dev,
			      unsigned int *run_duration_us,
			      unsigned int *idle_duration_us)
{
	*run_duration_us = READ_ONCE(ii_dev->run_duration_us);
	*idle_duration_us = READ_ONCE(ii_dev->idle_duration_us);
}
EXPORT_SYMBOL_NS_GPL(idle_inject_get_duration, IDLE_INJECT);

 
void idle_inject_set_latency(struct idle_inject_device *ii_dev,
			     unsigned int latency_us)
{
	WRITE_ONCE(ii_dev->latency_us, latency_us);
}
EXPORT_SYMBOL_NS_GPL(idle_inject_set_latency, IDLE_INJECT);

 
int idle_inject_start(struct idle_inject_device *ii_dev)
{
	unsigned int idle_duration_us = READ_ONCE(ii_dev->idle_duration_us);
	unsigned int run_duration_us = READ_ONCE(ii_dev->run_duration_us);

	if (!(idle_duration_us + run_duration_us))
		return -EINVAL;

	pr_debug("Starting injecting idle cycles on CPUs '%*pbl'\n",
		 cpumask_pr_args(to_cpumask(ii_dev->cpumask)));

	idle_inject_wakeup(ii_dev);

	hrtimer_start(&ii_dev->timer,
		      ns_to_ktime((idle_duration_us + run_duration_us) *
				  NSEC_PER_USEC),
		      HRTIMER_MODE_REL);

	return 0;
}
EXPORT_SYMBOL_NS_GPL(idle_inject_start, IDLE_INJECT);

 
void idle_inject_stop(struct idle_inject_device *ii_dev)
{
	struct idle_inject_thread *iit;
	unsigned int cpu;

	pr_debug("Stopping idle injection on CPUs '%*pbl'\n",
		 cpumask_pr_args(to_cpumask(ii_dev->cpumask)));

	hrtimer_cancel(&ii_dev->timer);

	 
	cpu_hotplug_disable();

	 
	for_each_cpu(cpu, to_cpumask(ii_dev->cpumask)) {
		iit = per_cpu_ptr(&idle_inject_thread, cpu);
		iit->should_run = 0;

		wait_task_inactive(iit->tsk, TASK_ANY);
	}

	cpu_hotplug_enable();
}
EXPORT_SYMBOL_NS_GPL(idle_inject_stop, IDLE_INJECT);

 
static void idle_inject_setup(unsigned int cpu)
{
	sched_set_fifo(current);
}

 
static int idle_inject_should_run(unsigned int cpu)
{
	struct idle_inject_thread *iit =
		per_cpu_ptr(&idle_inject_thread, cpu);

	return iit->should_run;
}

 

struct idle_inject_device *idle_inject_register_full(struct cpumask *cpumask,
						     bool (*update)(void))
{
	struct idle_inject_device *ii_dev;
	int cpu, cpu_rb;

	ii_dev = kzalloc(sizeof(*ii_dev) + cpumask_size(), GFP_KERNEL);
	if (!ii_dev)
		return NULL;

	cpumask_copy(to_cpumask(ii_dev->cpumask), cpumask);
	hrtimer_init(&ii_dev->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	ii_dev->timer.function = idle_inject_timer_fn;
	ii_dev->latency_us = UINT_MAX;
	ii_dev->update = update;

	for_each_cpu(cpu, to_cpumask(ii_dev->cpumask)) {

		if (per_cpu(idle_inject_device, cpu)) {
			pr_err("cpu%d is already registered\n", cpu);
			goto out_rollback;
		}

		per_cpu(idle_inject_device, cpu) = ii_dev;
	}

	return ii_dev;

out_rollback:
	for_each_cpu(cpu_rb, to_cpumask(ii_dev->cpumask)) {
		if (cpu == cpu_rb)
			break;
		per_cpu(idle_inject_device, cpu_rb) = NULL;
	}

	kfree(ii_dev);

	return NULL;
}
EXPORT_SYMBOL_NS_GPL(idle_inject_register_full, IDLE_INJECT);

 
struct idle_inject_device *idle_inject_register(struct cpumask *cpumask)
{
	return idle_inject_register_full(cpumask, NULL);
}
EXPORT_SYMBOL_NS_GPL(idle_inject_register, IDLE_INJECT);

 
void idle_inject_unregister(struct idle_inject_device *ii_dev)
{
	unsigned int cpu;

	idle_inject_stop(ii_dev);

	for_each_cpu(cpu, to_cpumask(ii_dev->cpumask))
		per_cpu(idle_inject_device, cpu) = NULL;

	kfree(ii_dev);
}
EXPORT_SYMBOL_NS_GPL(idle_inject_unregister, IDLE_INJECT);

static struct smp_hotplug_thread idle_inject_threads = {
	.store = &idle_inject_thread.tsk,
	.setup = idle_inject_setup,
	.thread_fn = idle_inject_fn,
	.thread_comm = "idle_inject/%u",
	.thread_should_run = idle_inject_should_run,
};

static int __init idle_inject_init(void)
{
	return smpboot_register_percpu_thread(&idle_inject_threads);
}
early_initcall(idle_inject_init);
