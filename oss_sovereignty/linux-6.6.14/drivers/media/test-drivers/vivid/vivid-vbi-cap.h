 
 

#ifndef _VIVID_VBI_CAP_H_
#define _VIVID_VBI_CAP_H_

void vivid_fill_time_of_day_packet(u8 *packet);
void vivid_raw_vbi_cap_process(struct vivid_dev *dev, struct vivid_buffer *buf);
void vivid_sliced_vbi_cap_process(struct vivid_dev *dev, struct vivid_buffer *buf);
void vivid_sliced_vbi_out_process(struct vivid_dev *dev, struct vivid_buffer *buf);
int vidioc_g_fmt_vbi_cap(struct file *file, void *priv,
					struct v4l2_format *f);
int vidioc_s_fmt_vbi_cap(struct file *file, void *priv,
					struct v4l2_format *f);
int vidioc_g_fmt_sliced_vbi_cap(struct file *file, void *fh, struct v4l2_format *fmt);
int vidioc_try_fmt_sliced_vbi_cap(struct file *file, void *fh, struct v4l2_format *fmt);
int vidioc_s_fmt_sliced_vbi_cap(struct file *file, void *fh, struct v4l2_format *fmt);
int vidioc_g_sliced_vbi_cap(struct file *file, void *fh, struct v4l2_sliced_vbi_cap *cap);

void vivid_fill_service_lines(struct v4l2_sliced_vbi_format *vbi, u32 service_set);

extern const struct vb2_ops vivid_vbi_cap_qops;

#endif
