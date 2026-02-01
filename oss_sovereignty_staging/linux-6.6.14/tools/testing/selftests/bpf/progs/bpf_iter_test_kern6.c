
 
#include "bpf_iter.h"
#include <bpf/bpf_helpers.h>

char _license[] SEC("license") = "GPL";

__u32 value_sum = 0;

SEC("iter/bpf_map_elem")
int dump_bpf_hash_map(struct bpf_iter__bpf_map_elem *ctx)
{
	void *value = ctx->value;

	if (value == (void *)0)
		return 0;

	 
	value_sum += *(__u32 *)(value - 4);
	return 0;
}
