#ifndef EF100_MAE_H
#define EF100_MAE_H
#include <net/devlink.h>
#include "net_driver.h"
#include "tc.h"
#include "mcdi_pcol.h"  
int efx_mae_allocate_mport(struct efx_nic *efx, u32 *id, u32 *label);
int efx_mae_free_mport(struct efx_nic *efx, u32 id);
void efx_mae_mport_wire(struct efx_nic *efx, u32 *out);
void efx_mae_mport_uplink(struct efx_nic *efx, u32 *out);
void efx_mae_mport_vf(struct efx_nic *efx, u32 vf_id, u32 *out);
void efx_mae_mport_mport(struct efx_nic *efx, u32 mport_id, u32 *out);
int efx_mae_lookup_mport(struct efx_nic *efx, u32 selector, u32 *id);
struct mae_mport_desc {
	u32 mport_id;
	u32 flags;
	u32 caller_flags;  
	u32 mport_type;  
	union {
		u32 port_idx;  
		u32 alias_mport_id;  
		struct {  
			u32 vnic_client_type;  
			u32 interface_idx;
			u16 pf_idx;
			u16 vf_idx;
		};
	};
	struct rhash_head linkage;
	struct devlink_port dl_port;
};
int efx_mae_enumerate_mports(struct efx_nic *efx);
struct mae_mport_desc *efx_mae_get_mport(struct efx_nic *efx, u32 mport_id);
void efx_mae_put_mport(struct efx_nic *efx, struct mae_mport_desc *desc);
struct efx_mae {
	struct efx_nic *efx;
	struct rhashtable mports_ht;
};
int efx_mae_start_counters(struct efx_nic *efx, struct efx_rx_queue *rx_queue);
int efx_mae_stop_counters(struct efx_nic *efx, struct efx_rx_queue *rx_queue);
void efx_mae_counters_grant_credits(struct work_struct *work);
int efx_mae_get_tables(struct efx_nic *efx);
void efx_mae_free_tables(struct efx_nic *efx);
#define MAE_NUM_FIELDS	(MAE_FIELD_ENC_VNET_ID + 1)
struct mae_caps {
	u32 match_field_count;
	u32 encap_types;
	u32 action_prios;
	u8 action_rule_fields[MAE_NUM_FIELDS];
	u8 outer_rule_fields[MAE_NUM_FIELDS];
};
int efx_mae_get_caps(struct efx_nic *efx, struct mae_caps *caps);
int efx_mae_match_check_caps(struct efx_nic *efx,
			     const struct efx_tc_match_fields *mask,
			     struct netlink_ext_ack *extack);
int efx_mae_match_check_caps_lhs(struct efx_nic *efx,
				 const struct efx_tc_match_fields *mask,
				 struct netlink_ext_ack *extack);
int efx_mae_check_encap_match_caps(struct efx_nic *efx, bool ipv6,
				   u8 ip_tos_mask, __be16 udp_sport_mask,
				   struct netlink_ext_ack *extack);
int efx_mae_check_encap_type_supported(struct efx_nic *efx,
				       enum efx_encap_type typ);
int efx_mae_allocate_counter(struct efx_nic *efx, struct efx_tc_counter *cnt);
int efx_mae_free_counter(struct efx_nic *efx, struct efx_tc_counter *cnt);
int efx_mae_allocate_encap_md(struct efx_nic *efx,
			      struct efx_tc_encap_action *encap);
int efx_mae_update_encap_md(struct efx_nic *efx,
			    struct efx_tc_encap_action *encap);
int efx_mae_free_encap_md(struct efx_nic *efx,
			  struct efx_tc_encap_action *encap);
int efx_mae_allocate_pedit_mac(struct efx_nic *efx,
			       struct efx_tc_mac_pedit_action *ped);
void efx_mae_free_pedit_mac(struct efx_nic *efx,
			    struct efx_tc_mac_pedit_action *ped);
int efx_mae_alloc_action_set(struct efx_nic *efx, struct efx_tc_action_set *act);
int efx_mae_free_action_set(struct efx_nic *efx, u32 fw_id);
int efx_mae_alloc_action_set_list(struct efx_nic *efx,
				  struct efx_tc_action_set_list *acts);
int efx_mae_free_action_set_list(struct efx_nic *efx,
				 struct efx_tc_action_set_list *acts);
int efx_mae_register_encap_match(struct efx_nic *efx,
				 struct efx_tc_encap_match *encap);
int efx_mae_unregister_encap_match(struct efx_nic *efx,
				   struct efx_tc_encap_match *encap);
int efx_mae_insert_lhs_rule(struct efx_nic *efx, struct efx_tc_lhs_rule *rule,
			    u32 prio);
int efx_mae_remove_lhs_rule(struct efx_nic *efx, struct efx_tc_lhs_rule *rule);
struct efx_tc_ct_entry;  
int efx_mae_insert_ct(struct efx_nic *efx, struct efx_tc_ct_entry *conn);
int efx_mae_remove_ct(struct efx_nic *efx, struct efx_tc_ct_entry *conn);
int efx_mae_insert_rule(struct efx_nic *efx, const struct efx_tc_match *match,
			u32 prio, u32 acts_id, u32 *id);
int efx_mae_update_rule(struct efx_nic *efx, u32 acts_id, u32 id);
int efx_mae_delete_rule(struct efx_nic *efx, u32 id);
int efx_init_mae(struct efx_nic *efx);
void efx_fini_mae(struct efx_nic *efx);
void efx_mae_remove_mport(void *desc, void *arg);
int efx_mae_fw_lookup_mport(struct efx_nic *efx, u32 selector, u32 *id);
int efx_mae_lookup_mport(struct efx_nic *efx, u32 vf, u32 *id);
#endif  
