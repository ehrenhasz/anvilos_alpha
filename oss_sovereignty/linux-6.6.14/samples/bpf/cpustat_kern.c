

#include <linux/version.h>
#include <linux/ptrace.h>
#include <uapi/linux/bpf.h>
#include <bpf/bpf_helpers.h>

 
#define MAX_CPU			8
#define MAX_PSTATE_ENTRIES	5
#define MAX_CSTATE_ENTRIES	3

static int cpu_opps[] = { 208000, 432000, 729000, 960000, 1200000 };

 
#define MAP_OFF_CSTATE_TIME	0
#define MAP_OFF_CSTATE_IDX	1
#define MAP_OFF_PSTATE_TIME	2
#define MAP_OFF_PSTATE_IDX	3
#define MAP_OFF_NUM		4

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, u32);
	__type(value, u64);
	__uint(max_entries, MAX_CPU * MAP_OFF_NUM);
} my_map SEC(".maps");

 
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, u32);
	__type(value, u64);
	__uint(max_entries, MAX_CPU * MAX_CSTATE_ENTRIES);
} cstate_duration SEC(".maps");

 
struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, u32);
	__type(value, u64);
	__uint(max_entries, MAX_CPU * MAX_PSTATE_ENTRIES);
} pstate_duration SEC(".maps");

 
struct cpu_args {
	u64 pad;
	u32 state;
	u32 cpu_id;
};

 
static u32 find_cpu_pstate_idx(u32 frequency)
{
	u32 i;

	for (i = 0; i < sizeof(cpu_opps) / sizeof(u32); i++) {
		if (frequency == cpu_opps[i])
			return i;
	}

	return i;
}

SEC("tracepoint/power/cpu_idle")
int bpf_prog1(struct cpu_args *ctx)
{
	u64 *cts, *pts, *cstate, *pstate, prev_state, cur_ts, delta;
	u32 key, cpu, pstate_idx;
	u64 *val;

	if (ctx->cpu_id > MAX_CPU)
		return 0;

	cpu = ctx->cpu_id;

	key = cpu * MAP_OFF_NUM + MAP_OFF_CSTATE_TIME;
	cts = bpf_map_lookup_elem(&my_map, &key);
	if (!cts)
		return 0;

	key = cpu * MAP_OFF_NUM + MAP_OFF_CSTATE_IDX;
	cstate = bpf_map_lookup_elem(&my_map, &key);
	if (!cstate)
		return 0;

	key = cpu * MAP_OFF_NUM + MAP_OFF_PSTATE_TIME;
	pts = bpf_map_lookup_elem(&my_map, &key);
	if (!pts)
		return 0;

	key = cpu * MAP_OFF_NUM + MAP_OFF_PSTATE_IDX;
	pstate = bpf_map_lookup_elem(&my_map, &key);
	if (!pstate)
		return 0;

	prev_state = *cstate;
	*cstate = ctx->state;

	if (!*cts) {
		*cts = bpf_ktime_get_ns();
		return 0;
	}

	cur_ts = bpf_ktime_get_ns();
	delta = cur_ts - *cts;
	*cts = cur_ts;

	 
	if (ctx->state != (u32)-1) {

		 
		if (!*pts)
			return 0;

		delta = cur_ts - *pts;

		pstate_idx = find_cpu_pstate_idx(*pstate);
		if (pstate_idx >= MAX_PSTATE_ENTRIES)
			return 0;

		key = cpu * MAX_PSTATE_ENTRIES + pstate_idx;
		val = bpf_map_lookup_elem(&pstate_duration, &key);
		if (val)
			__sync_fetch_and_add((long *)val, delta);

	 
	} else {

		key = cpu * MAX_CSTATE_ENTRIES + prev_state;
		val = bpf_map_lookup_elem(&cstate_duration, &key);
		if (val)
			__sync_fetch_and_add((long *)val, delta);
	}

	 
	if (*pts)
		*pts = cur_ts;

	return 0;
}

SEC("tracepoint/power/cpu_frequency")
int bpf_prog2(struct cpu_args *ctx)
{
	u64 *pts, *cstate, *pstate, prev_state, cur_ts, delta;
	u32 key, cpu, pstate_idx;
	u64 *val;

	cpu = ctx->cpu_id;

	key = cpu * MAP_OFF_NUM + MAP_OFF_PSTATE_TIME;
	pts = bpf_map_lookup_elem(&my_map, &key);
	if (!pts)
		return 0;

	key = cpu * MAP_OFF_NUM + MAP_OFF_PSTATE_IDX;
	pstate = bpf_map_lookup_elem(&my_map, &key);
	if (!pstate)
		return 0;

	key = cpu * MAP_OFF_NUM + MAP_OFF_CSTATE_IDX;
	cstate = bpf_map_lookup_elem(&my_map, &key);
	if (!cstate)
		return 0;

	prev_state = *pstate;
	*pstate = ctx->state;

	if (!*pts) {
		*pts = bpf_ktime_get_ns();
		return 0;
	}

	cur_ts = bpf_ktime_get_ns();
	delta = cur_ts - *pts;
	*pts = cur_ts;

	 
	if (*cstate != (u32)(-1))
		return 0;

	 
	pstate_idx = find_cpu_pstate_idx(*pstate);
	if (pstate_idx >= MAX_PSTATE_ENTRIES)
		return 0;

	key = cpu * MAX_PSTATE_ENTRIES + pstate_idx;
	val = bpf_map_lookup_elem(&pstate_duration, &key);
	if (val)
		__sync_fetch_and_add((long *)val, delta);

	return 0;
}

char _license[] SEC("license") = "GPL";
u32 _version SEC("version") = LINUX_VERSION_CODE;
