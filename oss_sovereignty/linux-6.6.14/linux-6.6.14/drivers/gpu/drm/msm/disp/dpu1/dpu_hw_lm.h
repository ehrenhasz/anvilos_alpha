#ifndef _DPU_HW_LM_H
#define _DPU_HW_LM_H
#include "dpu_hw_mdss.h"
#include "dpu_hw_util.h"
struct dpu_hw_mixer;
struct dpu_hw_mixer_cfg {
	u32 out_width;
	u32 out_height;
	bool right_mixer;
	int flags;
};
struct dpu_hw_color3_cfg {
	u8 keep_fg[DPU_STAGE_MAX];
};
struct dpu_hw_lm_ops {
	void (*setup_mixer_out)(struct dpu_hw_mixer *ctx,
		struct dpu_hw_mixer_cfg *cfg);
	void (*setup_blend_config)(struct dpu_hw_mixer *ctx, uint32_t stage,
		uint32_t fg_alpha, uint32_t bg_alpha, uint32_t blend_op);
	void (*setup_alpha_out)(struct dpu_hw_mixer *ctx, uint32_t mixer_op);
	void (*setup_border_color)(struct dpu_hw_mixer *ctx,
		struct dpu_mdss_color *color,
		u8 border_en);
	void (*setup_misr)(struct dpu_hw_mixer *ctx);
	int (*collect_misr)(struct dpu_hw_mixer *ctx, u32 *misr_value);
};
struct dpu_hw_mixer {
	struct dpu_hw_blk base;
	struct dpu_hw_blk_reg_map hw;
	enum dpu_lm  idx;
	const struct dpu_lm_cfg   *cap;
	const struct dpu_mdp_cfg  *mdp;
	const struct dpu_ctl_cfg  *ctl;
	struct dpu_hw_lm_ops ops;
	struct dpu_hw_mixer_cfg cfg;
};
static inline struct dpu_hw_mixer *to_dpu_hw_mixer(struct dpu_hw_blk *hw)
{
	return container_of(hw, struct dpu_hw_mixer, base);
}
struct dpu_hw_mixer *dpu_hw_lm_init(const struct dpu_lm_cfg *cfg,
		void __iomem *addr);
void dpu_hw_lm_destroy(struct dpu_hw_mixer *lm);
#endif  
