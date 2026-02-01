
 
#include <vmlinux.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_helpers.h>
#include "../bpf_testmod/bpf_testmod_kfunc.h"

static struct prog_test_ref_kfunc __kptr *v;
long total_sum = -1;

SEC("tc")
int test_jit_probe_mem(struct __sk_buff *ctx)
{
	struct prog_test_ref_kfunc *p;
	unsigned long zero = 0, sum;

	p = bpf_kfunc_call_test_acquire(&zero);
	if (!p)
		return 1;

	p = bpf_kptr_xchg(&v, p);
	if (p)
		goto release_out;

	 
	p = v;
	if (!p)
		return 1;

	asm volatile (
		"r9 = %[p];"
		"%[sum] = 0;"

		 
		"r8 = *(u32 *)(r9 + 0);"
		"%[sum] += r8;"

		 
		"r8 = *(u32 *)(r9 + 4);"
		"%[sum] += r8;"

		"r9 += 8;"
		 
		"r9 = *(u32 *)(r9 - 8);"
		"%[sum] += r9;"

		: [sum] "=r"(sum)
		: [p] "r"(p)
		: "r8", "r9"
	);

	total_sum = sum;
	return 0;
release_out:
	bpf_kfunc_call_test_release(p);
	return 1;
}

char _license[] SEC("license") = "GPL";
