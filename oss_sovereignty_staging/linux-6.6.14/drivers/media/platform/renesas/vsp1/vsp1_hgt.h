 
 
#ifndef __VSP1_HGT_H__
#define __VSP1_HGT_H__

#include <media/media-entity.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-subdev.h>

#include "vsp1_histo.h"

struct vsp1_device;

#define HGT_NUM_HUE_AREAS			6

struct vsp1_hgt {
	struct vsp1_histogram histo;

	struct v4l2_ctrl_handler ctrls;

	u8 hue_areas[HGT_NUM_HUE_AREAS * 2];
};

static inline struct vsp1_hgt *to_hgt(struct v4l2_subdev *subdev)
{
	return container_of(subdev, struct vsp1_hgt, histo.entity.subdev);
}

struct vsp1_hgt *vsp1_hgt_create(struct vsp1_device *vsp1);
void vsp1_hgt_frame_end(struct vsp1_entity *hgt);

#endif  
