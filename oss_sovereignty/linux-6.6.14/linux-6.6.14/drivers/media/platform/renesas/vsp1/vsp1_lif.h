#ifndef __VSP1_LIF_H__
#define __VSP1_LIF_H__
#include <media/media-entity.h>
#include <media/v4l2-subdev.h>
#include "vsp1_entity.h"
struct vsp1_device;
#define LIF_PAD_SINK				0
#define LIF_PAD_SOURCE				1
struct vsp1_lif {
	struct vsp1_entity entity;
};
static inline struct vsp1_lif *to_lif(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vsp1_lif, entity.subdev);
}
struct vsp1_lif *vsp1_lif_create(struct vsp1_device *vsp1, unsigned int index);
#endif  
