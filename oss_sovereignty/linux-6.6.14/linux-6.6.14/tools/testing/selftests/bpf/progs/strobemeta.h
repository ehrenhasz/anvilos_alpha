#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <linux/bpf.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <bpf/bpf_helpers.h>
typedef uint32_t pid_t;
struct task_struct {};
#define TASK_COMM_LEN 16
#define PERF_MAX_STACK_DEPTH 127
#define STROBE_TYPE_INVALID 0
#define STROBE_TYPE_INT 1
#define STROBE_TYPE_STR 2
#define STROBE_TYPE_MAP 3
#define STACK_TABLE_EPOCH_SHIFT 20
#define STROBE_MAX_STR_LEN 1
#define STROBE_MAX_CFGS 32
#define STROBE_MAX_PAYLOAD						\
	(STROBE_MAX_STRS * STROBE_MAX_STR_LEN +				\
	STROBE_MAX_MAPS * (1 + STROBE_MAX_MAP_ENTRIES * 2) * STROBE_MAX_STR_LEN)
struct strobe_value_header {
	uint16_t len;
	uint8_t _reserved[6];
};
struct strobe_value_generic {
	struct strobe_value_header header;
	union {
		int64_t val;
		void *ptr;
	};
};
struct strobe_value_int {
	struct strobe_value_header header;
	int64_t value;
};
struct strobe_value_str {
	struct strobe_value_header header;
	const char* value;
};
struct strobe_value_map {
	struct strobe_value_header header;
	const struct strobe_map_raw* value;
};
struct strobe_map_entry {
	const char* key;
	const char* val;
};
struct strobe_map_raw {
	int64_t id;
	int64_t cnt;
	const char* tag;
	struct strobe_map_entry entries[STROBE_MAX_MAP_ENTRIES];
};
#define TLS_NOT_SET -1
#define TLS_LOCAL_EXEC 0
#define TLS_IMM_EXEC 1
#define TLS_GENERAL_DYN 2
struct strobe_value_loc {
	int64_t tls_mode;
	int64_t offset;
};
struct strobemeta_cfg {
	int64_t req_meta_idx;
	struct strobe_value_loc int_locs[STROBE_MAX_INTS];
	struct strobe_value_loc str_locs[STROBE_MAX_STRS];
	struct strobe_value_loc map_locs[STROBE_MAX_MAPS];
};
struct strobe_map_descr {
	uint64_t id;
	int16_t tag_len;
	int16_t cnt;
	uint16_t key_lens[STROBE_MAX_MAP_ENTRIES];
	uint16_t val_lens[STROBE_MAX_MAP_ENTRIES];
};
struct strobemeta_payload {
	int64_t req_id;
	uint8_t req_meta_valid;
	uint64_t int_vals_set_mask;
	int64_t int_vals[STROBE_MAX_INTS];
	uint16_t str_lens[STROBE_MAX_STRS];
	struct strobe_map_descr map_descrs[STROBE_MAX_MAPS];
	char payload[STROBE_MAX_PAYLOAD];
};
struct strobelight_bpf_sample {
	uint64_t ktime;
	char comm[TASK_COMM_LEN];
	pid_t pid;
	int user_stack_id;
	int kernel_stack_id;
	int has_meta;
	struct strobemeta_payload metadata;
	char dummy_safeguard;
};
struct {
	__uint(type, BPF_MAP_TYPE_PERF_EVENT_ARRAY);
	__uint(max_entries, 32);
	__uint(key_size, sizeof(int));
	__uint(value_size, sizeof(int));
} samples SEC(".maps");
struct {
	__uint(type, BPF_MAP_TYPE_STACK_TRACE);
	__uint(max_entries, 16);
	__uint(key_size, sizeof(uint32_t));
	__uint(value_size, sizeof(uint64_t) * PERF_MAX_STACK_DEPTH);
} stacks_0 SEC(".maps");
struct {
	__uint(type, BPF_MAP_TYPE_STACK_TRACE);
	__uint(max_entries, 16);
	__uint(key_size, sizeof(uint32_t));
	__uint(value_size, sizeof(uint64_t) * PERF_MAX_STACK_DEPTH);
} stacks_1 SEC(".maps");
struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, 1);
	__type(key, uint32_t);
	__type(value, struct strobelight_bpf_sample);
} sample_heap SEC(".maps");
struct {
	__uint(type, BPF_MAP_TYPE_PERCPU_ARRAY);
	__uint(max_entries, STROBE_MAX_CFGS);
	__type(key, pid_t);
	__type(value, struct strobemeta_cfg);
} strobemeta_cfgs SEC(".maps");
typedef union dtv {
	size_t counter;
	struct {
		void* val;
		bool is_static;
	} pointer;
} dtv_t;
struct tcbhead {
	void* tcb;
	dtv_t* dtv;
};
struct tls_index {
	uint64_t module;
	uint64_t offset;
};
#ifdef SUBPROGS
__noinline
#else
__always_inline
#endif
static void *calc_location(struct strobe_value_loc *loc, void *tls_base)
{
	if (loc->tls_mode <= TLS_LOCAL_EXEC) {
		void *addr = tls_base + loc->offset;
		return (void *)((loc->tls_mode + 1) * (int64_t)addr);
	}
	struct tls_index tls_index;
	dtv_t *dtv;
	void *tls_ptr;
	bpf_probe_read_user(&tls_index, sizeof(struct tls_index),
			    (void *)loc->offset);
	if (tls_index.module > 0) {
		bpf_probe_read_user(&dtv, sizeof(dtv),
				    &((struct tcbhead *)tls_base)->dtv);
		dtv += tls_index.module;
	} else {
		dtv = NULL;
	}
	bpf_probe_read_user(&tls_ptr, sizeof(void *), dtv);
	return tls_ptr && tls_ptr != (void *)-1
		? tls_ptr + tls_index.offset
		: NULL;
}
#ifdef SUBPROGS
__noinline
#else
__always_inline
#endif
static void read_int_var(struct strobemeta_cfg *cfg,
			 size_t idx, void *tls_base,
			 struct strobe_value_generic *value,
			 struct strobemeta_payload *data)
{
	void *location = calc_location(&cfg->int_locs[idx], tls_base);
	if (!location)
		return;
	bpf_probe_read_user(value, sizeof(struct strobe_value_generic), location);
	data->int_vals[idx] = value->val;
	if (value->header.len)
		data->int_vals_set_mask |= (1 << idx);
}
static __always_inline uint64_t read_str_var(struct strobemeta_cfg *cfg,
					     size_t idx, void *tls_base,
					     struct strobe_value_generic *value,
					     struct strobemeta_payload *data,
					     void *payload)
{
	void *location;
	uint64_t len;
	data->str_lens[idx] = 0;
	location = calc_location(&cfg->str_locs[idx], tls_base);
	if (!location)
		return 0;
	bpf_probe_read_user(value, sizeof(struct strobe_value_generic), location);
	len = bpf_probe_read_user_str(payload, STROBE_MAX_STR_LEN, value->ptr);
	if (len > STROBE_MAX_STR_LEN)
		return 0;
	data->str_lens[idx] = len;
	return len;
}
static __always_inline void *read_map_var(struct strobemeta_cfg *cfg,
					  size_t idx, void *tls_base,
					  struct strobe_value_generic *value,
					  struct strobemeta_payload *data,
					  void *payload)
{
	struct strobe_map_descr* descr = &data->map_descrs[idx];
	struct strobe_map_raw map;
	void *location;
	uint64_t len;
	descr->tag_len = 0;  
	descr->cnt = -1;  
	location = calc_location(&cfg->map_locs[idx], tls_base);
	if (!location)
		return payload;
	bpf_probe_read_user(value, sizeof(struct strobe_value_generic), location);
	if (bpf_probe_read_user(&map, sizeof(struct strobe_map_raw), value->ptr))
		return payload;
	descr->id = map.id;
	descr->cnt = map.cnt;
	if (cfg->req_meta_idx == idx) {
		data->req_id = map.id;
		data->req_meta_valid = 1;
	}
	len = bpf_probe_read_user_str(payload, STROBE_MAX_STR_LEN, map.tag);
	if (len <= STROBE_MAX_STR_LEN) {
		descr->tag_len = len;
		payload += len;
	}
#ifdef NO_UNROLL
#pragma clang loop unroll(disable)
#else
#pragma unroll
#endif
	for (int i = 0; i < STROBE_MAX_MAP_ENTRIES; ++i) {
		if (i >= map.cnt)
			break;
		descr->key_lens[i] = 0;
		len = bpf_probe_read_user_str(payload, STROBE_MAX_STR_LEN,
					      map.entries[i].key);
		if (len <= STROBE_MAX_STR_LEN) {
			descr->key_lens[i] = len;
			payload += len;
		}
		descr->val_lens[i] = 0;
		len = bpf_probe_read_user_str(payload, STROBE_MAX_STR_LEN,
					      map.entries[i].val);
		if (len <= STROBE_MAX_STR_LEN) {
			descr->val_lens[i] = len;
			payload += len;
		}
	}
	return payload;
}
#ifdef USE_BPF_LOOP
enum read_type {
	READ_INT_VAR,
	READ_MAP_VAR,
	READ_STR_VAR,
};
struct read_var_ctx {
	struct strobemeta_payload *data;
	void *tls_base;
	struct strobemeta_cfg *cfg;
	void *payload;
	struct strobe_value_generic *value;
	enum read_type type;
};
static int read_var_callback(__u32 index, struct read_var_ctx *ctx)
{
	switch (ctx->type) {
	case READ_INT_VAR:
		if (index >= STROBE_MAX_INTS)
			return 1;
		read_int_var(ctx->cfg, index, ctx->tls_base, ctx->value, ctx->data);
		break;
	case READ_MAP_VAR:
		if (index >= STROBE_MAX_MAPS)
			return 1;
		ctx->payload = read_map_var(ctx->cfg, index, ctx->tls_base,
					    ctx->value, ctx->data, ctx->payload);
		break;
	case READ_STR_VAR:
		if (index >= STROBE_MAX_STRS)
			return 1;
		ctx->payload += read_str_var(ctx->cfg, index, ctx->tls_base,
					     ctx->value, ctx->data, ctx->payload);
		break;
	}
	return 0;
}
#endif  
#ifdef SUBPROGS
__noinline
#else
__always_inline
#endif
static void *read_strobe_meta(struct task_struct *task,
			      struct strobemeta_payload *data)
{
	pid_t pid = bpf_get_current_pid_tgid() >> 32;
	struct strobe_value_generic value = {0};
	struct strobemeta_cfg *cfg;
	void *tls_base, *payload;
	cfg = bpf_map_lookup_elem(&strobemeta_cfgs, &pid);
	if (!cfg)
		return NULL;
	data->int_vals_set_mask = 0;
	data->req_meta_valid = 0;
	payload = data->payload;
	tls_base = (void *)task;
#ifdef USE_BPF_LOOP
	struct read_var_ctx ctx = {
		.cfg = cfg,
		.tls_base = tls_base,
		.value = &value,
		.data = data,
		.payload = payload,
	};
	int err;
	ctx.type = READ_INT_VAR;
	err = bpf_loop(STROBE_MAX_INTS, read_var_callback, &ctx, 0);
	if (err != STROBE_MAX_INTS)
		return NULL;
	ctx.type = READ_STR_VAR;
	err = bpf_loop(STROBE_MAX_STRS, read_var_callback, &ctx, 0);
	if (err != STROBE_MAX_STRS)
		return NULL;
	ctx.type = READ_MAP_VAR;
	err = bpf_loop(STROBE_MAX_MAPS, read_var_callback, &ctx, 0);
	if (err != STROBE_MAX_MAPS)
		return NULL;
#else
#ifdef NO_UNROLL
#pragma clang loop unroll(disable)
#else
#pragma unroll
#endif  
	for (int i = 0; i < STROBE_MAX_INTS; ++i) {
		read_int_var(cfg, i, tls_base, &value, data);
	}
#ifdef NO_UNROLL
#pragma clang loop unroll(disable)
#else
#pragma unroll
#endif  
	for (int i = 0; i < STROBE_MAX_STRS; ++i) {
		payload += read_str_var(cfg, i, tls_base, &value, data, payload);
	}
#ifdef NO_UNROLL
#pragma clang loop unroll(disable)
#else
#pragma unroll
#endif  
	for (int i = 0; i < STROBE_MAX_MAPS; ++i) {
		payload = read_map_var(cfg, i, tls_base, &value, data, payload);
	}
#endif  
	return payload;
}
SEC("raw_tracepoint/kfree_skb")
int on_event(struct pt_regs *ctx) {
	pid_t pid =  bpf_get_current_pid_tgid() >> 32;
	struct strobelight_bpf_sample* sample;
	struct task_struct *task;
	uint32_t zero = 0;
	uint64_t ktime_ns;
	void *sample_end;
	sample = bpf_map_lookup_elem(&sample_heap, &zero);
	if (!sample)
		return 0;  
	sample->pid = pid;
	bpf_get_current_comm(&sample->comm, TASK_COMM_LEN);
	ktime_ns = bpf_ktime_get_ns();
	sample->ktime = ktime_ns;
	task = (struct task_struct *)bpf_get_current_task();
	sample_end = read_strobe_meta(task, &sample->metadata);
	sample->has_meta = sample_end != NULL;
	sample_end = sample_end ? : &sample->metadata;
	if ((ktime_ns >> STACK_TABLE_EPOCH_SHIFT) & 1) {
		sample->kernel_stack_id = bpf_get_stackid(ctx, &stacks_1, 0);
		sample->user_stack_id = bpf_get_stackid(ctx, &stacks_1, BPF_F_USER_STACK);
	} else {
		sample->kernel_stack_id = bpf_get_stackid(ctx, &stacks_0, 0);
		sample->user_stack_id = bpf_get_stackid(ctx, &stacks_0, BPF_F_USER_STACK);
	}
	uint64_t sample_size = sample_end - (void *)sample;
	if (sample_size < sizeof(struct strobelight_bpf_sample))
		bpf_perf_event_output(ctx, &samples, 0, sample, 1 + sample_size);
	return 0;
}
char _license[] SEC("license") = "GPL";
