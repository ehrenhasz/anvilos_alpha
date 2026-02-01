
 

#include "vmlinux.h"

#include <bpf/bpf_helpers.h>

extern const int bpf_prog_active __ksym;  

SEC("raw_tp/sys_enter")
int handler1(const void *ctx)
{
	int *active;
	__u32 cpu;

	cpu = bpf_get_smp_processor_id();
	active = (int *)bpf_per_cpu_ptr(&bpf_prog_active, cpu);
	if (active) {
		 
		 
		*(volatile int *)active = -1;
	}

	return 0;
}

__noinline int write_active(int *p)
{
	return p ? (*p = 42) : 0;
}

SEC("raw_tp/sys_enter")
int handler2(const void *ctx)
{
	int *active;

	active = bpf_this_cpu_ptr(&bpf_prog_active);
	write_active(active);
	return 0;
}

char _license[] SEC("license") = "GPL";
