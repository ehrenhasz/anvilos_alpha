
#include <linux/skbuff.h>
#include <linux/sctp.h>
#include <net/gso.h>
#include <net/gro.h>

 
struct sk_buff *skb_eth_gso_segment(struct sk_buff *skb,
				    netdev_features_t features, __be16 type)
{
	struct sk_buff *segs = ERR_PTR(-EPROTONOSUPPORT);
	struct packet_offload *ptype;

	rcu_read_lock();
	list_for_each_entry_rcu(ptype, &offload_base, list) {
		if (ptype->type == type && ptype->callbacks.gso_segment) {
			segs = ptype->callbacks.gso_segment(skb, features);
			break;
		}
	}
	rcu_read_unlock();

	return segs;
}
EXPORT_SYMBOL(skb_eth_gso_segment);

 
struct sk_buff *skb_mac_gso_segment(struct sk_buff *skb,
				    netdev_features_t features)
{
	struct sk_buff *segs = ERR_PTR(-EPROTONOSUPPORT);
	struct packet_offload *ptype;
	int vlan_depth = skb->mac_len;
	__be16 type = skb_network_protocol(skb, &vlan_depth);

	if (unlikely(!type))
		return ERR_PTR(-EINVAL);

	__skb_pull(skb, vlan_depth);

	rcu_read_lock();
	list_for_each_entry_rcu(ptype, &offload_base, list) {
		if (ptype->type == type && ptype->callbacks.gso_segment) {
			segs = ptype->callbacks.gso_segment(skb, features);
			break;
		}
	}
	rcu_read_unlock();

	__skb_push(skb, skb->data - skb_mac_header(skb));

	return segs;
}
EXPORT_SYMBOL(skb_mac_gso_segment);
 
static bool skb_needs_check(const struct sk_buff *skb, bool tx_path)
{
	if (tx_path)
		return skb->ip_summed != CHECKSUM_PARTIAL &&
		       skb->ip_summed != CHECKSUM_UNNECESSARY;

	return skb->ip_summed == CHECKSUM_NONE;
}

 
struct sk_buff *__skb_gso_segment(struct sk_buff *skb,
				  netdev_features_t features, bool tx_path)
{
	struct sk_buff *segs;

	if (unlikely(skb_needs_check(skb, tx_path))) {
		int err;

		 
		err = skb_cow_head(skb, 0);
		if (err < 0)
			return ERR_PTR(err);
	}

	 
	if (features & NETIF_F_GSO_PARTIAL) {
		netdev_features_t partial_features = NETIF_F_GSO_ROBUST;
		struct net_device *dev = skb->dev;

		partial_features |= dev->features & dev->gso_partial_features;
		if (!skb_gso_ok(skb, features | partial_features))
			features &= ~NETIF_F_GSO_PARTIAL;
	}

	BUILD_BUG_ON(SKB_GSO_CB_OFFSET +
		     sizeof(*SKB_GSO_CB(skb)) > sizeof(skb->cb));

	SKB_GSO_CB(skb)->mac_offset = skb_headroom(skb);
	SKB_GSO_CB(skb)->encap_level = 0;

	skb_reset_mac_header(skb);
	skb_reset_mac_len(skb);

	segs = skb_mac_gso_segment(skb, features);

	if (segs != skb && unlikely(skb_needs_check(skb, tx_path) && !IS_ERR(segs)))
		skb_warn_bad_offload(skb);

	return segs;
}
EXPORT_SYMBOL(__skb_gso_segment);

 
static unsigned int skb_gso_transport_seglen(const struct sk_buff *skb)
{
	const struct skb_shared_info *shinfo = skb_shinfo(skb);
	unsigned int thlen = 0;

	if (skb->encapsulation) {
		thlen = skb_inner_transport_header(skb) -
			skb_transport_header(skb);

		if (likely(shinfo->gso_type & (SKB_GSO_TCPV4 | SKB_GSO_TCPV6)))
			thlen += inner_tcp_hdrlen(skb);
	} else if (likely(shinfo->gso_type & (SKB_GSO_TCPV4 | SKB_GSO_TCPV6))) {
		thlen = tcp_hdrlen(skb);
	} else if (unlikely(skb_is_gso_sctp(skb))) {
		thlen = sizeof(struct sctphdr);
	} else if (shinfo->gso_type & SKB_GSO_UDP_L4) {
		thlen = sizeof(struct udphdr);
	}
	 
	return thlen + shinfo->gso_size;
}

 
static unsigned int skb_gso_network_seglen(const struct sk_buff *skb)
{
	unsigned int hdr_len = skb_transport_header(skb) -
			       skb_network_header(skb);

	return hdr_len + skb_gso_transport_seglen(skb);
}

 
static unsigned int skb_gso_mac_seglen(const struct sk_buff *skb)
{
	unsigned int hdr_len = skb_transport_header(skb) - skb_mac_header(skb);

	return hdr_len + skb_gso_transport_seglen(skb);
}

 
static inline bool skb_gso_size_check(const struct sk_buff *skb,
				      unsigned int seg_len,
				      unsigned int max_len) {
	const struct skb_shared_info *shinfo = skb_shinfo(skb);
	const struct sk_buff *iter;

	if (shinfo->gso_size != GSO_BY_FRAGS)
		return seg_len <= max_len;

	 
	seg_len -= GSO_BY_FRAGS;

	skb_walk_frags(skb, iter) {
		if (seg_len + skb_headlen(iter) > max_len)
			return false;
	}

	return true;
}

 
bool skb_gso_validate_network_len(const struct sk_buff *skb, unsigned int mtu)
{
	return skb_gso_size_check(skb, skb_gso_network_seglen(skb), mtu);
}
EXPORT_SYMBOL_GPL(skb_gso_validate_network_len);

 
bool skb_gso_validate_mac_len(const struct sk_buff *skb, unsigned int len)
{
	return skb_gso_size_check(skb, skb_gso_mac_seglen(skb), len);
}
EXPORT_SYMBOL_GPL(skb_gso_validate_mac_len);

