#ifndef _ICE_SWITCH_H_
#define _ICE_SWITCH_H_
#include "ice_common.h"
#define ICE_SW_CFG_MAX_BUF_LEN 2048
#define ICE_DFLT_VSI_INVAL 0xff
#define ICE_FLTR_RX BIT(0)
#define ICE_FLTR_TX BIT(1)
#define ICE_VSI_INVAL_ID 0xffff
#define ICE_INVAL_Q_HANDLE 0xFFFF
#define ICE_PROFID_IPV4_GTPC_TEID			41
#define ICE_PROFID_IPV4_GTPC_NO_TEID			42
#define ICE_PROFID_IPV4_GTPU_TEID			43
#define ICE_PROFID_IPV6_GTPC_TEID			44
#define ICE_PROFID_IPV6_GTPC_NO_TEID			45
#define ICE_PROFID_IPV6_GTPU_TEID			46
#define ICE_PROFID_IPV6_GTPU_IPV6_TCP_INNER		70
#define ICE_SW_RULE_VSI_LIST_SIZE(s, n)		struct_size((s), vsi, (n))
#define ICE_SW_RULE_RX_TX_HDR_SIZE(s, l)	struct_size((s), hdr_data, (l))
#define ICE_SW_RULE_RX_TX_ETH_HDR_SIZE(s)	\
	ICE_SW_RULE_RX_TX_HDR_SIZE((s), DUMMY_ETH_HDR_LEN)
#define ICE_SW_RULE_RX_TX_NO_HDR_SIZE(s)	\
	ICE_SW_RULE_RX_TX_HDR_SIZE((s), 0)
#define ICE_SW_RULE_LG_ACT_SIZE(s, n)		struct_size((s), act, (n))
#define DUMMY_ETH_HDR_LEN		16
struct ice_vsi_ctx {
	u16 vsi_num;
	u16 vsis_allocd;
	u16 vsis_unallocated;
	u16 flags;
	struct ice_aqc_vsi_props info;
	struct ice_sched_vsi_info sched;
	u8 alloc_from_pool;
	u8 vf_num;
	u16 num_lan_q_entries[ICE_MAX_TRAFFIC_CLASS];
	struct ice_q_ctx *lan_q_ctx[ICE_MAX_TRAFFIC_CLASS];
	u16 num_rdma_q_entries[ICE_MAX_TRAFFIC_CLASS];
	struct ice_q_ctx *rdma_q_ctx[ICE_MAX_TRAFFIC_CLASS];
};
enum ice_sw_lkup_type {
	ICE_SW_LKUP_ETHERTYPE = 0,
	ICE_SW_LKUP_MAC = 1,
	ICE_SW_LKUP_MAC_VLAN = 2,
	ICE_SW_LKUP_PROMISC = 3,
	ICE_SW_LKUP_VLAN = 4,
	ICE_SW_LKUP_DFLT = 5,
	ICE_SW_LKUP_ETHERTYPE_MAC = 8,
	ICE_SW_LKUP_PROMISC_VLAN = 9,
	ICE_SW_LKUP_LAST
};
enum ice_src_id {
	ICE_SRC_ID_UNKNOWN = 0,
	ICE_SRC_ID_VSI,
	ICE_SRC_ID_QUEUE,
	ICE_SRC_ID_LPORT,
};
struct ice_fltr_info {
	enum ice_sw_lkup_type lkup_type;
	enum ice_sw_fwd_act_type fltr_act;
	u16 fltr_rule_id;
	u16 flag;
	u16 src;
	enum ice_src_id src_id;
	union {
		struct {
			u8 mac_addr[ETH_ALEN];
		} mac;
		struct {
			u8 mac_addr[ETH_ALEN];
			u16 vlan_id;
		} mac_vlan;
		struct {
			u16 vlan_id;
			u16 tpid;
			u8 tpid_valid;
		} vlan;
		struct {
			u16 ethertype;
			u8 mac_addr[ETH_ALEN];  
		} ethertype_mac;
	} l_data;  
	union {
		u16 q_id:11;
		u16 hw_vsi_id:10;
		u16 vsi_list_id:10;
	} fwd_id;
	u16 vsi_handle;
	u8 qgrp_size;
	u8 lb_en;	 
	u8 lan_en;	 
};
struct ice_update_recipe_lkup_idx_params {
	u16 rid;
	u16 fv_idx;
	bool ignore_valid;
	u16 mask;
	bool mask_valid;
	u8 lkup_idx;
};
struct ice_adv_lkup_elem {
	enum ice_protocol_type type;
	union {
		union ice_prot_hdr h_u;	 
		u16 h_raw[sizeof(union ice_prot_hdr) / sizeof(u16)];
	};
	union {
		union ice_prot_hdr m_u;	 
		u16 m_raw[sizeof(union ice_prot_hdr) / sizeof(u16)];
	};
};
struct ice_sw_act_ctrl {
	u16 src;
	u16 flag;
	enum ice_sw_fwd_act_type fltr_act;
	union {
		u16 q_id:11;
		u16 vsi_id:10;
		u16 hw_vsi_id:10;
		u16 vsi_list_id:10;
	} fwd_id;
	u16 vsi_handle;
	u8 qgrp_size;
};
struct ice_rule_query_data {
	u16 rid;
	u16 rule_id;
	u16 vsi_handle;
};
struct ice_adv_rule_flags_info {
	u32 act;
	u8 act_valid;		 
};
struct ice_adv_rule_info {
	enum ice_sw_tunnel_type tun_type;
	u16 vlan_type;
	u16 fltr_rule_id;
	u32 priority;
	u16 need_pass_l2:1;
	u16 allow_pass_l2:1;
	u16 src_vsi;
	struct ice_sw_act_ctrl sw_act;
	struct ice_adv_rule_flags_info flags_info;
};
struct ice_sw_recipe {
	u8 is_root;
	u8 root_rid;
	u8 recp_created;
	u8 n_ext_words;
	struct ice_fv_word ext_words[ICE_MAX_CHAIN_WORDS];
	u16 word_masks[ICE_MAX_CHAIN_WORDS];
	u8 big_recp;
	u8 chain_idx;
	u8 n_grp_count;
	DECLARE_BITMAP(r_bitmap, ICE_MAX_NUM_RECIPES);
	enum ice_sw_tunnel_type tun_type;
	u8 adv_rule;
	struct list_head filt_rules;
	struct list_head filt_replay_rules;
	struct mutex filt_rule_lock;	 
	struct list_head fv_list;
	u8 num_profs, *prof_ids;
	DECLARE_BITMAP(res_idxs, ICE_MAX_FV_WORDS);
	u8 priority;
	u8 need_pass_l2:1;
	u8 allow_pass_l2:1;
	struct list_head rg_list;
	struct ice_aqc_recipe_data_elem *root_buf;
	struct ice_prot_lkup_ext lkup_exts;
};
struct ice_vsi_list_map_info {
	struct list_head list_entry;
	DECLARE_BITMAP(vsi_map, ICE_MAX_VSI);
	u16 vsi_list_id;
	u16 ref_cnt;
};
struct ice_fltr_list_entry {
	struct list_head list_entry;
	int status;
	struct ice_fltr_info fltr_info;
};
struct ice_fltr_mgmt_list_entry {
	struct ice_vsi_list_map_info *vsi_list_info;
	u16 vsi_count;
#define ICE_INVAL_LG_ACT_INDEX 0xffff
	u16 lg_act_idx;
#define ICE_INVAL_SW_MARKER_ID 0xffff
	u16 sw_marker_id;
	struct list_head list_entry;
	struct ice_fltr_info fltr_info;
#define ICE_INVAL_COUNTER_ID 0xff
	u8 counter_index;
};
struct ice_adv_fltr_mgmt_list_entry {
	struct list_head list_entry;
	struct ice_adv_lkup_elem *lkups;
	struct ice_adv_rule_info rule_info;
	u16 lkups_cnt;
	struct ice_vsi_list_map_info *vsi_list_info;
	u16 vsi_count;
};
enum ice_promisc_flags {
	ICE_PROMISC_UCAST_RX = 0x1,
	ICE_PROMISC_UCAST_TX = 0x2,
	ICE_PROMISC_MCAST_RX = 0x4,
	ICE_PROMISC_MCAST_TX = 0x8,
	ICE_PROMISC_BCAST_RX = 0x10,
	ICE_PROMISC_BCAST_TX = 0x20,
	ICE_PROMISC_VLAN_RX = 0x40,
	ICE_PROMISC_VLAN_TX = 0x80,
};
int
ice_add_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	    struct ice_sq_cd *cd);
int
ice_free_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	     bool keep_vsi_alloc, struct ice_sq_cd *cd);
int
ice_update_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	       struct ice_sq_cd *cd);
bool ice_is_vsi_valid(struct ice_hw *hw, u16 vsi_handle);
struct ice_vsi_ctx *ice_get_vsi_ctx(struct ice_hw *hw, u16 vsi_handle);
void ice_clear_all_vsi_ctx(struct ice_hw *hw);
int ice_get_initial_sw_cfg(struct ice_hw *hw);
int
ice_alloc_res_cntr(struct ice_hw *hw, u8 type, u8 alloc_shared, u16 num_items,
		   u16 *counter_id);
int
ice_free_res_cntr(struct ice_hw *hw, u8 type, u8 alloc_shared, u16 num_items,
		  u16 counter_id);
int ice_share_res(struct ice_hw *hw, u16 type, u8 shared, u16 res_id);
void ice_rule_add_tunnel_metadata(struct ice_adv_lkup_elem *lkup);
void ice_rule_add_direction_metadata(struct ice_adv_lkup_elem *lkup);
void ice_rule_add_vlan_metadata(struct ice_adv_lkup_elem *lkup);
void ice_rule_add_src_vsi_metadata(struct ice_adv_lkup_elem *lkup);
int
ice_add_adv_rule(struct ice_hw *hw, struct ice_adv_lkup_elem *lkups,
		 u16 lkups_cnt, struct ice_adv_rule_info *rinfo,
		 struct ice_rule_query_data *added_entry);
int ice_update_sw_rule_bridge_mode(struct ice_hw *hw);
int ice_add_vlan(struct ice_hw *hw, struct list_head *m_list);
int ice_remove_vlan(struct ice_hw *hw, struct list_head *v_list);
int ice_add_mac(struct ice_hw *hw, struct list_head *m_lst);
int ice_remove_mac(struct ice_hw *hw, struct list_head *m_lst);
bool ice_vlan_fltr_exist(struct ice_hw *hw, u16 vlan_id, u16 vsi_handle);
int ice_add_eth_mac(struct ice_hw *hw, struct list_head *em_list);
int ice_remove_eth_mac(struct ice_hw *hw, struct list_head *em_list);
int ice_cfg_rdma_fltr(struct ice_hw *hw, u16 vsi_handle, bool enable);
void ice_remove_vsi_fltr(struct ice_hw *hw, u16 vsi_handle);
int
ice_cfg_dflt_vsi(struct ice_port_info *pi, u16 vsi_handle, bool set,
		 u8 direction);
bool
ice_check_if_dflt_vsi(struct ice_port_info *pi, u16 vsi_handle,
		      bool *rule_exists);
int
ice_set_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
		    u16 vid);
int
ice_clear_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
		      u16 vid);
int
ice_set_vlan_vsi_promisc(struct ice_hw *hw, u16 vsi_handle, u8 promisc_mask,
			 bool rm_vlan_promisc);
int
ice_rem_adv_rule_by_id(struct ice_hw *hw,
		       struct ice_rule_query_data *remove_entry);
int ice_init_def_sw_recp(struct ice_hw *hw);
u16 ice_get_hw_vsi_num(struct ice_hw *hw, u16 vsi_handle);
int ice_replay_vsi_all_fltr(struct ice_hw *hw, u16 vsi_handle);
void ice_rm_all_sw_replay_rule_info(struct ice_hw *hw);
void ice_fill_eth_hdr(u8 *eth_hdr);
int
ice_aq_sw_rules(struct ice_hw *hw, void *rule_list, u16 rule_list_sz,
		u8 num_rules, enum ice_adminq_opc opc, struct ice_sq_cd *cd);
int
ice_update_recipe_lkup_idx(struct ice_hw *hw,
			   struct ice_update_recipe_lkup_idx_params *params);
void ice_change_proto_id_to_dvm(void);
struct ice_vsi_list_map_info *
ice_find_vsi_list_entry(struct ice_hw *hw, u8 recp_id, u16 vsi_handle,
			u16 *vsi_list_id);
int ice_alloc_recipe(struct ice_hw *hw, u16 *rid);
int ice_aq_get_recipe(struct ice_hw *hw,
		      struct ice_aqc_recipe_data_elem *s_recipe_list,
		      u16 *num_recipes, u16 recipe_root, struct ice_sq_cd *cd);
int ice_aq_add_recipe(struct ice_hw *hw,
		      struct ice_aqc_recipe_data_elem *s_recipe_list,
		      u16 num_recipes, struct ice_sq_cd *cd);
int
ice_aq_get_recipe_to_profile(struct ice_hw *hw, u32 profile_id, u8 *r_bitmap,
			     struct ice_sq_cd *cd);
int
ice_aq_map_recipe_to_profile(struct ice_hw *hw, u32 profile_id, u8 *r_bitmap,
			     struct ice_sq_cd *cd);
#endif  
