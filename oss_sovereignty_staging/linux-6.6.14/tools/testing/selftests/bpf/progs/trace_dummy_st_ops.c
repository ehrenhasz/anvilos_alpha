
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

int val = 0;

SEC("fentry/test_1")
int BPF_PROG(fentry_test_1, __u64 *st_ops_ctx)
{
	__u64 state;

	 
	bpf_probe_read_kernel(&state, sizeof(__u64), (void *)st_ops_ctx);
	 
	bpf_probe_read_kernel(&val, sizeof(__u32), (void *)state);

	return 0;
}

char _license[] SEC("license") = "GPL";
