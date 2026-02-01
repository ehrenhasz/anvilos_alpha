

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include "bpf_misc.h"

char _license[] SEC("license") = "GPL";

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1);
	__type(key, u64);
	__type(value, u64);
} m_hash SEC(".maps");

SEC("?raw_tp")
__failure __msg("R8 invalid mem access 'map_value_or_null")
int jeq_infer_not_null_ptr_to_btfid(void *ctx)
{
	struct bpf_map *map = (struct bpf_map *)&m_hash;
	struct bpf_map *inner_map = map->inner_map_meta;
	u64 key = 0, ret = 0, *val;

	val = bpf_map_lookup_elem(map, &key);
	 
	asm volatile("r8 = %[val];\n"
		     "r9 = %[inner_map];\n"
		     "if r8 != r9 goto +1;\n"
		     "%[ret] = *(u64 *)(r8 +0);\n"
		     : [ret] "+r"(ret)
		     : [inner_map] "r"(inner_map), [val] "r"(val)
		     : "r8", "r9");

	return ret;
}
