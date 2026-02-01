
 

#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "../bpf_experimental.h"
#include "bpf_misc.h"

struct node_data {
	long key;
	long data;
	struct bpf_rb_node node;
};

struct map_value {
	struct node_data __kptr *node;
};

struct node_data2 {
	long key[4];
};

 
struct node_data *just_here_because_btf_bug;

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__type(key, int);
	__type(value, struct map_value);
	__uint(max_entries, 2);
} some_nodes SEC(".maps");

SEC("tc")
__failure __msg("invalid kptr access, R2 type=ptr_node_data2 expected=ptr_node_data")
long stash_rb_nodes(void *ctx)
{
	struct map_value *mapval;
	struct node_data2 *res;
	int idx = 0;

	mapval = bpf_map_lookup_elem(&some_nodes, &idx);
	if (!mapval)
		return 1;

	res = bpf_obj_new(typeof(*res));
	if (!res)
		return 1;
	res->key[0] = 40;

	res = bpf_kptr_xchg(&mapval->node, res);
	if (res)
		bpf_obj_drop(res);
	return 0;
}

SEC("tc")
__failure __msg("R1 must have zero offset when passed to release func")
long drop_rb_node_off(void *ctx)
{
	struct map_value *mapval;
	struct node_data *res;
	int idx = 0;

	mapval = bpf_map_lookup_elem(&some_nodes, &idx);
	if (!mapval)
		return 1;

	res = bpf_obj_new(typeof(*res));
	if (!res)
		return 1;
	 
	bpf_obj_drop(&res->node);
	return 0;
}

char _license[] SEC("license") = "GPL";
