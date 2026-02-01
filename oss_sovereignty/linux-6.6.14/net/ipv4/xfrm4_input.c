
 

#include <linux/slab.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/netfilter.h>
#include <linux/netfilter_ipv4.h>
#include <net/ip.h>
#include <net/xfrm.h>

static int xfrm4_rcv_encap_finish2(struct net *net, struct sock *sk,
				   struct sk_buff *skb)
{
	return dst_input(skb);
}

static inline int xfrm4_rcv_encap_finish(struct net *net, struct sock *sk,
					 struct sk_buff *skb)
{
	if (!skb_dst(skb)) {
		const struct iphdr *iph = ip_hdr(skb);

		if (ip_route_input_noref(skb, iph->daddr, iph->saddr,
					 iph->tos, skb->dev))
			goto drop;
	}

	if (xfrm_trans_queue(skb, xfrm4_rcv_encap_finish2))
		goto drop;

	return 0;
drop:
	kfree_skb(skb);
	return NET_RX_DROP;
}

int xfrm4_transport_finish(struct sk_buff *skb, int async)
{
	struct xfrm_offload *xo = xfrm_offload(skb);
	struct iphdr *iph = ip_hdr(skb);

	iph->protocol = XFRM_MODE_SKB_CB(skb)->protocol;

#ifndef CONFIG_NETFILTER
	if (!async)
		return -iph->protocol;
#endif

	__skb_push(skb, skb->data - skb_network_header(skb));
	iph->tot_len = htons(skb->len);
	ip_send_check(iph);

	if (xo && (xo->flags & XFRM_GRO)) {
		skb_mac_header_rebuild(skb);
		skb_reset_transport_header(skb);
		return 0;
	}

	NF_HOOK(NFPROTO_IPV4, NF_INET_PRE_ROUTING,
		dev_net(skb->dev), NULL, skb, skb->dev, NULL,
		xfrm4_rcv_encap_finish);
	return 0;
}

 
int xfrm4_udp_encap_rcv(struct sock *sk, struct sk_buff *skb)
{
	struct udp_sock *up = udp_sk(sk);
	struct udphdr *uh;
	struct iphdr *iph;
	int iphlen, len;
	__u8 *udpdata;
	__be32 *udpdata32;
	u16 encap_type;

	encap_type = READ_ONCE(up->encap_type);
	 
	if (!encap_type)
		return 1;

	 
	len = skb->len - sizeof(struct udphdr);
	if (!pskb_may_pull(skb, sizeof(struct udphdr) + min(len, 8)))
		return 1;

	 
	uh = udp_hdr(skb);
	udpdata = (__u8 *)uh + sizeof(struct udphdr);
	udpdata32 = (__be32 *)udpdata;

	switch (encap_type) {
	default:
	case UDP_ENCAP_ESPINUDP:
		 
		if (len == 1 && udpdata[0] == 0xff) {
			goto drop;
		} else if (len > sizeof(struct ip_esp_hdr) && udpdata32[0] != 0) {
			 
			len = sizeof(struct udphdr);
		} else
			 
			return 1;
		break;
	case UDP_ENCAP_ESPINUDP_NON_IKE:
		 
		if (len == 1 && udpdata[0] == 0xff) {
			goto drop;
		} else if (len > 2 * sizeof(u32) + sizeof(struct ip_esp_hdr) &&
			   udpdata32[0] == 0 && udpdata32[1] == 0) {

			 
			len = sizeof(struct udphdr) + 2 * sizeof(u32);
		} else
			 
			return 1;
		break;
	}

	 
	if (skb_unclone(skb, GFP_ATOMIC))
		goto drop;

	 
	iph = ip_hdr(skb);
	iphlen = iph->ihl << 2;
	iph->tot_len = htons(ntohs(iph->tot_len) - len);
	if (skb->len < iphlen + len) {
		 
		goto drop;
	}

	 
	__skb_pull(skb, len);
	skb_reset_transport_header(skb);

	 
	return xfrm4_rcv_encap(skb, IPPROTO_ESP, 0, encap_type);

drop:
	kfree_skb(skb);
	return 0;
}
EXPORT_SYMBOL(xfrm4_udp_encap_rcv);

int xfrm4_rcv(struct sk_buff *skb)
{
	return xfrm4_rcv_spi(skb, ip_hdr(skb)->protocol, 0);
}
EXPORT_SYMBOL(xfrm4_rcv);
