 
 

#ifndef _ICE_LAG_H_
#define _ICE_LAG_H_

#include <linux/netdevice.h>

 
enum ice_lag_role {
	ICE_LAG_NONE,
	ICE_LAG_PRIMARY,
	ICE_LAG_BACKUP,
	ICE_LAG_UNSET
};

#define ICE_LAG_INVALID_PORT 0xFF

#define ICE_LAG_RESET_RETRIES		5

struct ice_pf;
struct ice_vf;

struct ice_lag_netdev_list {
	struct list_head node;
	struct net_device *netdev;
};

 
struct ice_lag {
	struct ice_pf *pf;  
	struct net_device *netdev;  
	struct net_device *upper_netdev;  
	struct list_head *netdev_head;
	struct notifier_block notif_block;
	s32 bond_mode;
	u16 bond_swid;  
	u8 active_port;  
	u8 bonded:1;  
	u8 primary:1;  
	u16 pf_recipe;
	u16 pf_rule_id;
	u16 cp_rule_idx;
	u8 role;
};

 
struct ice_lag_work {
	struct work_struct lag_task;
	struct ice_lag_netdev_list netdev_list;
	struct ice_lag *lag;
	unsigned long event;
	struct net_device *event_netdev;
	union {
		struct netdev_notifier_changeupper_info changeupper_info;
		struct netdev_notifier_bonding_info bonding_info;
		struct netdev_notifier_info notifier_info;
	} info;
};

void ice_lag_move_new_vf_nodes(struct ice_vf *vf);
int ice_init_lag(struct ice_pf *pf);
void ice_deinit_lag(struct ice_pf *pf);
void ice_lag_rebuild(struct ice_pf *pf);
bool ice_lag_is_switchdev_running(struct ice_pf *pf);
void ice_lag_move_vf_nodes_cfg(struct ice_lag *lag, u8 src_prt, u8 dst_prt);
#endif  
