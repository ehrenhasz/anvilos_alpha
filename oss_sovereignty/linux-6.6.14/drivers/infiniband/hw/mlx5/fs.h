 
 

#ifndef _MLX5_IB_FS_H
#define _MLX5_IB_FS_H

#include "mlx5_ib.h"

#if IS_ENABLED(CONFIG_INFINIBAND_USER_ACCESS)
int mlx5_ib_fs_init(struct mlx5_ib_dev *dev);
void mlx5_ib_fs_cleanup_anchor(struct mlx5_ib_dev *dev);
#else
static inline int mlx5_ib_fs_init(struct mlx5_ib_dev *dev)
{
	dev->flow_db = kzalloc(sizeof(*dev->flow_db), GFP_KERNEL);

	if (!dev->flow_db)
		return -ENOMEM;

	mutex_init(&dev->flow_db->lock);
	return 0;
}

inline void mlx5_ib_fs_cleanup_anchor(struct mlx5_ib_dev *dev) {}
#endif

static inline void mlx5_ib_fs_cleanup(struct mlx5_ib_dev *dev)
{
	 
	mlx5_ib_fs_cleanup_anchor(dev);
	kfree(dev->flow_db);
}
#endif  
