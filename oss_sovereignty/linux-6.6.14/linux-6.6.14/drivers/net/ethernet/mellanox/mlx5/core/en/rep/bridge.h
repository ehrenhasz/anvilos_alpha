#ifndef __MLX5_EN_REP_BRIDGE__
#define __MLX5_EN_REP_BRIDGE__
#include "en.h"
#if IS_ENABLED(CONFIG_MLX5_BRIDGE)
void mlx5e_rep_bridge_init(struct mlx5e_priv *priv);
void mlx5e_rep_bridge_cleanup(struct mlx5e_priv *priv);
#else  
static inline void mlx5e_rep_bridge_init(struct mlx5e_priv *priv) {}
static inline void mlx5e_rep_bridge_cleanup(struct mlx5e_priv *priv) {}
#endif  
#endif  
