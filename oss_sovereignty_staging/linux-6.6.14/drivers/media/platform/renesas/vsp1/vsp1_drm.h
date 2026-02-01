 
 
#ifndef __VSP1_DRM_H__
#define __VSP1_DRM_H__

#include <linux/mutex.h>
#include <linux/videodev2.h>
#include <linux/wait.h>

#include <media/vsp1.h>

#include "vsp1_pipe.h"

 
struct vsp1_drm_pipeline {
	struct vsp1_pipeline pipe;

	unsigned int width;
	unsigned int height;

	bool force_brx_release;
	wait_queue_head_t wait_queue;

	struct vsp1_entity *uif;
	struct vsp1_du_crc_config crc;

	 
	void (*du_complete)(void *data, unsigned int status, u32 crc);
	void *du_private;
};

 
struct vsp1_drm {
	struct vsp1_drm_pipeline pipe[VSP1_MAX_LIF];
	struct mutex lock;

	struct {
		struct v4l2_rect crop;
		struct v4l2_rect compose;
		unsigned int zpos;
	} inputs[VSP1_MAX_RPF];
};

static inline struct vsp1_drm_pipeline *
to_vsp1_drm_pipeline(struct vsp1_pipeline *pipe)
{
	return container_of(pipe, struct vsp1_drm_pipeline, pipe);
}

int vsp1_drm_init(struct vsp1_device *vsp1);
void vsp1_drm_cleanup(struct vsp1_device *vsp1);

#endif  
