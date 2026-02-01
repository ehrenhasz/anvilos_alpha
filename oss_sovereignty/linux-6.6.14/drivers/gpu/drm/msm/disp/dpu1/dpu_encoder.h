 
 

#ifndef __DPU_ENCODER_H__
#define __DPU_ENCODER_H__

#include <drm/drm_crtc.h>
#include "dpu_hw_mdss.h"

#define DPU_ENCODER_FRAME_EVENT_DONE			BIT(0)
#define DPU_ENCODER_FRAME_EVENT_ERROR			BIT(1)
#define DPU_ENCODER_FRAME_EVENT_PANEL_DEAD		BIT(2)
#define DPU_ENCODER_FRAME_EVENT_IDLE			BIT(3)

#define IDLE_TIMEOUT	(66 - 16/2)

 
struct msm_display_info {
	enum dpu_intf_type intf_type;
	uint32_t num_of_h_tiles;
	uint32_t h_tile_instance[MAX_H_TILES_PER_DISPLAY];
	bool is_cmd_mode;
	bool is_te_using_watchdog_timer;
};

 
void dpu_encoder_assign_crtc(struct drm_encoder *encoder,
			     struct drm_crtc *crtc);

 
void dpu_encoder_toggle_vblank_for_crtc(struct drm_encoder *encoder,
					struct drm_crtc *crtc, bool enable);

 
void dpu_encoder_register_frame_event_callback(struct drm_encoder *encoder,
		void (*cb)(void *, u32), void *data);

 
void dpu_encoder_prepare_for_kickoff(struct drm_encoder *encoder);

 
void dpu_encoder_trigger_kickoff_pending(struct drm_encoder *encoder);

 
void dpu_encoder_kickoff(struct drm_encoder *encoder);

 
int dpu_encoder_vsync_time(struct drm_encoder *drm_enc, ktime_t *wakeup_time);

 
int dpu_encoder_wait_for_event(struct drm_encoder *drm_encoder,
						enum msm_event_wait event);

 
enum dpu_intf_mode dpu_encoder_get_intf_mode(struct drm_encoder *encoder);

 
void dpu_encoder_virt_runtime_resume(struct drm_encoder *encoder);

 
struct drm_encoder *dpu_encoder_init(struct drm_device *dev,
		int drm_enc_mode,
		struct msm_display_info *disp_info);

 
void dpu_encoder_set_idle_timeout(struct drm_encoder *drm_enc,
							u32 idle_timeout);
 
int dpu_encoder_get_linecount(struct drm_encoder *drm_enc);

 
int dpu_encoder_get_vsync_count(struct drm_encoder *drm_enc);

bool dpu_encoder_is_widebus_enabled(const struct drm_encoder *drm_enc);

 
int dpu_encoder_get_crc_values_cnt(const struct drm_encoder *drm_enc);

 
void dpu_encoder_setup_misr(const struct drm_encoder *drm_encoder);

 
int dpu_encoder_get_crc(const struct drm_encoder *drm_enc, u32 *crcs, int pos);

 
bool dpu_encoder_use_dsc_merge(struct drm_encoder *drm_enc);

 
void dpu_encoder_prepare_wb_job(struct drm_encoder *drm_enc,
		struct drm_writeback_job *job);

 
void dpu_encoder_cleanup_wb_job(struct drm_encoder *drm_enc,
		struct drm_writeback_job *job);

 
bool dpu_encoder_is_valid_for_commit(struct drm_encoder *drm_enc);

#endif  
