 
 

#ifndef _VENC_VPU_IF_H_
#define _VENC_VPU_IF_H_

#include "venc_drv_if.h"

 
struct venc_vpu_inst {
	wait_queue_head_t wq_hd;
	int signaled;
	int failure;
	int state;
	int bs_size;
	int is_key_frm;
	unsigned int inst_addr;
	void *vsi;
	int id;
	struct mtk_vcodec_enc_ctx *ctx;
};

int vpu_enc_init(struct venc_vpu_inst *vpu);
int vpu_enc_set_param(struct venc_vpu_inst *vpu,
		      enum venc_set_param_type id,
		      struct venc_enc_param *param);
int vpu_enc_encode(struct venc_vpu_inst *vpu, unsigned int bs_mode,
		   struct venc_frm_buf *frm_buf,
		   struct mtk_vcodec_mem *bs_buf,
		   struct venc_frame_info *frame_info);
int vpu_enc_deinit(struct venc_vpu_inst *vpu);

#endif
