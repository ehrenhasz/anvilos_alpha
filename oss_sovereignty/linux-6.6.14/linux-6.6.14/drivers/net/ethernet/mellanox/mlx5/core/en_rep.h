#ifndef __MLX5E_REP_H__
#define __MLX5E_REP_H__
#include <net/ip_tunnels.h>
#include <linux/rhashtable.h>
#include <linux/mutex.h>
#include "eswitch.h"
#include "en.h"
#include "lib/port_tun.h"
#ifdef CONFIG_MLX5_ESWITCH
extern const struct mlx5e_rx_handlers mlx5e_rx_handlers_rep;
struct mlx5e_neigh_update_table {
	struct rhashtable       neigh_ht;
	struct list_head	neigh_list;
	struct mutex		encap_lock;
	struct notifier_block   netevent_nb;
	struct delayed_work     neigh_stats_work;
	unsigned long           min_interval;  
};
struct mlx5_tc_ct_priv;
struct mlx5_tc_int_port_priv;
struct mlx5e_rep_bond;
struct mlx5e_tc_tun_encap;
struct mlx5e_post_act;
struct mlx5e_flow_meters;
struct mlx5_rep_uplink_priv {
	struct list_head	    tc_indr_block_priv_list;
	struct mlx5_tun_entropy tun_entropy;
	struct mutex                unready_flows_lock;
	struct list_head            unready_flows;
	struct work_struct          reoffload_flows_work;
	struct mapping_ctx *tunnel_mapping;
	struct mapping_ctx *tunnel_enc_opts_mapping;
	struct mlx5e_post_act *post_act;
	struct mlx5_tc_ct_priv *ct_priv;
	struct mlx5e_tc_psample *tc_psample;
	struct mlx5e_rep_bond *bond;
	struct mlx5e_tc_tun_encap *encap;
	struct mlx5e_tc_int_port_priv *int_port_priv;
	struct mlx5e_flow_meters *flow_meters;
	struct mlx5e_tc_act_stats_handle *action_stats_handle;
	struct work_struct mpesw_work;
};
struct mlx5e_rep_priv {
	struct mlx5_eswitch_rep *rep;
	struct mlx5e_neigh_update_table neigh_update;
	struct net_device      *netdev;
	struct mlx5_flow_table *root_ft;
	struct mlx5_flow_handle *vport_rx_rule;
	struct list_head       vport_sqs_list;
	struct mlx5_rep_uplink_priv uplink_priv;  
	struct rtnl_link_stats64 prev_vf_vport_stats;
	struct mlx5_flow_handle *send_to_vport_meta_rule;
	struct rhashtable tc_ht;
	struct devlink_health_reporter *rep_vnic_reporter;
};
static inline
struct mlx5e_rep_priv *mlx5e_rep_to_rep_priv(struct mlx5_eswitch_rep *rep)
{
	return rep->rep_data[REP_ETH].priv;
}
struct mlx5e_neigh {
	union {
		__be32	v4;
		struct in6_addr v6;
	} dst_ip;
	int family;
};
struct mlx5e_neigh_hash_entry {
	struct rhash_head rhash_node;
	struct mlx5e_neigh m_neigh;
	struct mlx5e_priv *priv;
	struct net_device *neigh_dev;
	struct list_head neigh_list;
	spinlock_t encap_list_lock;
	struct list_head encap_list;
	refcount_t refcnt;
	unsigned long reported_lastuse;
	struct rcu_head rcu;
};
enum {
	MLX5_ENCAP_ENTRY_VALID     = BIT(0),
	MLX5_REFORMAT_DECAP        = BIT(1),
	MLX5_ENCAP_ENTRY_NO_ROUTE  = BIT(2),
};
struct mlx5e_decap_key {
	struct ethhdr key;
};
struct mlx5e_decap_entry {
	struct mlx5e_decap_key key;
	struct list_head flows;
	struct hlist_node hlist;
	refcount_t refcnt;
	struct completion res_ready;
	int compl_result;
	struct mlx5_pkt_reformat *pkt_reformat;
	struct rcu_head rcu;
};
struct mlx5e_mpls_info {
	u32             label;
	u8              tc;
	u8              bos;
	u8              ttl;
};
struct mlx5e_encap_entry {
	struct mlx5e_neigh_hash_entry *nhe;
	struct list_head encap_list;
	struct hlist_node encap_hlist;
	struct list_head flows;
	struct list_head route_list;
	struct mlx5_pkt_reformat *pkt_reformat;
	const struct ip_tunnel_info *tun_info;
	struct mlx5e_mpls_info mpls_info;
	unsigned char h_dest[ETH_ALEN];	 
	struct net_device *out_dev;
	int route_dev_ifindex;
	struct mlx5e_tc_tunnel *tunnel;
	int reformat_type;
	u8 flags;
	char *encap_header;
	int encap_size;
	refcount_t refcnt;
	struct completion res_ready;
	int compl_result;
	struct rcu_head rcu;
};
struct mlx5e_rep_sq_peer {
	struct mlx5_flow_handle *rule;
	void *peer;
};
struct mlx5e_rep_sq {
	struct mlx5_flow_handle	*send_to_vport_rule;
	struct xarray sq_peer;
	u32 sqn;
	struct list_head	 list;
};
int mlx5e_rep_init(void);
void mlx5e_rep_cleanup(void);
int mlx5e_rep_bond_init(struct mlx5e_rep_priv *rpriv);
void mlx5e_rep_bond_cleanup(struct mlx5e_rep_priv *rpriv);
int mlx5e_rep_bond_enslave(struct mlx5_eswitch *esw, struct net_device *netdev,
			   struct net_device *lag_dev);
void mlx5e_rep_bond_unslave(struct mlx5_eswitch *esw,
			    const struct net_device *netdev,
			    const struct net_device *lag_dev);
int mlx5e_rep_bond_update(struct mlx5e_priv *priv, bool cleanup);
bool mlx5e_rep_has_offload_stats(const struct net_device *dev, int attr_id);
int mlx5e_rep_get_offload_stats(int attr_id, const struct net_device *dev,
				void *sp);
bool mlx5e_is_uplink_rep(struct mlx5e_priv *priv);
void mlx5e_rep_activate_channels(struct mlx5e_priv *priv);
void mlx5e_rep_deactivate_channels(struct mlx5e_priv *priv);
void mlx5e_rep_queue_neigh_stats_work(struct mlx5e_priv *priv);
bool mlx5e_eswitch_vf_rep(const struct net_device *netdev);
bool mlx5e_eswitch_uplink_rep(const struct net_device *netdev);
static inline bool mlx5e_eswitch_rep(const struct net_device *netdev)
{
	return mlx5e_eswitch_vf_rep(netdev) ||
	       mlx5e_eswitch_uplink_rep(netdev);
}
#else  
static inline bool mlx5e_is_uplink_rep(struct mlx5e_priv *priv) { return false; }
static inline void mlx5e_rep_activate_channels(struct mlx5e_priv *priv) {}
static inline void mlx5e_rep_deactivate_channels(struct mlx5e_priv *priv) {}
static inline int mlx5e_rep_init(void) { return 0; };
static inline void mlx5e_rep_cleanup(void) {};
static inline bool mlx5e_rep_has_offload_stats(const struct net_device *dev,
					       int attr_id) { return false; }
static inline int mlx5e_rep_get_offload_stats(int attr_id,
					      const struct net_device *dev,
					      void *sp) { return -EOPNOTSUPP; }
#endif
static inline bool mlx5e_is_vport_rep(struct mlx5e_priv *priv)
{
	return (MLX5_ESWITCH_MANAGER(priv->mdev) && priv->ppriv);
}
#endif  
