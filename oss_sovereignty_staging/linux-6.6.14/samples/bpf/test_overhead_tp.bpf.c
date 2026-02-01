 
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>

 
SEC("tracepoint/task/task_rename")
int prog(struct trace_event_raw_task_rename *ctx)
{
	return 0;
}

 
SEC("tracepoint/fib/fib_table_lookup")
int prog2(struct trace_event_raw_fib_table_lookup *ctx)
{
	return 0;
}
char _license[] SEC("license") = "GPL";
