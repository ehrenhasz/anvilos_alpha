 
 
#ifndef _RKISP1_CSI_H
#define _RKISP1_CSI_H

struct rkisp1_csi;
struct rkisp1_device;
struct rkisp1_sensor_async;

int rkisp1_csi_init(struct rkisp1_device *rkisp1);
void rkisp1_csi_cleanup(struct rkisp1_device *rkisp1);

int rkisp1_csi_register(struct rkisp1_device *rkisp1);
void rkisp1_csi_unregister(struct rkisp1_device *rkisp1);

int rkisp1_csi_link_sensor(struct rkisp1_device *rkisp1, struct v4l2_subdev *sd,
			   struct rkisp1_sensor_async *s_asd,
			   unsigned int source_pad);

#endif  
