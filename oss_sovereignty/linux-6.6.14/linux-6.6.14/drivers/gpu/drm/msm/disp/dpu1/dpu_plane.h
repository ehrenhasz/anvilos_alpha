#ifndef _DPU_PLANE_H_
#define _DPU_PLANE_H_
#include <drm/drm_crtc.h>
#include "dpu_kms.h"
#include "dpu_hw_mdss.h"
#include "dpu_hw_sspp.h"
struct dpu_plane_state {
	struct drm_plane_state base;
	struct msm_gem_address_space *aspace;
	struct dpu_sw_pipe pipe;
	struct dpu_sw_pipe r_pipe;
	struct dpu_sw_pipe_cfg pipe_cfg;
	struct dpu_sw_pipe_cfg r_pipe_cfg;
	enum dpu_stage stage;
	bool needs_qos_remap;
	bool pending;
	u64 plane_fetch_bw;
	u64 plane_clk;
	bool needs_dirtyfb;
	unsigned int rotation;
};
#define to_dpu_plane_state(x) \
	container_of(x, struct dpu_plane_state, base)
void dpu_plane_flush(struct drm_plane *plane);
void dpu_plane_set_error(struct drm_plane *plane, bool error);
struct drm_plane *dpu_plane_init(struct drm_device *dev,
		uint32_t pipe, enum drm_plane_type type,
		unsigned long possible_crtcs);
int dpu_plane_color_fill(struct drm_plane *plane,
		uint32_t color, uint32_t alpha);
#ifdef CONFIG_DEBUG_FS
void dpu_plane_danger_signal_ctrl(struct drm_plane *plane, bool enable);
#else
static inline void dpu_plane_danger_signal_ctrl(struct drm_plane *plane, bool enable) {}
#endif
#endif  
