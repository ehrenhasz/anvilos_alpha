 
#include "vmlinux.h"
#include <linux/version.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

struct pair {
	u64 val;
	u64 ip;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__type(key, long);
	__type(value, struct pair);
	__uint(max_entries, 1000000);
} my_map SEC(".maps");

 
SEC("kprobe/kmem_cache_free")
int bpf_prog1(struct pt_regs *ctx)
{
	long ptr = PT_REGS_PARM2(ctx);

	bpf_map_delete_elem(&my_map, &ptr);
	return 0;
}

SEC("kretprobe/kmem_cache_alloc_node")
int bpf_prog2(struct pt_regs *ctx)
{
	long ptr = PT_REGS_RC(ctx);
	long ip = 0;

	 
	BPF_KRETPROBE_READ_RET_IP(ip, ctx);

	struct pair v = {
		.val = bpf_ktime_get_ns(),
		.ip = ip,
	};

	bpf_map_update_elem(&my_map, &ptr, &v, BPF_ANY);
	return 0;
}
char _license[] SEC("license") = "GPL";
u32 _version SEC("version") = LINUX_VERSION_CODE;
