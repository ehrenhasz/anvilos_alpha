
#include <inttypes.h>
#include <linux/bpf.h>
#include <bpf/bpf_endian.h>
#include <bpf/bpf_helpers.h>
#include <linux/if_ether.h>
#include <linux/ip.h>

 
SEC("lwt_xmit")
int test_lwt_reroute(struct __sk_buff *skb)
{
	struct iphdr *iph = NULL;
	void *start = (void *)(long)skb->data;
	void *end = (void *)(long)skb->data_end;

	 
	if (skb->mark != 0)
		return BPF_OK;

	if (start + sizeof(*iph) > end)
		return BPF_DROP;

	iph = (struct iphdr *)start;
	skb->mark = bpf_ntohl(iph->daddr) & 0xff;

	 
	if (skb->mark == 0)
		return BPF_OK;

	return BPF_LWT_REROUTE;
}

char _license[] SEC("license") = "GPL";
