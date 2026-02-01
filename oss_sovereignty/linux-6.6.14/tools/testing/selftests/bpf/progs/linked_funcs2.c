
 

#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "bpf_misc.h"

 
const volatile int my_tid __weak;
long syscall_id __weak;

int output_val2;
int output_ctx2;
int output_weak2;  

 
static __noinline int subprog(int x)
{
	 
	return x * 2;
}

 
int set_output_val2(int x)
{
	output_val2 = 2 * x + 2 * subprog(x);
	return 2 * x;
}

 
void set_output_ctx2(__u64 *ctx)
{
	output_ctx2 = ctx[1];  
}

 
__weak int set_output_weak(int x)
{
	static volatile int whatever;

	 
	whatever = 2 * bpf_core_type_size(struct task_struct);
	__sink(whatever);

	output_weak2 = x;
	return 2 * x;
}

extern int set_output_val1(int x);

 
__hidden extern void set_output_ctx1(__u64 *ctx);

SEC("?raw_tp/sys_enter")
int BPF_PROG(handler2, struct pt_regs *regs, long id)
{
	static volatile int whatever;

	if (my_tid != (u32)bpf_get_current_pid_tgid() || id != syscall_id)
		return 0;

	 
	whatever = bpf_core_type_size(struct task_struct);
	__sink(whatever);

	set_output_val1(2000);
	set_output_ctx1(ctx);  

	 
	set_output_weak(42);

	return 0;
}

char LICENSE[] SEC("license") = "GPL";
