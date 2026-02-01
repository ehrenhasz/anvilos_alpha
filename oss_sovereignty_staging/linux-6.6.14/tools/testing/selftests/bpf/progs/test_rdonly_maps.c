


#include <linux/ptrace.h>
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

const struct {
	unsigned a[4];
	 
	char _y;
} rdonly_values = { .a = {2, 3, 4, 5} };

struct {
	unsigned did_run;
	unsigned iters;
	unsigned sum;
} res = {};

SEC("raw_tracepoint/sys_enter:skip_loop")
int skip_loop(struct pt_regs *ctx)
{
	 
	unsigned * volatile p = (void *)&rdonly_values.a;
	unsigned iters = 0, sum = 0;

	 
	while (*p & 1) {
		iters++;
		sum += *p;
		p++;
	}
	res.did_run = 1;
	res.iters = iters;
	res.sum = sum;
	return 0;
}

SEC("raw_tracepoint/sys_enter:part_loop")
int part_loop(struct pt_regs *ctx)
{
	 
	unsigned * volatile p = (void *)&rdonly_values.a;
	unsigned iters = 0, sum = 0;

	 
	while (*p < 5) {
		iters++;
		sum += *p;
		p++;
	}
	res.did_run = 1;
	res.iters = iters;
	res.sum = sum;
	return 0;
}

SEC("raw_tracepoint/sys_enter:full_loop")
int full_loop(struct pt_regs *ctx)
{
	 
	unsigned * volatile p = (void *)&rdonly_values.a;
	int i = sizeof(rdonly_values.a) / sizeof(rdonly_values.a[0]);
	unsigned iters = 0, sum = 0;

	 
	while (i > 0 ) {
		iters++;
		sum += *p;
		p++;
		i--;
	}
	res.did_run = 1;
	res.iters = iters;
	res.sum = sum;
	return 0;
}

char _license[] SEC("license") = "GPL";
