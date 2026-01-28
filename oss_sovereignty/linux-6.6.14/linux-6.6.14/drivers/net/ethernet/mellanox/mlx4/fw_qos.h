#ifndef MLX4_FW_QOS_H
#define MLX4_FW_QOS_H
#include <linux/mlx4/cmd.h>
#include <linux/mlx4/device.h>
#define MLX4_NUM_UP 8
#define MLX4_NUM_TC 8
#define MLX4_DEFAULT_QOS_PRIO (0)
#define MLX4_VPP_DEFAULT_VPORT (0)
struct mlx4_vport_qos_param {
	u32 bw_share;
	u32 max_avg_bw;
	u8 enable;
};
int mlx4_SET_PORT_PRIO2TC(struct mlx4_dev *dev, u8 port, u8 *prio2tc);
int mlx4_SET_PORT_SCHEDULER(struct mlx4_dev *dev, u8 port, u8 *tc_tx_bw,
			    u8 *pg, u16 *ratelimit);
int mlx4_ALLOCATE_VPP_get(struct mlx4_dev *dev, u8 port,
			  u16 *available_vpp, u8 *vpp_p_up);
int mlx4_ALLOCATE_VPP_set(struct mlx4_dev *dev, u8 port, u8 *vpp_p_up);
int mlx4_SET_VPORT_QOS_get(struct mlx4_dev *dev, u8 port, u8 vport,
			   struct mlx4_vport_qos_param *out_param);
int mlx4_SET_VPORT_QOS_set(struct mlx4_dev *dev, u8 port, u8 vport,
			   struct mlx4_vport_qos_param *in_param);
#endif  
