#ifndef __MSM_KMS_H__
#define __MSM_KMS_H__
#include <linux/clk.h>
#include <linux/regulator/consumer.h>
#include "msm_drv.h"
#define MAX_PLANE	4
struct msm_kms_funcs {
	int (*hw_init)(struct msm_kms *kms);
	void (*irq_preinstall)(struct msm_kms *kms);
	int (*irq_postinstall)(struct msm_kms *kms);
	void (*irq_uninstall)(struct msm_kms *kms);
	irqreturn_t (*irq)(struct msm_kms *kms);
	int (*enable_vblank)(struct msm_kms *kms, struct drm_crtc *crtc);
	void (*disable_vblank)(struct msm_kms *kms, struct drm_crtc *crtc);
	void (*enable_commit)(struct msm_kms *kms);
	void (*disable_commit)(struct msm_kms *kms);
	void (*prepare_commit)(struct msm_kms *kms, struct drm_atomic_state *state);
	void (*flush_commit)(struct msm_kms *kms, unsigned crtc_mask);
	void (*wait_flush)(struct msm_kms *kms, unsigned crtc_mask);
	void (*complete_commit)(struct msm_kms *kms, unsigned crtc_mask);
	const struct msm_format *(*get_format)(struct msm_kms *kms,
					const uint32_t format,
					const uint64_t modifiers);
	int (*check_modified_format)(const struct msm_kms *kms,
			const struct msm_format *msm_fmt,
			const struct drm_mode_fb_cmd2 *cmd,
			struct drm_gem_object **bos);
	long (*round_pixclk)(struct msm_kms *kms, unsigned long rate,
			struct drm_encoder *encoder);
	int (*set_split_display)(struct msm_kms *kms,
			struct drm_encoder *encoder,
			struct drm_encoder *slave_encoder,
			bool is_cmd_mode);
	void (*destroy)(struct msm_kms *kms);
	void (*snapshot)(struct msm_disp_state *disp_state, struct msm_kms *kms);
#ifdef CONFIG_DEBUG_FS
	int (*debugfs_init)(struct msm_kms *kms, struct drm_minor *minor);
#endif
};
struct msm_kms;
struct msm_pending_timer {
	struct msm_hrtimer_work work;
	struct kthread_worker *worker;
	struct msm_kms *kms;
	unsigned crtc_idx;
};
struct msm_kms {
	const struct msm_kms_funcs *funcs;
	struct drm_device *dev;
	int irq;
	bool irq_requested;
	struct msm_gem_address_space *aspace;
	struct kthread_worker *dump_worker;
	struct kthread_work dump_work;
	struct mutex dump_mutex;
	struct mutex commit_lock[MAX_CRTCS];
	unsigned pending_crtc_mask;
	struct msm_pending_timer pending_timers[MAX_CRTCS];
};
static inline int msm_kms_init(struct msm_kms *kms,
		const struct msm_kms_funcs *funcs)
{
	unsigned i, ret;
	for (i = 0; i < ARRAY_SIZE(kms->commit_lock); i++)
		mutex_init(&kms->commit_lock[i]);
	kms->funcs = funcs;
	for (i = 0; i < ARRAY_SIZE(kms->pending_timers); i++) {
		ret = msm_atomic_init_pending_timer(&kms->pending_timers[i], kms, i);
		if (ret) {
			return ret;
		}
	}
	return 0;
}
static inline void msm_kms_destroy(struct msm_kms *kms)
{
	unsigned i;
	for (i = 0; i < ARRAY_SIZE(kms->pending_timers); i++)
		msm_atomic_destroy_pending_timer(&kms->pending_timers[i]);
}
#define for_each_crtc_mask(dev, crtc, crtc_mask) \
	drm_for_each_crtc(crtc, dev) \
		for_each_if (drm_crtc_mask(crtc) & (crtc_mask))
#define for_each_crtc_mask_reverse(dev, crtc, crtc_mask) \
	drm_for_each_crtc_reverse(crtc, dev) \
		for_each_if (drm_crtc_mask(crtc) & (crtc_mask))
#endif  
