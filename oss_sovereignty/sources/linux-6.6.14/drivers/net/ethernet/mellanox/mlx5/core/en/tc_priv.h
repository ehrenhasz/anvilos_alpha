


#ifndef __MLX5_EN_TC_PRIV_H__
#define __MLX5_EN_TC_PRIV_H__

#include "en_tc.h"
#include "en/tc/act/act.h"

#define MLX5E_TC_FLOW_BASE (MLX5E_TC_FLAG_LAST_EXPORTED_BIT + 1)

#define MLX5E_TC_MAX_SPLITS 1


enum {
	MLX5E_TC_FLOW_FLAG_INGRESS               = MLX5E_TC_FLAG_INGRESS_BIT,
	MLX5E_TC_FLOW_FLAG_EGRESS                = MLX5E_TC_FLAG_EGRESS_BIT,
	MLX5E_TC_FLOW_FLAG_ESWITCH               = MLX5E_TC_FLAG_ESW_OFFLOAD_BIT,
	MLX5E_TC_FLOW_FLAG_FT                    = MLX5E_TC_FLAG_FT_OFFLOAD_BIT,
	MLX5E_TC_FLOW_FLAG_NIC                   = MLX5E_TC_FLAG_NIC_OFFLOAD_BIT,
	MLX5E_TC_FLOW_FLAG_OFFLOADED             = MLX5E_TC_FLOW_BASE,
	MLX5E_TC_FLOW_FLAG_HAIRPIN               = MLX5E_TC_FLOW_BASE + 1,
	MLX5E_TC_FLOW_FLAG_HAIRPIN_RSS           = MLX5E_TC_FLOW_BASE + 2,
	MLX5E_TC_FLOW_FLAG_SLOW                  = MLX5E_TC_FLOW_BASE + 3,
	MLX5E_TC_FLOW_FLAG_DUP                   = MLX5E_TC_FLOW_BASE + 4,
	MLX5E_TC_FLOW_FLAG_NOT_READY             = MLX5E_TC_FLOW_BASE + 5,
	MLX5E_TC_FLOW_FLAG_DELETED               = MLX5E_TC_FLOW_BASE + 6,
	MLX5E_TC_FLOW_FLAG_L3_TO_L2_DECAP        = MLX5E_TC_FLOW_BASE + 7,
	MLX5E_TC_FLOW_FLAG_TUN_RX                = MLX5E_TC_FLOW_BASE + 8,
	MLX5E_TC_FLOW_FLAG_FAILED                = MLX5E_TC_FLOW_BASE + 9,
	MLX5E_TC_FLOW_FLAG_SAMPLE                = MLX5E_TC_FLOW_BASE + 10,
	MLX5E_TC_FLOW_FLAG_USE_ACT_STATS         = MLX5E_TC_FLOW_BASE + 11,
};

struct mlx5e_tc_flow_parse_attr {
	const struct ip_tunnel_info *tun_info[MLX5_MAX_FLOW_FWD_VPORTS];
	struct mlx5e_mpls_info mpls_info[MLX5_MAX_FLOW_FWD_VPORTS];
	struct net_device *filter_dev;
	struct mlx5_flow_spec spec;
	struct pedit_headers_action hdrs[__PEDIT_CMD_MAX];
	struct mlx5e_tc_mod_hdr_acts mod_hdr_acts;
	int mirred_ifindex[MLX5_MAX_FLOW_FWD_VPORTS];
	struct mlx5e_tc_act_parse_state parse_state;
};

struct mlx5_fs_chains *mlx5e_nic_chains(struct mlx5e_tc_table *tc);


struct encap_flow_item {
	struct mlx5e_encap_entry *e; 
	struct list_head list;
	int index;
};

struct encap_route_flow_item {
	struct mlx5e_route_entry *r; 
	int index;
};

struct mlx5e_tc_flow {
	struct rhash_head node;
	struct mlx5e_priv *priv;
	u64 cookie;
	unsigned long flags;
	struct mlx5_flow_handle *rule[MLX5E_TC_MAX_SPLITS + 1];

	
	struct list_head l3_to_l2_reformat;
	struct mlx5e_decap_entry *decap_reformat;

	
	struct list_head decap_routes;
	struct mlx5e_route_entry *decap_route;
	struct encap_route_flow_item encap_routes[MLX5_MAX_FLOW_FWD_VPORTS];

	
	struct encap_flow_item encaps[MLX5_MAX_FLOW_FWD_VPORTS];
	struct mlx5e_hairpin_entry *hpe; 
	struct list_head hairpin; 
	struct list_head peer[MLX5_MAX_PORTS];    
	struct list_head unready; 
	struct list_head peer_flows; 
	struct net_device *orig_dev; 
	int tmp_entry_index;
	struct list_head tmp_list; 
	refcount_t refcnt;
	struct rcu_head rcu_head;
	struct completion init_done;
	struct completion del_hw_done;
	struct mlx5_flow_attr *attr;
	struct list_head attrs;
	u32 chain_mapping;
};

struct mlx5_flow_handle *
mlx5e_tc_rule_offload(struct mlx5e_priv *priv,
		      struct mlx5_flow_spec *spec,
		      struct mlx5_flow_attr *attr);

void
mlx5e_tc_rule_unoffload(struct mlx5e_priv *priv,
			struct mlx5_flow_handle *rule,
			struct mlx5_flow_attr *attr);

u8 mlx5e_tc_get_ip_version(struct mlx5_flow_spec *spec, bool outer);

struct mlx5_flow_handle *
mlx5e_tc_offload_fdb_rules(struct mlx5_eswitch *esw,
			   struct mlx5e_tc_flow *flow,
			   struct mlx5_flow_spec *spec,
			   struct mlx5_flow_attr *attr);

struct mlx5_flow_attr *
mlx5e_tc_get_encap_attr(struct mlx5e_tc_flow *flow);

void mlx5e_tc_unoffload_flow_post_acts(struct mlx5e_tc_flow *flow);
int mlx5e_tc_offload_flow_post_acts(struct mlx5e_tc_flow *flow);

bool mlx5e_is_eswitch_flow(struct mlx5e_tc_flow *flow);
bool mlx5e_is_ft_flow(struct mlx5e_tc_flow *flow);
bool mlx5e_is_offloaded_flow(struct mlx5e_tc_flow *flow);
int mlx5e_get_flow_namespace(struct mlx5e_tc_flow *flow);
bool mlx5e_same_hw_devs(struct mlx5e_priv *priv, struct mlx5e_priv *peer_priv);

static inline void __flow_flag_set(struct mlx5e_tc_flow *flow, unsigned long flag)
{
	
	smp_mb__before_atomic();
	set_bit(flag, &flow->flags);
}

#define flow_flag_set(flow, flag) __flow_flag_set(flow, MLX5E_TC_FLOW_FLAG_##flag)

static inline bool __flow_flag_test_and_set(struct mlx5e_tc_flow *flow,
					    unsigned long flag)
{
	
	return test_and_set_bit(flag, &flow->flags);
}

#define flow_flag_test_and_set(flow, flag)			\
	__flow_flag_test_and_set(flow,				\
				 MLX5E_TC_FLOW_FLAG_##flag)

static inline void __flow_flag_clear(struct mlx5e_tc_flow *flow, unsigned long flag)
{
	
	smp_mb__before_atomic();
	clear_bit(flag, &flow->flags);
}

#define flow_flag_clear(flow, flag) __flow_flag_clear(flow,		\
						      MLX5E_TC_FLOW_FLAG_##flag)

static inline bool __flow_flag_test(struct mlx5e_tc_flow *flow, unsigned long flag)
{
	bool ret = test_bit(flag, &flow->flags);

	
	smp_mb__after_atomic();
	return ret;
}

#define flow_flag_test(flow, flag) __flow_flag_test(flow,		\
						    MLX5E_TC_FLOW_FLAG_##flag)

void mlx5e_tc_unoffload_from_slow_path(struct mlx5_eswitch *esw,
				       struct mlx5e_tc_flow *flow);
struct mlx5_flow_handle *
mlx5e_tc_offload_to_slow_path(struct mlx5_eswitch *esw,
			      struct mlx5e_tc_flow *flow,
			      struct mlx5_flow_spec *spec);

void mlx5e_tc_unoffload_fdb_rules(struct mlx5_eswitch *esw,
				  struct mlx5e_tc_flow *flow,
				  struct mlx5_flow_attr *attr);

struct mlx5e_tc_flow *mlx5e_flow_get(struct mlx5e_tc_flow *flow);
void mlx5e_flow_put(struct mlx5e_priv *priv, struct mlx5e_tc_flow *flow);

struct mlx5_fc *mlx5e_tc_get_counter(struct mlx5e_tc_flow *flow);

struct mlx5e_tc_int_port_priv *
mlx5e_get_int_port_priv(struct mlx5e_priv *priv);

struct mlx5e_flow_meters *mlx5e_get_flow_meters(struct mlx5_core_dev *dev);

void *mlx5e_get_match_headers_value(u32 flags, struct mlx5_flow_spec *spec);
void *mlx5e_get_match_headers_criteria(u32 flags, struct mlx5_flow_spec *spec);

#endif 
