#ifndef __DPU_RM_H__
#define __DPU_RM_H__
#include <linux/list.h>
#include "msm_kms.h"
#include "dpu_hw_top.h"
struct dpu_global_state;
struct dpu_rm {
	struct dpu_hw_blk *pingpong_blks[PINGPONG_MAX - PINGPONG_0];
	struct dpu_hw_blk *mixer_blks[LM_MAX - LM_0];
	struct dpu_hw_blk *ctl_blks[CTL_MAX - CTL_0];
	struct dpu_hw_intf *hw_intf[INTF_MAX - INTF_0];
	struct dpu_hw_wb *hw_wb[WB_MAX - WB_0];
	struct dpu_hw_blk *dspp_blks[DSPP_MAX - DSPP_0];
	struct dpu_hw_blk *merge_3d_blks[MERGE_3D_MAX - MERGE_3D_0];
	struct dpu_hw_blk *dsc_blks[DSC_MAX - DSC_0];
	struct dpu_hw_sspp *hw_sspp[SSPP_MAX - SSPP_NONE];
};
int dpu_rm_init(struct dpu_rm *rm,
		const struct dpu_mdss_cfg *cat,
		const struct msm_mdss_data *mdss_data,
		void __iomem *mmio);
int dpu_rm_destroy(struct dpu_rm *rm);
int dpu_rm_reserve(struct dpu_rm *rm,
		struct dpu_global_state *global_state,
		struct drm_encoder *drm_enc,
		struct drm_crtc_state *crtc_state,
		struct msm_display_topology topology);
void dpu_rm_release(struct dpu_global_state *global_state,
		struct drm_encoder *enc);
int dpu_rm_get_assigned_resources(struct dpu_rm *rm,
	struct dpu_global_state *global_state, uint32_t enc_id,
	enum dpu_hw_blk_type type, struct dpu_hw_blk **blks, int blks_size);
static inline struct dpu_hw_intf *dpu_rm_get_intf(struct dpu_rm *rm, enum dpu_intf intf_idx)
{
	return rm->hw_intf[intf_idx - INTF_0];
}
static inline struct dpu_hw_wb *dpu_rm_get_wb(struct dpu_rm *rm, enum dpu_wb wb_idx)
{
	return rm->hw_wb[wb_idx - WB_0];
}
static inline struct dpu_hw_sspp *dpu_rm_get_sspp(struct dpu_rm *rm, enum dpu_sspp sspp_idx)
{
	return rm->hw_sspp[sspp_idx - SSPP_NONE];
}
#endif  
