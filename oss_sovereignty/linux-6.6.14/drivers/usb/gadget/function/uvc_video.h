 
 
#ifndef __UVC_VIDEO_H__
#define __UVC_VIDEO_H__

struct uvc_video;

int uvcg_video_enable(struct uvc_video *video, int enable);

int uvcg_video_init(struct uvc_video *video, struct uvc_device *uvc);

#endif  
