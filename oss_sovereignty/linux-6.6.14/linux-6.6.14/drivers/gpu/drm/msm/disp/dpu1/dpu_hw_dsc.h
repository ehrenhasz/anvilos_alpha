#ifndef _DPU_HW_DSC_H
#define _DPU_HW_DSC_H
#include <drm/display/drm_dsc.h>
#define DSC_MODE_SPLIT_PANEL            BIT(0)
#define DSC_MODE_MULTIPLEX              BIT(1)
#define DSC_MODE_VIDEO                  BIT(2)
struct dpu_hw_dsc;
struct dpu_hw_dsc_ops {
	void (*dsc_disable)(struct dpu_hw_dsc *hw_dsc);
	void (*dsc_config)(struct dpu_hw_dsc *hw_dsc,
			   struct drm_dsc_config *dsc,
			   u32 mode,
			   u32 initial_lines);
	void (*dsc_config_thresh)(struct dpu_hw_dsc *hw_dsc,
				  struct drm_dsc_config *dsc);
	void (*dsc_bind_pingpong_blk)(struct dpu_hw_dsc *hw_dsc,
				  enum dpu_pingpong pp);
};
struct dpu_hw_dsc {
	struct dpu_hw_blk base;
	struct dpu_hw_blk_reg_map hw;
	enum dpu_dsc idx;
	const struct dpu_dsc_cfg *caps;
	struct dpu_hw_dsc_ops ops;
};
struct dpu_hw_dsc *dpu_hw_dsc_init(const struct dpu_dsc_cfg *cfg,
		void __iomem *addr);
struct dpu_hw_dsc *dpu_hw_dsc_init_1_2(const struct dpu_dsc_cfg *cfg,
				       void __iomem *addr);
void dpu_hw_dsc_destroy(struct dpu_hw_dsc *dsc);
static inline struct dpu_hw_dsc *to_dpu_hw_dsc(struct dpu_hw_blk *hw)
{
	return container_of(hw, struct dpu_hw_dsc, base);
}
#endif  
