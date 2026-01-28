#ifndef _FNIC_FIP_H_
#define _FNIC_FIP_H_
#define FCOE_CTLR_START_DELAY    2000     
#define FCOE_CTLR_FIPVLAN_TOV    2000     
#define FCOE_CTLR_MAX_SOL        8
#define FINC_MAX_FLOGI_REJECTS   8
struct vlan {
	__be16 vid;
	__be16 type;
};
struct fcoe_vlan {
	struct list_head list;
	u16 vid;		 
	u16 sol_count;		 
	u16 state;		 
};
enum fip_vlan_state {
	FIP_VLAN_AVAIL  = 0,	 
	FIP_VLAN_SENT   = 1,	 
	FIP_VLAN_USED   = 2,	 
	FIP_VLAN_FAILED = 3,	 
};
struct fip_vlan {
	struct ethhdr eth;
	struct fip_header fip;
	struct {
		struct fip_mac_desc mac;
		struct fip_wwn_desc wwnn;
	} desc;
};
#endif   
