 
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <linux/bpf.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/in.h>
#include <linux/pkt_cls.h>

#include <bpf/bpf_helpers.h>
#include <bpf/bpf_endian.h>

 
struct _vlan_hdr {
	__be16 h_vlan_TCI;
	__be16 h_vlan_encapsulated_proto;
};
#define VLAN_PRIO_MASK		0xe000  
#define VLAN_PRIO_SHIFT		13
#define VLAN_CFI_MASK		0x1000  
#define VLAN_TAG_PRESENT	VLAN_CFI_MASK
#define VLAN_VID_MASK		0x0fff  
#define VLAN_N_VID		4096

struct parse_pkt {
	__u16 l3_proto;
	__u16 l3_offset;
	__u16 vlan_outer;
	__u16 vlan_inner;
	__u8  vlan_outer_offset;
	__u8  vlan_inner_offset;
};

char _license[] SEC("license") = "GPL";

static __always_inline
bool parse_eth_frame(struct ethhdr *eth, void *data_end, struct parse_pkt *pkt)
{
	__u16 eth_type;
	__u8 offset;

	offset = sizeof(*eth);
	 
	if ((void *)eth + offset + (2*sizeof(struct _vlan_hdr)) > data_end)
		return false;

	eth_type = eth->h_proto;

	 
	if (eth_type == bpf_htons(ETH_P_8021Q)
	    || eth_type == bpf_htons(ETH_P_8021AD)) {
		struct _vlan_hdr *vlan_hdr;

		vlan_hdr = (void *)eth + offset;
		pkt->vlan_outer_offset = offset;
		pkt->vlan_outer = bpf_ntohs(vlan_hdr->h_vlan_TCI)
				& VLAN_VID_MASK;
		eth_type        = vlan_hdr->h_vlan_encapsulated_proto;
		offset += sizeof(*vlan_hdr);
	}

	 
	if (eth_type == bpf_htons(ETH_P_8021Q)
	    || eth_type == bpf_htons(ETH_P_8021AD)) {
		struct _vlan_hdr *vlan_hdr;

		vlan_hdr = (void *)eth + offset;
		pkt->vlan_inner_offset = offset;
		pkt->vlan_inner = bpf_ntohs(vlan_hdr->h_vlan_TCI)
				& VLAN_VID_MASK;
		eth_type        = vlan_hdr->h_vlan_encapsulated_proto;
		offset += sizeof(*vlan_hdr);
	}

	pkt->l3_proto = bpf_ntohs(eth_type);  
	pkt->l3_offset = offset;

	return true;
}

 
#define TESTVLAN 4011  


SEC("xdp_drop_vlan_4011")
int  xdp_prognum0(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data     = (void *)(long)ctx->data;
	struct parse_pkt pkt = { 0 };

	if (!parse_eth_frame(data, data_end, &pkt))
		return XDP_ABORTED;

	 
	if (pkt.vlan_outer == TESTVLAN)
		return XDP_ABORTED;
	 
	return XDP_PASS;
}
 

 
#define TO_VLAN	0

SEC("xdp_vlan_change")
int  xdp_prognum1(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data     = (void *)(long)ctx->data;
	struct parse_pkt pkt = { 0 };

	if (!parse_eth_frame(data, data_end, &pkt))
		return XDP_ABORTED;

	 
	if (pkt.vlan_outer == TESTVLAN) {
		struct _vlan_hdr *vlan_hdr = data + pkt.vlan_outer_offset;

		 
		vlan_hdr->h_vlan_TCI =
			bpf_htons((bpf_ntohs(vlan_hdr->h_vlan_TCI) & 0xf000)
				  | TO_VLAN);
	}

	return XDP_PASS;
}

 

#ifndef ETH_ALEN  
#define ETH_ALEN	6	 
#endif
#define VLAN_HDR_SZ	4	 

SEC("xdp_vlan_remove_outer")
int  xdp_prognum2(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data     = (void *)(long)ctx->data;
	struct parse_pkt pkt = { 0 };
	char *dest;

	if (!parse_eth_frame(data, data_end, &pkt))
		return XDP_ABORTED;

	 
	if (pkt.vlan_outer_offset == 0)
		return XDP_PASS;

	 
	dest = data;
	dest += VLAN_HDR_SZ;
	 
	__builtin_memmove(dest, data, ETH_ALEN * 2);
	 

	 
	bpf_xdp_adjust_head(ctx, VLAN_HDR_SZ);

	return XDP_PASS;
}

static __always_inline
void shift_mac_4bytes_32bit(void *data)
{
	__u32 *p = data;

	 
	p[3] = p[2];
	p[2] = p[1];
	p[1] = p[0];
}

SEC("xdp_vlan_remove_outer2")
int  xdp_prognum3(struct xdp_md *ctx)
{
	void *data_end = (void *)(long)ctx->data_end;
	void *data     = (void *)(long)ctx->data;
	struct ethhdr *orig_eth = data;
	struct parse_pkt pkt = { 0 };

	if (!parse_eth_frame(orig_eth, data_end, &pkt))
		return XDP_ABORTED;

	 
	if (pkt.vlan_outer_offset == 0)
		return XDP_PASS;

	 
	shift_mac_4bytes_32bit(data);

	 
	bpf_xdp_adjust_head(ctx, VLAN_HDR_SZ);

	return XDP_PASS;
}

 

SEC("tc_vlan_push")
int _tc_progA(struct __sk_buff *ctx)
{
	bpf_skb_vlan_push(ctx, bpf_htons(ETH_P_8021Q), TESTVLAN);

	return TC_ACT_OK;
}
 
