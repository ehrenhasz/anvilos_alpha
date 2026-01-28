
#ifndef __PERF_MEM_EVENTS_H
#define __PERF_MEM_EVENTS_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <linux/types.h>
#include <linux/refcount.h>
#include <linux/perf_event.h>
#include "stat.h"
#include "evsel.h"

struct perf_mem_event {
	bool		record;
	bool		supported;
	const char	*tag;
	const char	*name;
	const char	*sysfs_name;
};

struct mem_info {
	struct addr_map_symbol	iaddr;
	struct addr_map_symbol	daddr;
	union perf_mem_data_src	data_src;
	refcount_t		refcnt;
};

enum {
	PERF_MEM_EVENTS__LOAD,
	PERF_MEM_EVENTS__STORE,
	PERF_MEM_EVENTS__LOAD_STORE,
	PERF_MEM_EVENTS__MAX,
};

extern unsigned int perf_mem_events__loads_ldlat;

int perf_mem_events__parse(const char *str);
int perf_mem_events__init(void);

const char *perf_mem_events__name(int i, const char *pmu_name);
struct perf_mem_event *perf_mem_events__ptr(int i);
bool is_mem_loads_aux_event(struct evsel *leader);

void perf_mem_events__list(void);
int perf_mem_events__record_args(const char **rec_argv, int *argv_nr,
				 char **rec_tmp, int *tmp_nr);

int perf_mem__tlb_scnprintf(char *out, size_t sz, struct mem_info *mem_info);
int perf_mem__lvl_scnprintf(char *out, size_t sz, struct mem_info *mem_info);
int perf_mem__snp_scnprintf(char *out, size_t sz, struct mem_info *mem_info);
int perf_mem__lck_scnprintf(char *out, size_t sz, struct mem_info *mem_info);
int perf_mem__blk_scnprintf(char *out, size_t sz, struct mem_info *mem_info);

int perf_script__meminfo_scnprintf(char *bf, size_t size, struct mem_info *mem_info);

struct c2c_stats {
	u32	nr_entries;

	u32	locks;               
	u32	store;               
	u32	st_uncache;          
	u32	st_noadrs;           
	u32	st_l1hit;            
	u32	st_l1miss;           
	u32	st_na;               
	u32	load;                
	u32	ld_excl;             
	u32	ld_shared;           
	u32	ld_uncache;          
	u32	ld_io;               
	u32	ld_miss;             
	u32	ld_noadrs;           
	u32	ld_fbhit;            
	u32	ld_l1hit;            
	u32	ld_l2hit;            
	u32	ld_llchit;           
	u32	lcl_hitm;            
	u32	rmt_hitm;            
	u32	tot_hitm;            
	u32	lcl_peer;            
	u32	rmt_peer;            
	u32	tot_peer;            
	u32	rmt_hit;             
	u32	lcl_dram;            
	u32	rmt_dram;            
	u32	blk_data;            
	u32	blk_addr;            
	u32	nomap;               
	u32	noparse;             
};

struct hist_entry;
int c2c_decode_stats(struct c2c_stats *stats, struct mem_info *mi);
void c2c_add_stats(struct c2c_stats *stats, struct c2c_stats *add);

#endif 
