 

#ifndef __MLX5E_IPOB_H__
#define __MLX5E_IPOB_H__

#ifdef CONFIG_MLX5_CORE_IPOIB

#include <linux/mlx5/fs.h>
#include "en.h"

#define MLX5I_MAX_NUM_TC 1

extern const struct ethtool_ops mlx5i_ethtool_ops;
extern const struct ethtool_ops mlx5i_pkey_ethtool_ops;
extern const struct mlx5e_rx_handlers mlx5i_rx_handlers;

#define MLX5_IB_GRH_BYTES       40
#define MLX5_IPOIB_ENCAP_LEN    4
#define MLX5_IPOIB_PSEUDO_LEN   20
#define MLX5_IPOIB_HARD_LEN     (MLX5_IPOIB_PSEUDO_LEN + MLX5_IPOIB_ENCAP_LEN)

 
struct mlx5i_priv {
	struct rdma_netdev rn;  
	u32 qpn;
	bool   sub_interface;
	u32    num_sub_interfaces;
	u32    qkey;
	u16    pkey_index;
	struct mlx5i_pkey_qpn_ht *qpn_htbl;
	struct net_device *parent_dev;
	char  *mlx5e_priv[];
};

int mlx5i_create_tis(struct mlx5_core_dev *mdev, u32 underlay_qpn, u32 *tisn);

 
int mlx5i_create_underlay_qp(struct mlx5e_priv *priv);
void mlx5i_destroy_underlay_qp(struct mlx5_core_dev *mdev, u32 qpn);

 
int mlx5i_init_underlay_qp(struct mlx5e_priv *priv);
void mlx5i_uninit_underlay_qp(struct mlx5e_priv *priv);

 
int mlx5i_pkey_qpn_ht_init(struct net_device *netdev);
void mlx5i_pkey_qpn_ht_cleanup(struct net_device *netdev);

 
int mlx5i_pkey_add_qpn(struct net_device *netdev, u32 qpn);
int mlx5i_pkey_del_qpn(struct net_device *netdev, u32 qpn);

 
struct net_device *mlx5i_pkey_get_netdev(struct net_device *netdev, u32 qpn);

 
int mlx5i_dev_init(struct net_device *dev);
void mlx5i_dev_cleanup(struct net_device *dev);
int mlx5i_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);

 
int mlx5i_init(struct mlx5_core_dev *mdev, struct net_device *netdev);
void mlx5i_cleanup(struct mlx5e_priv *priv);

int mlx5i_update_nic_rx(struct mlx5e_priv *priv);

 
const struct mlx5e_profile *mlx5i_pkey_get_profile(void);

 
#define mlx5i_epriv(netdev) ((void *)(((struct mlx5i_priv *)netdev_priv(netdev))->mlx5e_priv))

struct mlx5_wqe_eth_pad {
	u8 rsvd0[16];
};

struct mlx5i_tx_wqe {
	struct mlx5_wqe_ctrl_seg     ctrl;
	struct mlx5_wqe_datagram_seg datagram;
	struct mlx5_wqe_eth_pad      pad;
	struct mlx5_wqe_eth_seg      eth;
	struct mlx5_wqe_data_seg     data[];
};

#define MLX5I_SQ_FETCH_WQE(sq, pi) \
	((struct mlx5i_tx_wqe *)mlx5e_fetch_wqe(&(sq)->wq, pi, sizeof(struct mlx5i_tx_wqe)))

void mlx5i_sq_xmit(struct mlx5e_txqsq *sq, struct sk_buff *skb,
		   struct mlx5_av *av, u32 dqpn, u32 dqkey, bool xmit_more);
void mlx5i_get_stats(struct net_device *dev, struct rtnl_link_stats64 *stats);

 
struct net_device *mlx5i_parent_get(struct net_device *netdev);
void mlx5i_parent_put(struct net_device *netdev);

#endif  
#endif  
