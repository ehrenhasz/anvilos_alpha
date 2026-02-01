
 
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char _license[] SEC("license") = "GPL";

struct percpu_attach_counter {
	 
	__u64 prev;
	 
	__u64 state;
};

struct attach_counter {
	 
	__u64 pending;
	 
	__u64 state;
};

struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_HASH);
	__uint(max_entries, 1024);
	__type(key, __u64);
	__type(value, struct percpu_attach_counter);
} percpu_attach_counters SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, __u64);
	__type(value, struct attach_counter);
} attach_counters SEC(".maps");

extern void cgroup_rstat_updated(struct cgroup *cgrp, int cpu) __ksym;
extern void cgroup_rstat_flush(struct cgroup *cgrp) __ksym;

static uint64_t cgroup_id(struct cgroup *cgrp)
{
	return cgrp->kn->id;
}

static int create_percpu_attach_counter(__u64 cg_id, __u64 state)
{
	struct percpu_attach_counter pcpu_init = {.state = state, .prev = 0};

	return bpf_map_update_elem(&percpu_attach_counters, &cg_id,
				   &pcpu_init, BPF_NOEXIST);
}

static int create_attach_counter(__u64 cg_id, __u64 state, __u64 pending)
{
	struct attach_counter init = {.state = state, .pending = pending};

	return bpf_map_update_elem(&attach_counters, &cg_id,
				   &init, BPF_NOEXIST);
}

SEC("fentry/cgroup_attach_task")
int BPF_PROG(counter, struct cgroup *dst_cgrp, struct task_struct *leader,
	     bool threadgroup)
{
	__u64 cg_id = cgroup_id(dst_cgrp);
	struct percpu_attach_counter *pcpu_counter = bpf_map_lookup_elem(
			&percpu_attach_counters,
			&cg_id);

	if (pcpu_counter)
		pcpu_counter->state += 1;
	else if (create_percpu_attach_counter(cg_id, 1))
		return 0;

	cgroup_rstat_updated(dst_cgrp, bpf_get_smp_processor_id());
	return 0;
}

SEC("fentry/bpf_rstat_flush")
int BPF_PROG(flusher, struct cgroup *cgrp, struct cgroup *parent, int cpu)
{
	struct percpu_attach_counter *pcpu_counter;
	struct attach_counter *total_counter, *parent_counter;
	__u64 cg_id = cgroup_id(cgrp);
	__u64 parent_cg_id = parent ? cgroup_id(parent) : 0;
	__u64 state;
	__u64 delta = 0;

	 
	pcpu_counter = bpf_map_lookup_percpu_elem(&percpu_attach_counters,
						  &cg_id, cpu);
	if (pcpu_counter) {
		state = pcpu_counter->state;
		delta += state - pcpu_counter->prev;
		pcpu_counter->prev = state;
	}

	total_counter = bpf_map_lookup_elem(&attach_counters, &cg_id);
	if (!total_counter) {
		if (create_attach_counter(cg_id, delta, 0))
			return 0;
		goto update_parent;
	}

	 
	if (total_counter->pending) {
		delta += total_counter->pending;
		total_counter->pending = 0;
	}

	 
	total_counter->state += delta;

update_parent:
	 
	if (!delta || !parent_cg_id)
		return 0;

	 
	parent_counter = bpf_map_lookup_elem(&attach_counters,
					     &parent_cg_id);
	if (parent_counter)
		parent_counter->pending += delta;
	else
		create_attach_counter(parent_cg_id, 0, delta);
	return 0;
}

SEC("iter.s/cgroup")
int BPF_PROG(dumper, struct bpf_iter_meta *meta, struct cgroup *cgrp)
{
	struct seq_file *seq = meta->seq;
	struct attach_counter *total_counter;
	__u64 cg_id = cgrp ? cgroup_id(cgrp) : 0;

	 
	if (!cg_id)
		return 1;

	 
	cgroup_rstat_flush(cgrp);

	total_counter = bpf_map_lookup_elem(&attach_counters, &cg_id);
	if (!total_counter) {
		BPF_SEQ_PRINTF(seq, "cg_id: %llu, attach_counter: 0\n",
			       cg_id);
	} else {
		BPF_SEQ_PRINTF(seq, "cg_id: %llu, attach_counter: %llu\n",
			       cg_id, total_counter->state);
	}
	return 0;
}
