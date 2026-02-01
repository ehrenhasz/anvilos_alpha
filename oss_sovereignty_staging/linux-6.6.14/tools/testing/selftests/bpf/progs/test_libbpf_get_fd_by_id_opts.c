

 

#include "vmlinux.h"
#include <errno.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

 
#define FMODE_WRITE	0x2

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, __u32);
	__type(value, __u32);
} data_input SEC(".maps");

char _license[] SEC("license") = "GPL";

SEC("lsm/bpf_map")
int BPF_PROG(check_access, struct bpf_map *map, fmode_t fmode)
{
	if (map != (struct bpf_map *)&data_input)
		return 0;

	if (fmode & FMODE_WRITE)
		return -EACCES;

	return 0;
}
