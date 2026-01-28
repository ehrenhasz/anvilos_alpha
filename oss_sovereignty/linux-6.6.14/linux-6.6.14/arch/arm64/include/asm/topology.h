#ifndef __ASM_TOPOLOGY_H
#define __ASM_TOPOLOGY_H
#include <linux/cpumask.h>
#ifdef CONFIG_NUMA
struct pci_bus;
int pcibus_to_node(struct pci_bus *bus);
#define cpumask_of_pcibus(bus)	(pcibus_to_node(bus) == -1 ?		\
				 cpu_all_mask :				\
				 cpumask_of_node(pcibus_to_node(bus)))
#endif  
#include <linux/arch_topology.h>
void update_freq_counters_refs(void);
#define arch_scale_freq_tick topology_scale_freq_tick
#define arch_set_freq_scale topology_set_freq_scale
#define arch_scale_freq_capacity topology_get_freq_scale
#define arch_scale_freq_invariant topology_scale_freq_invariant
#ifdef CONFIG_ACPI_CPPC_LIB
#define arch_init_invariance_cppc topology_init_cpu_capacity_cppc
#endif
#define arch_scale_cpu_capacity topology_get_cpu_scale
#define arch_update_cpu_topology topology_update_cpu_topology
#define arch_scale_thermal_pressure topology_get_thermal_pressure
#define arch_update_thermal_pressure	topology_update_thermal_pressure
#include <asm-generic/topology.h>
#endif  
