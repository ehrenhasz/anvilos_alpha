#ifndef __DPU_VBIF_H__
#define __DPU_VBIF_H__
#include "dpu_kms.h"
struct dpu_vbif_set_ot_params {
	u32 xin_id;
	u32 num;
	u32 width;
	u32 height;
	u32 frame_rate;
	bool rd;
	bool is_wfd;
	u32 vbif_idx;
	u32 clk_ctrl;
};
struct dpu_vbif_set_memtype_params {
	u32 xin_id;
	u32 vbif_idx;
	u32 clk_ctrl;
	bool is_cacheable;
};
struct dpu_vbif_set_qos_params {
	u32 vbif_idx;
	u32 xin_id;
	u32 clk_ctrl;
	u32 num;
	bool is_rt;
};
void dpu_vbif_set_ot_limit(struct dpu_kms *dpu_kms,
		struct dpu_vbif_set_ot_params *params);
void dpu_vbif_set_qos_remap(struct dpu_kms *dpu_kms,
		struct dpu_vbif_set_qos_params *params);
void dpu_vbif_clear_errors(struct dpu_kms *dpu_kms);
void dpu_vbif_init_memtypes(struct dpu_kms *dpu_kms);
void dpu_debugfs_vbif_init(struct dpu_kms *dpu_kms, struct dentry *debugfs_root);
#endif  
