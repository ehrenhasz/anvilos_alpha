 
 

#ifndef _MTK_VCODEC_ENC_DRV_H_
#define _MTK_VCODEC_ENC_DRV_H_

#include "../common/mtk_vcodec_cmn_drv.h"
#include "../common/mtk_vcodec_dbgfs.h"
#include "../common/mtk_vcodec_fw_priv.h"
#include "../common/mtk_vcodec_util.h"

#define MTK_VCODEC_ENC_NAME	"mtk-vcodec-enc"

#define MTK_ENC_CTX_IS_EXT(ctx) ((ctx)->dev->venc_pdata->uses_ext)
#define MTK_ENC_IOVA_IS_34BIT(ctx) ((ctx)->dev->venc_pdata->uses_34bit)

 
struct mtk_vcodec_enc_pdata {
	bool uses_ext;
	u64 min_bitrate;
	u64 max_bitrate;
	const struct mtk_video_fmt *capture_formats;
	size_t num_capture_formats;
	const struct mtk_video_fmt *output_formats;
	size_t num_output_formats;
	u8 core_id;
	bool uses_34bit;
};

 
enum mtk_encode_param {
	MTK_ENCODE_PARAM_NONE = 0,
	MTK_ENCODE_PARAM_BITRATE = (1 << 0),
	MTK_ENCODE_PARAM_FRAMERATE = (1 << 1),
	MTK_ENCODE_PARAM_INTRA_PERIOD = (1 << 2),
	MTK_ENCODE_PARAM_FORCE_INTRA = (1 << 3),
	MTK_ENCODE_PARAM_GOP_SIZE = (1 << 4),
};

 
struct mtk_enc_params {
	unsigned int	bitrate;
	unsigned int	num_b_frame;
	unsigned int	rc_frame;
	unsigned int	rc_mb;
	unsigned int	seq_hdr_mode;
	unsigned int	intra_period;
	unsigned int	gop_size;
	unsigned int	framerate_num;
	unsigned int	framerate_denom;
	unsigned int	h264_max_qp;
	unsigned int	h264_profile;
	unsigned int	h264_level;
	unsigned int	force_intra;
};

 
struct mtk_vcodec_enc_ctx {
	enum mtk_instance_type type;
	struct mtk_vcodec_enc_dev *dev;
	struct list_head list;

	struct v4l2_fh fh;
	struct v4l2_m2m_ctx *m2m_ctx;
	struct mtk_q_data q_data[2];
	int id;
	enum mtk_instance_state state;
	enum mtk_encode_param param_change;
	struct mtk_enc_params enc_params;

	const struct venc_common_if *enc_if;
	void *drv_handle;

	int int_cond[MTK_VDEC_HW_MAX];
	int int_type[MTK_VDEC_HW_MAX];
	wait_queue_head_t queue[MTK_VDEC_HW_MAX];
	unsigned int irq_status;

	struct v4l2_ctrl_handler ctrl_hdl;
	struct work_struct encode_work;
	struct v4l2_m2m_buffer empty_flush_buf;
	bool is_flushing;

	enum v4l2_colorspace colorspace;
	enum v4l2_ycbcr_encoding ycbcr_enc;
	enum v4l2_quantization quantization;
	enum v4l2_xfer_func xfer_func;

	struct mutex q_mutex;
	void *vpu_inst;
};

 
struct mtk_vcodec_enc_dev {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd_enc;

	struct v4l2_m2m_dev *m2m_dev_enc;
	struct platform_device *plat_dev;
	struct list_head ctx_list;
	struct mtk_vcodec_enc_ctx *curr_ctx;

	void __iomem *reg_base[NUM_MAX_VCODEC_REG_BASE];
	const struct mtk_vcodec_enc_pdata *venc_pdata;

	struct mtk_vcodec_fw *fw_handler;
	u64 id_counter;

	 
	struct mutex enc_mutex;
	struct mutex dev_mutex;
	struct workqueue_struct *encode_workqueue;

	int enc_irq;
	spinlock_t irqlock;

	struct mtk_vcodec_pm pm;
	unsigned int enc_capability;
	struct mtk_vcodec_dbgfs dbgfs;
};

static inline struct mtk_vcodec_enc_ctx *fh_to_enc_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_vcodec_enc_ctx, fh);
}

static inline struct mtk_vcodec_enc_ctx *ctrl_to_enc_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct mtk_vcodec_enc_ctx, ctrl_hdl);
}

 
static inline void
wake_up_enc_ctx(struct mtk_vcodec_enc_ctx *ctx, unsigned int reason, unsigned int hw_id)
{
	ctx->int_cond[hw_id] = 1;
	ctx->int_type[hw_id] = reason;
	wake_up_interruptible(&ctx->queue[hw_id]);
}

#define mtk_venc_err(ctx, fmt, args...)                               \
	mtk_vcodec_err((ctx)->id, (ctx)->dev->plat_dev, fmt, ##args)

#define mtk_venc_debug(ctx, fmt, args...)                              \
	mtk_vcodec_debug((ctx)->id, (ctx)->dev->plat_dev, fmt, ##args)

#define mtk_v4l2_venc_err(ctx, fmt, args...) mtk_v4l2_err((ctx)->dev->plat_dev, fmt, ##args)

#define mtk_v4l2_venc_dbg(level, ctx, fmt, args...)             \
	mtk_v4l2_debug((ctx)->dev->plat_dev, level, fmt, ##args)

#endif  
