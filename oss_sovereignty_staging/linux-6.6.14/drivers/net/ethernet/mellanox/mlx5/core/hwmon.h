 
#ifndef __MLX5_HWMON_H__
#define __MLX5_HWMON_H__

#include <linux/mlx5/driver.h>

#if IS_ENABLED(CONFIG_HWMON)

int mlx5_hwmon_dev_register(struct mlx5_core_dev *mdev);
void mlx5_hwmon_dev_unregister(struct mlx5_core_dev *mdev);

#else
static inline int mlx5_hwmon_dev_register(struct mlx5_core_dev *mdev)
{
	return 0;
}

static inline void mlx5_hwmon_dev_unregister(struct mlx5_core_dev *mdev) {}

#endif

#endif  
