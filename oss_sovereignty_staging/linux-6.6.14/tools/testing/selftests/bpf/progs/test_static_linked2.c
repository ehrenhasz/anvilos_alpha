
 

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

 
static volatile int static_var1 = 5;
static volatile int static_var2 = 6;
int var2 = -1;
 
const volatile long rovar2;

 
static __noinline int subprog(int x)
{
	 
	return x * 3;
}

SEC("raw_tp/sys_enter")
int handler2(const void *ctx)
{
	var2 = subprog(rovar2) + static_var1 + static_var2;

	return 0;
}

 
char _license[] SEC("license") = "GPL";
int _version SEC("version") = 1;
