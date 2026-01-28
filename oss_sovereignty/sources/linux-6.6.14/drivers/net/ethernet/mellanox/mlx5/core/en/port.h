

#ifndef __MLX5E_EN_PORT_H
#define __MLX5E_EN_PORT_H

#include <linux/mlx5/driver.h>
#include "en.h"

void mlx5_port_query_eth_autoneg(struct mlx5_core_dev *dev, u8 *an_status,
				 u8 *an_disable_cap, u8 *an_disable_admin);
int mlx5_port_set_eth_ptys(struct mlx5_core_dev *dev, bool an_disable,
			   u32 proto_admin, bool ext);
int mlx5e_port_linkspeed(struct mlx5_core_dev *mdev, u32 *speed);
int mlx5e_port_query_pbmc(struct mlx5_core_dev *mdev, void *out);
int mlx5e_port_set_pbmc(struct mlx5_core_dev *mdev, void *in);
int mlx5e_port_query_sbpr(struct mlx5_core_dev *mdev, u32 desc, u8 dir,
			  u8 pool_idx, void *out, int size_out);
int mlx5e_port_set_sbpr(struct mlx5_core_dev *mdev, u32 desc, u8 dir,
			u8 pool_idx, u32 infi_size, u32 size);
int mlx5e_port_set_sbcm(struct mlx5_core_dev *mdev, u32 desc, u8 pg_buff_idx,
			u8 dir, u8 infi_size, u32 max_buff, u8 pool_idx);
int mlx5e_port_query_priority2buffer(struct mlx5_core_dev *mdev, u8 *buffer);
int mlx5e_port_set_priority2buffer(struct mlx5_core_dev *mdev, u8 *buffer);

bool mlx5e_fec_in_caps(struct mlx5_core_dev *dev, int fec_policy);
int mlx5e_get_fec_mode(struct mlx5_core_dev *dev, u32 *fec_mode_active,
		       u16 *fec_configured_mode);
int mlx5e_set_fec_mode(struct mlx5_core_dev *dev, u16 fec_policy);

enum {
	MLX5E_FEC_NOFEC,
	MLX5E_FEC_FIRECODE,
	MLX5E_FEC_RS_528_514,
	MLX5E_FEC_RS_544_514 = 7,
	MLX5E_FEC_LLRS_272_257_1 = 9,
};

#endif
