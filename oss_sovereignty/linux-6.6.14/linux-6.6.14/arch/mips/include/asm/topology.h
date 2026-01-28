#ifndef __ASM_TOPOLOGY_H
#define __ASM_TOPOLOGY_H
#include <topology.h>
#include <linux/smp.h>
#ifdef CONFIG_SMP
#define topology_physical_package_id(cpu)	(cpu_data[cpu].package)
#define topology_core_id(cpu)			(cpu_core(&cpu_data[cpu]))
#define topology_core_cpumask(cpu)		(&cpu_core_map[cpu])
#define topology_sibling_cpumask(cpu)		(&cpu_sibling_map[cpu])
#endif
#endif  
