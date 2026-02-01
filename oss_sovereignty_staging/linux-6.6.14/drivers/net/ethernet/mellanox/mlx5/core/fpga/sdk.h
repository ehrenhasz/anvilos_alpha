 

#ifndef MLX5_FPGA_SDK_H
#define MLX5_FPGA_SDK_H

#include <linux/types.h>
#include <linux/dma-direction.h>

 
#define SBU_QP_QUEUE_SIZE 8
#define MLX5_FPGA_CMD_TIMEOUT_MSEC (60 * 1000)

 
enum mlx5_fpga_access_type {
	MLX5_FPGA_ACCESS_TYPE_I2C = 0x0,
	MLX5_FPGA_ACCESS_TYPE_DONTCARE = 0x0,
};

struct mlx5_fpga_conn;
struct mlx5_fpga_device;

 
struct mlx5_fpga_dma_entry {
	 
	void *data;
	 
	unsigned int size;
	 
	dma_addr_t dma_addr;
};

 
struct mlx5_fpga_dma_buf {
	 
	enum dma_data_direction dma_dir;
	 
	struct mlx5_fpga_dma_entry sg[2];
	 
	struct list_head list;
	 
	void (*complete)(struct mlx5_fpga_conn *conn,
			 struct mlx5_fpga_device *fdev,
			 struct mlx5_fpga_dma_buf *buf, u8 status);
};

 
struct mlx5_fpga_conn_attr {
	 
	unsigned int tx_size;
	 
	unsigned int rx_size;
	 
	void (*recv_cb)(void *cb_arg, struct mlx5_fpga_dma_buf *buf);
	 
	void *cb_arg;
};

 
struct mlx5_fpga_conn *
mlx5_fpga_sbu_conn_create(struct mlx5_fpga_device *fdev,
			  struct mlx5_fpga_conn_attr *attr);

 
void mlx5_fpga_sbu_conn_destroy(struct mlx5_fpga_conn *conn);

 
int mlx5_fpga_sbu_conn_sendmsg(struct mlx5_fpga_conn *conn,
			       struct mlx5_fpga_dma_buf *buf);

 
int mlx5_fpga_mem_read(struct mlx5_fpga_device *fdev, size_t size, u64 addr,
		       void *buf, enum mlx5_fpga_access_type access_type);

 
int mlx5_fpga_mem_write(struct mlx5_fpga_device *fdev, size_t size, u64 addr,
			void *buf, enum mlx5_fpga_access_type access_type);

 
int mlx5_fpga_get_sbu_caps(struct mlx5_fpga_device *fdev, int size, void *buf);

#endif  
