#ifndef _VDEC_VPU_IF_H_
#define _VDEC_VPU_IF_H_
struct mtk_vcodec_dec_ctx;
struct vdec_vpu_inst {
	int id;
	int core_id;
	void *vsi;
	int32_t failure;
	uint32_t inst_addr;
	uint32_t fw_abi_version;
	uint32_t inst_id;
	unsigned int signaled;
	struct mtk_vcodec_dec_ctx *ctx;
	wait_queue_head_t wq;
	mtk_vcodec_ipi_handler handler;
	unsigned int codec_type;
	unsigned int capture_type;
	unsigned int fb_sz[2];
};
int vpu_dec_init(struct vdec_vpu_inst *vpu);
int vpu_dec_start(struct vdec_vpu_inst *vpu, uint32_t *data, unsigned int len);
int vpu_dec_end(struct vdec_vpu_inst *vpu);
int vpu_dec_deinit(struct vdec_vpu_inst *vpu);
int vpu_dec_reset(struct vdec_vpu_inst *vpu);
int vpu_dec_core(struct vdec_vpu_inst *vpu);
int vpu_dec_core_end(struct vdec_vpu_inst *vpu);
int vpu_dec_get_param(struct vdec_vpu_inst *vpu, uint32_t *data,
		      unsigned int len, unsigned int param_type);
#endif
