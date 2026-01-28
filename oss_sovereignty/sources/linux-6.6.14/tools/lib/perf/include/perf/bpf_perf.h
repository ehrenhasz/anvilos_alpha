
#ifndef __LIBPERF_BPF_PERF_H
#define __LIBPERF_BPF_PERF_H

#include <linux/types.h>  


struct perf_event_attr_map_entry {
	__u32 link_id;
	__u32 diff_map_id;
};


#define BPF_PERF_DEFAULT_ATTR_MAP_PATH "perf_attr_map"

#endif 
