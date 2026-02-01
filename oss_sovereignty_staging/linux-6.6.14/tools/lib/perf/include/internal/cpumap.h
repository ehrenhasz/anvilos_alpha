 
#ifndef __LIBPERF_INTERNAL_CPUMAP_H
#define __LIBPERF_INTERNAL_CPUMAP_H

#include <linux/refcount.h>
#include <perf/cpumap.h>
#include <internal/rc_check.h>

 
DECLARE_RC_STRUCT(perf_cpu_map) {
	refcount_t	refcnt;
	 
	int		nr;
	 
	struct perf_cpu	map[];
};

#ifndef MAX_NR_CPUS
#define MAX_NR_CPUS	2048
#endif

struct perf_cpu_map *perf_cpu_map__alloc(int nr_cpus);
int perf_cpu_map__idx(const struct perf_cpu_map *cpus, struct perf_cpu cpu);
bool perf_cpu_map__is_subset(const struct perf_cpu_map *a, const struct perf_cpu_map *b);

void perf_cpu_map__set_nr(struct perf_cpu_map *map, int nr_cpus);

static inline refcount_t *perf_cpu_map__refcnt(struct perf_cpu_map *map)
{
	return &RC_CHK_ACCESS(map)->refcnt;
}
#endif  
