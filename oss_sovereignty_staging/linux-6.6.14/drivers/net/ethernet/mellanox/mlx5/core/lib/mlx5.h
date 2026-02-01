 

#ifndef __LIB_MLX5_H__
#define __LIB_MLX5_H__

#include "mlx5_core.h"

void mlx5_init_reserved_gids(struct mlx5_core_dev *dev);
void mlx5_cleanup_reserved_gids(struct mlx5_core_dev *dev);
int  mlx5_core_reserve_gids(struct mlx5_core_dev *dev, unsigned int count);
void mlx5_core_unreserve_gids(struct mlx5_core_dev *dev, unsigned int count);
int  mlx5_core_reserved_gid_alloc(struct mlx5_core_dev *dev, int *gid_index);
void mlx5_core_reserved_gid_free(struct mlx5_core_dev *dev, int gid_index);
int mlx5_crdump_enable(struct mlx5_core_dev *dev);
void mlx5_crdump_disable(struct mlx5_core_dev *dev);
int mlx5_crdump_collect(struct mlx5_core_dev *dev, u32 *cr_data);

static inline struct net *mlx5_core_net(struct mlx5_core_dev *dev)
{
	return devlink_net(priv_to_devlink(dev));
}

static inline struct net_device *mlx5_uplink_netdev_get(struct mlx5_core_dev *mdev)
{
	return mdev->mlx5e_res.uplink_netdev;
}
#endif
