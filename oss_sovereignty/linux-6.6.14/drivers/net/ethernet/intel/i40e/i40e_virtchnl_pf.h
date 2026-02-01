 
 

#ifndef _I40E_VIRTCHNL_PF_H_
#define _I40E_VIRTCHNL_PF_H_

#include "i40e.h"

#define I40E_MAX_VLANID 4095

#define I40E_VIRTCHNL_SUPPORTED_QTYPES 2

#define I40E_VLAN_PRIORITY_SHIFT	13
#define I40E_VLAN_MASK			0xFFF
#define I40E_PRIORITY_MASK		0xE000

#define I40E_MAX_VF_PROMISC_FLAGS	3

#define I40E_VF_STATE_WAIT_COUNT	20
#define I40E_VFR_WAIT_COUNT		100

 
enum i40e_queue_ctrl {
	I40E_QUEUE_CTRL_UNKNOWN = 0,
	I40E_QUEUE_CTRL_ENABLE,
	I40E_QUEUE_CTRL_ENABLECHECK,
	I40E_QUEUE_CTRL_DISABLE,
	I40E_QUEUE_CTRL_DISABLECHECK,
	I40E_QUEUE_CTRL_FASTDISABLE,
	I40E_QUEUE_CTRL_FASTDISABLECHECK,
};

 
enum i40e_vf_states {
	I40E_VF_STATE_INIT = 0,
	I40E_VF_STATE_ACTIVE,
	I40E_VF_STATE_RDMAENA,
	I40E_VF_STATE_DISABLED,
	I40E_VF_STATE_MC_PROMISC,
	I40E_VF_STATE_UC_PROMISC,
	I40E_VF_STATE_PRE_ENABLE,
	I40E_VF_STATE_RESETTING
};

 
enum i40e_vf_capabilities {
	I40E_VIRTCHNL_VF_CAP_PRIVILEGE = 0,
	I40E_VIRTCHNL_VF_CAP_L2,
	I40E_VIRTCHNL_VF_CAP_RDMA,
};

 
struct i40evf_channel {
	u16 vsi_idx;  
	u16 vsi_id;  
	u16 num_qps;  
	u64 max_tx_rate;  
};

 
struct i40e_vf {
	struct i40e_pf *pf;

	 
	s16 vf_id;
	 
	enum i40e_switch_element_types parent_type;
	struct virtchnl_version_info vf_ver;
	u32 driver_caps;  

	 
	u16 stag;

	struct virtchnl_ether_addr default_lan_addr;
	u16 port_vlan_id;
	bool pf_set_mac;	 
	bool trusted;

	 
	u16 lan_vsi_idx;	 
	u16 lan_vsi_id;		 

	u8 num_queue_pairs;	 
	u8 num_req_queues;	 
	u64 num_mdd_events;	 

	unsigned long vf_caps;	 
	unsigned long vf_states;	 
	unsigned int tx_rate;	 
	bool link_forced;
	bool link_up;		 
	bool spoofchk;
	u16 num_vlan;

	 
	bool adq_enabled;  
	u8 num_tc;
	struct i40evf_channel ch[I40E_MAX_VF_VSI];
	struct hlist_head cloud_filter_list;
	u16 num_cloud_filters;

	 
	struct virtchnl_rdma_qvlist_info *qvlist_info;
};

void i40e_free_vfs(struct i40e_pf *pf);
int i40e_pci_sriov_configure(struct pci_dev *dev, int num_vfs);
int i40e_alloc_vfs(struct i40e_pf *pf, u16 num_alloc_vfs);
int i40e_vc_process_vf_msg(struct i40e_pf *pf, s16 vf_id, u32 v_opcode,
			   u32 v_retval, u8 *msg, u16 msglen);
int i40e_vc_process_vflr_event(struct i40e_pf *pf);
bool i40e_reset_vf(struct i40e_vf *vf, bool flr);
bool i40e_reset_all_vfs(struct i40e_pf *pf, bool flr);
void i40e_vc_notify_vf_reset(struct i40e_vf *vf);

 
int i40e_ndo_set_vf_mac(struct net_device *netdev, int vf_id, u8 *mac);
int i40e_ndo_set_vf_port_vlan(struct net_device *netdev, int vf_id,
			      u16 vlan_id, u8 qos, __be16 vlan_proto);
int i40e_ndo_set_vf_bw(struct net_device *netdev, int vf_id, int min_tx_rate,
		       int max_tx_rate);
int i40e_ndo_set_vf_trust(struct net_device *netdev, int vf_id, bool setting);
int i40e_ndo_get_vf_config(struct net_device *netdev,
			   int vf_id, struct ifla_vf_info *ivi);
int i40e_ndo_set_vf_link_state(struct net_device *netdev, int vf_id, int link);
int i40e_ndo_set_vf_spoofchk(struct net_device *netdev, int vf_id, bool enable);

void i40e_vc_notify_link_state(struct i40e_pf *pf);
void i40e_vc_notify_reset(struct i40e_pf *pf);
#ifdef CONFIG_PCI_IOV
void i40e_restore_all_vfs_msi_state(struct pci_dev *pdev);
#endif  
int i40e_get_vf_stats(struct net_device *netdev, int vf_id,
		      struct ifla_vf_stats *vf_stats);

#endif  
