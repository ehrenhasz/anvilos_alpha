#ifndef EFX_TC_ENCAP_ACTIONS_H
#define EFX_TC_ENCAP_ACTIONS_H
#include "net_driver.h"
#if IS_ENABLED(CONFIG_SFC_SRIOV)
#include <linux/refcount.h>
#include <net/tc_act/tc_tunnel_key.h>
struct efx_neigh_binder {
	struct net *net;
	__be32 dst_ip;
	struct in6_addr dst_ip6;
	char ha[ETH_ALEN];
	bool n_valid;
	rwlock_t lock;
	u8 ttl;
	bool dying;
	struct net_device *egdev;
	netdevice_tracker dev_tracker;
	netns_tracker ns_tracker;
	refcount_t ref;
	unsigned long used;
	struct list_head users;
	struct rhash_head linkage;
	struct work_struct work;
	struct efx_nic *efx;
};
#define EFX_TC_MAX_ENCAP_HDR	126
struct efx_tc_encap_action {
	enum efx_encap_type type;
	struct ip_tunnel_key key;  
	u32 dest_mport;  
	u8 encap_hdr_len;
	bool n_valid;
	u8 encap_hdr[EFX_TC_MAX_ENCAP_HDR];
	struct efx_neigh_binder *neigh;
	struct list_head list;  
	struct list_head users;  
	struct rhash_head linkage;  
	refcount_t ref;
	u32 fw_id;  
};
int efx_tc_init_encap_actions(struct efx_nic *efx);
void efx_tc_destroy_encap_actions(struct efx_nic *efx);
void efx_tc_fini_encap_actions(struct efx_nic *efx);
struct efx_tc_flow_rule;
bool efx_tc_check_ready(struct efx_nic *efx, struct efx_tc_flow_rule *rule);
struct efx_tc_encap_action *efx_tc_flower_create_encap_md(
			struct efx_nic *efx, const struct ip_tunnel_info *info,
			struct net_device *egdev, struct netlink_ext_ack *extack);
void efx_tc_flower_release_encap_md(struct efx_nic *efx,
				    struct efx_tc_encap_action *encap);
void efx_tc_unregister_egdev(struct efx_nic *efx, struct net_device *net_dev);
int efx_tc_netevent_event(struct efx_nic *efx, unsigned long event,
			  void *ptr);
#else  
static inline int efx_tc_netevent_event(struct efx_nic *efx,
					unsigned long event, void *ptr)
{
	return NOTIFY_DONE;
}
#endif  
#endif  
