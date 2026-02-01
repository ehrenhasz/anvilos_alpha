 
#ifndef __MLX5_VXLAN_H__
#define __MLX5_VXLAN_H__

#include <linux/mlx5/driver.h>

struct mlx5_vxlan;
struct mlx5_vxlan_port;

static inline u8 mlx5_vxlan_max_udp_ports(struct mlx5_core_dev *mdev)
{
	return MLX5_CAP_ETH(mdev, max_vxlan_udp_ports) ?: 4;
}

static inline bool mlx5_vxlan_allowed(struct mlx5_vxlan *vxlan)
{
	 
	return !IS_ERR_OR_NULL(vxlan);
}

#if IS_ENABLED(CONFIG_VXLAN)
struct mlx5_vxlan *mlx5_vxlan_create(struct mlx5_core_dev *mdev);
void mlx5_vxlan_destroy(struct mlx5_vxlan *vxlan);
int mlx5_vxlan_add_port(struct mlx5_vxlan *vxlan, u16 port);
int mlx5_vxlan_del_port(struct mlx5_vxlan *vxlan, u16 port);
bool mlx5_vxlan_lookup_port(struct mlx5_vxlan *vxlan, u16 port);
void mlx5_vxlan_reset_to_default(struct mlx5_vxlan *vxlan);
#else
static inline struct mlx5_vxlan*
mlx5_vxlan_create(struct mlx5_core_dev *mdev) { return ERR_PTR(-EOPNOTSUPP); }
static inline void mlx5_vxlan_destroy(struct mlx5_vxlan *vxlan) { return; }
static inline int mlx5_vxlan_add_port(struct mlx5_vxlan *vxlan, u16 port) { return -EOPNOTSUPP; }
static inline int mlx5_vxlan_del_port(struct mlx5_vxlan *vxlan, u16 port) { return -EOPNOTSUPP; }
static inline bool mlx5_vxlan_lookup_port(struct mlx5_vxlan *vxlan, u16 port) { return false; }
static inline void mlx5_vxlan_reset_to_default(struct mlx5_vxlan *vxlan) { return; }
#endif

#endif  
