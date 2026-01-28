


#ifndef _ICE_VF_LIB_H_
#define _ICE_VF_LIB_H_

#include <linux/types.h>
#include <linux/hashtable.h>
#include <linux/bitmap.h>
#include <linux/mutex.h>
#include <linux/pci.h>
#include <net/devlink.h>
#include <linux/avf/virtchnl.h>
#include "ice_type.h"
#include "ice_virtchnl_fdir.h"
#include "ice_vsi_vlan_ops.h"

#define ICE_MAX_SRIOV_VFS		256


#define ICE_MAX_RSS_QS_PER_VF	16

struct ice_pf;
struct ice_vf;
struct ice_virtchnl_ops;


enum ice_virtchnl_cap {
	ICE_VIRTCHNL_VF_CAP_PRIVILEGE = 0,
};


enum ice_vf_states {
	ICE_VF_STATE_INIT = 0,		
	ICE_VF_STATE_ACTIVE,		
	ICE_VF_STATE_QS_ENA,		
	ICE_VF_STATE_DIS,
	ICE_VF_STATE_MC_PROMISC,
	ICE_VF_STATE_UC_PROMISC,
	ICE_VF_STATES_NBITS
};

struct ice_time_mac {
	unsigned long time_modified;
	u8 addr[ETH_ALEN];
};


struct ice_mdd_vf_events {
	u16 count;			
	
	u16 last_printed;
};


struct ice_vf_ops {
	enum ice_disq_rst_src reset_type;
	void (*free)(struct ice_vf *vf);
	void (*clear_reset_state)(struct ice_vf *vf);
	void (*clear_mbx_register)(struct ice_vf *vf);
	void (*trigger_reset_register)(struct ice_vf *vf, bool is_vflr);
	bool (*poll_reset_status)(struct ice_vf *vf);
	void (*clear_reset_trigger)(struct ice_vf *vf);
	void (*irq_close)(struct ice_vf *vf);
	int (*create_vsi)(struct ice_vf *vf);
	void (*post_vsi_rebuild)(struct ice_vf *vf);
};


struct ice_vfs {
	DECLARE_HASHTABLE(table, 8);	
	struct mutex table_lock;	
	u16 num_supported;		
	u16 num_qps_per;		
	u16 num_msix_per;		
	unsigned long last_printed_mdd_jiffies;	
};


struct ice_vf {
	struct hlist_node entry;
	struct rcu_head rcu;
	struct kref refcnt;
	struct ice_pf *pf;

	
	struct mutex cfg_lock;

	u16 vf_id;			
	u16 lan_vsi_idx;		
	u16 ctrl_vsi_idx;
	struct ice_vf_fdir fdir;
	
	int first_vector_idx;
	struct ice_sw *vf_sw_id;	
	struct virtchnl_version_info vf_ver;
	u32 driver_caps;		
	u8 dev_lan_addr[ETH_ALEN];
	u8 hw_lan_addr[ETH_ALEN];
	struct ice_time_mac legacy_last_added_umac;
	DECLARE_BITMAP(txq_ena, ICE_MAX_RSS_QS_PER_VF);
	DECLARE_BITMAP(rxq_ena, ICE_MAX_RSS_QS_PER_VF);
	struct ice_vlan port_vlan_info;	
	struct virtchnl_vlan_caps vlan_v2_caps;
	struct ice_mbx_vf_info mbx_info;
	u8 pf_set_mac:1;		
	u8 trusted:1;
	u8 spoofchk:1;
	u8 link_forced:1;
	u8 link_up:1;			
	
	u16 lan_vsi_num;		
	unsigned int min_tx_rate;	
	unsigned int max_tx_rate;	
	DECLARE_BITMAP(vf_states, ICE_VF_STATES_NBITS);	

	unsigned long vf_caps;		
	u8 num_req_qs;			
	u16 num_mac;
	u16 num_vf_qs;			
	struct ice_mdd_vf_events mdd_rx_events;
	struct ice_mdd_vf_events mdd_tx_events;
	DECLARE_BITMAP(opcodes_allowlist, VIRTCHNL_OP_MAX);

	struct ice_repr *repr;
	const struct ice_virtchnl_ops *virtchnl_ops;
	const struct ice_vf_ops *vf_ops;

	
	struct devlink_port devlink_port;
};


enum ice_vf_reset_flags {
	ICE_VF_RESET_VFLR = BIT(0), 
	ICE_VF_RESET_NOTIFY = BIT(1), 
	ICE_VF_RESET_LOCK = BIT(2), 
};

static inline u16 ice_vf_get_port_vlan_id(struct ice_vf *vf)
{
	return vf->port_vlan_info.vid;
}

static inline u8 ice_vf_get_port_vlan_prio(struct ice_vf *vf)
{
	return vf->port_vlan_info.prio;
}

static inline bool ice_vf_is_port_vlan_ena(struct ice_vf *vf)
{
	return (ice_vf_get_port_vlan_id(vf) || ice_vf_get_port_vlan_prio(vf));
}

static inline u16 ice_vf_get_port_vlan_tpid(struct ice_vf *vf)
{
	return vf->port_vlan_info.tpid;
}




#define ice_for_each_vf(pf, bkt, vf) \
	hash_for_each((pf)->vfs.table, (bkt), (vf), entry)


#define ice_for_each_vf_rcu(pf, bkt, vf) \
	hash_for_each_rcu((pf)->vfs.table, (bkt), (vf), entry)

#ifdef CONFIG_PCI_IOV
struct ice_vf *ice_get_vf_by_id(struct ice_pf *pf, u16 vf_id);
void ice_put_vf(struct ice_vf *vf);
bool ice_has_vfs(struct ice_pf *pf);
u16 ice_get_num_vfs(struct ice_pf *pf);
struct ice_vsi *ice_get_vf_vsi(struct ice_vf *vf);
bool ice_is_vf_disabled(struct ice_vf *vf);
int ice_check_vf_ready_for_cfg(struct ice_vf *vf);
void ice_set_vf_state_dis(struct ice_vf *vf);
bool ice_is_any_vf_in_unicast_promisc(struct ice_pf *pf);
void
ice_vf_get_promisc_masks(struct ice_vf *vf, struct ice_vsi *vsi,
			 u8 *ucast_m, u8 *mcast_m);
int
ice_vf_set_vsi_promisc(struct ice_vf *vf, struct ice_vsi *vsi, u8 promisc_m);
int
ice_vf_clear_vsi_promisc(struct ice_vf *vf, struct ice_vsi *vsi, u8 promisc_m);
int ice_reset_vf(struct ice_vf *vf, u32 flags);
void ice_reset_all_vfs(struct ice_pf *pf);
struct ice_vsi *ice_get_vf_ctrl_vsi(struct ice_pf *pf, struct ice_vsi *vsi);
#else 
static inline struct ice_vf *ice_get_vf_by_id(struct ice_pf *pf, u16 vf_id)
{
	return NULL;
}

static inline void ice_put_vf(struct ice_vf *vf)
{
}

static inline bool ice_has_vfs(struct ice_pf *pf)
{
	return false;
}

static inline u16 ice_get_num_vfs(struct ice_pf *pf)
{
	return 0;
}

static inline struct ice_vsi *ice_get_vf_vsi(struct ice_vf *vf)
{
	return NULL;
}

static inline bool ice_is_vf_disabled(struct ice_vf *vf)
{
	return true;
}

static inline int ice_check_vf_ready_for_cfg(struct ice_vf *vf)
{
	return -EOPNOTSUPP;
}

static inline void ice_set_vf_state_dis(struct ice_vf *vf)
{
}

static inline bool ice_is_any_vf_in_unicast_promisc(struct ice_pf *pf)
{
	return false;
}

static inline int
ice_vf_set_vsi_promisc(struct ice_vf *vf, struct ice_vsi *vsi, u8 promisc_m)
{
	return -EOPNOTSUPP;
}

static inline int
ice_vf_clear_vsi_promisc(struct ice_vf *vf, struct ice_vsi *vsi, u8 promisc_m)
{
	return -EOPNOTSUPP;
}

static inline int ice_reset_vf(struct ice_vf *vf, u32 flags)
{
	return 0;
}

static inline void ice_reset_all_vfs(struct ice_pf *pf)
{
}

static inline struct ice_vsi *
ice_get_vf_ctrl_vsi(struct ice_pf *pf, struct ice_vsi *vsi)
{
	return NULL;
}
#endif 

#endif 
