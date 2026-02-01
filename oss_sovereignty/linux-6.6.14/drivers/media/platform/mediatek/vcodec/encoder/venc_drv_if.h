 
 

#ifndef _VENC_DRV_IF_H_
#define _VENC_DRV_IF_H_

#include "mtk_vcodec_enc_drv.h"

 
enum venc_yuv_fmt {
	VENC_YUV_FORMAT_I420 = 3,
	VENC_YUV_FORMAT_YV12 = 5,
	VENC_YUV_FORMAT_NV12 = 6,
	VENC_YUV_FORMAT_NV21 = 7,
};

 
enum venc_start_opt {
	VENC_START_OPT_ENCODE_SEQUENCE_HEADER,
	VENC_START_OPT_ENCODE_FRAME,
};

 
enum venc_set_param_type {
	VENC_SET_PARAM_ENC,
	VENC_SET_PARAM_FORCE_INTRA,
	VENC_SET_PARAM_ADJUST_BITRATE,
	VENC_SET_PARAM_ADJUST_FRAMERATE,
	VENC_SET_PARAM_GOP_SIZE,
	VENC_SET_PARAM_INTRA_PERIOD,
	VENC_SET_PARAM_SKIP_FRAME,
	VENC_SET_PARAM_PREPEND_HEADER,
	VENC_SET_PARAM_TS_MODE,
};

 
struct venc_enc_param {
	enum venc_yuv_fmt input_yuv_fmt;
	unsigned int h264_profile;
	unsigned int h264_level;
	unsigned int width;
	unsigned int height;
	unsigned int buf_width;
	unsigned int buf_height;
	unsigned int frm_rate;
	unsigned int intra_period;
	unsigned int bitrate;
	unsigned int gop_size;
};

 
struct venc_frame_info {
	unsigned int frm_count;		 
	unsigned int skip_frm_count;	 
	unsigned int frm_type;		 
};

 
struct venc_frm_buf {
	struct mtk_vcodec_fb fb_addr[MTK_VCODEC_MAX_PLANES];
};

 
struct venc_done_result {
	unsigned int bs_size;
	bool is_key_frm;
};

extern const struct venc_common_if venc_h264_if;
extern const struct venc_common_if venc_vp8_if;

 
int venc_if_init(struct mtk_vcodec_enc_ctx *ctx, unsigned int fourcc);

 
int venc_if_deinit(struct mtk_vcodec_enc_ctx *ctx);

 
int venc_if_set_param(struct mtk_vcodec_enc_ctx *ctx,
		      enum venc_set_param_type type,
		      struct venc_enc_param *in);

 
int venc_if_encode(struct mtk_vcodec_enc_ctx *ctx,
		   enum venc_start_opt opt,
		   struct venc_frm_buf *frm_buf,
		   struct mtk_vcodec_mem *bs_buf,
		   struct venc_done_result *result);

#endif  
