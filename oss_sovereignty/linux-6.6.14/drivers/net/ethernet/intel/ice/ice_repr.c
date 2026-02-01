
 

#include "ice.h"
#include "ice_eswitch.h"
#include "ice_devlink.h"
#include "ice_sriov.h"
#include "ice_tc_lib.h"
#include "ice_dcb_lib.h"

 
static int ice_repr_get_sw_port_id(struct ice_repr *repr)
{
	return repr->vf->pf->hw.port_info->lport;
}

 
static int
ice_repr_get_phys_port_name(struct net_device *netdev, char *buf, size_t len)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_repr *repr = np->repr;
	int res;

	 
	if (repr->vf->devlink_port.devlink)
		return -EOPNOTSUPP;

	res = snprintf(buf, len, "pf%dvfr%d", ice_repr_get_sw_port_id(repr),
		       repr->vf->vf_id);
	if (res <= 0)
		return -EOPNOTSUPP;
	return 0;
}

 
static void
ice_repr_get_stats64(struct net_device *netdev, struct rtnl_link_stats64 *stats)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);
	struct ice_eth_stats *eth_stats;
	struct ice_vsi *vsi;

	if (ice_is_vf_disabled(np->repr->vf))
		return;
	vsi = np->repr->src_vsi;

	ice_update_vsi_stats(vsi);
	eth_stats = &vsi->eth_stats;

	stats->tx_packets = eth_stats->tx_unicast + eth_stats->tx_broadcast +
			    eth_stats->tx_multicast;
	stats->rx_packets = eth_stats->rx_unicast + eth_stats->rx_broadcast +
			    eth_stats->rx_multicast;
	stats->tx_bytes = eth_stats->tx_bytes;
	stats->rx_bytes = eth_stats->rx_bytes;
	stats->multicast = eth_stats->rx_multicast;
	stats->tx_errors = eth_stats->tx_errors;
	stats->tx_dropped = eth_stats->tx_discards;
	stats->rx_dropped = eth_stats->rx_discards;
}

 
struct ice_repr *ice_netdev_to_repr(struct net_device *netdev)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);

	return np->repr;
}

 
static int ice_repr_open(struct net_device *netdev)
{
	struct ice_repr *repr = ice_netdev_to_repr(netdev);
	struct ice_vf *vf;

	vf = repr->vf;
	vf->link_forced = true;
	vf->link_up = true;
	ice_vc_notify_vf_link_state(vf);

	netif_carrier_on(netdev);
	netif_tx_start_all_queues(netdev);

	return 0;
}

 
static int ice_repr_stop(struct net_device *netdev)
{
	struct ice_repr *repr = ice_netdev_to_repr(netdev);
	struct ice_vf *vf;

	vf = repr->vf;
	vf->link_forced = true;
	vf->link_up = false;
	ice_vc_notify_vf_link_state(vf);

	netif_carrier_off(netdev);
	netif_tx_stop_all_queues(netdev);

	return 0;
}

 
static int
ice_repr_sp_stats64(const struct net_device *dev,
		    struct rtnl_link_stats64 *stats)
{
	struct ice_netdev_priv *np = netdev_priv(dev);
	int vf_id = np->repr->vf->vf_id;
	struct ice_tx_ring *tx_ring;
	struct ice_rx_ring *rx_ring;
	u64 pkts, bytes;

	tx_ring = np->vsi->tx_rings[vf_id];
	ice_fetch_u64_stats_per_ring(&tx_ring->ring_stats->syncp,
				     tx_ring->ring_stats->stats,
				     &pkts, &bytes);
	stats->rx_packets = pkts;
	stats->rx_bytes = bytes;

	rx_ring = np->vsi->rx_rings[vf_id];
	ice_fetch_u64_stats_per_ring(&rx_ring->ring_stats->syncp,
				     rx_ring->ring_stats->stats,
				     &pkts, &bytes);
	stats->tx_packets = pkts;
	stats->tx_bytes = bytes;
	stats->tx_dropped = rx_ring->ring_stats->rx_stats.alloc_page_failed +
			    rx_ring->ring_stats->rx_stats.alloc_buf_failed;

	return 0;
}

static bool
ice_repr_ndo_has_offload_stats(const struct net_device *dev, int attr_id)
{
	return attr_id == IFLA_OFFLOAD_XSTATS_CPU_HIT;
}

static int
ice_repr_ndo_get_offload_stats(int attr_id, const struct net_device *dev,
			       void *sp)
{
	if (attr_id == IFLA_OFFLOAD_XSTATS_CPU_HIT)
		return ice_repr_sp_stats64(dev, (struct rtnl_link_stats64 *)sp);

	return -EINVAL;
}

static int
ice_repr_setup_tc_cls_flower(struct ice_repr *repr,
			     struct flow_cls_offload *flower)
{
	switch (flower->command) {
	case FLOW_CLS_REPLACE:
		return ice_add_cls_flower(repr->netdev, repr->src_vsi, flower);
	case FLOW_CLS_DESTROY:
		return ice_del_cls_flower(repr->src_vsi, flower);
	default:
		return -EINVAL;
	}
}

static int
ice_repr_setup_tc_block_cb(enum tc_setup_type type, void *type_data,
			   void *cb_priv)
{
	struct flow_cls_offload *flower = (struct flow_cls_offload *)type_data;
	struct ice_netdev_priv *np = (struct ice_netdev_priv *)cb_priv;

	switch (type) {
	case TC_SETUP_CLSFLOWER:
		return ice_repr_setup_tc_cls_flower(np->repr, flower);
	default:
		return -EOPNOTSUPP;
	}
}

static LIST_HEAD(ice_repr_block_cb_list);

static int
ice_repr_setup_tc(struct net_device *netdev, enum tc_setup_type type,
		  void *type_data)
{
	struct ice_netdev_priv *np = netdev_priv(netdev);

	switch (type) {
	case TC_SETUP_BLOCK:
		return flow_block_cb_setup_simple((struct flow_block_offload *)
						  type_data,
						  &ice_repr_block_cb_list,
						  ice_repr_setup_tc_block_cb,
						  np, np, true);
	default:
		return -EOPNOTSUPP;
	}
}

static const struct net_device_ops ice_repr_netdev_ops = {
	.ndo_get_phys_port_name = ice_repr_get_phys_port_name,
	.ndo_get_stats64 = ice_repr_get_stats64,
	.ndo_open = ice_repr_open,
	.ndo_stop = ice_repr_stop,
	.ndo_start_xmit = ice_eswitch_port_start_xmit,
	.ndo_setup_tc = ice_repr_setup_tc,
	.ndo_has_offload_stats = ice_repr_ndo_has_offload_stats,
	.ndo_get_offload_stats = ice_repr_ndo_get_offload_stats,
};

 
bool ice_is_port_repr_netdev(const struct net_device *netdev)
{
	return netdev && (netdev->netdev_ops == &ice_repr_netdev_ops);
}

 
static int
ice_repr_reg_netdev(struct net_device *netdev)
{
	eth_hw_addr_random(netdev);
	netdev->netdev_ops = &ice_repr_netdev_ops;
	ice_set_ethtool_repr_ops(netdev);

	netdev->hw_features |= NETIF_F_HW_TC;

	netif_carrier_off(netdev);
	netif_tx_stop_all_queues(netdev);

	return register_netdev(netdev);
}

 
static int ice_repr_add(struct ice_vf *vf)
{
	struct ice_q_vector *q_vector;
	struct ice_netdev_priv *np;
	struct ice_repr *repr;
	struct ice_vsi *vsi;
	int err;

	vsi = ice_get_vf_vsi(vf);
	if (!vsi)
		return -EINVAL;

	repr = kzalloc(sizeof(*repr), GFP_KERNEL);
	if (!repr)
		return -ENOMEM;

	repr->netdev = alloc_etherdev(sizeof(struct ice_netdev_priv));
	if (!repr->netdev) {
		err =  -ENOMEM;
		goto err_alloc;
	}

	repr->src_vsi = vsi;
	repr->vf = vf;
	vf->repr = repr;
	np = netdev_priv(repr->netdev);
	np->repr = repr;

	q_vector = kzalloc(sizeof(*q_vector), GFP_KERNEL);
	if (!q_vector) {
		err = -ENOMEM;
		goto err_alloc_q_vector;
	}
	repr->q_vector = q_vector;

	err = ice_devlink_create_vf_port(vf);
	if (err)
		goto err_devlink;

	repr->netdev->min_mtu = ETH_MIN_MTU;
	repr->netdev->max_mtu = ICE_MAX_MTU;

	SET_NETDEV_DEV(repr->netdev, ice_pf_to_dev(vf->pf));
	SET_NETDEV_DEVLINK_PORT(repr->netdev, &vf->devlink_port);
	err = ice_repr_reg_netdev(repr->netdev);
	if (err)
		goto err_netdev;

	ice_virtchnl_set_repr_ops(vf);

	return 0;

err_netdev:
	ice_devlink_destroy_vf_port(vf);
err_devlink:
	kfree(repr->q_vector);
	vf->repr->q_vector = NULL;
err_alloc_q_vector:
	free_netdev(repr->netdev);
	repr->netdev = NULL;
err_alloc:
	kfree(repr);
	vf->repr = NULL;
	return err;
}

 
static void ice_repr_rem(struct ice_vf *vf)
{
	if (!vf->repr)
		return;

	kfree(vf->repr->q_vector);
	vf->repr->q_vector = NULL;
	unregister_netdev(vf->repr->netdev);
	ice_devlink_destroy_vf_port(vf);
	free_netdev(vf->repr->netdev);
	vf->repr->netdev = NULL;
	kfree(vf->repr);
	vf->repr = NULL;

	ice_virtchnl_set_dflt_ops(vf);
}

 
void ice_repr_rem_from_all_vfs(struct ice_pf *pf)
{
	struct devlink *devlink;
	struct ice_vf *vf;
	unsigned int bkt;

	lockdep_assert_held(&pf->vfs.table_lock);

	ice_for_each_vf(pf, bkt, vf)
		ice_repr_rem(vf);

	 
	devlink = priv_to_devlink(pf);
	devl_lock(devlink);
	devl_rate_nodes_destroy(devlink);
	devl_unlock(devlink);
}

 
int ice_repr_add_for_all_vfs(struct ice_pf *pf)
{
	struct devlink *devlink;
	struct ice_vf *vf;
	unsigned int bkt;
	int err;

	lockdep_assert_held(&pf->vfs.table_lock);

	ice_for_each_vf(pf, bkt, vf) {
		err = ice_repr_add(vf);
		if (err)
			goto err;
	}

	 
	if (ice_is_adq_active(pf) || ice_is_dcb_active(pf))
		return 0;

	devlink = priv_to_devlink(pf);
	ice_devlink_rate_init_tx_topology(devlink, ice_get_main_vsi(pf));

	return 0;

err:
	ice_repr_rem_from_all_vfs(pf);

	return err;
}

 
void ice_repr_start_tx_queues(struct ice_repr *repr)
{
	netif_carrier_on(repr->netdev);
	netif_tx_start_all_queues(repr->netdev);
}

 
void ice_repr_stop_tx_queues(struct ice_repr *repr)
{
	netif_carrier_off(repr->netdev);
	netif_tx_stop_all_queues(repr->netdev);
}

 
void ice_repr_set_traffic_vsi(struct ice_repr *repr, struct ice_vsi *vsi)
{
	struct ice_netdev_priv *np = netdev_priv(repr->netdev);

	np->vsi = vsi;
}
