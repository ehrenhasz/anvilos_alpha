#ifndef _BR_PRIVATE_CFM_H_
#define _BR_PRIVATE_CFM_H_
#include "br_private.h"
#include <uapi/linux/cfm_bridge.h>
struct br_cfm_mep_create {
	enum br_cfm_domain domain;  
	enum br_cfm_mep_direction direction;  
	u32 ifindex;  
};
int br_cfm_mep_create(struct net_bridge *br,
		      const u32 instance,
		      struct br_cfm_mep_create *const create,
		      struct netlink_ext_ack *extack);
int br_cfm_mep_delete(struct net_bridge *br,
		      const u32 instance,
		      struct netlink_ext_ack *extack);
struct br_cfm_mep_config {
	u32 mdlevel;
	u32 mepid;  
	struct mac_addr unicast_mac;  
};
int br_cfm_mep_config_set(struct net_bridge *br,
			  const u32 instance,
			  const struct br_cfm_mep_config *const config,
			  struct netlink_ext_ack *extack);
struct br_cfm_maid {
	u8 data[CFM_MAID_LENGTH];
};
struct br_cfm_cc_config {
	struct br_cfm_maid exp_maid;
	enum br_cfm_ccm_interval exp_interval;
	bool enable;  
};
int br_cfm_cc_config_set(struct net_bridge *br,
			 const u32 instance,
			 const struct br_cfm_cc_config *const config,
			 struct netlink_ext_ack *extack);
int br_cfm_cc_peer_mep_add(struct net_bridge *br, const u32 instance,
			   u32 peer_mep_id,
			   struct netlink_ext_ack *extack);
int br_cfm_cc_peer_mep_remove(struct net_bridge *br, const u32 instance,
			      u32 peer_mep_id,
			      struct netlink_ext_ack *extack);
int br_cfm_cc_rdi_set(struct net_bridge *br, const u32 instance,
		      const bool rdi, struct netlink_ext_ack *extack);
struct br_cfm_cc_ccm_tx_info {
	struct mac_addr dmac;
	u32 period;
	bool seq_no_update;  
	bool if_tlv;  
	u8 if_tlv_value;  
	bool port_tlv;  
	u8 port_tlv_value;  
};
int br_cfm_cc_ccm_tx(struct net_bridge *br, const u32 instance,
		     const struct br_cfm_cc_ccm_tx_info *const tx_info,
		     struct netlink_ext_ack *extack);
struct br_cfm_mep_status {
	bool opcode_unexp_seen;  
	bool version_unexp_seen;  
	bool rx_level_low_seen;  
};
struct br_cfm_cc_peer_status {
	u8 port_tlv_value;  
	u8 if_tlv_value;  
	u8 ccm_defect:1;
	u8 rdi:1;
	u8 seen:1;  
	u8 tlv_seen:1;  
	u8 seq_unexp_seen:1;
};
struct br_cfm_mep {
	struct hlist_node		head;
	u32				instance;
	struct br_cfm_mep_create	create;
	struct br_cfm_mep_config	config;
	struct br_cfm_cc_config		cc_config;
	struct br_cfm_cc_ccm_tx_info	cc_ccm_tx_info;
	struct hlist_head		peer_mep_list;
	struct net_bridge_port __rcu	*b_port;
	unsigned long			ccm_tx_end;
	struct delayed_work		ccm_tx_dwork;
	u32				ccm_tx_snumber;
	u32				ccm_rx_snumber;
	struct br_cfm_mep_status	status;
	bool				rdi;
	struct rcu_head			rcu;
};
struct br_cfm_peer_mep {
	struct hlist_node		head;
	struct br_cfm_mep		*mep;
	struct delayed_work		ccm_rx_dwork;
	u32				mepid;
	struct br_cfm_cc_peer_status	cc_status;
	u32				ccm_rx_count_miss;
	struct rcu_head			rcu;
};
#endif  
