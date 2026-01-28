

#ifndef __MLX5_FPGA_CORE_H__
#define __MLX5_FPGA_CORE_H__

#ifdef CONFIG_MLX5_FPGA

#include <linux/mlx5/eq.h>

#include "mlx5_core.h"
#include "lib/eq.h"
#include "fpga/cmd.h"


struct mlx5_fpga_device {
	struct mlx5_core_dev *mdev;
	struct mlx5_nb fpga_err_nb;
	struct mlx5_nb fpga_qp_err_nb;
	spinlock_t state_lock; 
	enum mlx5_fpga_status state;
	enum mlx5_fpga_image last_admin_image;
	enum mlx5_fpga_image last_oper_image;

	
	struct {
		u32 pdn;
		u32 mkey;
		struct mlx5_uars_page *uar;
	} conn_res;
};

#define mlx5_fpga_dbg(__adev, format, ...) \
	mlx5_core_dbg((__adev)->mdev, "FPGA: %s:%d:(pid %d): " format, \
		       __func__, __LINE__, current->pid, ##__VA_ARGS__)

#define mlx5_fpga_err(__adev, format, ...) \
	mlx5_core_err((__adev)->mdev, "FPGA: %s:%d:(pid %d): " format, \
		      __func__, __LINE__, current->pid, ##__VA_ARGS__)

#define mlx5_fpga_warn(__adev, format, ...) \
	mlx5_core_warn((__adev)->mdev, "FPGA: %s:%d:(pid %d): " format, \
		       __func__, __LINE__, current->pid, ##__VA_ARGS__)

#define mlx5_fpga_warn_ratelimited(__adev, format, ...) \
	mlx5_core_err_rl((__adev)->mdev, "FPGA: %s:%d: " \
			 format, __func__, __LINE__, ##__VA_ARGS__)

#define mlx5_fpga_notice(__adev, format, ...) \
	mlx5_core_info((__adev)->mdev, "FPGA: " format, ##__VA_ARGS__)

#define mlx5_fpga_info(__adev, format, ...) \
	mlx5_core_info((__adev)->mdev, "FPGA: " format, ##__VA_ARGS__)

int mlx5_fpga_init(struct mlx5_core_dev *mdev);
void mlx5_fpga_cleanup(struct mlx5_core_dev *mdev);
int mlx5_fpga_device_start(struct mlx5_core_dev *mdev);
void mlx5_fpga_device_stop(struct mlx5_core_dev *mdev);

#else

static inline int mlx5_fpga_init(struct mlx5_core_dev *mdev)
{
	return 0;
}

static inline void mlx5_fpga_cleanup(struct mlx5_core_dev *mdev)
{
}

static inline int mlx5_fpga_device_start(struct mlx5_core_dev *mdev)
{
	return 0;
}

static inline void mlx5_fpga_device_stop(struct mlx5_core_dev *mdev)
{
}

#endif

#endif 
