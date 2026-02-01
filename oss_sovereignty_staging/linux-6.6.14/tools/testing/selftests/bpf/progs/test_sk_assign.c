



#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/in.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/pkt_cls.h>
#include <linux/tcp.h>
#include <sys/socket.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>
#include "bpf_misc.h"

#if defined(IPROUTE2_HAVE_LIBBPF)
 
struct {
	__uint(type, BPF_MAP_TYPE_SOCKMAP);
	__type(key, int);
	__type(value, __u64);
	__uint(pinning, LIBBPF_PIN_BY_NAME);
	__uint(max_entries, 1);
} server_map SEC(".maps");
#else
 
#define PIN_GLOBAL_NS 2

 
struct {
	__u32 type;
	__u32 size_key;
	__u32 size_value;
	__u32 max_elem;
	__u32 flags;
	__u32 id;
	__u32 pinning;
} server_map SEC("maps") = {
	.type = BPF_MAP_TYPE_SOCKMAP,
	.size_key = sizeof(int),
	.size_value  = sizeof(__u64),
	.max_elem = 1,
	.pinning = PIN_GLOBAL_NS,
};
#endif

char _license[] SEC("license") = "GPL";

 
static inline struct bpf_sock_tuple *
get_tuple(struct __sk_buff *skb, bool *ipv4, bool *tcp)
{
	void *data_end = (void *)(long)skb->data_end;
	void *data = (void *)(long)skb->data;
	struct bpf_sock_tuple *result;
	struct ethhdr *eth;
	__u8 proto = 0;
	__u64 ihl_len;

	eth = (struct ethhdr *)(data);
	if (eth + 1 > data_end)
		return NULL;

	if (eth->h_proto == bpf_htons(ETH_P_IP)) {
		struct iphdr *iph = (struct iphdr *)(data + sizeof(*eth));

		if (iph + 1 > data_end)
			return NULL;
		if (iph->ihl != 5)
			 
			return NULL;
		ihl_len = iph->ihl * 4;
		proto = iph->protocol;
		*ipv4 = true;
		result = (struct bpf_sock_tuple *)&iph->saddr;
	} else if (eth->h_proto == bpf_htons(ETH_P_IPV6)) {
		struct ipv6hdr *ip6h = (struct ipv6hdr *)(data + sizeof(*eth));

		if (ip6h + 1 > data_end)
			return NULL;
		ihl_len = sizeof(*ip6h);
		proto = ip6h->nexthdr;
		*ipv4 = false;
		result = (struct bpf_sock_tuple *)&ip6h->saddr;
	} else {
		return (struct bpf_sock_tuple *)data;
	}

	if (proto != IPPROTO_TCP && proto != IPPROTO_UDP)
		return NULL;

	*tcp = (proto == IPPROTO_TCP);
	__sink(ihl_len);
	return result;
}

static inline int
handle_udp(struct __sk_buff *skb, struct bpf_sock_tuple *tuple, bool ipv4)
{
	struct bpf_sock *sk;
	const int zero = 0;
	size_t tuple_len;
	__be16 dport;
	int ret;

	tuple_len = ipv4 ? sizeof(tuple->ipv4) : sizeof(tuple->ipv6);
	if ((void *)tuple + tuple_len > (void *)(long)skb->data_end)
		return TC_ACT_SHOT;

	sk = bpf_sk_lookup_udp(skb, tuple, tuple_len, BPF_F_CURRENT_NETNS, 0);
	if (sk)
		goto assign;

	dport = ipv4 ? tuple->ipv4.dport : tuple->ipv6.dport;
	if (dport != bpf_htons(4321))
		return TC_ACT_OK;

	sk = bpf_map_lookup_elem(&server_map, &zero);
	if (!sk)
		return TC_ACT_SHOT;

assign:
	ret = bpf_sk_assign(skb, sk, 0);
	bpf_sk_release(sk);
	return ret;
}

static inline int
handle_tcp(struct __sk_buff *skb, struct bpf_sock_tuple *tuple, bool ipv4)
{
	struct bpf_sock *sk;
	const int zero = 0;
	size_t tuple_len;
	__be16 dport;
	int ret;

	tuple_len = ipv4 ? sizeof(tuple->ipv4) : sizeof(tuple->ipv6);
	if ((void *)tuple + tuple_len > (void *)(long)skb->data_end)
		return TC_ACT_SHOT;

	sk = bpf_skc_lookup_tcp(skb, tuple, tuple_len, BPF_F_CURRENT_NETNS, 0);
	if (sk) {
		if (sk->state != BPF_TCP_LISTEN)
			goto assign;
		bpf_sk_release(sk);
	}

	dport = ipv4 ? tuple->ipv4.dport : tuple->ipv6.dport;
	if (dport != bpf_htons(4321))
		return TC_ACT_OK;

	sk = bpf_map_lookup_elem(&server_map, &zero);
	if (!sk)
		return TC_ACT_SHOT;

	if (sk->state != BPF_TCP_LISTEN) {
		bpf_sk_release(sk);
		return TC_ACT_SHOT;
	}

assign:
	ret = bpf_sk_assign(skb, sk, 0);
	bpf_sk_release(sk);
	return ret;
}

SEC("tc")
int bpf_sk_assign_test(struct __sk_buff *skb)
{
	struct bpf_sock_tuple *tuple;
	bool ipv4 = false;
	bool tcp = false;
	int ret = 0;

	tuple = get_tuple(skb, &ipv4, &tcp);
	if (!tuple)
		return TC_ACT_SHOT;

	 
	if (tcp)
		ret = handle_tcp(skb, tuple, ipv4);
	else
		ret = handle_udp(skb, tuple, ipv4);

	return ret == 0 ? TC_ACT_OK : TC_ACT_SHOT;
}
