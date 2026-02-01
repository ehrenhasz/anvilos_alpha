
 

#include "main.h"

#include <linux/atomic.h>
#include <linux/build_bug.h>
#include <linux/byteorder/generic.h>
#include <linux/container_of.h>
#include <linux/crc32c.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/genetlink.h>
#include <linux/gfp.h>
#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/init.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/printk.h>
#include <linux/rculist.h>
#include <linux/rcupdate.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stddef.h>
#include <linux/string.h>
#include <linux/workqueue.h>
#include <net/dsfield.h>
#include <net/rtnetlink.h>
#include <uapi/linux/batadv_packet.h>
#include <uapi/linux/batman_adv.h>

#include "bat_algo.h"
#include "bat_iv_ogm.h"
#include "bat_v.h"
#include "bridge_loop_avoidance.h"
#include "distributed-arp-table.h"
#include "gateway_client.h"
#include "gateway_common.h"
#include "hard-interface.h"
#include "log.h"
#include "multicast.h"
#include "netlink.h"
#include "network-coding.h"
#include "originator.h"
#include "routing.h"
#include "send.h"
#include "soft-interface.h"
#include "tp_meter.h"
#include "translation-table.h"

 
struct list_head batadv_hardif_list;
unsigned int batadv_hardif_generation;
static int (*batadv_rx_handler[256])(struct sk_buff *skb,
				     struct batadv_hard_iface *recv_if);

unsigned char batadv_broadcast_addr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

struct workqueue_struct *batadv_event_workqueue;

static void batadv_recv_handler_init(void);

#define BATADV_UEV_TYPE_VAR	"BATTYPE="
#define BATADV_UEV_ACTION_VAR	"BATACTION="
#define BATADV_UEV_DATA_VAR	"BATDATA="

static char *batadv_uev_action_str[] = {
	"add",
	"del",
	"change",
	"loopdetect",
};

static char *batadv_uev_type_str[] = {
	"gw",
	"bla",
};

static int __init batadv_init(void)
{
	int ret;

	ret = batadv_tt_cache_init();
	if (ret < 0)
		return ret;

	INIT_LIST_HEAD(&batadv_hardif_list);
	batadv_algo_init();

	batadv_recv_handler_init();

	batadv_v_init();
	batadv_iv_init();
	batadv_nc_init();
	batadv_tp_meter_init();

	batadv_event_workqueue = create_singlethread_workqueue("bat_events");
	if (!batadv_event_workqueue)
		goto err_create_wq;

	register_netdevice_notifier(&batadv_hard_if_notifier);
	rtnl_link_register(&batadv_link_ops);
	batadv_netlink_register();

	pr_info("B.A.T.M.A.N. advanced %s (compatibility version %i) loaded\n",
		BATADV_SOURCE_VERSION, BATADV_COMPAT_VERSION);

	return 0;

err_create_wq:
	batadv_tt_cache_destroy();

	return -ENOMEM;
}

static void __exit batadv_exit(void)
{
	batadv_netlink_unregister();
	rtnl_link_unregister(&batadv_link_ops);
	unregister_netdevice_notifier(&batadv_hard_if_notifier);

	destroy_workqueue(batadv_event_workqueue);
	batadv_event_workqueue = NULL;

	rcu_barrier();

	batadv_tt_cache_destroy();
}

 
int batadv_mesh_init(struct net_device *soft_iface)
{
	struct batadv_priv *bat_priv = netdev_priv(soft_iface);
	int ret;

	spin_lock_init(&bat_priv->forw_bat_list_lock);
	spin_lock_init(&bat_priv->forw_bcast_list_lock);
	spin_lock_init(&bat_priv->tt.changes_list_lock);
	spin_lock_init(&bat_priv->tt.req_list_lock);
	spin_lock_init(&bat_priv->tt.roam_list_lock);
	spin_lock_init(&bat_priv->tt.last_changeset_lock);
	spin_lock_init(&bat_priv->tt.commit_lock);
	spin_lock_init(&bat_priv->gw.list_lock);
#ifdef CONFIG_BATMAN_ADV_MCAST
	spin_lock_init(&bat_priv->mcast.mla_lock);
	spin_lock_init(&bat_priv->mcast.want_lists_lock);
#endif
	spin_lock_init(&bat_priv->tvlv.container_list_lock);
	spin_lock_init(&bat_priv->tvlv.handler_list_lock);
	spin_lock_init(&bat_priv->softif_vlan_list_lock);
	spin_lock_init(&bat_priv->tp_list_lock);

	INIT_HLIST_HEAD(&bat_priv->forw_bat_list);
	INIT_HLIST_HEAD(&bat_priv->forw_bcast_list);
	INIT_HLIST_HEAD(&bat_priv->gw.gateway_list);
#ifdef CONFIG_BATMAN_ADV_MCAST
	INIT_HLIST_HEAD(&bat_priv->mcast.want_all_unsnoopables_list);
	INIT_HLIST_HEAD(&bat_priv->mcast.want_all_ipv4_list);
	INIT_HLIST_HEAD(&bat_priv->mcast.want_all_ipv6_list);
#endif
	INIT_LIST_HEAD(&bat_priv->tt.changes_list);
	INIT_HLIST_HEAD(&bat_priv->tt.req_list);
	INIT_LIST_HEAD(&bat_priv->tt.roam_list);
#ifdef CONFIG_BATMAN_ADV_MCAST
	INIT_HLIST_HEAD(&bat_priv->mcast.mla_list);
#endif
	INIT_HLIST_HEAD(&bat_priv->tvlv.container_list);
	INIT_HLIST_HEAD(&bat_priv->tvlv.handler_list);
	INIT_HLIST_HEAD(&bat_priv->softif_vlan_list);
	INIT_HLIST_HEAD(&bat_priv->tp_list);

	bat_priv->gw.generation = 0;

	ret = batadv_originator_init(bat_priv);
	if (ret < 0) {
		atomic_set(&bat_priv->mesh_state, BATADV_MESH_DEACTIVATING);
		goto err_orig;
	}

	ret = batadv_tt_init(bat_priv);
	if (ret < 0) {
		atomic_set(&bat_priv->mesh_state, BATADV_MESH_DEACTIVATING);
		goto err_tt;
	}

	ret = batadv_v_mesh_init(bat_priv);
	if (ret < 0) {
		atomic_set(&bat_priv->mesh_state, BATADV_MESH_DEACTIVATING);
		goto err_v;
	}

	ret = batadv_bla_init(bat_priv);
	if (ret < 0) {
		atomic_set(&bat_priv->mesh_state, BATADV_MESH_DEACTIVATING);
		goto err_bla;
	}

	ret = batadv_dat_init(bat_priv);
	if (ret < 0) {
		atomic_set(&bat_priv->mesh_state, BATADV_MESH_DEACTIVATING);
		goto err_dat;
	}

	ret = batadv_nc_mesh_init(bat_priv);
	if (ret < 0) {
		atomic_set(&bat_priv->mesh_state, BATADV_MESH_DEACTIVATING);
		goto err_nc;
	}

	batadv_gw_init(bat_priv);
	batadv_mcast_init(bat_priv);

	atomic_set(&bat_priv->gw.reselect, 0);
	atomic_set(&bat_priv->mesh_state, BATADV_MESH_ACTIVE);

	return 0;

err_nc:
	batadv_dat_free(bat_priv);
err_dat:
	batadv_bla_free(bat_priv);
err_bla:
	batadv_v_mesh_free(bat_priv);
err_v:
	batadv_tt_free(bat_priv);
err_tt:
	batadv_originator_free(bat_priv);
err_orig:
	batadv_purge_outstanding_packets(bat_priv, NULL);
	atomic_set(&bat_priv->mesh_state, BATADV_MESH_INACTIVE);

	return ret;
}

 
void batadv_mesh_free(struct net_device *soft_iface)
{
	struct batadv_priv *bat_priv = netdev_priv(soft_iface);

	atomic_set(&bat_priv->mesh_state, BATADV_MESH_DEACTIVATING);

	batadv_purge_outstanding_packets(bat_priv, NULL);

	batadv_gw_node_free(bat_priv);

	batadv_v_mesh_free(bat_priv);
	batadv_nc_mesh_free(bat_priv);
	batadv_dat_free(bat_priv);
	batadv_bla_free(bat_priv);

	batadv_mcast_free(bat_priv);

	 
	batadv_tt_free(bat_priv);

	 
	batadv_originator_free(bat_priv);

	batadv_gw_free(bat_priv);

	free_percpu(bat_priv->bat_counters);
	bat_priv->bat_counters = NULL;

	atomic_set(&bat_priv->mesh_state, BATADV_MESH_INACTIVE);
}

 
bool batadv_is_my_mac(struct batadv_priv *bat_priv, const u8 *addr)
{
	const struct batadv_hard_iface *hard_iface;
	bool is_my_mac = false;

	rcu_read_lock();
	list_for_each_entry_rcu(hard_iface, &batadv_hardif_list, list) {
		if (hard_iface->if_status != BATADV_IF_ACTIVE)
			continue;

		if (hard_iface->soft_iface != bat_priv->soft_iface)
			continue;

		if (batadv_compare_eth(hard_iface->net_dev->dev_addr, addr)) {
			is_my_mac = true;
			break;
		}
	}
	rcu_read_unlock();
	return is_my_mac;
}

 
int batadv_max_header_len(void)
{
	int header_len = 0;

	header_len = max_t(int, header_len,
			   sizeof(struct batadv_unicast_packet));
	header_len = max_t(int, header_len,
			   sizeof(struct batadv_unicast_4addr_packet));
	header_len = max_t(int, header_len,
			   sizeof(struct batadv_bcast_packet));

#ifdef CONFIG_BATMAN_ADV_NC
	header_len = max_t(int, header_len,
			   sizeof(struct batadv_coded_packet));
#endif

	return header_len + ETH_HLEN;
}

 
void batadv_skb_set_priority(struct sk_buff *skb, int offset)
{
	struct iphdr ip_hdr_tmp, *ip_hdr;
	struct ipv6hdr ip6_hdr_tmp, *ip6_hdr;
	struct ethhdr ethhdr_tmp, *ethhdr;
	struct vlan_ethhdr *vhdr, vhdr_tmp;
	u32 prio;

	 
	if (skb->priority >= 256 && skb->priority <= 263)
		return;

	ethhdr = skb_header_pointer(skb, offset, sizeof(*ethhdr), &ethhdr_tmp);
	if (!ethhdr)
		return;

	switch (ethhdr->h_proto) {
	case htons(ETH_P_8021Q):
		vhdr = skb_header_pointer(skb, offset + sizeof(*vhdr),
					  sizeof(*vhdr), &vhdr_tmp);
		if (!vhdr)
			return;
		prio = ntohs(vhdr->h_vlan_TCI) & VLAN_PRIO_MASK;
		prio = prio >> VLAN_PRIO_SHIFT;
		break;
	case htons(ETH_P_IP):
		ip_hdr = skb_header_pointer(skb, offset + sizeof(*ethhdr),
					    sizeof(*ip_hdr), &ip_hdr_tmp);
		if (!ip_hdr)
			return;
		prio = (ipv4_get_dsfield(ip_hdr) & 0xfc) >> 5;
		break;
	case htons(ETH_P_IPV6):
		ip6_hdr = skb_header_pointer(skb, offset + sizeof(*ethhdr),
					     sizeof(*ip6_hdr), &ip6_hdr_tmp);
		if (!ip6_hdr)
			return;
		prio = (ipv6_get_dsfield(ip6_hdr) & 0xfc) >> 5;
		break;
	default:
		return;
	}

	skb->priority = prio + 256;
}

static int batadv_recv_unhandled_packet(struct sk_buff *skb,
					struct batadv_hard_iface *recv_if)
{
	kfree_skb(skb);

	return NET_RX_DROP;
}

 

 
int batadv_batman_skb_recv(struct sk_buff *skb, struct net_device *dev,
			   struct packet_type *ptype,
			   struct net_device *orig_dev)
{
	struct batadv_priv *bat_priv;
	struct batadv_ogm_packet *batadv_ogm_packet;
	struct batadv_hard_iface *hard_iface;
	u8 idx;

	hard_iface = container_of(ptype, struct batadv_hard_iface,
				  batman_adv_ptype);

	 
	if (!kref_get_unless_zero(&hard_iface->refcount))
		goto err_out;

	skb = skb_share_check(skb, GFP_ATOMIC);

	 
	if (!skb)
		goto err_put;

	 
	if (unlikely(!pskb_may_pull(skb, 2)))
		goto err_free;

	 
	if (unlikely(skb->mac_len != ETH_HLEN || !skb_mac_header(skb)))
		goto err_free;

	if (!hard_iface->soft_iface)
		goto err_free;

	bat_priv = netdev_priv(hard_iface->soft_iface);

	if (atomic_read(&bat_priv->mesh_state) != BATADV_MESH_ACTIVE)
		goto err_free;

	 
	if (hard_iface->if_status != BATADV_IF_ACTIVE)
		goto err_free;

	batadv_ogm_packet = (struct batadv_ogm_packet *)skb->data;

	if (batadv_ogm_packet->version != BATADV_COMPAT_VERSION) {
		batadv_dbg(BATADV_DBG_BATMAN, bat_priv,
			   "Drop packet: incompatible batman version (%i)\n",
			   batadv_ogm_packet->version);
		goto err_free;
	}

	 
	memset(skb->cb, 0, sizeof(struct batadv_skb_cb));

	idx = batadv_ogm_packet->packet_type;
	(*batadv_rx_handler[idx])(skb, hard_iface);

	batadv_hardif_put(hard_iface);

	 
	return NET_RX_SUCCESS;

err_free:
	kfree_skb(skb);
err_put:
	batadv_hardif_put(hard_iface);
err_out:
	return NET_RX_DROP;
}

static void batadv_recv_handler_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(batadv_rx_handler); i++)
		batadv_rx_handler[i] = batadv_recv_unhandled_packet;

	for (i = BATADV_UNICAST_MIN; i <= BATADV_UNICAST_MAX; i++)
		batadv_rx_handler[i] = batadv_recv_unhandled_unicast_packet;

	 
	BUILD_BUG_ON(sizeof(struct batadv_bla_claim_dst) != 6);
	BUILD_BUG_ON(sizeof(struct batadv_ogm_packet) != 24);
	BUILD_BUG_ON(sizeof(struct batadv_icmp_header) != 20);
	BUILD_BUG_ON(sizeof(struct batadv_icmp_packet) != 20);
	BUILD_BUG_ON(sizeof(struct batadv_icmp_packet_rr) != 116);
	BUILD_BUG_ON(sizeof(struct batadv_unicast_packet) != 10);
	BUILD_BUG_ON(sizeof(struct batadv_unicast_4addr_packet) != 18);
	BUILD_BUG_ON(sizeof(struct batadv_frag_packet) != 20);
	BUILD_BUG_ON(sizeof(struct batadv_bcast_packet) != 14);
	BUILD_BUG_ON(sizeof(struct batadv_coded_packet) != 46);
	BUILD_BUG_ON(sizeof(struct batadv_unicast_tvlv_packet) != 20);
	BUILD_BUG_ON(sizeof(struct batadv_tvlv_hdr) != 4);
	BUILD_BUG_ON(sizeof(struct batadv_tvlv_gateway_data) != 8);
	BUILD_BUG_ON(sizeof(struct batadv_tvlv_tt_vlan_data) != 8);
	BUILD_BUG_ON(sizeof(struct batadv_tvlv_tt_change) != 12);
	BUILD_BUG_ON(sizeof(struct batadv_tvlv_roam_adv) != 8);

	i = sizeof_field(struct sk_buff, cb);
	BUILD_BUG_ON(sizeof(struct batadv_skb_cb) > i);

	 
	batadv_rx_handler[BATADV_BCAST] = batadv_recv_bcast_packet;

	 
	 
	batadv_rx_handler[BATADV_UNICAST_4ADDR] = batadv_recv_unicast_packet;
	 
	batadv_rx_handler[BATADV_UNICAST] = batadv_recv_unicast_packet;
	 
	batadv_rx_handler[BATADV_UNICAST_TVLV] = batadv_recv_unicast_tvlv;
	 
	batadv_rx_handler[BATADV_ICMP] = batadv_recv_icmp_packet;
	 
	batadv_rx_handler[BATADV_UNICAST_FRAG] = batadv_recv_frag_packet;
}

 
int
batadv_recv_handler_register(u8 packet_type,
			     int (*recv_handler)(struct sk_buff *,
						 struct batadv_hard_iface *))
{
	int (*curr)(struct sk_buff *skb,
		    struct batadv_hard_iface *recv_if);
	curr = batadv_rx_handler[packet_type];

	if (curr != batadv_recv_unhandled_packet &&
	    curr != batadv_recv_unhandled_unicast_packet)
		return -EBUSY;

	batadv_rx_handler[packet_type] = recv_handler;
	return 0;
}

 
void batadv_recv_handler_unregister(u8 packet_type)
{
	batadv_rx_handler[packet_type] = batadv_recv_unhandled_packet;
}

 
__be32 batadv_skb_crc32(struct sk_buff *skb, u8 *payload_ptr)
{
	u32 crc = 0;
	unsigned int from;
	unsigned int to = skb->len;
	struct skb_seq_state st;
	const u8 *data;
	unsigned int len;
	unsigned int consumed = 0;

	from = (unsigned int)(payload_ptr - skb->data);

	skb_prepare_seq_read(skb, from, to, &st);
	while ((len = skb_seq_read(consumed, &data, &st)) != 0) {
		crc = crc32c(crc, data, len);
		consumed += len;
	}

	return htonl(crc);
}

 
unsigned short batadv_get_vid(struct sk_buff *skb, size_t header_len)
{
	struct ethhdr *ethhdr = (struct ethhdr *)(skb->data + header_len);
	struct vlan_ethhdr *vhdr;
	unsigned short vid;

	if (ethhdr->h_proto != htons(ETH_P_8021Q))
		return BATADV_NO_FLAGS;

	if (!pskb_may_pull(skb, header_len + VLAN_ETH_HLEN))
		return BATADV_NO_FLAGS;

	vhdr = (struct vlan_ethhdr *)(skb->data + header_len);
	vid = ntohs(vhdr->h_vlan_TCI) & VLAN_VID_MASK;
	vid |= BATADV_VLAN_HAS_TAG;

	return vid;
}

 
bool batadv_vlan_ap_isola_get(struct batadv_priv *bat_priv, unsigned short vid)
{
	bool ap_isolation_enabled = false;
	struct batadv_softif_vlan *vlan;

	 
	vlan = batadv_softif_vlan_get(bat_priv, vid);
	if (vlan) {
		ap_isolation_enabled = atomic_read(&vlan->ap_isolation);
		batadv_softif_vlan_put(vlan);
	}

	return ap_isolation_enabled;
}

 
int batadv_throw_uevent(struct batadv_priv *bat_priv, enum batadv_uev_type type,
			enum batadv_uev_action action, const char *data)
{
	int ret = -ENOMEM;
	struct kobject *bat_kobj;
	char *uevent_env[4] = { NULL, NULL, NULL, NULL };

	bat_kobj = &bat_priv->soft_iface->dev.kobj;

	uevent_env[0] = kasprintf(GFP_ATOMIC,
				  "%s%s", BATADV_UEV_TYPE_VAR,
				  batadv_uev_type_str[type]);
	if (!uevent_env[0])
		goto out;

	uevent_env[1] = kasprintf(GFP_ATOMIC,
				  "%s%s", BATADV_UEV_ACTION_VAR,
				  batadv_uev_action_str[action]);
	if (!uevent_env[1])
		goto out;

	 
	if (action != BATADV_UEV_DEL) {
		uevent_env[2] = kasprintf(GFP_ATOMIC,
					  "%s%s", BATADV_UEV_DATA_VAR, data);
		if (!uevent_env[2])
			goto out;
	}

	ret = kobject_uevent_env(bat_kobj, KOBJ_CHANGE, uevent_env);
out:
	kfree(uevent_env[0]);
	kfree(uevent_env[1]);
	kfree(uevent_env[2]);

	if (ret)
		batadv_dbg(BATADV_DBG_BATMAN, bat_priv,
			   "Impossible to send uevent for (%s,%s,%s) event (err: %d)\n",
			   batadv_uev_type_str[type],
			   batadv_uev_action_str[action],
			   (action == BATADV_UEV_DEL ? "NULL" : data), ret);
	return ret;
}

module_init(batadv_init);
module_exit(batadv_exit);

MODULE_LICENSE("GPL");

MODULE_AUTHOR(BATADV_DRIVER_AUTHOR);
MODULE_DESCRIPTION(BATADV_DRIVER_DESC);
MODULE_VERSION(BATADV_SOURCE_VERSION);
MODULE_ALIAS_RTNL_LINK("batadv");
MODULE_ALIAS_GENL_FAMILY(BATADV_NL_NAME);
