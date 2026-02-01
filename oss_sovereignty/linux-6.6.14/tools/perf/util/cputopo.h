 
#ifndef __PERF_CPUTOPO_H
#define __PERF_CPUTOPO_H

#include <linux/types.h>

struct cpu_topology {
	 
	u32	  package_cpus_lists;
	 
	u32	  die_cpus_lists;
	 
	u32	  core_cpus_lists;
	 
	const char **package_cpus_list;
	 
	const char **die_cpus_list;
	 
	const char **core_cpus_list;
};

struct numa_topology_node {
	char		*cpus;
	u32		 node;
	u64		 mem_total;
	u64		 mem_free;
};

struct numa_topology {
	u32				nr;
	struct numa_topology_node	nodes[];
};

struct hybrid_topology_node {
	char		*pmu_name;
	char		*cpus;
};

struct hybrid_topology {
	u32				nr;
	struct hybrid_topology_node	nodes[];
};

 
const struct cpu_topology *online_topology(void);

struct cpu_topology *cpu_topology__new(void);
void cpu_topology__delete(struct cpu_topology *tp);
 
bool cpu_topology__smt_on(const struct cpu_topology *topology);
 
bool cpu_topology__core_wide(const struct cpu_topology *topology,
			     const char *user_requested_cpu_list);

struct numa_topology *numa_topology__new(void);
void numa_topology__delete(struct numa_topology *tp);

struct hybrid_topology *hybrid_topology__new(void);
void hybrid_topology__delete(struct hybrid_topology *tp);

#endif  
