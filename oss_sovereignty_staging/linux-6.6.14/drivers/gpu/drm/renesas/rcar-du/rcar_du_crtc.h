 
 

#ifndef __RCAR_DU_CRTC_H__
#define __RCAR_DU_CRTC_H__

#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <linux/wait.h>

#include <drm/drm_crtc.h>
#include <drm/drm_writeback.h>

#include <media/vsp1.h>

struct rcar_du_group;
struct rcar_du_vsp;

 
struct rcar_du_crtc {
	struct drm_crtc crtc;

	struct rcar_du_device *dev;
	struct clk *clock;
	struct clk *extclock;
	unsigned int mmio_offset;
	unsigned int index;
	bool initialized;

	u32 dsysr;

	bool vblank_enable;
	struct drm_pending_vblank_event *event;
	wait_queue_head_t flip_wait;

	spinlock_t vblank_lock;
	wait_queue_head_t vblank_wait;
	unsigned int vblank_count;

	struct rcar_du_group *group;
	struct platform_device *cmm;
	struct rcar_du_vsp *vsp;
	unsigned int vsp_pipe;

	const char *const *sources;
	unsigned int sources_count;

	struct drm_writeback_connector writeback;
};

#define to_rcar_crtc(c)		container_of(c, struct rcar_du_crtc, crtc)
#define wb_to_rcar_crtc(c)	container_of(c, struct rcar_du_crtc, writeback)

 
struct rcar_du_crtc_state {
	struct drm_crtc_state state;

	struct vsp1_du_crc_config crc;
	unsigned int outputs;
};

#define to_rcar_crtc_state(s) container_of(s, struct rcar_du_crtc_state, state)

int rcar_du_crtc_create(struct rcar_du_group *rgrp, unsigned int swindex,
			unsigned int hwindex);

void rcar_du_crtc_finish_page_flip(struct rcar_du_crtc *rcrtc);

void rcar_du_crtc_dsysr_clr_set(struct rcar_du_crtc *rcrtc, u32 clr, u32 set);

#endif  
