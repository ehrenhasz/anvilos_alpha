
 
#include "bpf_iter.h"
#include <bpf/bpf_helpers.h>

char _license[] SEC("license") = "GPL";

struct key_t {
	int a;
	int b;
	int c;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 3);
	__type(key, struct key_t);
	__type(value, __u64);
} hashmap1 SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 3);
	__type(key, __u64);
	__type(value, __u64);
} hashmap2 SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 3);
	__type(key, struct key_t);
	__type(value, __u32);
} hashmap3 SEC(".maps");

 
bool in_test_mode = 0;

 
__u32 key_sum_a = 0, key_sum_b = 0, key_sum_c = 0;
__u64 val_sum = 0;

SEC("iter/bpf_map_elem")
int dump_bpf_hash_map(struct bpf_iter__bpf_map_elem *ctx)
{
	struct seq_file *seq = ctx->meta->seq;
	__u32 seq_num = ctx->meta->seq_num;
	struct bpf_map *map = ctx->map;
	struct key_t *key = ctx->key;
	struct key_t tmp_key;
	__u64 *val = ctx->value;
	__u64 tmp_val = 0;
	int ret;

	if (in_test_mode) {
		 
		if (key == (void *)0 || val == (void *)0)
			return 0;

		 
		__builtin_memcpy(&tmp_key, key, sizeof(struct key_t));
		ret = bpf_map_update_elem(&hashmap1, &tmp_key, &tmp_val, 0);
		if (ret)
			return 0;
		ret = bpf_map_delete_elem(&hashmap1, &tmp_key);
		if (ret)
			return 0;

		key_sum_a += key->a;
		key_sum_b += key->b;
		key_sum_c += key->c;
		val_sum += *val;
		return 0;
	}

	 
	if (seq_num == 0)
		BPF_SEQ_PRINTF(seq, "map dump starts\n");

	if (key == (void *)0 || val == (void *)0) {
		BPF_SEQ_PRINTF(seq, "map dump ends\n");
		return 0;
	}

	BPF_SEQ_PRINTF(seq, "%d: (%x %d %x) (%llx)\n", map->id,
		       key->a, key->b, key->c, *val);

	return 0;
}

SEC("iter.s/bpf_map_elem")
int sleepable_dummy_dump(struct bpf_iter__bpf_map_elem *ctx)
{
	if (ctx->meta->seq_num == 0)
		BPF_SEQ_PRINTF(ctx->meta->seq, "map dump starts\n");

	return 0;
}
