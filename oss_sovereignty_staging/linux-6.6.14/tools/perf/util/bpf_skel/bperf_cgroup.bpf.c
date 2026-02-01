


#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

#define MAX_LEVELS  10  
#define MAX_EVENTS  32  





struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(int));
	__uint(max_entries, 1);
} events SEC(".maps");


struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(key_size, sizeof(__u64));
	__uint(value_size, sizeof(__u32));
	__uint(max_entries, 1);
} cgrp_idx SEC(".maps");


struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(struct bpf_perf_event_value));
} prev_readings SEC(".maps");



struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(key_size, sizeof(__u32));
	__uint(value_size, sizeof(struct bpf_perf_event_value));
} cgrp_readings SEC(".maps");

 
struct cgroup___new {
	int level;
	struct cgroup *ancestors[];
} __attribute__((preserve_access_index));

 
struct cgroup___old {
	int level;
	u64 ancestor_ids[];
} __attribute__((preserve_access_index));

const volatile __u32 num_events = 1;
const volatile __u32 num_cpus = 1;

int enabled = 0;
int use_cgroup_v2 = 0;
int perf_subsys_id = -1;

static inline __u64 get_cgroup_v1_ancestor_id(struct cgroup *cgrp, int level)
{
	 
	struct cgroup___new *cgrp_new = (void *)cgrp;

	if (bpf_core_field_exists(cgrp_new->ancestors)) {
		return BPF_CORE_READ(cgrp_new, ancestors[level], kn, id);
	} else {
		 
		struct cgroup___old *cgrp_old = (void *)cgrp;

		return BPF_CORE_READ(cgrp_old, ancestor_ids[level]);
	}
}

static inline int get_cgroup_v1_idx(__u32 *cgrps, int size)
{
	struct task_struct *p = (void *)bpf_get_current_task();
	struct cgroup *cgrp;
	register int i = 0;
	__u32 *elem;
	int level;
	int cnt;

	if (perf_subsys_id == -1) {
#if __has_builtin(__builtin_preserve_enum_value)
		perf_subsys_id = bpf_core_enum_value(enum cgroup_subsys_id,
						     perf_event_cgrp_id);
#else
		perf_subsys_id = perf_event_cgrp_id;
#endif
	}
	cgrp = BPF_CORE_READ(p, cgroups, subsys[perf_subsys_id], cgroup);
	level = BPF_CORE_READ(cgrp, level);

	for (cnt = 0; i < MAX_LEVELS; i++) {
		__u64 cgrp_id;

		if (i > level)
			break;

		
		cgrp_id = get_cgroup_v1_ancestor_id(cgrp, i);
		elem = bpf_map_lookup_elem(&cgrp_idx, &cgrp_id);
		if (!elem)
			continue;

		cgrps[cnt++] = *elem;
		if (cnt == size)
			break;
	}

	return cnt;
}

static inline int get_cgroup_v2_idx(__u32 *cgrps, int size)
{
	register int i = 0;
	__u32 *elem;
	int cnt;

	for (cnt = 0; i < MAX_LEVELS; i++) {
		__u64 cgrp_id = bpf_get_current_ancestor_cgroup_id(i);

		if (cgrp_id == 0)
			break;

		
		elem = bpf_map_lookup_elem(&cgrp_idx, &cgrp_id);
		if (!elem)
			continue;

		cgrps[cnt++] = *elem;
		if (cnt == size)
			break;
	}

	return cnt;
}

static int bperf_cgroup_count(void)
{
	register __u32 idx = 0;  
	register int c = 0;
	struct bpf_perf_event_value val, delta, *prev_val, *cgrp_val;
	__u32 cpu = bpf_get_smp_processor_id();
	__u32 cgrp_idx[MAX_LEVELS];
	int cgrp_cnt;
	__u32 key, cgrp;
	long err;

	if (use_cgroup_v2)
		cgrp_cnt = get_cgroup_v2_idx(cgrp_idx, MAX_LEVELS);
	else
		cgrp_cnt = get_cgroup_v1_idx(cgrp_idx, MAX_LEVELS);

	for ( ; idx < MAX_EVENTS; idx++) {
		if (idx == num_events)
			break;

		
		key = idx;
		
		prev_val = bpf_map_lookup_elem(&prev_readings, &key);
		if (!prev_val) {
			val.counter = val.enabled = val.running = 0;
			bpf_map_update_elem(&prev_readings, &key, &val, BPF_ANY);

			prev_val = bpf_map_lookup_elem(&prev_readings, &key);
			if (!prev_val)
				continue;
		}

		
		key = idx * num_cpus + cpu;
		err = bpf_perf_event_read_value(&events, key, &val, sizeof(val));
		if (err)
			continue;

		if (enabled) {
			delta.counter = val.counter - prev_val->counter;
			delta.enabled = val.enabled - prev_val->enabled;
			delta.running = val.running - prev_val->running;

			for (c = 0; c < MAX_LEVELS; c++) {
				if (c == cgrp_cnt)
					break;

				cgrp = cgrp_idx[c];

				
				key = cgrp * num_events + idx;
				cgrp_val = bpf_map_lookup_elem(&cgrp_readings, &key);
				if (cgrp_val) {
					cgrp_val->counter += delta.counter;
					cgrp_val->enabled += delta.enabled;
					cgrp_val->running += delta.running;
				} else {
					bpf_map_update_elem(&cgrp_readings, &key,
							    &delta, BPF_ANY);
				}
			}
		}

		*prev_val = val;
	}
	return 0;
}


SEC("perf_event")
int BPF_PROG(on_cgrp_switch)
{
	return bperf_cgroup_count();
}

SEC("raw_tp/sched_switch")
int BPF_PROG(trigger_read)
{
	return bperf_cgroup_count();
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
