
 

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

char _license[] SEC("license") = "GPL";

SEC("tc")
int time_tai(struct __sk_buff *skb)
{
	__u64 ts1, ts2;

	 
	ts1 = bpf_ktime_get_tai_ns();
	ts2 = bpf_ktime_get_tai_ns();

	 
	skb->tstamp = ts1;
	skb->cb[0] = ts2 & 0xffffffff;
	skb->cb[1] = ts2 >> 32;

	return 0;
}
