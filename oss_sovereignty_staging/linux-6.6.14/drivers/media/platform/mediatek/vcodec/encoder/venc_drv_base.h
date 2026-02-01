 
 

#ifndef _VENC_DRV_BASE_
#define _VENC_DRV_BASE_

#include "mtk_vcodec_enc_drv.h"

#include "venc_drv_if.h"

struct venc_common_if {
	 
	int (*init)(struct mtk_vcodec_enc_ctx *ctx);

	 
	int (*encode)(void *handle, enum venc_start_opt opt,
		      struct venc_frm_buf *frm_buf,
		      struct mtk_vcodec_mem *bs_buf,
		      struct venc_done_result *result);

	 
	int (*set_param)(void *handle, enum venc_set_param_type type,
			 struct venc_enc_param *in);

	 
	int (*deinit)(void *handle);
};

#endif
