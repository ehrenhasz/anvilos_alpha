 
 

#ifndef __MLX5_ESWITCH_ACL_LGCY_H__
#define __MLX5_ESWITCH_ACL_LGCY_H__

#include "eswitch.h"

 
int esw_acl_egress_lgcy_setup(struct mlx5_eswitch *esw, struct mlx5_vport *vport);
void esw_acl_egress_lgcy_cleanup(struct mlx5_eswitch *esw, struct mlx5_vport *vport);

 
int esw_acl_ingress_lgcy_setup(struct mlx5_eswitch *esw, struct mlx5_vport *vport);
void esw_acl_ingress_lgcy_cleanup(struct mlx5_eswitch *esw, struct mlx5_vport *vport);

#endif  
