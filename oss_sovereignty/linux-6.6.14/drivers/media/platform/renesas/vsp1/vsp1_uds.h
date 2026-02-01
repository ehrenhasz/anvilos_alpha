 
 
#ifndef __VSP1_UDS_H__
#define __VSP1_UDS_H__

#include <media/media-entity.h>
#include <media/v4l2-subdev.h>

#include "vsp1_entity.h"

struct vsp1_device;

#define UDS_PAD_SINK				0
#define UDS_PAD_SOURCE				1

struct vsp1_uds {
	struct vsp1_entity entity;
	bool scale_alpha;
};

static inline struct vsp1_uds *to_uds(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vsp1_uds, entity.subdev);
}

struct vsp1_uds *vsp1_uds_create(struct vsp1_device *vsp1, unsigned int index);

void vsp1_uds_set_alpha(struct vsp1_entity *uds, struct vsp1_dl_body *dlb,
			unsigned int alpha);

#endif  
