#ifndef _VIVID_OSD_H_
#define _VIVID_OSD_H_
int vivid_fb_init(struct vivid_dev *dev);
void vivid_fb_release_buffers(struct vivid_dev *dev);
void vivid_clear_fb(struct vivid_dev *dev);
#endif
