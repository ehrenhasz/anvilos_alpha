
 
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/kprobes.h>
#include <linux/nmi.h>
#include <linux/cpu.h>
#include <linux/sched/debug.h>

#ifdef arch_trigger_cpumask_backtrace
 
static DECLARE_BITMAP(backtrace_mask, NR_CPUS) __read_mostly;

 
static unsigned long backtrace_flag;

 
void nmi_trigger_cpumask_backtrace(const cpumask_t *mask,
				   int exclude_cpu,
				   void (*raise)(cpumask_t *mask))
{
	int i, this_cpu = get_cpu();

	if (test_and_set_bit(0, &backtrace_flag)) {
		 
		put_cpu();
		return;
	}

	cpumask_copy(to_cpumask(backtrace_mask), mask);
	if (exclude_cpu != -1)
		cpumask_clear_cpu(exclude_cpu, to_cpumask(backtrace_mask));

	 
	if (cpumask_test_cpu(this_cpu, to_cpumask(backtrace_mask)))
		nmi_cpu_backtrace(NULL);

	if (!cpumask_empty(to_cpumask(backtrace_mask))) {
		pr_info("Sending NMI from CPU %d to CPUs %*pbl:\n",
			this_cpu, nr_cpumask_bits, to_cpumask(backtrace_mask));
		nmi_backtrace_stall_snap(to_cpumask(backtrace_mask));
		raise(to_cpumask(backtrace_mask));
	}

	 
	for (i = 0; i < 10 * 1000; i++) {
		if (cpumask_empty(to_cpumask(backtrace_mask)))
			break;
		mdelay(1);
		touch_softlockup_watchdog();
	}
	nmi_backtrace_stall_check(to_cpumask(backtrace_mask));

	 
	printk_trigger_flush();

	clear_bit_unlock(0, &backtrace_flag);
	put_cpu();
}


static bool backtrace_idle;
module_param(backtrace_idle, bool, 0644);

bool nmi_cpu_backtrace(struct pt_regs *regs)
{
	int cpu = smp_processor_id();
	unsigned long flags;

	if (cpumask_test_cpu(cpu, to_cpumask(backtrace_mask))) {
		 
		printk_cpu_sync_get_irqsave(flags);
		if (!READ_ONCE(backtrace_idle) && regs && cpu_in_idle(instruction_pointer(regs))) {
			pr_warn("NMI backtrace for cpu %d skipped: idling at %pS\n",
				cpu, (void *)instruction_pointer(regs));
		} else {
			pr_warn("NMI backtrace for cpu %d\n", cpu);
			if (regs)
				show_regs(regs);
			else
				dump_stack();
		}
		printk_cpu_sync_put_irqrestore(flags);
		cpumask_clear_cpu(cpu, to_cpumask(backtrace_mask));
		return true;
	}

	return false;
}
NOKPROBE_SYMBOL(nmi_cpu_backtrace);
#endif
