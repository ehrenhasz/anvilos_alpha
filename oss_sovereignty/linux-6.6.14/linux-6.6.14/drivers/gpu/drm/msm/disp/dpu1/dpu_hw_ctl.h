#ifndef _DPU_HW_CTL_H
#define _DPU_HW_CTL_H
#include "dpu_hw_mdss.h"
#include "dpu_hw_util.h"
#include "dpu_hw_catalog.h"
#include "dpu_hw_sspp.h"
enum dpu_ctl_mode_sel {
	DPU_CTL_MODE_SEL_VID = 0,
	DPU_CTL_MODE_SEL_CMD
};
struct dpu_hw_ctl;
struct dpu_hw_stage_cfg {
	enum dpu_sspp stage[DPU_STAGE_MAX][PIPES_PER_STAGE];
	enum dpu_sspp_multirect_index multirect_index
					[DPU_STAGE_MAX][PIPES_PER_STAGE];
};
struct dpu_hw_intf_cfg {
	enum dpu_intf intf;
	enum dpu_wb wb;
	enum dpu_3d_blend_mode mode_3d;
	enum dpu_merge_3d merge_3d;
	enum dpu_ctl_mode_sel intf_mode_sel;
	int stream_sel;
	unsigned int dsc;
};
struct dpu_hw_ctl_ops {
	void (*trigger_start)(struct dpu_hw_ctl *ctx);
	bool (*is_started)(struct dpu_hw_ctl *ctx);
	void (*trigger_pending)(struct dpu_hw_ctl *ctx);
	void (*clear_pending_flush)(struct dpu_hw_ctl *ctx);
	u32 (*get_pending_flush)(struct dpu_hw_ctl *ctx);
	void (*update_pending_flush)(struct dpu_hw_ctl *ctx,
		u32 flushbits);
	void (*update_pending_flush_wb)(struct dpu_hw_ctl *ctx,
		enum dpu_wb blk);
	void (*update_pending_flush_intf)(struct dpu_hw_ctl *ctx,
		enum dpu_intf blk);
	void (*update_pending_flush_merge_3d)(struct dpu_hw_ctl *ctx,
		enum dpu_merge_3d blk);
	void (*update_pending_flush_sspp)(struct dpu_hw_ctl *ctx,
		enum dpu_sspp blk);
	void (*update_pending_flush_mixer)(struct dpu_hw_ctl *ctx,
		enum dpu_lm blk);
	void (*update_pending_flush_dspp)(struct dpu_hw_ctl *ctx,
		enum dpu_dspp blk, u32 dspp_sub_blk);
	void (*update_pending_flush_dsc)(struct dpu_hw_ctl *ctx,
					 enum dpu_dsc blk);
	void (*trigger_flush)(struct dpu_hw_ctl *ctx);
	u32 (*get_flush_register)(struct dpu_hw_ctl *ctx);
	void (*setup_intf_cfg)(struct dpu_hw_ctl *ctx,
		struct dpu_hw_intf_cfg *cfg);
	void (*reset_intf_cfg)(struct dpu_hw_ctl *ctx,
			struct dpu_hw_intf_cfg *cfg);
	int (*reset)(struct dpu_hw_ctl *c);
	int (*wait_reset_status)(struct dpu_hw_ctl *ctx);
	void (*clear_all_blendstages)(struct dpu_hw_ctl *ctx);
	void (*setup_blendstage)(struct dpu_hw_ctl *ctx,
		enum dpu_lm lm, struct dpu_hw_stage_cfg *cfg);
	void (*set_active_pipes)(struct dpu_hw_ctl *ctx,
		unsigned long *fetch_active);
};
struct dpu_hw_ctl {
	struct dpu_hw_blk base;
	struct dpu_hw_blk_reg_map hw;
	int idx;
	const struct dpu_ctl_cfg *caps;
	int mixer_count;
	const struct dpu_lm_cfg *mixer_hw_caps;
	u32 pending_flush_mask;
	u32 pending_intf_flush_mask;
	u32 pending_wb_flush_mask;
	u32 pending_merge_3d_flush_mask;
	u32 pending_dspp_flush_mask[DSPP_MAX - DSPP_0];
	u32 pending_dsc_flush_mask;
	struct dpu_hw_ctl_ops ops;
};
static inline struct dpu_hw_ctl *to_dpu_hw_ctl(struct dpu_hw_blk *hw)
{
	return container_of(hw, struct dpu_hw_ctl, base);
}
struct dpu_hw_ctl *dpu_hw_ctl_init(const struct dpu_ctl_cfg *cfg,
		void __iomem *addr,
		u32 mixer_count,
		const struct dpu_lm_cfg *mixer);
void dpu_hw_ctl_destroy(struct dpu_hw_ctl *ctx);
#endif  
