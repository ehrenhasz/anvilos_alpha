 
#ifndef __PERF_EVENTS_STATS_
#define __PERF_EVENTS_STATS_

#include <stdio.h>
#include <perf/event.h>
#include <linux/types.h>
#include "auxtrace.h"

 
struct events_stats {
	u64 total_lost;
	u64 total_lost_samples;
	u64 total_aux_lost;
	u64 total_aux_partial;
	u64 total_aux_collision;
	u64 total_invalid_chains;
	u32 nr_events[PERF_RECORD_HEADER_MAX];
	u32 nr_lost_warned;
	u32 nr_unknown_events;
	u32 nr_invalid_chains;
	u32 nr_unknown_id;
	u32 nr_unprocessable_samples;
	u32 nr_auxtrace_errors[PERF_AUXTRACE_ERROR_MAX];
	u32 nr_proc_map_timeout;
};

struct hists_stats {
	u64 total_period;
	u64 total_non_filtered_period;
	u32 nr_samples;
	u32 nr_non_filtered_samples;
	u32 nr_lost_samples;
};

void events_stats__inc(struct events_stats *stats, u32 type);

size_t events_stats__fprintf(struct events_stats *stats, FILE *fp,
			     bool skip_empty);

#endif  
