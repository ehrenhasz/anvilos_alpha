 
 

#ifndef NFP_NET_REPR_H
#define NFP_NET_REPR_H

struct metadata_dst;
struct nfp_app;
struct nfp_net;
struct nfp_port;

#include <net/dst_metadata.h>

 
struct nfp_reprs {
	unsigned int num_reprs;
	struct net_device __rcu *reprs[];
};

 
struct nfp_repr_pcpu_stats {
	u64 rx_packets;
	u64 rx_bytes;
	u64 tx_packets;
	u64 tx_bytes;
	u64 tx_drops;
	struct u64_stats_sync syncp;
};

 
struct nfp_repr {
	struct net_device *netdev;
	struct metadata_dst *dst;
	struct nfp_port *port;
	struct nfp_app *app;
	struct nfp_repr_pcpu_stats __percpu *stats;
	void *app_priv;
};

 
enum nfp_repr_type {
	NFP_REPR_TYPE_PHYS_PORT,
	NFP_REPR_TYPE_PF,
	NFP_REPR_TYPE_VF,

	__NFP_REPR_TYPE_MAX,
};
#define NFP_REPR_TYPE_MAX (__NFP_REPR_TYPE_MAX - 1)

extern const struct net_device_ops nfp_repr_netdev_ops;

static inline bool nfp_netdev_is_nfp_repr(struct net_device *netdev)
{
	return netdev->netdev_ops == &nfp_repr_netdev_ops;
}

static inline int nfp_repr_get_port_id(struct net_device *netdev)
{
	struct nfp_repr *priv = netdev_priv(netdev);

	return priv->dst->u.port_info.port_id;
}

struct net_device *
nfp_repr_get_locked(struct nfp_app *app, struct nfp_reprs *set,
		    unsigned int id);

void nfp_repr_inc_rx_stats(struct net_device *netdev, unsigned int len);
void
nfp_repr_transfer_features(struct net_device *netdev, struct net_device *lower);
int nfp_repr_init(struct nfp_app *app, struct net_device *netdev,
		  u32 cmsg_port_id, struct nfp_port *port,
		  struct net_device *pf_netdev);
void nfp_repr_free(struct net_device *netdev);
struct net_device *
nfp_repr_alloc_mqs(struct nfp_app *app, unsigned int txqs, unsigned int rxqs);
void nfp_repr_clean_and_free(struct nfp_repr *repr);
void nfp_reprs_clean_and_free(struct nfp_app *app, struct nfp_reprs *reprs);
void nfp_reprs_clean_and_free_by_type(struct nfp_app *app,
				      enum nfp_repr_type type);
struct nfp_reprs *nfp_reprs_alloc(unsigned int num_reprs);
int nfp_reprs_resync_phys_ports(struct nfp_app *app);

static inline struct net_device *nfp_repr_alloc(struct nfp_app *app)
{
	return nfp_repr_alloc_mqs(app, 1, 1);
}
#endif  
