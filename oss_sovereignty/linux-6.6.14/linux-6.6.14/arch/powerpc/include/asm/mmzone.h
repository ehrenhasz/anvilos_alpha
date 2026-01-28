#ifndef _ASM_MMZONE_H_
#define _ASM_MMZONE_H_
#ifdef __KERNEL__
#include <linux/cpumask.h>
#ifdef CONFIG_NUMA
extern struct pglist_data *node_data[];
#define NODE_DATA(nid)		(node_data[nid])
extern int numa_cpu_lookup_table[];
extern cpumask_var_t node_to_cpumask_map[];
#ifdef CONFIG_MEMORY_HOTPLUG
extern unsigned long max_pfn;
u64 memory_hotplug_max(void);
#else
#define memory_hotplug_max() memblock_end_of_DRAM()
#endif
#else
#define memory_hotplug_max() memblock_end_of_DRAM()
#endif  
#ifdef CONFIG_FA_DUMP
#define __HAVE_ARCH_RESERVED_KERNEL_PAGES
#endif
#ifdef CONFIG_MEMORY_HOTPLUG
extern int create_section_mapping(unsigned long start, unsigned long end,
				  int nid, pgprot_t prot);
#endif
#endif  
#endif  
