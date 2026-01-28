#ifndef _DPU_HW_DSPP_H
#define _DPU_HW_DSPP_H
struct dpu_hw_dspp;
struct dpu_hw_pcc_coeff {
	__u32 r;
	__u32 g;
	__u32 b;
};
struct dpu_hw_pcc_cfg {
	struct dpu_hw_pcc_coeff r;
	struct dpu_hw_pcc_coeff g;
	struct dpu_hw_pcc_coeff b;
};
struct dpu_hw_dspp_ops {
	void (*setup_pcc)(struct dpu_hw_dspp *ctx, struct dpu_hw_pcc_cfg *cfg);
};
struct dpu_hw_dspp {
	struct dpu_hw_blk base;
	struct dpu_hw_blk_reg_map hw;
	int idx;
	const struct dpu_dspp_cfg *cap;
	struct dpu_hw_dspp_ops ops;
};
static inline struct dpu_hw_dspp *to_dpu_hw_dspp(struct dpu_hw_blk *hw)
{
	return container_of(hw, struct dpu_hw_dspp, base);
}
struct dpu_hw_dspp *dpu_hw_dspp_init(const struct dpu_dspp_cfg *cfg,
	void __iomem *addr);
void dpu_hw_dspp_destroy(struct dpu_hw_dspp *dspp);
#endif  
