
 

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

 
static volatile long static_var1 = 2;
static volatile int static_var2 = 3;
int var1 = -1;
 
const volatile int rovar1;

 
static __noinline int subprog(int x)
{
	 
	return x * 2;
}

SEC("raw_tp/sys_enter")
int handler1(const void *ctx)
{
	var1 = subprog(rovar1) + static_var1 + static_var2;

	return 0;
}

char LICENSE[] SEC("license") = "GPL";
int VERSION SEC("version") = 1;
