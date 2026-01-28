#ifndef _DPU_CRTC_H_
#define _DPU_CRTC_H_
#include <linux/kthread.h>
#include <drm/drm_crtc.h>
#include "dpu_kms.h"
#include "dpu_core_perf.h"
#define DPU_CRTC_NAME_SIZE	12
#define DPU_CRTC_FRAME_EVENT_SIZE	4
enum dpu_crtc_client_type {
	RT_CLIENT,
	NRT_CLIENT,
};
enum dpu_crtc_smmu_state {
	ATTACHED = 0,
	DETACHED,
	ATTACH_ALL_REQ,
	DETACH_ALL_REQ,
};
enum dpu_crtc_smmu_state_transition_type {
	NONE,
	PRE_COMMIT,
	POST_COMMIT
};
struct dpu_crtc_smmu_state_data {
	uint32_t state;
	uint32_t transition_type;
	uint32_t transition_error;
};
enum dpu_crtc_crc_source {
	DPU_CRTC_CRC_SOURCE_NONE = 0,
	DPU_CRTC_CRC_SOURCE_LAYER_MIXER,
	DPU_CRTC_CRC_SOURCE_ENCODER,
	DPU_CRTC_CRC_SOURCE_MAX,
	DPU_CRTC_CRC_SOURCE_INVALID = -1
};
struct dpu_crtc_mixer {
	struct dpu_hw_mixer *hw_lm;
	struct dpu_hw_ctl *lm_ctl;
	struct dpu_hw_dspp *hw_dspp;
	u32 mixer_op_mode;
};
struct dpu_crtc_frame_event {
	struct kthread_work work;
	struct drm_crtc *crtc;
	struct list_head list;
	ktime_t ts;
	u32 event;
};
#define DPU_CRTC_MAX_EVENT_COUNT	16
struct dpu_crtc {
	struct drm_crtc base;
	char name[DPU_CRTC_NAME_SIZE];
	struct drm_pending_vblank_event *event;
	u32 vsync_count;
	u32 vblank_cb_count;
	u64 play_count;
	ktime_t vblank_cb_time;
	bool enabled;
	struct list_head feature_list;
	struct list_head active_list;
	struct list_head dirty_list;
	struct list_head ad_dirty;
	struct list_head ad_active;
	atomic_t frame_pending;
	struct dpu_crtc_frame_event frame_events[DPU_CRTC_FRAME_EVENT_SIZE];
	struct list_head frame_event_list;
	spinlock_t spin_lock;
	struct completion frame_done_comp;
	spinlock_t event_lock;
	struct dpu_core_perf_params cur_perf;
	struct dpu_crtc_smmu_state_data smmu_state;
};
#define to_dpu_crtc(x) container_of(x, struct dpu_crtc, base)
struct dpu_crtc_state {
	struct drm_crtc_state base;
	bool bw_control;
	bool bw_split_vote;
	struct drm_rect lm_bounds[CRTC_DUAL_MIXERS];
	uint64_t input_fence_timeout_ns;
	struct dpu_core_perf_params new_perf;
	u32 num_mixers;
	struct dpu_crtc_mixer mixers[CRTC_DUAL_MIXERS];
	u32 num_ctls;
	struct dpu_hw_ctl *hw_ctls[CRTC_DUAL_MIXERS];
	enum dpu_crtc_crc_source crc_source;
	int crc_frame_skip_count;
};
#define to_dpu_crtc_state(x) \
	container_of(x, struct dpu_crtc_state, base)
static inline int dpu_crtc_frame_pending(struct drm_crtc *crtc)
{
	return crtc ? atomic_read(&to_dpu_crtc(crtc)->frame_pending) : -EINVAL;
}
int dpu_crtc_vblank(struct drm_crtc *crtc, bool en);
void dpu_crtc_vblank_callback(struct drm_crtc *crtc);
void dpu_crtc_commit_kickoff(struct drm_crtc *crtc);
void dpu_crtc_complete_commit(struct drm_crtc *crtc);
struct drm_crtc *dpu_crtc_init(struct drm_device *dev, struct drm_plane *plane,
			       struct drm_plane *cursor);
int dpu_crtc_register_custom_event(struct dpu_kms *kms,
		struct drm_crtc *crtc_drm, u32 event, bool en);
enum dpu_intf_mode dpu_crtc_get_intf_mode(struct drm_crtc *crtc);
static inline enum dpu_crtc_client_type dpu_crtc_get_client_type(
						struct drm_crtc *crtc)
{
	return crtc && crtc->state ? RT_CLIENT : NRT_CLIENT;
}
#endif  
