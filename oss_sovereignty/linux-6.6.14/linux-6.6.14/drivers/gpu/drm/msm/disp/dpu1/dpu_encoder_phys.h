#ifndef __DPU_ENCODER_PHYS_H__
#define __DPU_ENCODER_PHYS_H__
#include <drm/drm_writeback.h>
#include <linux/jiffies.h>
#include "dpu_kms.h"
#include "dpu_hw_intf.h"
#include "dpu_hw_wb.h"
#include "dpu_hw_pingpong.h"
#include "dpu_hw_ctl.h"
#include "dpu_hw_top.h"
#include "dpu_encoder.h"
#include "dpu_crtc.h"
#define DPU_ENCODER_NAME_MAX	16
#define KICKOFF_TIMEOUT_MS		84
#define KICKOFF_TIMEOUT_JIFFIES		msecs_to_jiffies(KICKOFF_TIMEOUT_MS)
enum dpu_enc_split_role {
	ENC_ROLE_SOLO,
	ENC_ROLE_MASTER,
	ENC_ROLE_SLAVE,
};
enum dpu_enc_enable_state {
	DPU_ENC_DISABLING,
	DPU_ENC_DISABLED,
	DPU_ENC_ENABLING,
	DPU_ENC_ENABLED,
	DPU_ENC_ERR_NEEDS_HW_RESET
};
struct dpu_encoder_phys;
struct dpu_encoder_phys_ops {
	void (*prepare_commit)(struct dpu_encoder_phys *encoder);
	bool (*is_master)(struct dpu_encoder_phys *encoder);
	void (*atomic_mode_set)(struct dpu_encoder_phys *encoder,
			struct drm_crtc_state *crtc_state,
			struct drm_connector_state *conn_state);
	void (*enable)(struct dpu_encoder_phys *encoder);
	void (*disable)(struct dpu_encoder_phys *encoder);
	int (*atomic_check)(struct dpu_encoder_phys *encoder,
			    struct drm_crtc_state *crtc_state,
			    struct drm_connector_state *conn_state);
	void (*destroy)(struct dpu_encoder_phys *encoder);
	int (*control_vblank_irq)(struct dpu_encoder_phys *enc, bool enable);
	int (*wait_for_commit_done)(struct dpu_encoder_phys *phys_enc);
	int (*wait_for_tx_complete)(struct dpu_encoder_phys *phys_enc);
	int (*wait_for_vblank)(struct dpu_encoder_phys *phys_enc);
	void (*prepare_for_kickoff)(struct dpu_encoder_phys *phys_enc);
	void (*handle_post_kickoff)(struct dpu_encoder_phys *phys_enc);
	void (*trigger_start)(struct dpu_encoder_phys *phys_enc);
	bool (*needs_single_flush)(struct dpu_encoder_phys *phys_enc);
	void (*irq_control)(struct dpu_encoder_phys *phys, bool enable);
	void (*prepare_idle_pc)(struct dpu_encoder_phys *phys_enc);
	void (*restore)(struct dpu_encoder_phys *phys);
	int (*get_line_count)(struct dpu_encoder_phys *phys);
	int (*get_frame_count)(struct dpu_encoder_phys *phys);
	void (*prepare_wb_job)(struct dpu_encoder_phys *phys_enc,
			struct drm_writeback_job *job);
	void (*cleanup_wb_job)(struct dpu_encoder_phys *phys_enc,
			struct drm_writeback_job *job);
	bool (*is_valid_for_commit)(struct dpu_encoder_phys *phys_enc);
};
enum dpu_intr_idx {
	INTR_IDX_VSYNC,
	INTR_IDX_PINGPONG,
	INTR_IDX_UNDERRUN,
	INTR_IDX_CTL_START,
	INTR_IDX_RDPTR,
	INTR_IDX_WB_DONE,
	INTR_IDX_MAX,
};
struct dpu_encoder_phys {
	struct drm_encoder *parent;
	struct dpu_encoder_phys_ops ops;
	struct dpu_hw_mdp *hw_mdptop;
	struct dpu_hw_ctl *hw_ctl;
	struct dpu_hw_pingpong *hw_pp;
	struct dpu_hw_intf *hw_intf;
	struct dpu_hw_wb *hw_wb;
	struct dpu_kms *dpu_kms;
	struct drm_display_mode cached_mode;
	enum dpu_enc_split_role split_role;
	enum dpu_intf_mode intf_mode;
	spinlock_t *enc_spinlock;
	enum dpu_enc_enable_state enable_state;
	atomic_t vblank_refcount;
	atomic_t vsync_cnt;
	atomic_t underrun_cnt;
	atomic_t pending_ctlstart_cnt;
	atomic_t pending_kickoff_cnt;
	wait_queue_head_t pending_kickoff_wq;
	int irq[INTR_IDX_MAX];
	bool has_intf_te;
};
static inline int dpu_encoder_phys_inc_pending(struct dpu_encoder_phys *phys)
{
	atomic_inc_return(&phys->pending_ctlstart_cnt);
	return atomic_inc_return(&phys->pending_kickoff_cnt);
}
struct dpu_encoder_phys_wb {
	struct dpu_encoder_phys base;
	atomic_t wbirq_refcount;
	int wb_done_timeout_cnt;
	struct dpu_hw_wb_cfg wb_cfg;
	struct drm_writeback_connector *wb_conn;
	struct drm_writeback_job *wb_job;
	struct dpu_hw_fmt_layout dest;
};
struct dpu_encoder_phys_cmd {
	struct dpu_encoder_phys base;
	int stream_sel;
	bool serialize_wait4pp;
	int pp_timeout_report_cnt;
	atomic_t pending_vblank_cnt;
	wait_queue_head_t pending_vblank_wq;
};
struct dpu_enc_phys_init_params {
	struct dpu_kms *dpu_kms;
	struct drm_encoder *parent;
	enum dpu_enc_split_role split_role;
	struct dpu_hw_intf *hw_intf;
	struct dpu_hw_wb *hw_wb;
	spinlock_t *enc_spinlock;
};
struct dpu_encoder_wait_info {
	wait_queue_head_t *wq;
	atomic_t *atomic_cnt;
	s64 timeout_ms;
};
struct dpu_encoder_phys *dpu_encoder_phys_vid_init(
		struct dpu_enc_phys_init_params *p);
struct dpu_encoder_phys *dpu_encoder_phys_cmd_init(
		struct dpu_enc_phys_init_params *p);
struct dpu_encoder_phys *dpu_encoder_phys_wb_init(
		struct dpu_enc_phys_init_params *p);
void dpu_encoder_helper_trigger_start(struct dpu_encoder_phys *phys_enc);
static inline enum dpu_3d_blend_mode dpu_encoder_helper_get_3d_blend_mode(
		struct dpu_encoder_phys *phys_enc)
{
	struct dpu_crtc_state *dpu_cstate;
	if (!phys_enc || phys_enc->enable_state == DPU_ENC_DISABLING)
		return BLEND_3D_NONE;
	dpu_cstate = to_dpu_crtc_state(phys_enc->parent->crtc->state);
	if (phys_enc->split_role == ENC_ROLE_SOLO &&
	    dpu_cstate->num_mixers == CRTC_DUAL_MIXERS &&
	    !dpu_encoder_use_dsc_merge(phys_enc->parent))
		return BLEND_3D_H_ROW_INT;
	return BLEND_3D_NONE;
}
unsigned int dpu_encoder_helper_get_dsc(struct dpu_encoder_phys *phys_enc);
void dpu_encoder_helper_split_config(
		struct dpu_encoder_phys *phys_enc,
		enum dpu_intf interface);
void dpu_encoder_helper_report_irq_timeout(struct dpu_encoder_phys *phys_enc,
		enum dpu_intr_idx intr_idx);
int dpu_encoder_helper_wait_for_irq(struct dpu_encoder_phys *phys_enc,
		int irq,
		void (*func)(void *arg, int irq_idx),
		struct dpu_encoder_wait_info *wait_info);
void dpu_encoder_helper_phys_cleanup(struct dpu_encoder_phys *phys_enc);
void dpu_encoder_vblank_callback(struct drm_encoder *drm_enc,
				 struct dpu_encoder_phys *phy_enc);
void dpu_encoder_underrun_callback(struct drm_encoder *drm_enc,
				   struct dpu_encoder_phys *phy_enc);
void dpu_encoder_frame_done_callback(
		struct drm_encoder *drm_enc,
		struct dpu_encoder_phys *ready_phys, u32 event);
void dpu_encoder_phys_init(struct dpu_encoder_phys *phys,
			   struct dpu_enc_phys_init_params *p);
#endif  
