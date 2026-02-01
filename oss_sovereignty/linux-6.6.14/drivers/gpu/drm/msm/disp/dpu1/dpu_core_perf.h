 
 

#ifndef _DPU_CORE_PERF_H_
#define _DPU_CORE_PERF_H_

#include <linux/types.h>
#include <linux/dcache.h>
#include <linux/mutex.h>
#include <drm/drm_crtc.h>

#include "dpu_hw_catalog.h"

 
struct dpu_core_perf_params {
	u64 max_per_pipe_ib;
	u64 bw_ctl;
	u64 core_clk_rate;
};

 
struct dpu_core_perf_tune {
	u32 mode;
};

 
struct dpu_core_perf {
	const struct dpu_perf_cfg *perf_cfg;
	u64 core_clk_rate;
	u64 max_core_clk_rate;
	struct dpu_core_perf_tune perf_tune;
	u32 enable_bw_release;
	u64 fix_core_clk_rate;
	u64 fix_core_ib_vote;
	u64 fix_core_ab_vote;
};

 
int dpu_core_perf_crtc_check(struct drm_crtc *crtc,
		struct drm_crtc_state *state);

 
int dpu_core_perf_crtc_update(struct drm_crtc *crtc,
			      int params_changed);

 
void dpu_core_perf_crtc_release_bw(struct drm_crtc *crtc);

 
int dpu_core_perf_init(struct dpu_core_perf *perf,
		const struct dpu_perf_cfg *perf_cfg,
		unsigned long max_core_clk_rate);

struct dpu_kms;

 
int dpu_core_perf_debugfs_init(struct dpu_kms *dpu_kms, struct dentry *parent);

#endif  
