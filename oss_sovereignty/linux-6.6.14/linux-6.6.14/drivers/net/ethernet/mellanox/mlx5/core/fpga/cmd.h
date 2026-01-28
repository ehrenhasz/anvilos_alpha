#ifndef __MLX5_FPGA_H__
#define __MLX5_FPGA_H__
#include <linux/mlx5/driver.h>
enum mlx5_fpga_id {
	MLX5_FPGA_NEWTON = 0,
	MLX5_FPGA_EDISON = 1,
	MLX5_FPGA_MORSE = 2,
	MLX5_FPGA_MORSEQ = 3,
};
enum mlx5_fpga_image {
	MLX5_FPGA_IMAGE_USER = 0,
	MLX5_FPGA_IMAGE_FACTORY,
};
enum mlx5_fpga_status {
	MLX5_FPGA_STATUS_SUCCESS = 0,
	MLX5_FPGA_STATUS_FAILURE = 1,
	MLX5_FPGA_STATUS_IN_PROGRESS = 2,
	MLX5_FPGA_STATUS_NONE = 0xFFFF,
};
struct mlx5_fpga_query {
	enum mlx5_fpga_image admin_image;
	enum mlx5_fpga_image oper_image;
	enum mlx5_fpga_status status;
};
enum mlx5_fpga_qpc_field_select {
	MLX5_FPGA_QPC_STATE = BIT(0),
};
struct mlx5_fpga_qp_counters {
	u64 rx_ack_packets;
	u64 rx_send_packets;
	u64 tx_ack_packets;
	u64 tx_send_packets;
	u64 rx_total_drop;
};
int mlx5_fpga_caps(struct mlx5_core_dev *dev);
int mlx5_fpga_query(struct mlx5_core_dev *dev, struct mlx5_fpga_query *query);
int mlx5_fpga_ctrl_op(struct mlx5_core_dev *dev, u8 op);
int mlx5_fpga_access_reg(struct mlx5_core_dev *dev, u8 size, u64 addr,
			 void *buf, bool write);
int mlx5_fpga_sbu_caps(struct mlx5_core_dev *dev, void *caps, int size);
int mlx5_fpga_create_qp(struct mlx5_core_dev *dev, void *fpga_qpc,
			u32 *fpga_qpn);
int mlx5_fpga_modify_qp(struct mlx5_core_dev *dev, u32 fpga_qpn,
			enum mlx5_fpga_qpc_field_select fields, void *fpga_qpc);
int mlx5_fpga_query_qp(struct mlx5_core_dev *dev, u32 fpga_qpn, void *fpga_qpc);
int mlx5_fpga_query_qp_counters(struct mlx5_core_dev *dev, u32 fpga_qpn,
				bool clear, struct mlx5_fpga_qp_counters *data);
int mlx5_fpga_destroy_qp(struct mlx5_core_dev *dev, u32 fpga_qpn);
#endif  
