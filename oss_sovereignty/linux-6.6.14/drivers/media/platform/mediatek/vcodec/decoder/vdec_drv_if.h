 
 

#ifndef _VDEC_DRV_IF_H_
#define _VDEC_DRV_IF_H_

#include "mtk_vcodec_dec.h"


 
enum vdec_fb_status {
	FB_ST_NORMAL		= 0,
	FB_ST_DISPLAY		= (1 << 0),
	FB_ST_FREE		= (1 << 1)
};

 
enum vdec_get_param_type {
	GET_PARAM_DISP_FRAME_BUFFER,
	GET_PARAM_FREE_FRAME_BUFFER,
	GET_PARAM_PIC_INFO,
	GET_PARAM_CROP_INFO,
	GET_PARAM_DPB_SIZE
};

 
struct vdec_fb_node {
	struct list_head list;
	struct vdec_fb *fb;
};

extern const struct vdec_common_if vdec_h264_if;
extern const struct vdec_common_if vdec_h264_slice_if;
extern const struct vdec_common_if vdec_h264_slice_multi_if;
extern const struct vdec_common_if vdec_vp8_if;
extern const struct vdec_common_if vdec_vp8_slice_if;
extern const struct vdec_common_if vdec_vp9_if;
extern const struct vdec_common_if vdec_vp9_slice_lat_if;
extern const struct vdec_common_if vdec_hevc_slice_multi_if;
extern const struct vdec_common_if vdec_av1_slice_lat_if;

 
int vdec_if_init(struct mtk_vcodec_dec_ctx *ctx, unsigned int fourcc);

 
void vdec_if_deinit(struct mtk_vcodec_dec_ctx *ctx);

 
int vdec_if_decode(struct mtk_vcodec_dec_ctx *ctx, struct mtk_vcodec_mem *bs,
		   struct vdec_fb *fb, bool *res_chg);

 
int vdec_if_get_param(struct mtk_vcodec_dec_ctx *ctx, enum vdec_get_param_type type,
		      void *out);

#endif
