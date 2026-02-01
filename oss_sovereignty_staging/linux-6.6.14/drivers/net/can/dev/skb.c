
 

#include <linux/can/dev.h>
#include <linux/module.h>

#define MOD_DESC "CAN device driver interface"

MODULE_DESCRIPTION(MOD_DESC);
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Wolfgang Grandegger <wg@grandegger.com>");

 
void can_flush_echo_skb(struct net_device *dev)
{
	struct can_priv *priv = netdev_priv(dev);
	struct net_device_stats *stats = &dev->stats;
	int i;

	for (i = 0; i < priv->echo_skb_max; i++) {
		if (priv->echo_skb[i]) {
			kfree_skb(priv->echo_skb[i]);
			priv->echo_skb[i] = NULL;
			stats->tx_dropped++;
			stats->tx_aborted_errors++;
		}
	}
}

 
int can_put_echo_skb(struct sk_buff *skb, struct net_device *dev,
		     unsigned int idx, unsigned int frame_len)
{
	struct can_priv *priv = netdev_priv(dev);

	if (idx >= priv->echo_skb_max) {
		netdev_err(dev, "%s: BUG! Trying to access can_priv::echo_skb out of bounds (%u/max %u)\n",
			   __func__, idx, priv->echo_skb_max);
		return -EINVAL;
	}

	 
	if (!(dev->flags & IFF_ECHO) ||
	    (skb->protocol != htons(ETH_P_CAN) &&
	     skb->protocol != htons(ETH_P_CANFD) &&
	     skb->protocol != htons(ETH_P_CANXL))) {
		kfree_skb(skb);
		return 0;
	}

	if (!priv->echo_skb[idx]) {
		skb = can_create_echo_skb(skb);
		if (!skb)
			return -ENOMEM;

		 
		skb->ip_summed = CHECKSUM_UNNECESSARY;
		skb->dev = dev;

		 
		can_skb_prv(skb)->frame_len = frame_len;

		if (skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)
			skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;

		skb_tx_timestamp(skb);

		 
		priv->echo_skb[idx] = skb;
	} else {
		 
		netdev_err(dev, "%s: BUG! echo_skb %d is occupied!\n", __func__, idx);
		kfree_skb(skb);
		return -EBUSY;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(can_put_echo_skb);

struct sk_buff *
__can_get_echo_skb(struct net_device *dev, unsigned int idx,
		   unsigned int *len_ptr, unsigned int *frame_len_ptr)
{
	struct can_priv *priv = netdev_priv(dev);

	if (idx >= priv->echo_skb_max) {
		netdev_err(dev, "%s: BUG! Trying to access can_priv::echo_skb out of bounds (%u/max %u)\n",
			   __func__, idx, priv->echo_skb_max);
		return NULL;
	}

	if (priv->echo_skb[idx]) {
		 
		struct sk_buff *skb = priv->echo_skb[idx];
		struct can_skb_priv *can_skb_priv = can_skb_prv(skb);

		if (skb_shinfo(skb)->tx_flags & SKBTX_IN_PROGRESS)
			skb_tstamp_tx(skb, skb_hwtstamps(skb));

		 
		*len_ptr = can_skb_get_data_len(skb);

		if (frame_len_ptr)
			*frame_len_ptr = can_skb_priv->frame_len;

		priv->echo_skb[idx] = NULL;

		if (skb->pkt_type == PACKET_LOOPBACK) {
			skb->pkt_type = PACKET_BROADCAST;
		} else {
			dev_consume_skb_any(skb);
			return NULL;
		}

		return skb;
	}

	return NULL;
}

 
unsigned int can_get_echo_skb(struct net_device *dev, unsigned int idx,
			      unsigned int *frame_len_ptr)
{
	struct sk_buff *skb;
	unsigned int len;

	skb = __can_get_echo_skb(dev, idx, &len, frame_len_ptr);
	if (!skb)
		return 0;

	skb_get(skb);
	if (netif_rx(skb) == NET_RX_SUCCESS)
		dev_consume_skb_any(skb);
	else
		dev_kfree_skb_any(skb);

	return len;
}
EXPORT_SYMBOL_GPL(can_get_echo_skb);

 
void can_free_echo_skb(struct net_device *dev, unsigned int idx,
		       unsigned int *frame_len_ptr)
{
	struct can_priv *priv = netdev_priv(dev);

	if (idx >= priv->echo_skb_max) {
		netdev_err(dev, "%s: BUG! Trying to access can_priv::echo_skb out of bounds (%u/max %u)\n",
			   __func__, idx, priv->echo_skb_max);
		return;
	}

	if (priv->echo_skb[idx]) {
		struct sk_buff *skb = priv->echo_skb[idx];
		struct can_skb_priv *can_skb_priv = can_skb_prv(skb);

		if (frame_len_ptr)
			*frame_len_ptr = can_skb_priv->frame_len;

		dev_kfree_skb_any(skb);
		priv->echo_skb[idx] = NULL;
	}
}
EXPORT_SYMBOL_GPL(can_free_echo_skb);

 
static void init_can_skb_reserve(struct sk_buff *skb)
{
	skb->pkt_type = PACKET_BROADCAST;
	skb->ip_summed = CHECKSUM_UNNECESSARY;

	skb_reset_mac_header(skb);
	skb_reset_network_header(skb);
	skb_reset_transport_header(skb);

	can_skb_reserve(skb);
	can_skb_prv(skb)->skbcnt = 0;
}

struct sk_buff *alloc_can_skb(struct net_device *dev, struct can_frame **cf)
{
	struct sk_buff *skb;

	skb = netdev_alloc_skb(dev, sizeof(struct can_skb_priv) +
			       sizeof(struct can_frame));
	if (unlikely(!skb)) {
		*cf = NULL;

		return NULL;
	}

	skb->protocol = htons(ETH_P_CAN);
	init_can_skb_reserve(skb);
	can_skb_prv(skb)->ifindex = dev->ifindex;

	*cf = skb_put_zero(skb, sizeof(struct can_frame));

	return skb;
}
EXPORT_SYMBOL_GPL(alloc_can_skb);

struct sk_buff *alloc_canfd_skb(struct net_device *dev,
				struct canfd_frame **cfd)
{
	struct sk_buff *skb;

	skb = netdev_alloc_skb(dev, sizeof(struct can_skb_priv) +
			       sizeof(struct canfd_frame));
	if (unlikely(!skb)) {
		*cfd = NULL;

		return NULL;
	}

	skb->protocol = htons(ETH_P_CANFD);
	init_can_skb_reserve(skb);
	can_skb_prv(skb)->ifindex = dev->ifindex;

	*cfd = skb_put_zero(skb, sizeof(struct canfd_frame));

	 
	(*cfd)->flags = CANFD_FDF;

	return skb;
}
EXPORT_SYMBOL_GPL(alloc_canfd_skb);

struct sk_buff *alloc_canxl_skb(struct net_device *dev,
				struct canxl_frame **cxl,
				unsigned int data_len)
{
	struct sk_buff *skb;

	if (data_len < CANXL_MIN_DLEN || data_len > CANXL_MAX_DLEN)
		goto out_error;

	skb = netdev_alloc_skb(dev, sizeof(struct can_skb_priv) +
			       CANXL_HDR_SIZE + data_len);
	if (unlikely(!skb))
		goto out_error;

	skb->protocol = htons(ETH_P_CANXL);
	init_can_skb_reserve(skb);
	can_skb_prv(skb)->ifindex = dev->ifindex;

	*cxl = skb_put_zero(skb, CANXL_HDR_SIZE + data_len);

	 
	(*cxl)->flags = CANXL_XLF;
	(*cxl)->len = data_len;

	return skb;

out_error:
	*cxl = NULL;

	return NULL;
}
EXPORT_SYMBOL_GPL(alloc_canxl_skb);

struct sk_buff *alloc_can_err_skb(struct net_device *dev, struct can_frame **cf)
{
	struct sk_buff *skb;

	skb = alloc_can_skb(dev, cf);
	if (unlikely(!skb))
		return NULL;

	(*cf)->can_id = CAN_ERR_FLAG;
	(*cf)->len = CAN_ERR_DLC;

	return skb;
}
EXPORT_SYMBOL_GPL(alloc_can_err_skb);

 
static bool can_skb_headroom_valid(struct net_device *dev, struct sk_buff *skb)
{
	 
	if (WARN_ON_ONCE(skb_headroom(skb) < sizeof(struct can_skb_priv)))
		return false;

	 
	if (skb->ip_summed == CHECKSUM_NONE) {
		 
		can_skb_prv(skb)->ifindex = dev->ifindex;
		can_skb_prv(skb)->skbcnt = 0;

		skb->ip_summed = CHECKSUM_UNNECESSARY;

		 
		if (dev->flags & IFF_ECHO)
			skb->pkt_type = PACKET_LOOPBACK;
		else
			skb->pkt_type = PACKET_HOST;

		skb_reset_mac_header(skb);
		skb_reset_network_header(skb);
		skb_reset_transport_header(skb);

		 
		if (can_is_canfd_skb(skb)) {
			struct canfd_frame *cfd;

			cfd = (struct canfd_frame *)skb->data;
			cfd->flags |= CANFD_FDF;
		}
	}

	return true;
}

 
bool can_dropped_invalid_skb(struct net_device *dev, struct sk_buff *skb)
{
	switch (ntohs(skb->protocol)) {
	case ETH_P_CAN:
		if (!can_is_can_skb(skb))
			goto inval_skb;
		break;

	case ETH_P_CANFD:
		if (!can_is_canfd_skb(skb))
			goto inval_skb;
		break;

	case ETH_P_CANXL:
		if (!can_is_canxl_skb(skb))
			goto inval_skb;
		break;

	default:
		goto inval_skb;
	}

	if (!can_skb_headroom_valid(dev, skb))
		goto inval_skb;

	return false;

inval_skb:
	kfree_skb(skb);
	dev->stats.tx_dropped++;
	return true;
}
EXPORT_SYMBOL_GPL(can_dropped_invalid_skb);
