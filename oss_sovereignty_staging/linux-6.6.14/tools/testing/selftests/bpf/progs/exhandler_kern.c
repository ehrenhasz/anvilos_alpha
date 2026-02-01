
 

#include "vmlinux.h"

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>

char _license[] SEC("license") = "GPL";

unsigned int exception_triggered;
int test_pid;

 
SEC("tp_btf/task_newtask")
int BPF_PROG(trace_task_newtask, struct task_struct *task, u64 clone_flags)
{
	int pid = bpf_get_current_pid_tgid() >> 32;
	struct callback_head *work;
	void *func;

	if (test_pid != pid)
		return 0;

	 
	work = task->task_works;
	func = work->func;
	 
	barrier_var(work);
	if (work)
		return 0;
	barrier_var(func);
	if (func)
		return 0;
	exception_triggered++;
	return 0;
}
