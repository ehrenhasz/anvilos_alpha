
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

#define ETH_ALEN 6
#define HDR_SZ (sizeof(struct ethhdr) + sizeof(struct ipv6hdr) + sizeof(struct udphdr))

 
enum frame_mark {
	MARK_XMIT	= 0U,
	MARK_IN		= 0x42,
	MARK_SKB	= 0x45,
};

const volatile int ifindex_out;
const volatile int ifindex_in;
const volatile __u8 expect_dst[ETH_ALEN];
volatile int pkts_seen_xdp = 0;
volatile int pkts_seen_zero = 0;
volatile int pkts_seen_tc = 0;
volatile int retcode = XDP_REDIRECT;

SEC("xdp")
int xdp_redirect(struct xdp_md *xdp)
{
	__u32 *metadata = (void *)(long)xdp->data_meta;
	void *data_end = (void *)(long)xdp->data_end;
	void *data = (void *)(long)xdp->data;

	__u8 *payload = data + HDR_SZ;
	int ret = retcode;

	if (payload + 1 > data_end)
		return XDP_ABORTED;

	if (xdp->ingress_ifindex != ifindex_in)
		return XDP_ABORTED;

	if (metadata + 1 > data)
		return XDP_ABORTED;

	if (*metadata != 0x42)
		return XDP_ABORTED;

	if (*payload == MARK_XMIT)
		pkts_seen_zero++;

	*payload = MARK_IN;

	if (bpf_xdp_adjust_meta(xdp, sizeof(__u64)))
		return XDP_ABORTED;

	if (retcode > XDP_PASS)
		retcode--;

	if (ret == XDP_REDIRECT)
		return bpf_redirect(ifindex_out, 0);

	return ret;
}

static bool check_pkt(void *data, void *data_end, const __u32 mark)
{
	struct ipv6hdr *iph = data + sizeof(struct ethhdr);
	__u8 *payload = data + HDR_SZ;

	if (payload + 1 > data_end)
		return false;

	if (iph->nexthdr != IPPROTO_UDP || *payload != MARK_IN)
		return false;

	 
	*payload = mark;
	return true;
}

SEC("xdp")
int xdp_count_pkts(struct xdp_md *xdp)
{
	void *data = (void *)(long)xdp->data;
	void *data_end = (void *)(long)xdp->data_end;

	if (check_pkt(data, data_end, MARK_XMIT))
		pkts_seen_xdp++;

	 
	return XDP_DROP;
}

SEC("tc")
int tc_count_pkts(struct __sk_buff *skb)
{
	void *data = (void *)(long)skb->data;
	void *data_end = (void *)(long)skb->data_end;

	if (check_pkt(data, data_end, MARK_SKB))
		pkts_seen_tc++;

	 
	return 0;
}

char _license[] SEC("license") = "GPL";
