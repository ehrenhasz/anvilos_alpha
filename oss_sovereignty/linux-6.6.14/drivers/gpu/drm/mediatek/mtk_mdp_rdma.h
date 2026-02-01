 
 

#ifndef __MTK_MDP_RDMA_H__
#define __MTK_MDP_RDMA_H__

struct mtk_mdp_rdma_cfg {
	unsigned int	pitch;
	unsigned int	addr0;
	unsigned int	width;
	unsigned int	height;
	unsigned int	x_left;
	unsigned int	y_top;
	int		fmt;
	int		color_encoding;
};

#endif 
