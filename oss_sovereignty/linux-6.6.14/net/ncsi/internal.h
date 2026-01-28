#ifndef __NCSI_INTERNAL_H__
#define __NCSI_INTERNAL_H__
enum {
	NCSI_CAP_BASE		= 0,
	NCSI_CAP_GENERIC	= 0,
	NCSI_CAP_BC,
	NCSI_CAP_MC,
	NCSI_CAP_BUFFER,
	NCSI_CAP_AEN,
	NCSI_CAP_VLAN,
	NCSI_CAP_MAX
};
enum {
	NCSI_CAP_GENERIC_HWA             = 0x01,  
	NCSI_CAP_GENERIC_HDS             = 0x02,  
	NCSI_CAP_GENERIC_FC              = 0x04,  
	NCSI_CAP_GENERIC_FC1             = 0x08,  
	NCSI_CAP_GENERIC_MC              = 0x10,  
	NCSI_CAP_GENERIC_HWA_UNKNOWN     = 0x00,  
	NCSI_CAP_GENERIC_HWA_SUPPORT     = 0x20,  
	NCSI_CAP_GENERIC_HWA_NOT_SUPPORT = 0x40,  
	NCSI_CAP_GENERIC_HWA_RESERVED    = 0x60,  
	NCSI_CAP_GENERIC_HWA_MASK        = 0x60,  
	NCSI_CAP_GENERIC_MASK            = 0x7f,
	NCSI_CAP_BC_ARP                  = 0x01,  
	NCSI_CAP_BC_DHCPC                = 0x02,  
	NCSI_CAP_BC_DHCPS                = 0x04,  
	NCSI_CAP_BC_NETBIOS              = 0x08,  
	NCSI_CAP_BC_MASK                 = 0x0f,
	NCSI_CAP_MC_IPV6_NEIGHBOR        = 0x01,  
	NCSI_CAP_MC_IPV6_ROUTER          = 0x02,  
	NCSI_CAP_MC_DHCPV6_RELAY         = 0x04,  
	NCSI_CAP_MC_DHCPV6_WELL_KNOWN    = 0x08,  
	NCSI_CAP_MC_IPV6_MLD             = 0x10,  
	NCSI_CAP_MC_IPV6_NEIGHBOR_S      = 0x20,  
	NCSI_CAP_MC_MASK                 = 0x3f,
	NCSI_CAP_AEN_LSC                 = 0x01,  
	NCSI_CAP_AEN_CR                  = 0x02,  
	NCSI_CAP_AEN_HDS                 = 0x04,  
	NCSI_CAP_AEN_MASK                = 0x07,
	NCSI_CAP_VLAN_ONLY               = 0x01,  
	NCSI_CAP_VLAN_NO                 = 0x02,  
	NCSI_CAP_VLAN_ANY                = 0x04,  
	NCSI_CAP_VLAN_MASK               = 0x07
};
enum {
	NCSI_MODE_BASE		= 0,
	NCSI_MODE_ENABLE	= 0,
	NCSI_MODE_TX_ENABLE,
	NCSI_MODE_LINK,
	NCSI_MODE_VLAN,
	NCSI_MODE_BC,
	NCSI_MODE_MC,
	NCSI_MODE_AEN,
	NCSI_MODE_FC,
	NCSI_MODE_MAX
};
enum {
	MLX_MC_RBT_SUPPORT  = 0x01,  
	MLX_MC_RBT_AVL      = 0x08,  
};
#define NCSI_OEM_MFR_MLX_ID             0x8119
#define NCSI_OEM_MFR_BCM_ID             0x113d
#define NCSI_OEM_MFR_INTEL_ID           0x157
#define NCSI_OEM_INTEL_CMD_GMA          0x06    
#define NCSI_OEM_INTEL_CMD_KEEP_PHY     0x20    
#define NCSI_OEM_BCM_CMD_GMA            0x01    
#define NCSI_OEM_MLX_CMD_GMA            0x00    
#define NCSI_OEM_MLX_CMD_GMA_PARAM      0x1b    
#define NCSI_OEM_MLX_CMD_SMAF           0x01    
#define NCSI_OEM_MLX_CMD_SMAF_PARAM     0x07    
#define NCSI_OEM_INTEL_CMD_GMA_LEN      5
#define NCSI_OEM_INTEL_CMD_KEEP_PHY_LEN 7
#define NCSI_OEM_BCM_CMD_GMA_LEN        12
#define NCSI_OEM_MLX_CMD_GMA_LEN        8
#define NCSI_OEM_MLX_CMD_SMAF_LEN        60
#define MLX_SMAF_MAC_ADDR_OFFSET         8      
#define MLX_SMAF_MED_SUPPORT_OFFSET      14     
#define BCM_MAC_ADDR_OFFSET             28
#define MLX_MAC_ADDR_OFFSET             8
#define INTEL_MAC_ADDR_OFFSET           1
struct ncsi_channel_version {
	u8   major;		 
	u8   minor;		 
	u8   update;		 
	char alpha1;		 
	char alpha2;		 
	u8  fw_name[12];	 
	u32 fw_version;		 
	u16 pci_ids[4];		 
	u32 mf_id;		 
};
struct ncsi_channel_cap {
	u32 index;	 
	u32 cap;	 
};
struct ncsi_channel_mode {
	u32 index;	 
	u32 enable;	 
	u32 size;	 
	u32 data[8];	 
};
struct ncsi_channel_mac_filter {
	u8	n_uc;
	u8	n_mc;
	u8	n_mixed;
	u64	bitmap;
	unsigned char	*addrs;
};
struct ncsi_channel_vlan_filter {
	u8	n_vids;
	u64	bitmap;
	u16	*vids;
};
struct ncsi_channel_stats {
	u32 hnc_cnt_hi;		 
	u32 hnc_cnt_lo;		 
	u32 hnc_rx_bytes;	 
	u32 hnc_tx_bytes;	 
	u32 hnc_rx_uc_pkts;	 
	u32 hnc_rx_mc_pkts;      
	u32 hnc_rx_bc_pkts;	 
	u32 hnc_tx_uc_pkts;	 
	u32 hnc_tx_mc_pkts;	 
	u32 hnc_tx_bc_pkts;	 
	u32 hnc_fcs_err;	 
	u32 hnc_align_err;	 
	u32 hnc_false_carrier;	 
	u32 hnc_runt_pkts;	 
	u32 hnc_jabber_pkts;	 
	u32 hnc_rx_pause_xon;	 
	u32 hnc_rx_pause_xoff;	 
	u32 hnc_tx_pause_xon;	 
	u32 hnc_tx_pause_xoff;	 
	u32 hnc_tx_s_collision;	 
	u32 hnc_tx_m_collision;	 
	u32 hnc_l_collision;	 
	u32 hnc_e_collision;	 
	u32 hnc_rx_ctl_frames;	 
	u32 hnc_rx_64_frames;	 
	u32 hnc_rx_127_frames;	 
	u32 hnc_rx_255_frames;	 
	u32 hnc_rx_511_frames;	 
	u32 hnc_rx_1023_frames;	 
	u32 hnc_rx_1522_frames;	 
	u32 hnc_rx_9022_frames;	 
	u32 hnc_tx_64_frames;	 
	u32 hnc_tx_127_frames;	 
	u32 hnc_tx_255_frames;	 
	u32 hnc_tx_511_frames;	 
	u32 hnc_tx_1023_frames;	 
	u32 hnc_tx_1522_frames;	 
	u32 hnc_tx_9022_frames;	 
	u32 hnc_rx_valid_bytes;	 
	u32 hnc_rx_runt_pkts;	 
	u32 hnc_rx_jabber_pkts;	 
	u32 ncsi_rx_cmds;	 
	u32 ncsi_dropped_cmds;	 
	u32 ncsi_cmd_type_errs;	 
	u32 ncsi_cmd_csum_errs;	 
	u32 ncsi_rx_pkts;	 
	u32 ncsi_tx_pkts;	 
	u32 ncsi_tx_aen_pkts;	 
	u32 pt_tx_pkts;		 
	u32 pt_tx_dropped;	 
	u32 pt_tx_channel_err;	 
	u32 pt_tx_us_err;	 
	u32 pt_rx_pkts;		 
	u32 pt_rx_dropped;	 
	u32 pt_rx_channel_err;	 
	u32 pt_rx_us_err;	 
	u32 pt_rx_os_err;	 
};
struct ncsi_dev_priv;
struct ncsi_package;
#define NCSI_PACKAGE_SHIFT	5
#define NCSI_PACKAGE_INDEX(c)	(((c) >> NCSI_PACKAGE_SHIFT) & 0x7)
#define NCSI_RESERVED_CHANNEL	0x1f
#define NCSI_CHANNEL_INDEX(c)	((c) & ((1 << NCSI_PACKAGE_SHIFT) - 1))
#define NCSI_TO_CHANNEL(p, c)	(((p) << NCSI_PACKAGE_SHIFT) | (c))
#define NCSI_MAX_PACKAGE	8
#define NCSI_MAX_CHANNEL	32
struct ncsi_channel {
	unsigned char               id;
	int                         state;
#define NCSI_CHANNEL_INACTIVE		1
#define NCSI_CHANNEL_ACTIVE		2
#define NCSI_CHANNEL_INVISIBLE		3
	bool                        reconfigure_needed;
	spinlock_t                  lock;	 
	struct ncsi_package         *package;
	struct ncsi_channel_version version;
	struct ncsi_channel_cap	    caps[NCSI_CAP_MAX];
	struct ncsi_channel_mode    modes[NCSI_MODE_MAX];
	struct ncsi_channel_mac_filter	mac_filter;
	struct ncsi_channel_vlan_filter	vlan_filter;
	struct ncsi_channel_stats   stats;
	struct {
		struct timer_list   timer;
		bool                enabled;
		unsigned int        state;
#define NCSI_CHANNEL_MONITOR_START	0
#define NCSI_CHANNEL_MONITOR_RETRY	1
#define NCSI_CHANNEL_MONITOR_WAIT	2
#define NCSI_CHANNEL_MONITOR_WAIT_MAX	5
	} monitor;
	struct list_head            node;
	struct list_head            link;
};
struct ncsi_package {
	unsigned char        id;           
	unsigned char        uuid[16];     
	struct ncsi_dev_priv *ndp;         
	spinlock_t           lock;         
	unsigned int         channel_num;  
	struct list_head     channels;     
	struct list_head     node;         
	bool                 multi_channel;  
	u32                  channel_whitelist;  
	struct ncsi_channel  *preferred_channel;  
};
struct ncsi_request {
	unsigned char        id;       
	bool                 used;     
	unsigned int         flags;    
#define NCSI_REQ_FLAG_EVENT_DRIVEN	1
#define NCSI_REQ_FLAG_NETLINK_DRIVEN	2
	struct ncsi_dev_priv *ndp;     
	struct sk_buff       *cmd;     
	struct sk_buff       *rsp;     
	struct timer_list    timer;    
	bool                 enabled;  
	u32                  snd_seq;      
	u32                  snd_portid;   
	struct nlmsghdr      nlhdr;        
};
enum {
	ncsi_dev_state_major		= 0xff00,
	ncsi_dev_state_minor		= 0x00ff,
	ncsi_dev_state_probe_deselect	= 0x0201,
	ncsi_dev_state_probe_package,
	ncsi_dev_state_probe_channel,
	ncsi_dev_state_probe_mlx_gma,
	ncsi_dev_state_probe_mlx_smaf,
	ncsi_dev_state_probe_cis,
	ncsi_dev_state_probe_keep_phy,
	ncsi_dev_state_probe_gvi,
	ncsi_dev_state_probe_gc,
	ncsi_dev_state_probe_gls,
	ncsi_dev_state_probe_dp,
	ncsi_dev_state_config_sp	= 0x0301,
	ncsi_dev_state_config_cis,
	ncsi_dev_state_config_oem_gma,
	ncsi_dev_state_config_clear_vids,
	ncsi_dev_state_config_svf,
	ncsi_dev_state_config_ev,
	ncsi_dev_state_config_sma,
	ncsi_dev_state_config_ebf,
	ncsi_dev_state_config_dgmf,
	ncsi_dev_state_config_ecnt,
	ncsi_dev_state_config_ec,
	ncsi_dev_state_config_ae,
	ncsi_dev_state_config_gls,
	ncsi_dev_state_config_done,
	ncsi_dev_state_suspend_select	= 0x0401,
	ncsi_dev_state_suspend_gls,
	ncsi_dev_state_suspend_dcnt,
	ncsi_dev_state_suspend_dc,
	ncsi_dev_state_suspend_deselect,
	ncsi_dev_state_suspend_done
};
struct vlan_vid {
	struct list_head list;
	__be16 proto;
	u16 vid;
};
struct ncsi_dev_priv {
	struct ncsi_dev     ndev;             
	unsigned int        flags;            
#define NCSI_DEV_PROBED		1             
#define NCSI_DEV_HWA		2             
#define NCSI_DEV_RESHUFFLE	4
#define NCSI_DEV_RESET		8             
	unsigned int        gma_flag;         
	spinlock_t          lock;             
	unsigned int        package_probe_id; 
	unsigned int        package_num;      
	struct list_head    packages;         
	struct ncsi_channel *hot_channel;     
	struct ncsi_request requests[256];    
	unsigned int        request_id;       
#define NCSI_REQ_START_IDX	1
	unsigned int        pending_req_num;  
	struct ncsi_package *active_package;  
	struct ncsi_channel *active_channel;  
	struct list_head    channel_queue;    
	struct work_struct  work;             
	struct packet_type  ptype;            
	struct list_head    node;             
#define NCSI_MAX_VLAN_VIDS	15
	struct list_head    vlan_vids;        
	bool                multi_package;    
	bool                mlx_multi_host;   
	u32                 package_whitelist;  
};
struct ncsi_cmd_arg {
	struct ncsi_dev_priv *ndp;         
	unsigned char        type;         
	unsigned char        id;           
	unsigned char        package;      
	unsigned char        channel;      
	unsigned short       payload;      
	unsigned int         req_flags;    
	union {
		unsigned char  bytes[16];  
		unsigned short words[8];
		unsigned int   dwords[4];
	};
	unsigned char        *data;        
	struct genl_info     *info;        
};
extern struct list_head ncsi_dev_list;
extern spinlock_t ncsi_dev_lock;
#define TO_NCSI_DEV_PRIV(nd) \
	container_of(nd, struct ncsi_dev_priv, ndev)
#define NCSI_FOR_EACH_DEV(ndp) \
	list_for_each_entry_rcu(ndp, &ncsi_dev_list, node)
#define NCSI_FOR_EACH_PACKAGE(ndp, np) \
	list_for_each_entry_rcu(np, &ndp->packages, node)
#define NCSI_FOR_EACH_CHANNEL(np, nc) \
	list_for_each_entry_rcu(nc, &np->channels, node)
int ncsi_reset_dev(struct ncsi_dev *nd);
void ncsi_start_channel_monitor(struct ncsi_channel *nc);
void ncsi_stop_channel_monitor(struct ncsi_channel *nc);
struct ncsi_channel *ncsi_find_channel(struct ncsi_package *np,
				       unsigned char id);
struct ncsi_channel *ncsi_add_channel(struct ncsi_package *np,
				      unsigned char id);
struct ncsi_package *ncsi_find_package(struct ncsi_dev_priv *ndp,
				       unsigned char id);
struct ncsi_package *ncsi_add_package(struct ncsi_dev_priv *ndp,
				      unsigned char id);
void ncsi_remove_package(struct ncsi_package *np);
void ncsi_find_package_and_channel(struct ncsi_dev_priv *ndp,
				   unsigned char id,
				   struct ncsi_package **np,
				   struct ncsi_channel **nc);
struct ncsi_request *ncsi_alloc_request(struct ncsi_dev_priv *ndp,
					unsigned int req_flags);
void ncsi_free_request(struct ncsi_request *nr);
struct ncsi_dev *ncsi_find_dev(struct net_device *dev);
int ncsi_process_next_channel(struct ncsi_dev_priv *ndp);
bool ncsi_channel_has_link(struct ncsi_channel *channel);
bool ncsi_channel_is_last(struct ncsi_dev_priv *ndp,
			  struct ncsi_channel *channel);
int ncsi_update_tx_channel(struct ncsi_dev_priv *ndp,
			   struct ncsi_package *np,
			   struct ncsi_channel *disable,
			   struct ncsi_channel *enable);
u32 ncsi_calculate_checksum(unsigned char *data, int len);
int ncsi_xmit_cmd(struct ncsi_cmd_arg *nca);
int ncsi_rcv_rsp(struct sk_buff *skb, struct net_device *dev,
		 struct packet_type *pt, struct net_device *orig_dev);
int ncsi_aen_handler(struct ncsi_dev_priv *ndp, struct sk_buff *skb);
#endif  
