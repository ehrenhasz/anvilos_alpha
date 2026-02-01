

#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/nmi.h>
#include <linux/percpu-defs.h>

static cpumask_t __read_mostly watchdog_cpus;

static unsigned int watchdog_next_cpu(unsigned int cpu)
{
	unsigned int next_cpu;

	next_cpu = cpumask_next(cpu, &watchdog_cpus);
	if (next_cpu >= nr_cpu_ids)
		next_cpu = cpumask_first(&watchdog_cpus);

	if (next_cpu == cpu)
		return nr_cpu_ids;

	return next_cpu;
}

int __init watchdog_hardlockup_probe(void)
{
	return 0;
}

void watchdog_hardlockup_enable(unsigned int cpu)
{
	unsigned int next_cpu;

	 
	watchdog_hardlockup_touch_cpu(cpu);

	 
	next_cpu = watchdog_next_cpu(cpu);
	if (next_cpu < nr_cpu_ids)
		watchdog_hardlockup_touch_cpu(next_cpu);

	 
	smp_wmb();

	cpumask_set_cpu(cpu, &watchdog_cpus);
}

void watchdog_hardlockup_disable(unsigned int cpu)
{
	unsigned int next_cpu = watchdog_next_cpu(cpu);

	 
	if (next_cpu < nr_cpu_ids)
		watchdog_hardlockup_touch_cpu(next_cpu);

	 
	smp_wmb();

	cpumask_clear_cpu(cpu, &watchdog_cpus);
}

void watchdog_buddy_check_hardlockup(int hrtimer_interrupts)
{
	unsigned int next_cpu;

	 
	if (hrtimer_interrupts % 3 != 0)
		return;

	 
	next_cpu = watchdog_next_cpu(smp_processor_id());
	if (next_cpu >= nr_cpu_ids)
		return;

	 
	smp_rmb();

	watchdog_hardlockup_check(next_cpu, NULL);
}
