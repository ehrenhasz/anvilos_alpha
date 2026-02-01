
 

#include "send.h"
#include "main.h"

#include <linux/atomic.h>
#include <linux/bug.h>
#include <linux/byteorder/generic.h>
#include <linux/container_of.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/gfp.h>
#include <linux/if.h>
#include <linux/if_ether.h>
#include <linux/jiffies.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/printk.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/workqueue.h>

#include "distributed-arp-table.h"
#include "fragmentation.h"
#include "gateway_client.h"
#include "hard-interface.h"
#include "log.h"
#include "network-coding.h"
#include "originator.h"
#include "routing.h"
#include "soft-interface.h"
#include "translation-table.h"

static void batadv_send_outstanding_bcast_packet(struct work_struct *work);

 
int batadv_send_skb_packet(struct sk_buff *skb,
			   struct batadv_hard_iface *hard_iface,
			   const u8 *dst_addr)
{
	struct batadv_priv *bat_priv;
	struct ethhdr *ethhdr;
	int ret;

	bat_priv = netdev_priv(hard_iface->soft_iface);

	if (hard_iface->if_status != BATADV_IF_ACTIVE)
		goto send_skb_err;

	if (unlikely(!hard_iface->net_dev))
		goto send_skb_err;

	if (!(hard_iface->net_dev->flags & IFF_UP)) {
		pr_warn("Interface %s is not up - can't send packet via that interface!\n",
			hard_iface->net_dev->name);
		goto send_skb_err;
	}

	 
	if (batadv_skb_head_push(skb, ETH_HLEN) < 0)
		goto send_skb_err;

	skb_reset_mac_header(skb);

	ethhdr = eth_hdr(skb);
	ether_addr_copy(ethhdr->h_source, hard_iface->net_dev->dev_addr);
	ether_addr_copy(ethhdr->h_dest, dst_addr);
	ethhdr->h_proto = htons(ETH_P_BATMAN);

	skb_set_network_header(skb, ETH_HLEN);
	skb->protocol = htons(ETH_P_BATMAN);

	skb->dev = hard_iface->net_dev;

	 
	batadv_nc_skb_store_for_decoding(bat_priv, skb);

	 
	ret = dev_queue_xmit(skb);
	return net_xmit_eval(ret);
send_skb_err:
	kfree_skb(skb);
	return NET_XMIT_DROP;
}

 
int batadv_send_broadcast_skb(struct sk_buff *skb,
			      struct batadv_hard_iface *hard_iface)
{
	return batadv_send_skb_packet(skb, hard_iface, batadv_broadcast_addr);
}

 
int batadv_send_unicast_skb(struct sk_buff *skb,
			    struct batadv_neigh_node *neigh)
{
#ifdef CONFIG_BATMAN_ADV_BATMAN_V
	struct batadv_hardif_neigh_node *hardif_neigh;
#endif
	int ret;

	ret = batadv_send_skb_packet(skb, neigh->if_incoming, neigh->addr);

#ifdef CONFIG_BATMAN_ADV_BATMAN_V
	hardif_neigh = batadv_hardif_neigh_get(neigh->if_incoming, neigh->addr);

	if (hardif_neigh && ret != NET_XMIT_DROP)
		hardif_neigh->bat_v.last_unicast_tx = jiffies;

	batadv_hardif_neigh_put(hardif_neigh);
#endif

	return ret;
}

 
int batadv_send_skb_to_orig(struct sk_buff *skb,
			    struct batadv_orig_node *orig_node,
			    struct batadv_hard_iface *recv_if)
{
	struct batadv_priv *bat_priv = orig_node->bat_priv;
	struct batadv_neigh_node *neigh_node;
	int ret;

	 
	neigh_node = batadv_find_router(bat_priv, orig_node, recv_if);
	if (!neigh_node) {
		ret = -EINVAL;
		goto free_skb;
	}

	 
	if (atomic_read(&bat_priv->fragmentation) &&
	    skb->len > neigh_node->if_incoming->net_dev->mtu) {
		 
		ret = batadv_frag_send_packet(skb, orig_node, neigh_node);
		 
		skb = NULL;

		goto put_neigh_node;
	}

	 
	if (recv_if && batadv_nc_skb_forward(skb, neigh_node))
		ret = -EINPROGRESS;
	else
		ret = batadv_send_unicast_skb(skb, neigh_node);

	 
	skb = NULL;

put_neigh_node:
	batadv_neigh_node_put(neigh_node);
free_skb:
	kfree_skb(skb);

	return ret;
}

 
static bool
batadv_send_skb_push_fill_unicast(struct sk_buff *skb, int hdr_size,
				  struct batadv_orig_node *orig_node)
{
	struct batadv_unicast_packet *unicast_packet;
	u8 ttvn = (u8)atomic_read(&orig_node->last_ttvn);

	if (batadv_skb_head_push(skb, hdr_size) < 0)
		return false;

	unicast_packet = (struct batadv_unicast_packet *)skb->data;
	unicast_packet->version = BATADV_COMPAT_VERSION;
	 
	unicast_packet->packet_type = BATADV_UNICAST;
	 
	unicast_packet->ttl = BATADV_TTL;
	 
	ether_addr_copy(unicast_packet->dest, orig_node->orig);
	 
	unicast_packet->ttvn = ttvn;

	return true;
}

 
static bool batadv_send_skb_prepare_unicast(struct sk_buff *skb,
					    struct batadv_orig_node *orig_node)
{
	size_t uni_size = sizeof(struct batadv_unicast_packet);

	return batadv_send_skb_push_fill_unicast(skb, uni_size, orig_node);
}

 
bool batadv_send_skb_prepare_unicast_4addr(struct batadv_priv *bat_priv,
					   struct sk_buff *skb,
					   struct batadv_orig_node *orig,
					   int packet_subtype)
{
	struct batadv_hard_iface *primary_if;
	struct batadv_unicast_4addr_packet *uc_4addr_packet;
	bool ret = false;

	primary_if = batadv_primary_if_get_selected(bat_priv);
	if (!primary_if)
		goto out;

	 
	if (!batadv_send_skb_push_fill_unicast(skb, sizeof(*uc_4addr_packet),
					       orig))
		goto out;

	uc_4addr_packet = (struct batadv_unicast_4addr_packet *)skb->data;
	uc_4addr_packet->u.packet_type = BATADV_UNICAST_4ADDR;
	ether_addr_copy(uc_4addr_packet->src, primary_if->net_dev->dev_addr);
	uc_4addr_packet->subtype = packet_subtype;
	uc_4addr_packet->reserved = 0;

	ret = true;
out:
	batadv_hardif_put(primary_if);
	return ret;
}

 
int batadv_send_skb_unicast(struct batadv_priv *bat_priv,
			    struct sk_buff *skb, int packet_type,
			    int packet_subtype,
			    struct batadv_orig_node *orig_node,
			    unsigned short vid)
{
	struct batadv_unicast_packet *unicast_packet;
	struct ethhdr *ethhdr;
	int ret = NET_XMIT_DROP;

	if (!orig_node)
		goto out;

	switch (packet_type) {
	case BATADV_UNICAST:
		if (!batadv_send_skb_prepare_unicast(skb, orig_node))
			goto out;
		break;
	case BATADV_UNICAST_4ADDR:
		if (!batadv_send_skb_prepare_unicast_4addr(bat_priv, skb,
							   orig_node,
							   packet_subtype))
			goto out;
		break;
	default:
		 
		goto out;
	}

	 
	ethhdr = eth_hdr(skb);
	unicast_packet = (struct batadv_unicast_packet *)skb->data;

	 
	if (batadv_tt_global_client_is_roaming(bat_priv, ethhdr->h_dest, vid))
		unicast_packet->ttvn = unicast_packet->ttvn - 1;

	ret = batadv_send_skb_to_orig(skb, orig_node, NULL);
	  
	skb = NULL;

out:
	kfree_skb(skb);
	return ret;
}

 
int batadv_send_skb_via_tt_generic(struct batadv_priv *bat_priv,
				   struct sk_buff *skb, int packet_type,
				   int packet_subtype, u8 *dst_hint,
				   unsigned short vid)
{
	struct ethhdr *ethhdr = (struct ethhdr *)skb->data;
	struct batadv_orig_node *orig_node;
	u8 *src, *dst;
	int ret;

	src = ethhdr->h_source;
	dst = ethhdr->h_dest;

	 
	if (dst_hint) {
		src = NULL;
		dst = dst_hint;
	}
	orig_node = batadv_transtable_search(bat_priv, src, dst, vid);

	ret = batadv_send_skb_unicast(bat_priv, skb, packet_type,
				      packet_subtype, orig_node, vid);

	batadv_orig_node_put(orig_node);

	return ret;
}

 
int batadv_send_skb_via_gw(struct batadv_priv *bat_priv, struct sk_buff *skb,
			   unsigned short vid)
{
	struct batadv_orig_node *orig_node;
	int ret;

	orig_node = batadv_gw_get_selected_orig(bat_priv);
	ret = batadv_send_skb_unicast(bat_priv, skb, BATADV_UNICAST_4ADDR,
				      BATADV_P_DATA, orig_node, vid);

	batadv_orig_node_put(orig_node);

	return ret;
}

 
void batadv_forw_packet_free(struct batadv_forw_packet *forw_packet,
			     bool dropped)
{
	if (dropped)
		kfree_skb(forw_packet->skb);
	else
		consume_skb(forw_packet->skb);

	batadv_hardif_put(forw_packet->if_incoming);
	batadv_hardif_put(forw_packet->if_outgoing);
	if (forw_packet->queue_left)
		atomic_inc(forw_packet->queue_left);
	kfree(forw_packet);
}

 
struct batadv_forw_packet *
batadv_forw_packet_alloc(struct batadv_hard_iface *if_incoming,
			 struct batadv_hard_iface *if_outgoing,
			 atomic_t *queue_left,
			 struct batadv_priv *bat_priv,
			 struct sk_buff *skb)
{
	struct batadv_forw_packet *forw_packet;
	const char *qname;

	if (queue_left && !batadv_atomic_dec_not_zero(queue_left)) {
		qname = "unknown";

		if (queue_left == &bat_priv->bcast_queue_left)
			qname = "bcast";

		if (queue_left == &bat_priv->batman_queue_left)
			qname = "batman";

		batadv_dbg(BATADV_DBG_BATMAN, bat_priv,
			   "%s queue is full\n", qname);

		return NULL;
	}

	forw_packet = kmalloc(sizeof(*forw_packet), GFP_ATOMIC);
	if (!forw_packet)
		goto err;

	if (if_incoming)
		kref_get(&if_incoming->refcount);

	if (if_outgoing)
		kref_get(&if_outgoing->refcount);

	INIT_HLIST_NODE(&forw_packet->list);
	INIT_HLIST_NODE(&forw_packet->cleanup_list);
	forw_packet->skb = skb;
	forw_packet->queue_left = queue_left;
	forw_packet->if_incoming = if_incoming;
	forw_packet->if_outgoing = if_outgoing;
	forw_packet->num_packets = 0;

	return forw_packet;

err:
	if (queue_left)
		atomic_inc(queue_left);

	return NULL;
}

 
static bool
batadv_forw_packet_was_stolen(struct batadv_forw_packet *forw_packet)
{
	return !hlist_unhashed(&forw_packet->cleanup_list);
}

 
bool batadv_forw_packet_steal(struct batadv_forw_packet *forw_packet,
			      spinlock_t *lock)
{
	 
	spin_lock_bh(lock);
	if (batadv_forw_packet_was_stolen(forw_packet)) {
		spin_unlock_bh(lock);
		return false;
	}

	hlist_del_init(&forw_packet->list);

	 
	hlist_add_fake(&forw_packet->cleanup_list);

	spin_unlock_bh(lock);
	return true;
}

 
static void
batadv_forw_packet_list_steal(struct hlist_head *forw_list,
			      struct hlist_head *cleanup_list,
			      const struct batadv_hard_iface *hard_iface)
{
	struct batadv_forw_packet *forw_packet;
	struct hlist_node *safe_tmp_node;

	hlist_for_each_entry_safe(forw_packet, safe_tmp_node,
				  forw_list, list) {
		 
		if (hard_iface &&
		    forw_packet->if_incoming != hard_iface &&
		    forw_packet->if_outgoing != hard_iface)
			continue;

		hlist_del(&forw_packet->list);
		hlist_add_head(&forw_packet->cleanup_list, cleanup_list);
	}
}

 
static void batadv_forw_packet_list_free(struct hlist_head *head)
{
	struct batadv_forw_packet *forw_packet;
	struct hlist_node *safe_tmp_node;

	hlist_for_each_entry_safe(forw_packet, safe_tmp_node, head,
				  cleanup_list) {
		cancel_delayed_work_sync(&forw_packet->delayed_work);

		hlist_del(&forw_packet->cleanup_list);
		batadv_forw_packet_free(forw_packet, true);
	}
}

 
static void batadv_forw_packet_queue(struct batadv_forw_packet *forw_packet,
				     spinlock_t *lock, struct hlist_head *head,
				     unsigned long send_time)
{
	spin_lock_bh(lock);

	 
	if (batadv_forw_packet_was_stolen(forw_packet)) {
		 
		WARN_ONCE(hlist_fake(&forw_packet->cleanup_list),
			  "Requeuing after batadv_forw_packet_steal() not allowed!\n");

		spin_unlock_bh(lock);
		return;
	}

	hlist_del_init(&forw_packet->list);
	hlist_add_head(&forw_packet->list, head);

	queue_delayed_work(batadv_event_workqueue,
			   &forw_packet->delayed_work,
			   send_time - jiffies);
	spin_unlock_bh(lock);
}

 
static void
batadv_forw_packet_bcast_queue(struct batadv_priv *bat_priv,
			       struct batadv_forw_packet *forw_packet,
			       unsigned long send_time)
{
	batadv_forw_packet_queue(forw_packet, &bat_priv->forw_bcast_list_lock,
				 &bat_priv->forw_bcast_list, send_time);
}

 
void batadv_forw_packet_ogmv1_queue(struct batadv_priv *bat_priv,
				    struct batadv_forw_packet *forw_packet,
				    unsigned long send_time)
{
	batadv_forw_packet_queue(forw_packet, &bat_priv->forw_bat_list_lock,
				 &bat_priv->forw_bat_list, send_time);
}

 
static int batadv_forw_bcast_packet_to_list(struct batadv_priv *bat_priv,
					    struct sk_buff *skb,
					    unsigned long delay,
					    bool own_packet,
					    struct batadv_hard_iface *if_in,
					    struct batadv_hard_iface *if_out)
{
	struct batadv_forw_packet *forw_packet;
	unsigned long send_time = jiffies;
	struct sk_buff *newskb;

	newskb = skb_clone(skb, GFP_ATOMIC);
	if (!newskb)
		goto err;

	forw_packet = batadv_forw_packet_alloc(if_in, if_out,
					       &bat_priv->bcast_queue_left,
					       bat_priv, newskb);
	if (!forw_packet)
		goto err_packet_free;

	forw_packet->own = own_packet;

	INIT_DELAYED_WORK(&forw_packet->delayed_work,
			  batadv_send_outstanding_bcast_packet);

	send_time += delay ? delay : msecs_to_jiffies(5);

	batadv_forw_packet_bcast_queue(bat_priv, forw_packet, send_time);
	return NETDEV_TX_OK;

err_packet_free:
	kfree_skb(newskb);
err:
	return NETDEV_TX_BUSY;
}

 
static int batadv_forw_bcast_packet_if(struct batadv_priv *bat_priv,
				       struct sk_buff *skb,
				       unsigned long delay,
				       bool own_packet,
				       struct batadv_hard_iface *if_in,
				       struct batadv_hard_iface *if_out)
{
	unsigned int num_bcasts = if_out->num_bcasts;
	struct sk_buff *newskb;
	int ret = NETDEV_TX_OK;

	if (!delay) {
		newskb = skb_clone(skb, GFP_ATOMIC);
		if (!newskb)
			return NETDEV_TX_BUSY;

		batadv_send_broadcast_skb(newskb, if_out);
		num_bcasts--;
	}

	 
	if (num_bcasts >= 1) {
		BATADV_SKB_CB(skb)->num_bcasts = num_bcasts;

		ret = batadv_forw_bcast_packet_to_list(bat_priv, skb, delay,
						       own_packet, if_in,
						       if_out);
	}

	return ret;
}

 
static bool batadv_send_no_broadcast(struct batadv_priv *bat_priv,
				     struct sk_buff *skb, bool own_packet,
				     struct batadv_hard_iface *if_out)
{
	struct batadv_hardif_neigh_node *neigh_node = NULL;
	struct batadv_bcast_packet *bcast_packet;
	u8 *orig_neigh;
	u8 *neigh_addr;
	char *type;
	int ret;

	if (!own_packet) {
		neigh_addr = eth_hdr(skb)->h_source;
		neigh_node = batadv_hardif_neigh_get(if_out,
						     neigh_addr);
	}

	bcast_packet = (struct batadv_bcast_packet *)skb->data;
	orig_neigh = neigh_node ? neigh_node->orig : NULL;

	ret = batadv_hardif_no_broadcast(if_out, bcast_packet->orig,
					 orig_neigh);

	batadv_hardif_neigh_put(neigh_node);

	 
	if (!ret)
		return false;

	 
	switch (ret) {
	case BATADV_HARDIF_BCAST_NORECIPIENT:
		type = "no neighbor";
		break;
	case BATADV_HARDIF_BCAST_DUPFWD:
		type = "single neighbor is source";
		break;
	case BATADV_HARDIF_BCAST_DUPORIG:
		type = "single neighbor is originator";
		break;
	default:
		type = "unknown";
	}

	batadv_dbg(BATADV_DBG_BATMAN, bat_priv,
		   "BCAST packet from orig %pM on %s suppressed: %s\n",
		   bcast_packet->orig,
		   if_out->net_dev->name, type);

	return true;
}

 
static int __batadv_forw_bcast_packet(struct batadv_priv *bat_priv,
				      struct sk_buff *skb,
				      unsigned long delay,
				      bool own_packet)
{
	struct batadv_hard_iface *hard_iface;
	struct batadv_hard_iface *primary_if;
	int ret = NETDEV_TX_OK;

	primary_if = batadv_primary_if_get_selected(bat_priv);
	if (!primary_if)
		return NETDEV_TX_BUSY;

	rcu_read_lock();
	list_for_each_entry_rcu(hard_iface, &batadv_hardif_list, list) {
		if (hard_iface->soft_iface != bat_priv->soft_iface)
			continue;

		if (!kref_get_unless_zero(&hard_iface->refcount))
			continue;

		if (batadv_send_no_broadcast(bat_priv, skb, own_packet,
					     hard_iface)) {
			batadv_hardif_put(hard_iface);
			continue;
		}

		ret = batadv_forw_bcast_packet_if(bat_priv, skb, delay,
						  own_packet, primary_if,
						  hard_iface);
		batadv_hardif_put(hard_iface);

		if (ret == NETDEV_TX_BUSY)
			break;
	}
	rcu_read_unlock();

	batadv_hardif_put(primary_if);
	return ret;
}

 
int batadv_forw_bcast_packet(struct batadv_priv *bat_priv,
			     struct sk_buff *skb,
			     unsigned long delay,
			     bool own_packet)
{
	return __batadv_forw_bcast_packet(bat_priv, skb, delay, own_packet);
}

 
void batadv_send_bcast_packet(struct batadv_priv *bat_priv,
			      struct sk_buff *skb,
			      unsigned long delay,
			      bool own_packet)
{
	__batadv_forw_bcast_packet(bat_priv, skb, delay, own_packet);
	consume_skb(skb);
}

 
static bool
batadv_forw_packet_bcasts_left(struct batadv_forw_packet *forw_packet)
{
	return BATADV_SKB_CB(forw_packet->skb)->num_bcasts;
}

 
static void
batadv_forw_packet_bcasts_dec(struct batadv_forw_packet *forw_packet)
{
	BATADV_SKB_CB(forw_packet->skb)->num_bcasts--;
}

 
bool batadv_forw_packet_is_rebroadcast(struct batadv_forw_packet *forw_packet)
{
	unsigned char num_bcasts = BATADV_SKB_CB(forw_packet->skb)->num_bcasts;

	return num_bcasts != forw_packet->if_outgoing->num_bcasts;
}

 
static void batadv_send_outstanding_bcast_packet(struct work_struct *work)
{
	unsigned long send_time = jiffies + msecs_to_jiffies(5);
	struct batadv_forw_packet *forw_packet;
	struct delayed_work *delayed_work;
	struct batadv_priv *bat_priv;
	struct sk_buff *skb1;
	bool dropped = false;

	delayed_work = to_delayed_work(work);
	forw_packet = container_of(delayed_work, struct batadv_forw_packet,
				   delayed_work);
	bat_priv = netdev_priv(forw_packet->if_incoming->soft_iface);

	if (atomic_read(&bat_priv->mesh_state) == BATADV_MESH_DEACTIVATING) {
		dropped = true;
		goto out;
	}

	if (batadv_dat_drop_broadcast_packet(bat_priv, forw_packet)) {
		dropped = true;
		goto out;
	}

	 
	skb1 = skb_clone(forw_packet->skb, GFP_ATOMIC);
	if (!skb1)
		goto out;

	batadv_send_broadcast_skb(skb1, forw_packet->if_outgoing);
	batadv_forw_packet_bcasts_dec(forw_packet);

	if (batadv_forw_packet_bcasts_left(forw_packet)) {
		batadv_forw_packet_bcast_queue(bat_priv, forw_packet,
					       send_time);
		return;
	}

out:
	 
	if (batadv_forw_packet_steal(forw_packet,
				     &bat_priv->forw_bcast_list_lock))
		batadv_forw_packet_free(forw_packet, dropped);
}

 
void
batadv_purge_outstanding_packets(struct batadv_priv *bat_priv,
				 const struct batadv_hard_iface *hard_iface)
{
	struct hlist_head head = HLIST_HEAD_INIT;

	if (hard_iface)
		batadv_dbg(BATADV_DBG_BATMAN, bat_priv,
			   "%s(): %s\n",
			   __func__, hard_iface->net_dev->name);
	else
		batadv_dbg(BATADV_DBG_BATMAN, bat_priv,
			   "%s()\n", __func__);

	 
	spin_lock_bh(&bat_priv->forw_bcast_list_lock);
	batadv_forw_packet_list_steal(&bat_priv->forw_bcast_list, &head,
				      hard_iface);
	spin_unlock_bh(&bat_priv->forw_bcast_list_lock);

	 
	spin_lock_bh(&bat_priv->forw_bat_list_lock);
	batadv_forw_packet_list_steal(&bat_priv->forw_bat_list, &head,
				      hard_iface);
	spin_unlock_bh(&bat_priv->forw_bat_list_lock);

	 
	batadv_forw_packet_list_free(&head);
}
