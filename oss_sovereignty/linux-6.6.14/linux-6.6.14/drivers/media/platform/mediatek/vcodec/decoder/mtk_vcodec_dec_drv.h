#ifndef _MTK_VCODEC_DEC_DRV_H_
#define _MTK_VCODEC_DEC_DRV_H_
#include "../common/mtk_vcodec_cmn_drv.h"
#include "../common/mtk_vcodec_dbgfs.h"
#include "../common/mtk_vcodec_fw_priv.h"
#include "../common/mtk_vcodec_util.h"
#include "vdec_msg_queue.h"
#define MTK_VCODEC_DEC_NAME	"mtk-vcodec-dec"
#define IS_VDEC_LAT_ARCH(hw_arch) ((hw_arch) >= MTK_VDEC_LAT_SINGLE_CORE)
#define IS_VDEC_INNER_RACING(capability) ((capability) & MTK_VCODEC_INNER_RACING)
enum mtk_vdec_format_types {
	MTK_VDEC_FORMAT_MM21 = 0x20,
	MTK_VDEC_FORMAT_MT21C = 0x40,
	MTK_VDEC_FORMAT_H264_SLICE = 0x100,
	MTK_VDEC_FORMAT_VP8_FRAME = 0x200,
	MTK_VDEC_FORMAT_VP9_FRAME = 0x400,
	MTK_VDEC_FORMAT_AV1_FRAME = 0x800,
	MTK_VDEC_FORMAT_HEVC_FRAME = 0x1000,
	MTK_VCODEC_INNER_RACING = 0x20000,
	MTK_VDEC_IS_SUPPORT_10BIT = 0x40000,
};
enum mtk_vdec_hw_count {
	MTK_VDEC_NO_HW = 0,
	MTK_VDEC_ONE_CORE,
	MTK_VDEC_ONE_LAT_ONE_CORE,
	MTK_VDEC_MAX_HW_COUNT,
};
enum mtk_vdec_hw_arch {
	MTK_VDEC_PURE_SINGLE_CORE,
	MTK_VDEC_LAT_SINGLE_CORE,
};
struct vdec_pic_info {
	unsigned int pic_w;
	unsigned int pic_h;
	unsigned int buf_w;
	unsigned int buf_h;
	unsigned int fb_sz[VIDEO_MAX_PLANES];
	unsigned int cap_fourcc;
	unsigned int reserved;
};
struct mtk_vcodec_dec_pdata {
	void (*init_vdec_params)(struct mtk_vcodec_dec_ctx *ctx);
	int (*ctrls_setup)(struct mtk_vcodec_dec_ctx *ctx);
	void (*worker)(struct work_struct *work);
	int (*flush_decoder)(struct mtk_vcodec_dec_ctx *ctx);
	struct vdec_fb *(*get_cap_buffer)(struct mtk_vcodec_dec_ctx *ctx);
	void (*cap_to_disp)(struct mtk_vcodec_dec_ctx *ctx, int error,
			    struct media_request *src_buf_req);
	const struct vb2_ops *vdec_vb2_ops;
	const struct mtk_video_fmt *vdec_formats;
	const int *num_formats;
	const struct mtk_video_fmt *default_out_fmt;
	const struct mtk_video_fmt *default_cap_fmt;
	enum mtk_vdec_hw_arch hw_arch;
	bool is_subdev_supported;
	bool uses_stateless_api;
};
struct mtk_vcodec_dec_ctx {
	enum mtk_instance_type type;
	struct mtk_vcodec_dec_dev *dev;
	struct list_head list;
	struct v4l2_fh fh;
	struct v4l2_m2m_ctx *m2m_ctx;
	struct mtk_q_data q_data[2];
	int id;
	enum mtk_instance_state state;
	const struct vdec_common_if *dec_if;
	void *drv_handle;
	struct vdec_pic_info picinfo;
	int dpb_size;
	int int_cond[MTK_VDEC_HW_MAX];
	int int_type[MTK_VDEC_HW_MAX];
	wait_queue_head_t queue[MTK_VDEC_HW_MAX];
	unsigned int irq_status;
	struct v4l2_ctrl_handler ctrl_hdl;
	struct work_struct decode_work;
	struct vdec_pic_info last_decoded_picinfo;
	struct v4l2_m2m_buffer empty_flush_buf;
	bool is_flushing;
	u32 current_codec;
	u32 capture_fourcc;
	enum v4l2_colorspace colorspace;
	enum v4l2_ycbcr_encoding ycbcr_enc;
	enum v4l2_quantization quantization;
	enum v4l2_xfer_func xfer_func;
	int decoded_frame_cnt;
	struct mutex lock;
	int hw_id;
	struct vdec_msg_queue msg_queue;
	void *vpu_inst;
	bool is_10bit_bitstream;
};
struct mtk_vcodec_dec_dev {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd_dec;
	struct media_device mdev_dec;
	struct v4l2_m2m_dev *m2m_dev_dec;
	struct platform_device *plat_dev;
	struct list_head ctx_list;
	struct mtk_vcodec_dec_ctx *curr_ctx;
	void __iomem *reg_base[NUM_MAX_VCODEC_REG_BASE];
	const struct mtk_vcodec_dec_pdata *vdec_pdata;
	struct regmap *vdecsys_regmap;
	struct mtk_vcodec_fw *fw_handler;
	u64 id_counter;
	struct mutex dec_mutex[MTK_VDEC_HW_MAX];
	struct mutex dev_mutex;
	struct workqueue_struct *decode_workqueue;
	spinlock_t irqlock;
	int dec_irq;
	struct mtk_vcodec_pm pm;
	unsigned int dec_capability;
	struct workqueue_struct *core_workqueue;
	void *subdev_dev[MTK_VDEC_HW_MAX];
	int (*subdev_prob_done)(struct mtk_vcodec_dec_dev *vdec_dev);
	DECLARE_BITMAP(subdev_bitmap, MTK_VDEC_HW_MAX);
	atomic_t dec_active_cnt;
	u32 vdec_racing_info[132];
	struct mutex dec_racing_info_mutex;
	struct mtk_vcodec_dbgfs dbgfs;
};
static inline struct mtk_vcodec_dec_ctx *fh_to_dec_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_vcodec_dec_ctx, fh);
}
static inline struct mtk_vcodec_dec_ctx *ctrl_to_dec_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct mtk_vcodec_dec_ctx, ctrl_hdl);
}
static inline void
wake_up_dec_ctx(struct mtk_vcodec_dec_ctx *ctx, unsigned int reason, unsigned int hw_id)
{
	ctx->int_cond[hw_id] = 1;
	ctx->int_type[hw_id] = reason;
	wake_up_interruptible(&ctx->queue[hw_id]);
}
#define mtk_vdec_err(ctx, fmt, args...)                               \
	mtk_vcodec_err((ctx)->id, (ctx)->dev->plat_dev, fmt, ##args)
#define mtk_vdec_debug(ctx, fmt, args...)                             \
	mtk_vcodec_debug((ctx)->id, (ctx)->dev->plat_dev, fmt, ##args)
#define mtk_v4l2_vdec_err(ctx, fmt, args...) mtk_v4l2_err((ctx)->dev->plat_dev, fmt, ##args)
#define mtk_v4l2_vdec_dbg(level, ctx, fmt, args...)             \
	mtk_v4l2_debug((ctx)->dev->plat_dev, level, fmt, ##args)
#endif  
