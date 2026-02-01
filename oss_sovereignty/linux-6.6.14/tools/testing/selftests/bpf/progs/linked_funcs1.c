
 

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "bpf_misc.h"

 
const volatile int my_tid __weak;
long syscall_id __weak;

int output_val1;
int output_ctx1;
int output_weak1;

 
static __noinline int subprog(int x)
{
	 
	return x * 1;
}

 
int set_output_val1(int x)
{
	output_val1 = x + subprog(x);
	return x;
}

 
void set_output_ctx1(__u64 *ctx)
{
	output_ctx1 = ctx[1];  
}

 
__weak int set_output_weak(int x)
{
	static volatile int whatever;

	 
	whatever = bpf_core_type_size(struct task_struct);
	__sink(whatever);

	output_weak1 = x;
	return x;
}

extern int set_output_val2(int x);

 
__hidden extern void set_output_ctx2(__u64 *ctx);

SEC("?raw_tp/sys_enter")
int BPF_PROG(handler1, struct pt_regs *regs, long id)
{
	static volatile int whatever;

	if (my_tid != (u32)bpf_get_current_pid_tgid() || id != syscall_id)
		return 0;

	 
	whatever = bpf_core_type_size(struct task_struct);
	__sink(whatever);

	set_output_val2(1000);
	set_output_ctx2(ctx);  

	 
	set_output_weak(42);

	return 0;
}

char LICENSE[] SEC("license") = "GPL";
