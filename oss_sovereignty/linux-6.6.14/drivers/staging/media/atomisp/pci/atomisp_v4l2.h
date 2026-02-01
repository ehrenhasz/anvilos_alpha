 
 

#ifndef __ATOMISP_V4L2_H__
#define __ATOMISP_V4L2_H__

struct atomisp_video_pipe;
struct v4l2_device;
struct atomisp_device;
struct firmware;

int atomisp_video_init(struct atomisp_video_pipe *video);
void atomisp_video_unregister(struct atomisp_video_pipe *video);
const struct firmware *atomisp_load_firmware(struct atomisp_device *isp);
int atomisp_csi_lane_config(struct atomisp_device *isp);
int atomisp_register_device_nodes(struct atomisp_device *isp);

#endif  
