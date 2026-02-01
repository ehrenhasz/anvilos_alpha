 
 

#ifndef __ATOMISP_TPG_H__
#define __ATOMISP_TPG_H__

#include <media/media-entity.h>
#include <media/v4l2-subdev.h>

struct atomisp_tpg_device {
	struct v4l2_subdev sd;
	struct atomisp_device *isp;
	struct media_pad pads[1];
};

void atomisp_tpg_cleanup(struct atomisp_device *isp);
int atomisp_tpg_init(struct atomisp_device *isp);
void atomisp_tpg_unregister_entities(struct atomisp_tpg_device *tpg);
int atomisp_tpg_register_entities(struct atomisp_tpg_device *tpg,
				  struct v4l2_device *vdev);

#endif  
