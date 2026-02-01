
 
#include <linux/bpf.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/pkt_cls.h>
#include <linux/swab.h>
#include <stdbool.h>
#include <stdint.h>


#include <linux/udp.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

#define IP_DF 0x4000  

SEC("schedcls/ingress6/nat_6")
int sched_cls_ingress6_nat_6_prog(struct __sk_buff *skb)
{
	const int l2_header_size =  sizeof(struct ethhdr);
	void *data = (void *)(long)skb->data;
	const void *data_end = (void *)(long)skb->data_end;
	const struct ethhdr * const eth = data;  
	const struct ipv6hdr * const ip6 =  (void *)(eth + 1);

	
	if  (skb->pkt_type != PACKET_HOST)
		return TC_ACT_OK;

	
	if (skb->protocol != bpf_htons(ETH_P_IPV6))
		return TC_ACT_OK;

	
	if (data + l2_header_size + sizeof(*ip6) > data_end)
		return TC_ACT_OK;

	
	if (eth->h_proto != bpf_htons(ETH_P_IPV6))
		return TC_ACT_OK;

	
	if (ip6->version != 6)
		return TC_ACT_OK;
	
	if (bpf_ntohs(ip6->payload_len) > 0xFFFF - sizeof(struct iphdr))
		return TC_ACT_OK;
	switch (ip6->nexthdr) {
	case IPPROTO_TCP:  
	case IPPROTO_UDP:  
	case IPPROTO_GRE:  
	case IPPROTO_ESP:  
		break;
	default:  
		return TC_ACT_OK;
	}

	struct ethhdr eth2;  

	eth2 = *eth;                     
	eth2.h_proto = bpf_htons(ETH_P_IP);  

	struct iphdr ip = {
		.version = 4,                                                      
		.ihl = sizeof(struct iphdr) / sizeof(__u32),                       
		.tos = (ip6->priority << 4) + (ip6->flow_lbl[0] >> 4),             
		.tot_len = bpf_htons(bpf_ntohs(ip6->payload_len) + sizeof(struct iphdr)),  
		.id = 0,                                                           
		.frag_off = bpf_htons(IP_DF),                                          
		.ttl = ip6->hop_limit,                                             
		.protocol = ip6->nexthdr,                                          
		.check = 0,                                                        
		.saddr = 0x0201a8c0,                            
		.daddr = 0x0101a8c0,                                         
	};

	
	__wsum sum4 = 0;

	for (int i = 0; i < sizeof(ip) / sizeof(__u16); ++i)
		sum4 += ((__u16 *)&ip)[i];

	
	sum4 = (sum4 & 0xFFFF) + (sum4 >> 16);  
	sum4 = (sum4 & 0xFFFF) + (sum4 >> 16);  
	ip.check = (__u16)~sum4;                

	
	__wsum sum6 = 0;
	
	for (int i = 0; i < sizeof(*ip6) / sizeof(__u16); ++i)
		sum6 += ~((__u16 *)ip6)[i];  

	
	

	
	
	if (bpf_skb_change_proto(skb, bpf_htons(ETH_P_IP), 0))
		return TC_ACT_OK;
	bpf_csum_update(skb, sum6);

	data = (void *)(long)skb->data;
	data_end = (void *)(long)skb->data_end;
	if (data + l2_header_size + sizeof(struct iphdr) > data_end)
		return TC_ACT_SHOT;

	struct ethhdr *new_eth = data;

	
	*new_eth = eth2;

	
	*(struct iphdr *)(new_eth + 1) = ip;
	return bpf_redirect(skb->ifindex, BPF_F_INGRESS);
}

SEC("schedcls/egress4/snat4")
int sched_cls_egress4_snat4_prog(struct __sk_buff *skb)
{
	const int l2_header_size =  sizeof(struct ethhdr);
	void *data = (void *)(long)skb->data;
	const void *data_end = (void *)(long)skb->data_end;
	const struct ethhdr *const eth = data;  
	const struct iphdr *const ip4 = (void *)(eth + 1);

	
	if (skb->protocol != bpf_htons(ETH_P_IP))
		return TC_ACT_OK;

	
	if (data + l2_header_size + sizeof(struct ipv6hdr) > data_end)
		return TC_ACT_OK;

	
	if (eth->h_proto != bpf_htons(ETH_P_IP))
		return TC_ACT_OK;

	
	if (ip4->version != 4)
		return TC_ACT_OK;

	
	if (ip4->ihl != 5)
		return TC_ACT_OK;

	
	if (bpf_htons(ip4->tot_len) > 0xFFFF - sizeof(struct ipv6hdr))
		return TC_ACT_OK;

	
	__wsum sum4 = 0;

	for (int i = 0; i < sizeof(*ip4) / sizeof(__u16); ++i)
		sum4 += ((__u16 *)ip4)[i];

	
	sum4 = (sum4 & 0xFFFF) + (sum4 >> 16);  
	sum4 = (sum4 & 0xFFFF) + (sum4 >> 16);  
	
	if (sum4 != 0xFFFF)
		return TC_ACT_OK;

	
	if (bpf_ntohs(ip4->tot_len) < sizeof(*ip4))
		return TC_ACT_OK;

	
	if (ip4->frag_off & ~bpf_htons(IP_DF))
		return TC_ACT_OK;

	switch (ip4->protocol) {
	case IPPROTO_TCP:  
	case IPPROTO_GRE:  
	case IPPROTO_ESP:  
		break;         

	case IPPROTO_UDP:  
		if (data + sizeof(*ip4) + sizeof(struct udphdr) > data_end)
			return TC_ACT_OK;
		const struct udphdr *uh = (const struct udphdr *)(ip4 + 1);
		
		
		
		
		if (!uh->check)
			return TC_ACT_OK;
		break;

	default:  
		return TC_ACT_OK;
	}
	struct ethhdr eth2;  

	eth2 = *eth;                     
	eth2.h_proto = bpf_htons(ETH_P_IPV6);  

	struct ipv6hdr ip6 = {
		.version = 6,                                    
		.priority = ip4->tos >> 4,                       
		.flow_lbl = {(ip4->tos & 0xF) << 4, 0, 0},       
		.payload_len = bpf_htons(bpf_ntohs(ip4->tot_len) - 20),  
		.nexthdr = ip4->protocol,                        
		.hop_limit = ip4->ttl,                           
	};
	ip6.saddr.in6_u.u6_addr32[0] = bpf_htonl(0x20010db8);
	ip6.saddr.in6_u.u6_addr32[1] = 0;
	ip6.saddr.in6_u.u6_addr32[2] = 0;
	ip6.saddr.in6_u.u6_addr32[3] = bpf_htonl(1);
	ip6.daddr.in6_u.u6_addr32[0] = bpf_htonl(0x20010db8);
	ip6.daddr.in6_u.u6_addr32[1] = 0;
	ip6.daddr.in6_u.u6_addr32[2] = 0;
	ip6.daddr.in6_u.u6_addr32[3] = bpf_htonl(2);

	
	__wsum sum6 = 0;
	
	for (int i = 0; i < sizeof(ip6) / sizeof(__u16); ++i)
		sum6 += ((__u16 *)&ip6)[i];

	
	
	if (bpf_skb_change_proto(skb, bpf_htons(ETH_P_IPV6), 0))
		return TC_ACT_OK;

	
	
	
	
	
	
	
	
	bpf_csum_update(skb, sum6);

	
	data = (void *)(long)skb->data;
	data_end = (void *)(long)skb->data_end;

	
	
	if (data + l2_header_size + sizeof(ip6) > data_end)
		return TC_ACT_SHOT;

	struct ethhdr *new_eth = data;

	
	*new_eth = eth2;
	
	*(struct ipv6hdr *)(new_eth + 1) = ip6;
	return TC_ACT_OK;
}

char _license[] SEC("license") = ("GPL");
