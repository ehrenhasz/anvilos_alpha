
 

#include <linux/module.h>
#include <media/drv-intf/exynos-fimc.h>
#include "common.h"

 
struct v4l2_subdev *fimc_find_remote_sensor(struct media_entity *entity)
{
	struct media_pad *pad = &entity->pads[0];
	struct v4l2_subdev *sd;

	while (pad->flags & MEDIA_PAD_FL_SINK) {
		 
		pad = media_pad_remote_pad_first(pad);
		if (!pad || !is_media_entity_v4l2_subdev(pad->entity))
			break;

		sd = media_entity_to_v4l2_subdev(pad->entity);

		if (sd->grp_id == GRP_ID_FIMC_IS_SENSOR ||
		    sd->grp_id == GRP_ID_SENSOR)
			return sd;
		 
		pad = &sd->entity.pads[0];
	}
	return NULL;
}
EXPORT_SYMBOL(fimc_find_remote_sensor);

void __fimc_vidioc_querycap(struct device *dev, struct v4l2_capability *cap)
{
	strscpy(cap->driver, dev->driver->name, sizeof(cap->driver));
	strscpy(cap->card, dev->driver->name, sizeof(cap->card));
}
EXPORT_SYMBOL(__fimc_vidioc_querycap);

MODULE_LICENSE("GPL");
