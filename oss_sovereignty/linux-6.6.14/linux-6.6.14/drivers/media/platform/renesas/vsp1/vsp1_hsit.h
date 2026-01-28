#ifndef __VSP1_HSIT_H__
#define __VSP1_HSIT_H__
#include <media/media-entity.h>
#include <media/v4l2-subdev.h>
#include "vsp1_entity.h"
struct vsp1_device;
#define HSIT_PAD_SINK				0
#define HSIT_PAD_SOURCE				1
struct vsp1_hsit {
	struct vsp1_entity entity;
	bool inverse;
};
static inline struct vsp1_hsit *to_hsit(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vsp1_hsit, entity.subdev);
}
struct vsp1_hsit *vsp1_hsit_create(struct vsp1_device *vsp1, bool inverse);
#endif  
