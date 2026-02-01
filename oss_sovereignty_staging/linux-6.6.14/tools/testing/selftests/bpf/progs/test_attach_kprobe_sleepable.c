


#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include "bpf_misc.h"

int kprobe_res = 0;

 
SEC("kprobe/" SYS_PREFIX "sys_nanosleep")
int handle_kprobe_sleepable(struct pt_regs *ctx)
{
	kprobe_res = 1;
	return 0;
}

char _license[] SEC("license") = "GPL";
