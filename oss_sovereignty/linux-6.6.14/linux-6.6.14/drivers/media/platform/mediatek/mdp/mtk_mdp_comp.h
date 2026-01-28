#ifndef __MTK_MDP_COMP_H__
#define __MTK_MDP_COMP_H__
enum mtk_mdp_comp_type {
	MTK_MDP_RDMA,
	MTK_MDP_RSZ,
	MTK_MDP_WDMA,
	MTK_MDP_WROT,
};
struct mtk_mdp_comp {
	struct list_head	node;
	struct device_node	*dev_node;
	struct clk		*clk[2];
	enum mtk_mdp_comp_type	type;
};
int mtk_mdp_comp_init(struct device *dev, struct device_node *node,
		      struct mtk_mdp_comp *comp,
		      enum mtk_mdp_comp_type comp_type);
void mtk_mdp_comp_deinit(struct device *dev, struct mtk_mdp_comp *comp);
void mtk_mdp_comp_clock_on(struct device *dev, struct mtk_mdp_comp *comp);
void mtk_mdp_comp_clock_off(struct device *dev, struct mtk_mdp_comp *comp);
#endif  
