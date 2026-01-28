#ifndef __DPU_KMS_H__
#define __DPU_KMS_H__
#include <linux/interconnect.h>
#include <drm/drm_drv.h>
#include "msm_drv.h"
#include "msm_kms.h"
#include "msm_mmu.h"
#include "msm_gem.h"
#include "dpu_hw_catalog.h"
#include "dpu_hw_ctl.h"
#include "dpu_hw_lm.h"
#include "dpu_hw_interrupts.h"
#include "dpu_hw_top.h"
#include "dpu_rm.h"
#include "dpu_core_perf.h"
#define DRMID(x) ((x) ? (x)->base.id : -1)
#define DPU_DEBUG(fmt, ...)                                                \
	do {                                                               \
		if (drm_debug_enabled(DRM_UT_KMS))                         \
			DRM_DEBUG(fmt, ##__VA_ARGS__); \
		else                                                       \
			pr_debug(fmt, ##__VA_ARGS__);                      \
	} while (0)
#define DPU_DEBUG_DRIVER(fmt, ...)                                         \
	do {                                                               \
		if (drm_debug_enabled(DRM_UT_DRIVER))                      \
			DRM_ERROR(fmt, ##__VA_ARGS__); \
		else                                                       \
			pr_debug(fmt, ##__VA_ARGS__);                      \
	} while (0)
#define DPU_ERROR(fmt, ...) pr_err("[dpu error]" fmt, ##__VA_ARGS__)
#define ktime_compare_safe(A, B) \
	ktime_compare(ktime_sub((A), (B)), ktime_set(0, 0))
struct dpu_kms {
	struct msm_kms base;
	struct drm_device *dev;
	const struct dpu_mdss_cfg *catalog;
	const struct msm_mdss_data *mdss;
	void __iomem *mmio, *vbif[VBIF_MAX];
	struct regulator *vdd;
	struct regulator *mmagic;
	struct regulator *venus;
	struct dpu_hw_intr *hw_intr;
	struct dpu_core_perf perf;
	struct drm_modeset_lock global_state_lock;
	struct drm_private_obj global_state;
	struct dpu_rm rm;
	bool rm_init;
	struct dpu_hw_vbif *hw_vbif[VBIF_MAX];
	struct dpu_hw_mdp *hw_mdp;
	bool has_danger_ctrl;
	struct platform_device *pdev;
	bool rpm_enabled;
	struct clk_bulk_data *clocks;
	size_t num_clocks;
	atomic_t bandwidth_ref;
	struct icc_path *path[2];
	u32 num_paths;
};
struct vsync_info {
	u32 frame_count;
	u32 line_count;
};
#define DPU_ENC_WR_PTR_START_TIMEOUT_US 20000
#define DPU_ENC_MAX_POLL_TIMEOUT_US	2000
#define to_dpu_kms(x) container_of(x, struct dpu_kms, base)
#define to_dpu_global_state(x) container_of(x, struct dpu_global_state, base)
struct dpu_global_state {
	struct drm_private_state base;
	uint32_t pingpong_to_enc_id[PINGPONG_MAX - PINGPONG_0];
	uint32_t mixer_to_enc_id[LM_MAX - LM_0];
	uint32_t ctl_to_enc_id[CTL_MAX - CTL_0];
	uint32_t dspp_to_enc_id[DSPP_MAX - DSPP_0];
	uint32_t dsc_to_enc_id[DSC_MAX - DSC_0];
};
struct dpu_global_state
	*dpu_kms_get_existing_global_state(struct dpu_kms *dpu_kms);
struct dpu_global_state
	*__must_check dpu_kms_get_global_state(struct drm_atomic_state *s);
void dpu_debugfs_create_regset32(const char *name, umode_t mode,
		void *parent,
		uint32_t offset, uint32_t length, struct dpu_kms *dpu_kms);
void *dpu_debugfs_get_root(struct dpu_kms *dpu_kms);
#define DPU_KMS_INFO_MAX_SIZE	4096
int dpu_enable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);
void dpu_disable_vblank(struct msm_kms *kms, struct drm_crtc *crtc);
unsigned long dpu_kms_get_clk_rate(struct dpu_kms *dpu_kms, char *clock_name);
#endif  
