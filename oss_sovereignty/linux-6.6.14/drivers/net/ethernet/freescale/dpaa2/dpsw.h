#ifndef __FSL_DPSW_H
#define __FSL_DPSW_H
struct fsl_mc_io;
#define DPSW_MAX_PRIORITIES	8
#define DPSW_MAX_IF		64
int dpsw_open(struct fsl_mc_io *mc_io, u32 cmd_flags, int dpsw_id, u16 *token);
int dpsw_close(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);
#define DPSW_OPT_FLOODING_DIS		0x0000000000000001ULL
#define DPSW_OPT_MULTICAST_DIS		0x0000000000000004ULL
#define DPSW_OPT_CTRL_IF_DIS		0x0000000000000010ULL
enum dpsw_component_type {
	DPSW_COMPONENT_TYPE_C_VLAN = 0,
	DPSW_COMPONENT_TYPE_S_VLAN
};
enum dpsw_flooding_cfg {
	DPSW_FLOODING_PER_VLAN = 0,
	DPSW_FLOODING_PER_FDB,
};
enum dpsw_broadcast_cfg {
	DPSW_BROADCAST_PER_OBJECT = 0,
	DPSW_BROADCAST_PER_FDB,
};
int dpsw_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);
int dpsw_disable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);
int dpsw_reset(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);
#define DPSW_IRQ_INDEX_IF		0x0000
#define DPSW_IRQ_INDEX_L2SW		0x0001
#define DPSW_IRQ_EVENT_LINK_CHANGED	0x0001
#define DPSW_IRQ_EVENT_ENDPOINT_CHANGED	0x0002
struct dpsw_irq_cfg {
	u64 addr;
	u32 val;
	int irq_num;
};
int dpsw_set_irq_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u8 irq_index, u8 en);
int dpsw_set_irq_mask(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		      u8 irq_index, u32 mask);
int dpsw_get_irq_status(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u8 irq_index, u32 *status);
int dpsw_clear_irq_status(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			  u8 irq_index, u32 status);
struct dpsw_attr {
	int id;
	u64 options;
	u16 max_vlans;
	u8 max_meters_per_if;
	u8 max_fdbs;
	u16 max_fdb_entries;
	u16 fdb_aging_time;
	u16 max_fdb_mc_groups;
	u16 num_ifs;
	u16 mem_size;
	u16 num_vlans;
	u8 num_fdbs;
	enum dpsw_component_type component_type;
	enum dpsw_flooding_cfg flooding_cfg;
	enum dpsw_broadcast_cfg broadcast_cfg;
};
int dpsw_get_attributes(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			struct dpsw_attr *attr);
struct dpsw_ctrl_if_attr {
	u32 rx_fqid;
	u32 rx_err_fqid;
	u32 tx_err_conf_fqid;
};
int dpsw_ctrl_if_get_attributes(struct fsl_mc_io *mc_io, u32 cmd_flags,
				u16 token, struct dpsw_ctrl_if_attr *attr);
enum dpsw_queue_type {
	DPSW_QUEUE_RX,
	DPSW_QUEUE_TX_ERR_CONF,
	DPSW_QUEUE_RX_ERR,
};
#define DPSW_MAX_DPBP     8
struct dpsw_ctrl_if_pools_cfg {
	u8 num_dpbp;
	struct {
		int dpbp_id;
		u16 buffer_size;
		int backup_pool;
	} pools[DPSW_MAX_DPBP];
};
int dpsw_ctrl_if_set_pools(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   const struct dpsw_ctrl_if_pools_cfg *cfg);
#define DPSW_CTRL_IF_QUEUE_OPT_USER_CTX		0x00000001
#define DPSW_CTRL_IF_QUEUE_OPT_DEST		0x00000002
enum dpsw_ctrl_if_dest {
	DPSW_CTRL_IF_DEST_NONE = 0,
	DPSW_CTRL_IF_DEST_DPIO = 1,
};
struct dpsw_ctrl_if_dest_cfg {
	enum dpsw_ctrl_if_dest dest_type;
	int dest_id;
	u8 priority;
};
struct dpsw_ctrl_if_queue_cfg {
	u32 options;
	u64 user_ctx;
	struct dpsw_ctrl_if_dest_cfg dest_cfg;
};
int dpsw_ctrl_if_set_queue(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   enum dpsw_queue_type qtype,
			   const struct dpsw_ctrl_if_queue_cfg *cfg);
int dpsw_ctrl_if_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);
int dpsw_ctrl_if_disable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token);
enum dpsw_action {
	DPSW_ACTION_DROP = 0,
	DPSW_ACTION_REDIRECT = 1
};
#define DPSW_LINK_OPT_AUTONEG		0x0000000000000001ULL
#define DPSW_LINK_OPT_HALF_DUPLEX	0x0000000000000002ULL
#define DPSW_LINK_OPT_PAUSE		0x0000000000000004ULL
#define DPSW_LINK_OPT_ASYM_PAUSE	0x0000000000000008ULL
struct dpsw_link_cfg {
	u32 rate;
	u64 options;
};
int dpsw_if_set_link_cfg(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id,
			 struct dpsw_link_cfg *cfg);
struct dpsw_link_state {
	u32 rate;
	u64 options;
	u8 up;
};
int dpsw_if_get_link_state(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 if_id, struct dpsw_link_state *state);
struct dpsw_tci_cfg {
	u8 pcp;
	u8 dei;
	u16 vlan_id;
};
int dpsw_if_set_tci(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id,
		    const struct dpsw_tci_cfg *cfg);
int dpsw_if_get_tci(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id,
		    struct dpsw_tci_cfg *cfg);
enum dpsw_stp_state {
	DPSW_STP_STATE_DISABLED = 0,
	DPSW_STP_STATE_LISTENING = 1,
	DPSW_STP_STATE_LEARNING = 2,
	DPSW_STP_STATE_FORWARDING = 3,
	DPSW_STP_STATE_BLOCKING = 0
};
struct dpsw_stp_cfg {
	u16 vlan_id;
	enum dpsw_stp_state state;
};
int dpsw_if_set_stp(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id,
		    const struct dpsw_stp_cfg *cfg);
enum dpsw_accepted_frames {
	DPSW_ADMIT_ALL = 1,
	DPSW_ADMIT_ONLY_VLAN_TAGGED = 3
};
enum dpsw_counter {
	DPSW_CNT_ING_FRAME = 0x0,
	DPSW_CNT_ING_BYTE = 0x1,
	DPSW_CNT_ING_FLTR_FRAME = 0x2,
	DPSW_CNT_ING_FRAME_DISCARD = 0x3,
	DPSW_CNT_ING_MCAST_FRAME = 0x4,
	DPSW_CNT_ING_MCAST_BYTE = 0x5,
	DPSW_CNT_ING_BCAST_FRAME = 0x6,
	DPSW_CNT_ING_BCAST_BYTES = 0x7,
	DPSW_CNT_EGR_FRAME = 0x8,
	DPSW_CNT_EGR_BYTE = 0x9,
	DPSW_CNT_EGR_FRAME_DISCARD = 0xa,
	DPSW_CNT_EGR_STP_FRAME_DISCARD = 0xb,
	DPSW_CNT_ING_NO_BUFF_DISCARD = 0xc,
};
int dpsw_if_get_counter(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u16 if_id, enum dpsw_counter type, u64 *counter);
int dpsw_if_enable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id);
int dpsw_if_disable(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 if_id);
struct dpsw_if_attr {
	u8 num_tcs;
	u32 rate;
	u32 options;
	int enabled;
	int accept_all_vlan;
	enum dpsw_accepted_frames admit_untagged;
	u16 qdid;
};
int dpsw_if_get_attributes(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 if_id, struct dpsw_if_attr *attr);
int dpsw_if_set_max_frame_length(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
				 u16 if_id, u16 frame_length);
struct dpsw_vlan_cfg {
	u16 fdb_id;
};
int dpsw_vlan_add(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		  u16 vlan_id, const struct dpsw_vlan_cfg *cfg);
#define DPSW_VLAN_ADD_IF_OPT_FDB_ID            0x0001
struct dpsw_vlan_if_cfg {
	u16 num_ifs;
	u16 options;
	u16 if_id[DPSW_MAX_IF];
	u16 fdb_id;
};
int dpsw_vlan_add_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		     u16 vlan_id, const struct dpsw_vlan_if_cfg *cfg);
int dpsw_vlan_add_if_untagged(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 vlan_id, const struct dpsw_vlan_if_cfg *cfg);
int dpsw_vlan_remove_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			u16 vlan_id, const struct dpsw_vlan_if_cfg *cfg);
int dpsw_vlan_remove_if_untagged(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
				 u16 vlan_id, const struct dpsw_vlan_if_cfg *cfg);
int dpsw_vlan_remove(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		     u16 vlan_id);
enum dpsw_fdb_entry_type {
	DPSW_FDB_ENTRY_STATIC = 0,
	DPSW_FDB_ENTRY_DINAMIC = 1
};
struct dpsw_fdb_unicast_cfg {
	enum dpsw_fdb_entry_type type;
	u8 mac_addr[6];
	u16 if_egress;
};
int dpsw_fdb_add_unicast(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			 u16 fdb_id, const struct dpsw_fdb_unicast_cfg *cfg);
int dpsw_fdb_remove_unicast(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			    u16 fdb_id, const struct dpsw_fdb_unicast_cfg *cfg);
#define DPSW_FDB_ENTRY_TYPE_DYNAMIC  BIT(0)
#define DPSW_FDB_ENTRY_TYPE_UNICAST  BIT(1)
struct fdb_dump_entry {
	u8 mac_addr[6];
	u8 type;
	u8 if_info;
	u8 if_mask[8];
};
int dpsw_fdb_dump(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 fdb_id,
		  u64 iova_addr, u32 iova_size, u16 *num_entries);
struct dpsw_fdb_multicast_cfg {
	enum dpsw_fdb_entry_type type;
	u8 mac_addr[6];
	u16 num_ifs;
	u16 if_id[DPSW_MAX_IF];
};
int dpsw_fdb_add_multicast(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 fdb_id, const struct dpsw_fdb_multicast_cfg *cfg);
int dpsw_fdb_remove_multicast(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 fdb_id, const struct dpsw_fdb_multicast_cfg *cfg);
enum dpsw_learning_mode {
	DPSW_LEARNING_MODE_DIS = 0,
	DPSW_LEARNING_MODE_HW = 1,
	DPSW_LEARNING_MODE_NON_SECURE = 2,
	DPSW_LEARNING_MODE_SECURE = 3
};
struct dpsw_fdb_attr {
	u16 max_fdb_entries;
	u16 fdb_ageing_time;
	enum dpsw_learning_mode learning_mode;
	u16 num_fdb_mc_groups;
	u16 max_fdb_mc_groups;
};
int dpsw_get_api_version(struct fsl_mc_io *mc_io, u32 cmd_flags,
			 u16 *major_ver, u16 *minor_ver);
int dpsw_if_get_port_mac_addr(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 if_id, u8 mac_addr[6]);
struct dpsw_fdb_cfg {
	u16 num_fdb_entries;
	u16 fdb_ageing_time;
};
int dpsw_fdb_add(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 *fdb_id,
		 const struct dpsw_fdb_cfg *cfg);
int dpsw_fdb_remove(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 fdb_id);
enum dpsw_flood_type {
	DPSW_BROADCAST = 0,
	DPSW_FLOODING,
};
struct dpsw_egress_flood_cfg {
	u16 fdb_id;
	enum dpsw_flood_type flood_type;
	u16 num_ifs;
	u16 if_id[DPSW_MAX_IF];
};
int dpsw_set_egress_flood(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			  const struct dpsw_egress_flood_cfg *cfg);
int dpsw_if_set_learning_mode(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 if_id, enum dpsw_learning_mode mode);
struct dpsw_acl_cfg {
	u16 max_entries;
};
int dpsw_acl_add(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token, u16 *acl_id,
		 const struct dpsw_acl_cfg *cfg);
int dpsw_acl_remove(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		    u16 acl_id);
struct dpsw_acl_if_cfg {
	u16 num_ifs;
	u16 if_id[DPSW_MAX_IF];
};
int dpsw_acl_add_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		    u16 acl_id, const struct dpsw_acl_if_cfg *cfg);
int dpsw_acl_remove_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		       u16 acl_id, const struct dpsw_acl_if_cfg *cfg);
struct dpsw_acl_fields {
	u8 l2_dest_mac[6];
	u8 l2_source_mac[6];
	u16 l2_tpid;
	u8 l2_pcp_dei;
	u16 l2_vlan_id;
	u16 l2_ether_type;
	u8 l3_dscp;
	u8 l3_protocol;
	u32 l3_source_ip;
	u32 l3_dest_ip;
	u16 l4_source_port;
	u16 l4_dest_port;
};
struct dpsw_acl_key {
	struct dpsw_acl_fields match;
	struct dpsw_acl_fields mask;
};
enum dpsw_acl_action {
	DPSW_ACL_ACTION_DROP,
	DPSW_ACL_ACTION_REDIRECT,
	DPSW_ACL_ACTION_ACCEPT,
	DPSW_ACL_ACTION_REDIRECT_TO_CTRL_IF
};
struct dpsw_acl_result {
	enum dpsw_acl_action action;
	u16 if_id;
};
struct dpsw_acl_entry_cfg {
	u64 key_iova;
	struct dpsw_acl_result result;
	int precedence;
};
void dpsw_acl_prepare_entry_cfg(const struct dpsw_acl_key *key,
				u8 *entry_cfg_buf);
int dpsw_acl_add_entry(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
		       u16 acl_id, const struct dpsw_acl_entry_cfg *cfg);
int dpsw_acl_remove_entry(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			  u16 acl_id, const struct dpsw_acl_entry_cfg *cfg);
enum dpsw_reflection_filter {
	DPSW_REFLECTION_FILTER_INGRESS_ALL = 0,
	DPSW_REFLECTION_FILTER_INGRESS_VLAN = 1
};
struct dpsw_reflection_cfg {
	enum dpsw_reflection_filter filter;
	u16 vlan_id;
};
int dpsw_set_reflection_if(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 if_id);
int dpsw_if_add_reflection(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			   u16 if_id, const struct dpsw_reflection_cfg *cfg);
int dpsw_if_remove_reflection(struct fsl_mc_io *mc_io, u32 cmd_flags, u16 token,
			      u16 if_id, const struct dpsw_reflection_cfg *cfg);
#endif  
