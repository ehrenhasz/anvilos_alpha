#ifndef _VDEC_DRV_BASE_
#define _VDEC_DRV_BASE_
#include "vdec_drv_if.h"
struct vdec_common_if {
	int (*init)(struct mtk_vcodec_dec_ctx *ctx);
	int (*decode)(void *h_vdec, struct mtk_vcodec_mem *bs,
		      struct vdec_fb *fb, bool *res_chg);
	int (*get_param)(void *h_vdec, enum vdec_get_param_type type,
			 void *out);
	void (*deinit)(void *h_vdec);
};
#endif
