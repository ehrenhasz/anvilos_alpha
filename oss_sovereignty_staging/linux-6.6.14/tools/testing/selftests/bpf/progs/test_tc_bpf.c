

#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>

 

SEC("tc")
int cls(struct __sk_buff *skb)
{
	return 0;
}

 
SEC("tcx/ingress")
int pkt_ptr(struct __sk_buff *skb)
{
	struct iphdr *iph = (void *)(long)skb->data + sizeof(struct ethhdr);

	if ((long)(iph + 1) > (long)skb->data_end)
		return 1;
	return 0;
}
