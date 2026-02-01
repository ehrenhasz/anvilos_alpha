
 

#include "hsr_forward.h"
#include <linux/types.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_vlan.h>
#include "hsr_main.h"
#include "hsr_framereg.h"

struct hsr_node;

 
static bool is_supervision_frame(struct hsr_priv *hsr, struct sk_buff *skb)
{
	struct ethhdr *eth_hdr;
	struct hsr_sup_tag *hsr_sup_tag;
	struct hsrv1_ethhdr_sp *hsr_V1_hdr;
	struct hsr_sup_tlv *hsr_sup_tlv;
	u16 total_length = 0;

	WARN_ON_ONCE(!skb_mac_header_was_set(skb));
	eth_hdr = (struct ethhdr *)skb_mac_header(skb);

	 
	if (!ether_addr_equal(eth_hdr->h_dest,
			      hsr->sup_multicast_addr))
		return false;

	 
	if (!(eth_hdr->h_proto == htons(ETH_P_PRP) ||
	      eth_hdr->h_proto == htons(ETH_P_HSR)))
		return false;

	 
	if (eth_hdr->h_proto == htons(ETH_P_HSR)) {  
		total_length = sizeof(struct hsrv1_ethhdr_sp);
		if (!pskb_may_pull(skb, total_length))
			return false;

		hsr_V1_hdr = (struct hsrv1_ethhdr_sp *)skb_mac_header(skb);
		if (hsr_V1_hdr->hsr.encap_proto != htons(ETH_P_PRP))
			return false;

		hsr_sup_tag = &hsr_V1_hdr->hsr_sup;
	} else {
		total_length = sizeof(struct hsrv0_ethhdr_sp);
		if (!pskb_may_pull(skb, total_length))
			return false;

		hsr_sup_tag =
		     &((struct hsrv0_ethhdr_sp *)skb_mac_header(skb))->hsr_sup;
	}

	if (hsr_sup_tag->tlv.HSR_TLV_type != HSR_TLV_ANNOUNCE &&
	    hsr_sup_tag->tlv.HSR_TLV_type != HSR_TLV_LIFE_CHECK &&
	    hsr_sup_tag->tlv.HSR_TLV_type != PRP_TLV_LIFE_CHECK_DD &&
	    hsr_sup_tag->tlv.HSR_TLV_type != PRP_TLV_LIFE_CHECK_DA)
		return false;
	if (hsr_sup_tag->tlv.HSR_TLV_length != 12 &&
	    hsr_sup_tag->tlv.HSR_TLV_length != sizeof(struct hsr_sup_payload))
		return false;

	 
	total_length += sizeof(struct hsr_sup_tlv) + hsr_sup_tag->tlv.HSR_TLV_length;
	if (!pskb_may_pull(skb, total_length))
		return false;
	skb_pull(skb, total_length);
	hsr_sup_tlv = (struct hsr_sup_tlv *)skb->data;
	skb_push(skb, total_length);

	 
	if (hsr_sup_tlv->HSR_TLV_type == PRP_TLV_REDBOX_MAC) {
		 
		if (hsr_sup_tlv->HSR_TLV_length != sizeof(struct hsr_sup_payload))
			return false;

		 
		total_length += sizeof(struct hsr_sup_tlv) + hsr_sup_tlv->HSR_TLV_length;
		if (!pskb_may_pull(skb, total_length))
			return false;

		 
		skb_pull(skb, total_length);
		hsr_sup_tlv = (struct hsr_sup_tlv *)skb->data;
		skb_push(skb, total_length);
	}

	 
	if (hsr_sup_tlv->HSR_TLV_type == HSR_TLV_EOT &&
	    hsr_sup_tlv->HSR_TLV_length != 0)
		return false;

	return true;
}

static struct sk_buff *create_stripped_skb_hsr(struct sk_buff *skb_in,
					       struct hsr_frame_info *frame)
{
	struct sk_buff *skb;
	int copylen;
	unsigned char *dst, *src;

	skb_pull(skb_in, HSR_HLEN);
	skb = __pskb_copy(skb_in, skb_headroom(skb_in) - HSR_HLEN, GFP_ATOMIC);
	skb_push(skb_in, HSR_HLEN);
	if (!skb)
		return NULL;

	skb_reset_mac_header(skb);

	if (skb->ip_summed == CHECKSUM_PARTIAL)
		skb->csum_start -= HSR_HLEN;

	copylen = 2 * ETH_ALEN;
	if (frame->is_vlan)
		copylen += VLAN_HLEN;
	src = skb_mac_header(skb_in);
	dst = skb_mac_header(skb);
	memcpy(dst, src, copylen);

	skb->protocol = eth_hdr(skb)->h_proto;
	return skb;
}

struct sk_buff *hsr_get_untagged_frame(struct hsr_frame_info *frame,
				       struct hsr_port *port)
{
	if (!frame->skb_std) {
		if (frame->skb_hsr)
			frame->skb_std =
				create_stripped_skb_hsr(frame->skb_hsr, frame);
		else
			netdev_warn_once(port->dev,
					 "Unexpected frame received in hsr_get_untagged_frame()\n");

		if (!frame->skb_std)
			return NULL;
	}

	return skb_clone(frame->skb_std, GFP_ATOMIC);
}

struct sk_buff *prp_get_untagged_frame(struct hsr_frame_info *frame,
				       struct hsr_port *port)
{
	if (!frame->skb_std) {
		if (frame->skb_prp) {
			 
			skb_trim(frame->skb_prp,
				 frame->skb_prp->len - HSR_HLEN);
			frame->skb_std =
				__pskb_copy(frame->skb_prp,
					    skb_headroom(frame->skb_prp),
					    GFP_ATOMIC);
		} else {
			 
			WARN_ONCE(1, "%s:%d: Unexpected frame received (port_src %s)\n",
				  __FILE__, __LINE__, port->dev->name);
			return NULL;
		}
	}

	return skb_clone(frame->skb_std, GFP_ATOMIC);
}

static void prp_set_lan_id(struct prp_rct *trailer,
			   struct hsr_port *port)
{
	int lane_id;

	if (port->type == HSR_PT_SLAVE_A)
		lane_id = 0;
	else
		lane_id = 1;

	 
	lane_id |= port->hsr->net_id;
	set_prp_lan_id(trailer, lane_id);
}

 
static struct sk_buff *prp_fill_rct(struct sk_buff *skb,
				    struct hsr_frame_info *frame,
				    struct hsr_port *port)
{
	struct prp_rct *trailer;
	int min_size = ETH_ZLEN;
	int lsdu_size;

	if (!skb)
		return skb;

	if (frame->is_vlan)
		min_size = VLAN_ETH_ZLEN;

	if (skb_put_padto(skb, min_size))
		return NULL;

	trailer = (struct prp_rct *)skb_put(skb, HSR_HLEN);
	lsdu_size = skb->len - 14;
	if (frame->is_vlan)
		lsdu_size -= 4;
	prp_set_lan_id(trailer, port);
	set_prp_LSDU_size(trailer, lsdu_size);
	trailer->sequence_nr = htons(frame->sequence_nr);
	trailer->PRP_suffix = htons(ETH_P_PRP);
	skb->protocol = eth_hdr(skb)->h_proto;

	return skb;
}

static void hsr_set_path_id(struct hsr_ethhdr *hsr_ethhdr,
			    struct hsr_port *port)
{
	int path_id;

	if (port->type == HSR_PT_SLAVE_A)
		path_id = 0;
	else
		path_id = 1;

	set_hsr_tag_path(&hsr_ethhdr->hsr_tag, path_id);
}

static struct sk_buff *hsr_fill_tag(struct sk_buff *skb,
				    struct hsr_frame_info *frame,
				    struct hsr_port *port, u8 proto_version)
{
	struct hsr_ethhdr *hsr_ethhdr;
	int lsdu_size;

	 
	if (skb_put_padto(skb, ETH_ZLEN + HSR_HLEN))
		return NULL;

	lsdu_size = skb->len - 14;
	if (frame->is_vlan)
		lsdu_size -= 4;

	hsr_ethhdr = (struct hsr_ethhdr *)skb_mac_header(skb);

	hsr_set_path_id(hsr_ethhdr, port);
	set_hsr_tag_LSDU_size(&hsr_ethhdr->hsr_tag, lsdu_size);
	hsr_ethhdr->hsr_tag.sequence_nr = htons(frame->sequence_nr);
	hsr_ethhdr->hsr_tag.encap_proto = hsr_ethhdr->ethhdr.h_proto;
	hsr_ethhdr->ethhdr.h_proto = htons(proto_version ?
			ETH_P_HSR : ETH_P_PRP);
	skb->protocol = hsr_ethhdr->ethhdr.h_proto;

	return skb;
}

 
struct sk_buff *hsr_create_tagged_frame(struct hsr_frame_info *frame,
					struct hsr_port *port)
{
	unsigned char *dst, *src;
	struct sk_buff *skb;
	int movelen;

	if (frame->skb_hsr) {
		struct hsr_ethhdr *hsr_ethhdr =
			(struct hsr_ethhdr *)skb_mac_header(frame->skb_hsr);

		 
		hsr_set_path_id(hsr_ethhdr, port);
		return skb_clone(frame->skb_hsr, GFP_ATOMIC);
	} else if (port->dev->features & NETIF_F_HW_HSR_TAG_INS) {
		return skb_clone(frame->skb_std, GFP_ATOMIC);
	}

	 
	skb = __pskb_copy(frame->skb_std,
			  skb_headroom(frame->skb_std) + HSR_HLEN, GFP_ATOMIC);
	if (!skb)
		return NULL;
	skb_reset_mac_header(skb);

	if (skb->ip_summed == CHECKSUM_PARTIAL)
		skb->csum_start += HSR_HLEN;

	movelen = ETH_HLEN;
	if (frame->is_vlan)
		movelen += VLAN_HLEN;

	src = skb_mac_header(skb);
	dst = skb_push(skb, HSR_HLEN);
	memmove(dst, src, movelen);
	skb_reset_mac_header(skb);

	 
	return hsr_fill_tag(skb, frame, port, port->hsr->prot_version);
}

struct sk_buff *prp_create_tagged_frame(struct hsr_frame_info *frame,
					struct hsr_port *port)
{
	struct sk_buff *skb;

	if (frame->skb_prp) {
		struct prp_rct *trailer = skb_get_PRP_rct(frame->skb_prp);

		if (trailer) {
			prp_set_lan_id(trailer, port);
		} else {
			WARN_ONCE(!trailer, "errored PRP skb");
			return NULL;
		}
		return skb_clone(frame->skb_prp, GFP_ATOMIC);
	} else if (port->dev->features & NETIF_F_HW_HSR_TAG_INS) {
		return skb_clone(frame->skb_std, GFP_ATOMIC);
	}

	skb = skb_copy_expand(frame->skb_std, 0,
			      skb_tailroom(frame->skb_std) + HSR_HLEN,
			      GFP_ATOMIC);
	return prp_fill_rct(skb, frame, port);
}

static void hsr_deliver_master(struct sk_buff *skb, struct net_device *dev,
			       struct hsr_node *node_src)
{
	bool was_multicast_frame;
	int res, recv_len;

	was_multicast_frame = (skb->pkt_type == PACKET_MULTICAST);
	hsr_addr_subst_source(node_src, skb);
	skb_pull(skb, ETH_HLEN);
	recv_len = skb->len;
	res = netif_rx(skb);
	if (res == NET_RX_DROP) {
		dev->stats.rx_dropped++;
	} else {
		dev->stats.rx_packets++;
		dev->stats.rx_bytes += recv_len;
		if (was_multicast_frame)
			dev->stats.multicast++;
	}
}

static int hsr_xmit(struct sk_buff *skb, struct hsr_port *port,
		    struct hsr_frame_info *frame)
{
	if (frame->port_rcv->type == HSR_PT_MASTER) {
		hsr_addr_subst_dest(frame->node_src, skb, port);

		 
		ether_addr_copy(eth_hdr(skb)->h_source, port->dev->dev_addr);
	}
	return dev_queue_xmit(skb);
}

bool prp_drop_frame(struct hsr_frame_info *frame, struct hsr_port *port)
{
	return ((frame->port_rcv->type == HSR_PT_SLAVE_A &&
		 port->type ==  HSR_PT_SLAVE_B) ||
		(frame->port_rcv->type == HSR_PT_SLAVE_B &&
		 port->type ==  HSR_PT_SLAVE_A));
}

bool hsr_drop_frame(struct hsr_frame_info *frame, struct hsr_port *port)
{
	if (port->dev->features & NETIF_F_HW_HSR_FWD)
		return prp_drop_frame(frame, port);

	return false;
}

 
static void hsr_forward_do(struct hsr_frame_info *frame)
{
	struct hsr_port *port;
	struct sk_buff *skb;
	bool sent = false;

	hsr_for_each_port(frame->port_rcv->hsr, port) {
		struct hsr_priv *hsr = port->hsr;
		 
		if (port == frame->port_rcv)
			continue;

		 
		if (port->type == HSR_PT_MASTER && !frame->is_local_dest)
			continue;

		 
		if (port->type != HSR_PT_MASTER && frame->is_local_exclusive)
			continue;

		 
		if ((port->dev->features & NETIF_F_HW_HSR_DUP) && sent)
			continue;

		 
		if (!frame->is_from_san &&
		    hsr_register_frame_out(port, frame->node_src,
					   frame->sequence_nr))
			continue;

		if (frame->is_supervision && port->type == HSR_PT_MASTER) {
			hsr_handle_sup_frame(frame);
			continue;
		}

		 
		if (hsr->proto_ops->drop_frame &&
		    hsr->proto_ops->drop_frame(frame, port))
			continue;

		if (port->type != HSR_PT_MASTER)
			skb = hsr->proto_ops->create_tagged_frame(frame, port);
		else
			skb = hsr->proto_ops->get_untagged_frame(frame, port);

		if (!skb) {
			frame->port_rcv->dev->stats.rx_dropped++;
			continue;
		}

		skb->dev = port->dev;
		if (port->type == HSR_PT_MASTER) {
			hsr_deliver_master(skb, port->dev, frame->node_src);
		} else {
			if (!hsr_xmit(skb, port, frame))
				sent = true;
		}
	}
}

static void check_local_dest(struct hsr_priv *hsr, struct sk_buff *skb,
			     struct hsr_frame_info *frame)
{
	if (hsr_addr_is_self(hsr, eth_hdr(skb)->h_dest)) {
		frame->is_local_exclusive = true;
		skb->pkt_type = PACKET_HOST;
	} else {
		frame->is_local_exclusive = false;
	}

	if (skb->pkt_type == PACKET_HOST ||
	    skb->pkt_type == PACKET_MULTICAST ||
	    skb->pkt_type == PACKET_BROADCAST) {
		frame->is_local_dest = true;
	} else {
		frame->is_local_dest = false;
	}
}

static void handle_std_frame(struct sk_buff *skb,
			     struct hsr_frame_info *frame)
{
	struct hsr_port *port = frame->port_rcv;
	struct hsr_priv *hsr = port->hsr;

	frame->skb_hsr = NULL;
	frame->skb_prp = NULL;
	frame->skb_std = skb;

	if (port->type != HSR_PT_MASTER) {
		frame->is_from_san = true;
	} else {
		 
		lockdep_assert_held(&hsr->seqnr_lock);
		frame->sequence_nr = hsr->sequence_nr;
		hsr->sequence_nr++;
	}
}

int hsr_fill_frame_info(__be16 proto, struct sk_buff *skb,
			struct hsr_frame_info *frame)
{
	struct hsr_port *port = frame->port_rcv;
	struct hsr_priv *hsr = port->hsr;

	 
	if ((!hsr->prot_version && proto == htons(ETH_P_PRP)) ||
	    proto == htons(ETH_P_HSR)) {
		 
		if (skb->mac_len < sizeof(struct hsr_ethhdr))
			return -EINVAL;

		 
		frame->skb_std = NULL;
		frame->skb_prp = NULL;
		frame->skb_hsr = skb;
		frame->sequence_nr = hsr_get_skb_sequence_nr(skb);
		return 0;
	}

	 
	handle_std_frame(skb, frame);

	return 0;
}

int prp_fill_frame_info(__be16 proto, struct sk_buff *skb,
			struct hsr_frame_info *frame)
{
	 
	struct prp_rct *rct = skb_get_PRP_rct(skb);

	if (rct &&
	    prp_check_lsdu_size(skb, rct, frame->is_supervision)) {
		frame->skb_hsr = NULL;
		frame->skb_std = NULL;
		frame->skb_prp = skb;
		frame->sequence_nr = prp_get_skb_sequence_nr(rct);
		return 0;
	}
	handle_std_frame(skb, frame);

	return 0;
}

static int fill_frame_info(struct hsr_frame_info *frame,
			   struct sk_buff *skb, struct hsr_port *port)
{
	struct hsr_priv *hsr = port->hsr;
	struct hsr_vlan_ethhdr *vlan_hdr;
	struct ethhdr *ethhdr;
	__be16 proto;
	int ret;

	 
	if (skb->mac_len < sizeof(struct ethhdr))
		return -EINVAL;

	memset(frame, 0, sizeof(*frame));
	frame->is_supervision = is_supervision_frame(port->hsr, skb);
	frame->node_src = hsr_get_node(port, &hsr->node_db, skb,
				       frame->is_supervision,
				       port->type);
	if (!frame->node_src)
		return -1;  

	ethhdr = (struct ethhdr *)skb_mac_header(skb);
	frame->is_vlan = false;
	proto = ethhdr->h_proto;

	if (proto == htons(ETH_P_8021Q))
		frame->is_vlan = true;

	if (frame->is_vlan) {
		vlan_hdr = (struct hsr_vlan_ethhdr *)ethhdr;
		proto = vlan_hdr->vlanhdr.h_vlan_encapsulated_proto;
		 
		netdev_warn_once(skb->dev, "VLAN not yet supported");
		return -EINVAL;
	}

	frame->is_from_san = false;
	frame->port_rcv = port;
	ret = hsr->proto_ops->fill_frame_info(proto, skb, frame);
	if (ret)
		return ret;

	check_local_dest(port->hsr, skb, frame);

	return 0;
}

 
void hsr_forward_skb(struct sk_buff *skb, struct hsr_port *port)
{
	struct hsr_frame_info frame;

	rcu_read_lock();
	if (fill_frame_info(&frame, skb, port) < 0)
		goto out_drop;

	hsr_register_frame_in(frame.node_src, port, frame.sequence_nr);
	hsr_forward_do(&frame);
	rcu_read_unlock();
	 
	if (port->type == HSR_PT_MASTER) {
		port->dev->stats.tx_packets++;
		port->dev->stats.tx_bytes += skb->len;
	}

	kfree_skb(frame.skb_hsr);
	kfree_skb(frame.skb_prp);
	kfree_skb(frame.skb_std);
	return;

out_drop:
	rcu_read_unlock();
	port->dev->stats.tx_dropped++;
	kfree_skb(skb);
}
