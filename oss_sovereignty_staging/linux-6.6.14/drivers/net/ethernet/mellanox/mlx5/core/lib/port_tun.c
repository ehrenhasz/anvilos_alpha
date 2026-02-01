 
 

#include <linux/mlx5/driver.h>
#include <linux/mlx5/port.h>
#include "mlx5_core.h"
#include "lib/port_tun.h"

struct mlx5_port_tun_entropy_flags {
	bool force_supported, force_enabled;
	bool calc_supported, calc_enabled;
	bool gre_calc_supported, gre_calc_enabled;
};

static void mlx5_query_port_tun_entropy(struct mlx5_core_dev *mdev,
					struct mlx5_port_tun_entropy_flags *entropy_flags)
{
	u32 out[MLX5_ST_SZ_DW(pcmr_reg)];
	 
	entropy_flags->force_supported = false;
	entropy_flags->calc_supported = false;
	entropy_flags->gre_calc_supported = false;
	entropy_flags->force_enabled = false;
	entropy_flags->calc_enabled = true;
	entropy_flags->gre_calc_enabled = true;

	if (!MLX5_CAP_GEN(mdev, ports_check))
		return;

	if (mlx5_query_ports_check(mdev, out, sizeof(out)))
		return;

	entropy_flags->force_supported = !!(MLX5_GET(pcmr_reg, out, entropy_force_cap));
	entropy_flags->calc_supported = !!(MLX5_GET(pcmr_reg, out, entropy_calc_cap));
	entropy_flags->gre_calc_supported = !!(MLX5_GET(pcmr_reg, out, entropy_gre_calc_cap));
	entropy_flags->force_enabled = !!(MLX5_GET(pcmr_reg, out, entropy_force));
	entropy_flags->calc_enabled = !!(MLX5_GET(pcmr_reg, out, entropy_calc));
	entropy_flags->gre_calc_enabled = !!(MLX5_GET(pcmr_reg, out, entropy_gre_calc));
}

static int mlx5_set_port_tun_entropy_calc(struct mlx5_core_dev *mdev, u8 enable,
					  u8 force)
{
	u32 in[MLX5_ST_SZ_DW(pcmr_reg)] = {0};
	int err;

	err = mlx5_query_ports_check(mdev, in, sizeof(in));
	if (err)
		return err;
	MLX5_SET(pcmr_reg, in, local_port, 1);
	MLX5_SET(pcmr_reg, in, entropy_force, force);
	MLX5_SET(pcmr_reg, in, entropy_calc, enable);
	return mlx5_set_ports_check(mdev, in, sizeof(in));
}

static int mlx5_set_port_gre_tun_entropy_calc(struct mlx5_core_dev *mdev,
					      u8 enable, u8 force)
{
	u32 in[MLX5_ST_SZ_DW(pcmr_reg)] = {0};
	int err;

	err = mlx5_query_ports_check(mdev, in, sizeof(in));
	if (err)
		return err;
	MLX5_SET(pcmr_reg, in, local_port, 1);
	MLX5_SET(pcmr_reg, in, entropy_force, force);
	MLX5_SET(pcmr_reg, in, entropy_gre_calc, enable);
	return mlx5_set_ports_check(mdev, in, sizeof(in));
}

void mlx5_init_port_tun_entropy(struct mlx5_tun_entropy *tun_entropy,
				struct mlx5_core_dev *mdev)
{
	struct mlx5_port_tun_entropy_flags entropy_flags;

	tun_entropy->mdev = mdev;
	mutex_init(&tun_entropy->lock);
	mlx5_query_port_tun_entropy(mdev, &entropy_flags);
	tun_entropy->num_enabling_entries = 0;
	tun_entropy->num_disabling_entries = 0;
	tun_entropy->enabled = entropy_flags.calc_supported ?
			       entropy_flags.calc_enabled : true;
}

static int mlx5_set_entropy(struct mlx5_tun_entropy *tun_entropy,
			    int reformat_type, bool enable)
{
	struct mlx5_port_tun_entropy_flags entropy_flags;
	int err;

	mlx5_query_port_tun_entropy(tun_entropy->mdev, &entropy_flags);
	 
	if (entropy_flags.gre_calc_supported &&
	    reformat_type == MLX5_REFORMAT_TYPE_L2_TO_NVGRE) {
		if (!entropy_flags.force_supported)
			return 0;
		err = mlx5_set_port_gre_tun_entropy_calc(tun_entropy->mdev,
							 enable, !enable);
		if (err)
			return err;
	} else if (entropy_flags.calc_supported) {
		 
		if (entropy_flags.force_enabled &&
		    enable == entropy_flags.calc_enabled) {
			mlx5_core_warn(tun_entropy->mdev,
				       "Unexpected entropy calc setting - expected %d",
				       !entropy_flags.calc_enabled);
			return -EOPNOTSUPP;
		}
		 
		if (tun_entropy->num_enabling_entries)
			return -EOPNOTSUPP;
		err = mlx5_set_port_tun_entropy_calc(tun_entropy->mdev, enable,
						     entropy_flags.force_supported);
		if (err)
			return err;
		tun_entropy->enabled = enable;
		 
		if (entropy_flags.force_supported && enable) {
			err = mlx5_set_port_tun_entropy_calc(tun_entropy->mdev, 1, 0);
			if (err)
				return err;
		}
	}

	return 0;
}

 
int mlx5_tun_entropy_refcount_inc(struct mlx5_tun_entropy *tun_entropy,
				  int reformat_type)
{
	int err = -EOPNOTSUPP;

	mutex_lock(&tun_entropy->lock);
	if ((reformat_type == MLX5_REFORMAT_TYPE_L2_TO_VXLAN ||
	     reformat_type == MLX5_REFORMAT_TYPE_L2_TO_L3_TUNNEL) &&
	    tun_entropy->enabled) {
		 
		tun_entropy->num_enabling_entries++;
		err = 0;
	} else if (reformat_type == MLX5_REFORMAT_TYPE_L2_TO_NVGRE) {
		 
		if (tun_entropy->num_disabling_entries == 0)
			err = mlx5_set_entropy(tun_entropy, reformat_type, 0);
		else
			err = 0;
		if (!err)
			tun_entropy->num_disabling_entries++;
	}
	mutex_unlock(&tun_entropy->lock);

	return err;
}

void mlx5_tun_entropy_refcount_dec(struct mlx5_tun_entropy *tun_entropy,
				   int reformat_type)
{
	mutex_lock(&tun_entropy->lock);
	if (reformat_type == MLX5_REFORMAT_TYPE_L2_TO_VXLAN)
		tun_entropy->num_enabling_entries--;
	else if (reformat_type == MLX5_REFORMAT_TYPE_L2_TO_NVGRE &&
		 --tun_entropy->num_disabling_entries == 0)
		mlx5_set_entropy(tun_entropy, reformat_type, 1);
	mutex_unlock(&tun_entropy->lock);
}

