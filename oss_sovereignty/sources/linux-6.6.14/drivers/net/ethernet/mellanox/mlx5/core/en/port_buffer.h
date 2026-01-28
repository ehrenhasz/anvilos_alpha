
#ifndef __MLX5_EN_PORT_BUFFER_H__
#define __MLX5_EN_PORT_BUFFER_H__

#include "en.h"
#include "port.h"

#define MLX5E_MAX_NETWORK_BUFFER 8
#define MLX5E_TOTAL_BUFFERS 10
#define MLX5E_DEFAULT_CABLE_LEN 7 

#define MLX5_BUFFER_SUPPORTED(mdev) (MLX5_CAP_GEN(mdev, pcam_reg) && \
				     MLX5_CAP_PCAM_REG(mdev, pbmc) && \
				     MLX5_CAP_PCAM_REG(mdev, pptb))

enum {
	MLX5E_PORT_BUFFER_CABLE_LEN   = BIT(0),
	MLX5E_PORT_BUFFER_PFC         = BIT(1),
	MLX5E_PORT_BUFFER_PRIO2BUFFER = BIT(2),
	MLX5E_PORT_BUFFER_SIZE        = BIT(3),
};

struct mlx5e_bufferx_reg {
	u8   lossy;
	u8   epsb;
	u32  size;
	u32  xoff;
	u32  xon;
};

struct mlx5e_port_buffer {
	u32                       port_buffer_size;
	u32                       spare_buffer_size;
	u32                       headroom_size;	  
	u32                       internal_buffers_size;  
	struct mlx5e_bufferx_reg  buffer[MLX5E_MAX_NETWORK_BUFFER];
};

int mlx5e_port_manual_buffer_config(struct mlx5e_priv *priv,
				    u32 change, unsigned int mtu,
				    struct ieee_pfc *pfc,
				    u32 *buffer_size,
				    u8 *prio2buffer);

int mlx5e_port_query_buffer(struct mlx5e_priv *priv,
			    struct mlx5e_port_buffer *port_buffer);
#endif
