 

#include <linux/kernel.h>
#include <linux/mlx5/driver.h>
#include "mlx5_core.h"

int mlx5_core_alloc_pd(struct mlx5_core_dev *dev, u32 *pdn)
{
	u32 out[MLX5_ST_SZ_DW(alloc_pd_out)] = {};
	u32 in[MLX5_ST_SZ_DW(alloc_pd_in)] = {};
	int err;

	MLX5_SET(alloc_pd_in, in, opcode, MLX5_CMD_OP_ALLOC_PD);
	err = mlx5_cmd_exec_inout(dev, alloc_pd, in, out);
	if (!err)
		*pdn = MLX5_GET(alloc_pd_out, out, pd);
	return err;
}
EXPORT_SYMBOL(mlx5_core_alloc_pd);

int mlx5_core_dealloc_pd(struct mlx5_core_dev *dev, u32 pdn)
{
	u32 in[MLX5_ST_SZ_DW(dealloc_pd_in)] = {};

	MLX5_SET(dealloc_pd_in, in, opcode, MLX5_CMD_OP_DEALLOC_PD);
	MLX5_SET(dealloc_pd_in, in, pd, pdn);
	return mlx5_cmd_exec_in(dev, dealloc_pd, in);
}
EXPORT_SYMBOL(mlx5_core_dealloc_pd);
