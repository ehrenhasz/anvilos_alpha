
 

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/socket.h>
#include <linux/in.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/fddidevice.h>
#include <linux/if_ether.h>
#include <linux/skbuff.h>
#include <linux/errno.h>
#include <net/arp.h>
#include <net/sock.h>

 

static int fddi_header(struct sk_buff *skb, struct net_device *dev,
		       unsigned short type,
		       const void *daddr, const void *saddr, unsigned int len)
{
	int hl = FDDI_K_SNAP_HLEN;
	struct fddihdr *fddi;

	if(type != ETH_P_IP && type != ETH_P_IPV6 && type != ETH_P_ARP)
		hl=FDDI_K_8022_HLEN-3;
	fddi = skb_push(skb, hl);
	fddi->fc			 = FDDI_FC_K_ASYNC_LLC_DEF;
	if(type == ETH_P_IP || type == ETH_P_IPV6 || type == ETH_P_ARP)
	{
		fddi->hdr.llc_snap.dsap		 = FDDI_EXTENDED_SAP;
		fddi->hdr.llc_snap.ssap		 = FDDI_EXTENDED_SAP;
		fddi->hdr.llc_snap.ctrl		 = FDDI_UI_CMD;
		fddi->hdr.llc_snap.oui[0]	 = 0x00;
		fddi->hdr.llc_snap.oui[1]	 = 0x00;
		fddi->hdr.llc_snap.oui[2]	 = 0x00;
		fddi->hdr.llc_snap.ethertype	 = htons(type);
	}

	 

	if (saddr != NULL)
		memcpy(fddi->saddr, saddr, dev->addr_len);
	else
		memcpy(fddi->saddr, dev->dev_addr, dev->addr_len);

	if (daddr != NULL)
	{
		memcpy(fddi->daddr, daddr, dev->addr_len);
		return hl;
	}

	return -hl;
}

 

__be16 fddi_type_trans(struct sk_buff *skb, struct net_device *dev)
{
	struct fddihdr *fddi = (struct fddihdr *)skb->data;
	__be16 type;

	 

	skb->dev = dev;
	skb_reset_mac_header(skb);	 

	if(fddi->hdr.llc_8022_1.dsap==0xe0)
	{
		skb_pull(skb, FDDI_K_8022_HLEN-3);
		type = htons(ETH_P_802_2);
	}
	else
	{
		skb_pull(skb, FDDI_K_SNAP_HLEN);		 
		type=fddi->hdr.llc_snap.ethertype;
	}

	 

	if (*fddi->daddr & 0x01)
	{
		if (memcmp(fddi->daddr, dev->broadcast, FDDI_K_ALEN) == 0)
			skb->pkt_type = PACKET_BROADCAST;
		else
			skb->pkt_type = PACKET_MULTICAST;
	}

	else if (dev->flags & IFF_PROMISC)
	{
		if (memcmp(fddi->daddr, dev->dev_addr, FDDI_K_ALEN))
			skb->pkt_type = PACKET_OTHERHOST;
	}

	 

	return type;
}

EXPORT_SYMBOL(fddi_type_trans);

static const struct header_ops fddi_header_ops = {
	.create		= fddi_header,
};


static void fddi_setup(struct net_device *dev)
{
	dev->header_ops		= &fddi_header_ops;
	dev->type		= ARPHRD_FDDI;
	dev->hard_header_len	= FDDI_K_SNAP_HLEN+3;	 
	dev->mtu		= FDDI_K_SNAP_DLEN;	 
	dev->min_mtu		= FDDI_K_SNAP_HLEN;
	dev->max_mtu		= FDDI_K_SNAP_DLEN;
	dev->addr_len		= FDDI_K_ALEN;
	dev->tx_queue_len	= 100;			 
	dev->flags		= IFF_BROADCAST | IFF_MULTICAST;

	memset(dev->broadcast, 0xFF, FDDI_K_ALEN);
}

 
struct net_device *alloc_fddidev(int sizeof_priv)
{
	return alloc_netdev(sizeof_priv, "fddi%d", NET_NAME_UNKNOWN,
			    fddi_setup);
}
EXPORT_SYMBOL(alloc_fddidev);

MODULE_LICENSE("GPL");
