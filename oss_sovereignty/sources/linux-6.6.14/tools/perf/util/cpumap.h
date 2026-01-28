
#ifndef __PERF_CPUMAP_H
#define __PERF_CPUMAP_H

#include <stdbool.h>
#include <stdio.h>
#include <perf/cpumap.h>
#include <linux/refcount.h>


struct aggr_cpu_id {
	
	int thread_idx;
	
	int node;
	
	int socket;
	
	int die;
	
	int cache_lvl;
	
	int cache;
	
	int core;
	
	struct perf_cpu cpu;
};


struct cpu_aggr_map {
	refcount_t refcnt;
	
	int nr;
	
	struct aggr_cpu_id map[];
};

#define cpu_aggr_map__for_each_idx(idx, aggr_map)				\
	for ((idx) = 0; (idx) < aggr_map->nr; (idx)++)

struct perf_record_cpu_map_data;

bool perf_record_cpu_map_data__test_bit(int i, const struct perf_record_cpu_map_data *data);

struct perf_cpu_map *perf_cpu_map__empty_new(int nr);

struct perf_cpu_map *cpu_map__new_data(const struct perf_record_cpu_map_data *data);
size_t cpu_map__snprint(struct perf_cpu_map *map, char *buf, size_t size);
size_t cpu_map__snprint_mask(struct perf_cpu_map *map, char *buf, size_t size);
size_t cpu_map__fprintf(struct perf_cpu_map *map, FILE *fp);
struct perf_cpu_map *cpu_map__online(void); 

int cpu__setup_cpunode_map(void);

int cpu__max_node(void);
struct perf_cpu cpu__max_cpu(void);
struct perf_cpu cpu__max_present_cpu(void);


static inline bool cpu_map__is_dummy(const struct perf_cpu_map *cpus)
{
	return perf_cpu_map__nr(cpus) == 1 && perf_cpu_map__cpu(cpus, 0).cpu == -1;
}


int cpu__get_node(struct perf_cpu cpu);

int cpu__get_socket_id(struct perf_cpu cpu);

int cpu__get_die_id(struct perf_cpu cpu);

int cpu__get_core_id(struct perf_cpu cpu);


struct cpu_aggr_map *cpu_aggr_map__empty_new(int nr);

typedef struct aggr_cpu_id (*aggr_cpu_id_get_t)(struct perf_cpu cpu, void *data);


struct cpu_aggr_map *cpu_aggr_map__new(const struct perf_cpu_map *cpus,
				       aggr_cpu_id_get_t get_id,
				       void *data, bool needs_sort);

bool aggr_cpu_id__equal(const struct aggr_cpu_id *a, const struct aggr_cpu_id *b);
bool aggr_cpu_id__is_empty(const struct aggr_cpu_id *a);
struct aggr_cpu_id aggr_cpu_id__empty(void);



struct aggr_cpu_id aggr_cpu_id__socket(struct perf_cpu cpu, void *data);

struct aggr_cpu_id aggr_cpu_id__die(struct perf_cpu cpu, void *data);

struct aggr_cpu_id aggr_cpu_id__core(struct perf_cpu cpu, void *data);

struct aggr_cpu_id aggr_cpu_id__cpu(struct perf_cpu cpu, void *data);

struct aggr_cpu_id aggr_cpu_id__node(struct perf_cpu cpu, void *data);

struct aggr_cpu_id aggr_cpu_id__global(struct perf_cpu cpu, void *data);
#endif 
