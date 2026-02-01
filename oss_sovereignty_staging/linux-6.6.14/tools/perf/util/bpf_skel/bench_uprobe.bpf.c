

#include "vmlinux.h"
#include <bpf/bpf_tracing.h>

unsigned int nr_uprobes;

SEC("uprobe")
int BPF_UPROBE(empty)
{
       return 0;
}

SEC("uprobe")
int BPF_UPROBE(trace_printk)
{
	char fmt[] = "perf bench uprobe %u";

	bpf_trace_printk(fmt, sizeof(fmt), ++nr_uprobes);
	return 0;
}

char LICENSE[] SEC("license") = "Dual BSD/GPL";
