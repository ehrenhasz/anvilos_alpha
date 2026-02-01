 
#include <linux/interrupt.h>
#include <linux/nodemask.h>
#include <linux/export.h>
#include <linux/mmzone.h>
#include <linux/init.h>
#include <linux/smp.h>
#include <linux/irq.h>
#include <asm/io_apic.h>
#include <asm/cpu.h>

static DEFINE_PER_CPU(struct x86_cpu, cpu_devices);

#ifdef CONFIG_HOTPLUG_CPU
int arch_register_cpu(int cpu)
{
	struct x86_cpu *xc = per_cpu_ptr(&cpu_devices, cpu);

	xc->cpu.hotpluggable = cpu > 0;
	return register_cpu(&xc->cpu, cpu);
}
EXPORT_SYMBOL(arch_register_cpu);

void arch_unregister_cpu(int num)
{
	unregister_cpu(&per_cpu(cpu_devices, num).cpu);
}
EXPORT_SYMBOL(arch_unregister_cpu);
#else  

int __init arch_register_cpu(int num)
{
	return register_cpu(&per_cpu(cpu_devices, num).cpu, num);
}
#endif  

static int __init topology_init(void)
{
	int i;

	for_each_present_cpu(i)
		arch_register_cpu(i);

	return 0;
}
subsys_initcall(topology_init);
