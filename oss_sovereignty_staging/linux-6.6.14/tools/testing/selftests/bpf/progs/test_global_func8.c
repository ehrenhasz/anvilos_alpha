
 
#include <stddef.h>
#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include "bpf_misc.h"

__noinline int foo(struct __sk_buff *skb)
{
	return bpf_get_prandom_u32();
}

SEC("cgroup_skb/ingress")
__success
int global_func8(struct __sk_buff *skb)
{
	if (!foo(skb))
		return 0;

	return 1;
}
