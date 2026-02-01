 
 

#ifndef _CEDRUS_VIDEO_H_
#define _CEDRUS_VIDEO_H_

struct cedrus_format {
	u32		pixelformat;
	u32		directions;
	unsigned int	capabilities;
};

extern const struct v4l2_ioctl_ops cedrus_ioctl_ops;

int cedrus_queue_init(void *priv, struct vb2_queue *src_vq,
		      struct vb2_queue *dst_vq);
void cedrus_prepare_format(struct v4l2_pix_format *pix_fmt);
void cedrus_reset_cap_format(struct cedrus_ctx *ctx);
void cedrus_reset_out_format(struct cedrus_ctx *ctx);

#endif
