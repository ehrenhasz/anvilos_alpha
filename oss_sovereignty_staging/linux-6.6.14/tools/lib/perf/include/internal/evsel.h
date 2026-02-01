 
#ifndef __LIBPERF_INTERNAL_EVSEL_H
#define __LIBPERF_INTERNAL_EVSEL_H

#include <linux/types.h>
#include <linux/perf_event.h>
#include <stdbool.h>
#include <sys/types.h>
#include <internal/cpumap.h>

struct perf_thread_map;
struct xyarray;

 
struct perf_sample_id {
	struct hlist_node	 node;
	u64			 id;
	struct perf_evsel	*evsel;
        
	int			 idx;
	struct perf_cpu		 cpu;
	pid_t			 tid;

	 
	pid_t			 machine_pid;
	struct perf_cpu		 vcpu;

	 
	u64			 period;
};

struct perf_evsel {
	struct list_head	 node;
	struct perf_event_attr	 attr;
	 
	struct perf_cpu_map	*cpus;
	 
	struct perf_cpu_map	*own_cpus;
	struct perf_thread_map	*threads;
	struct xyarray		*fd;
	struct xyarray		*mmap;
	struct xyarray		*sample_id;
	u64			*id;
	u32			 ids;
	struct perf_evsel	*leader;

	 
	int			 nr_members;
	 
	bool			 system_wide;
	 
	bool			 requires_cpu;
	 
	bool			 is_pmu_core;
	int			 idx;
};

void perf_evsel__init(struct perf_evsel *evsel, struct perf_event_attr *attr,
		      int idx);
int perf_evsel__alloc_fd(struct perf_evsel *evsel, int ncpus, int nthreads);
void perf_evsel__close_fd(struct perf_evsel *evsel);
void perf_evsel__free_fd(struct perf_evsel *evsel);
int perf_evsel__read_size(struct perf_evsel *evsel);
int perf_evsel__apply_filter(struct perf_evsel *evsel, const char *filter);

int perf_evsel__alloc_id(struct perf_evsel *evsel, int ncpus, int nthreads);
void perf_evsel__free_id(struct perf_evsel *evsel);

#endif  
