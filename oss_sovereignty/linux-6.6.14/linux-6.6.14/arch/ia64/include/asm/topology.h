#ifndef _ASM_IA64_TOPOLOGY_H
#define _ASM_IA64_TOPOLOGY_H
#include <asm/acpi.h>
#include <asm/numa.h>
#include <asm/smp.h>
#ifdef CONFIG_NUMA
#define PENALTY_FOR_NODE_WITH_CPUS 255
#define RECLAIM_DISTANCE 15
#define cpumask_of_node(node) ((node) == -1 ?				\
			       cpu_all_mask :				\
			       &node_to_cpu_mask[node])
#define pcibus_to_node(bus) PCI_CONTROLLER(bus)->node
void build_cpu_to_node_map(void);
#endif  
#ifdef CONFIG_SMP
#define topology_physical_package_id(cpu)	(cpu_data(cpu)->socket_id)
#define topology_core_id(cpu)			(cpu_data(cpu)->core_id)
#define topology_core_cpumask(cpu)		(&cpu_core_map[cpu])
#define topology_sibling_cpumask(cpu)		(&per_cpu(cpu_sibling_map, cpu))
#endif
extern void arch_fix_phys_package_id(int num, u32 slot);
#define cpumask_of_pcibus(bus)	(pcibus_to_node(bus) == -1 ?		\
				 cpu_all_mask :				\
				 cpumask_of_node(pcibus_to_node(bus)))
#include <asm-generic/topology.h>
#endif  
