 
 

#ifndef _MTK_VCODEC_DEC_H_
#define _MTK_VCODEC_DEC_H_

#include <media/videobuf2-core.h>
#include <media/v4l2-mem2mem.h>

#include "mtk_vcodec_dec_drv.h"

#define VCODEC_DEC_ALIGNED_64 64
#define VCODEC_CAPABILITY_4K_DISABLED	0x10
#define VCODEC_DEC_4K_CODED_WIDTH	4096U
#define VCODEC_DEC_4K_CODED_HEIGHT	2304U
#define MTK_VDEC_MAX_W	2048U
#define MTK_VDEC_MAX_H	1088U
#define MTK_VDEC_MIN_W	64U
#define MTK_VDEC_MIN_H	64U

#define MTK_VDEC_IRQ_STATUS_DEC_SUCCESS        0x10000

 
struct vdec_fb {
	struct mtk_vcodec_mem	base_y;
	struct mtk_vcodec_mem	base_c;
	unsigned int	status;
};

 
struct mtk_video_dec_buf {
	struct v4l2_m2m_buffer	m2m_buf;

	bool	used;
	bool	queued_in_vb2;
	bool	queued_in_v4l2;
	bool	error;

	union {
		struct vdec_fb	frame_buffer;
		struct mtk_vcodec_mem	bs_buffer;
	};
};

extern const struct v4l2_ioctl_ops mtk_vdec_ioctl_ops;
extern const struct v4l2_m2m_ops mtk_vdec_m2m_ops;
extern const struct media_device_ops mtk_vcodec_media_ops;
extern const struct mtk_vcodec_dec_pdata mtk_vdec_8173_pdata;
extern const struct mtk_vcodec_dec_pdata mtk_vdec_8183_pdata;
extern const struct mtk_vcodec_dec_pdata mtk_lat_sig_core_pdata;
extern const struct mtk_vcodec_dec_pdata mtk_vdec_single_core_pdata;


 
void mtk_vdec_unlock(struct mtk_vcodec_dec_ctx *ctx);
void mtk_vdec_lock(struct mtk_vcodec_dec_ctx *ctx);
int mtk_vcodec_dec_queue_init(void *priv, struct vb2_queue *src_vq,
			   struct vb2_queue *dst_vq);
void mtk_vcodec_dec_set_default_params(struct mtk_vcodec_dec_ctx *ctx);
void mtk_vcodec_dec_release(struct mtk_vcodec_dec_ctx *ctx);

 
int vb2ops_vdec_queue_setup(struct vb2_queue *vq, unsigned int *nbuffers,
			    unsigned int *nplanes, unsigned int sizes[],
			    struct device *alloc_devs[]);
int vb2ops_vdec_buf_prepare(struct vb2_buffer *vb);
void vb2ops_vdec_buf_finish(struct vb2_buffer *vb);
int vb2ops_vdec_buf_init(struct vb2_buffer *vb);
int vb2ops_vdec_start_streaming(struct vb2_queue *q, unsigned int count);
void vb2ops_vdec_stop_streaming(struct vb2_queue *q);


#endif  
