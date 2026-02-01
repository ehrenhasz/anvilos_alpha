 

#ifndef MLX5_IB_CMD_H
#define MLX5_IB_CMD_H

#include "mlx5_ib.h"
#include <linux/kernel.h>
#include <linux/mlx5/driver.h>

int mlx5r_cmd_query_special_mkeys(struct mlx5_ib_dev *dev);
int mlx5_cmd_query_cong_params(struct mlx5_core_dev *dev, int cong_point,
			       void *out);
int mlx5_cmd_dealloc_pd(struct mlx5_core_dev *dev, u32 pdn, u16 uid);
void mlx5_cmd_destroy_tir(struct mlx5_core_dev *dev, u32 tirn, u16 uid);
void mlx5_cmd_destroy_tis(struct mlx5_core_dev *dev, u32 tisn, u16 uid);
int mlx5_cmd_destroy_rqt(struct mlx5_core_dev *dev, u32 rqtn, u16 uid);
int mlx5_cmd_alloc_transport_domain(struct mlx5_core_dev *dev, u32 *tdn,
				    u16 uid);
void mlx5_cmd_dealloc_transport_domain(struct mlx5_core_dev *dev, u32 tdn,
				       u16 uid);
int mlx5_cmd_attach_mcg(struct mlx5_core_dev *dev, union ib_gid *mgid,
			u32 qpn, u16 uid);
int mlx5_cmd_detach_mcg(struct mlx5_core_dev *dev, union ib_gid *mgid,
			u32 qpn, u16 uid);
int mlx5_cmd_xrcd_alloc(struct mlx5_core_dev *dev, u32 *xrcdn, u16 uid);
int mlx5_cmd_xrcd_dealloc(struct mlx5_core_dev *dev, u32 xrcdn, u16 uid);
int mlx5_cmd_mad_ifc(struct mlx5_core_dev *dev, const void *inb, void *outb,
		     u16 opmod, u8 port);
int mlx5_cmd_uar_alloc(struct mlx5_core_dev *dev, u32 *uarn, u16 uid);
int mlx5_cmd_uar_dealloc(struct mlx5_core_dev *dev, u32 uarn, u16 uid);
#endif  
