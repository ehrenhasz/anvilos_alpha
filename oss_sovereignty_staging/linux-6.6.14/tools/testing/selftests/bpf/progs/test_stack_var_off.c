

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

int probe_res;

char input[4] = {};
int test_pid;

SEC("tracepoint/syscalls/sys_enter_nanosleep")
int probe(void *ctx)
{
	 
	char stack_buf[16];
	unsigned long len;
	unsigned long last;

	if ((bpf_get_current_pid_tgid() >> 32) != test_pid)
		return 0;

	 
	__builtin_memcpy(stack_buf, input, 4);

	 
	len = stack_buf[0] & 0xf;
	last = (len - 1) & 0xf;

	 
	stack_buf[len] = 42;

	 
	probe_res = stack_buf[last];
	return 0;
}

char _license[] SEC("license") = "GPL";
