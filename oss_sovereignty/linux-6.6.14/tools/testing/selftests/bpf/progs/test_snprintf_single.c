
 

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

 
const char fmt[10];

SEC("raw_tp/sys_enter")
int handler(const void *ctx)
{
	unsigned long long arg = 42;

	bpf_snprintf(NULL, 0, fmt, &arg, sizeof(arg));

	return 0;
}

char _license[] SEC("license") = "GPL";
