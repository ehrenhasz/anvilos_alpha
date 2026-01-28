#ifndef HANTRO_H_
#define HANTRO_H_
#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/wait.h>
#include <linux/clk.h>
#include <linux/reset.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-mem2mem.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-dma-contig.h>
#include "hantro_hw.h"
struct hantro_ctx;
struct hantro_codec_ops;
struct hantro_postproc_ops;
#define HANTRO_JPEG_ENCODER	BIT(0)
#define HANTRO_ENCODERS		0x0000ffff
#define HANTRO_MPEG2_DECODER	BIT(16)
#define HANTRO_VP8_DECODER	BIT(17)
#define HANTRO_H264_DECODER	BIT(18)
#define HANTRO_HEVC_DECODER	BIT(19)
#define HANTRO_VP9_DECODER	BIT(20)
#define HANTRO_AV1_DECODER	BIT(21)
#define HANTRO_DECODERS		0xffff0000
struct hantro_irq {
	const char *name;
	irqreturn_t (*handler)(int irq, void *priv);
};
struct hantro_variant {
	unsigned int enc_offset;
	unsigned int dec_offset;
	const struct hantro_fmt *enc_fmts;
	unsigned int num_enc_fmts;
	const struct hantro_fmt *dec_fmts;
	unsigned int num_dec_fmts;
	const struct hantro_fmt *postproc_fmts;
	unsigned int num_postproc_fmts;
	const struct hantro_postproc_ops *postproc_ops;
	unsigned int codec;
	const struct hantro_codec_ops *codec_ops;
	int (*init)(struct hantro_dev *vpu);
	int (*runtime_resume)(struct hantro_dev *vpu);
	const struct hantro_irq *irqs;
	int num_irqs;
	const char * const *clk_names;
	int num_clocks;
	const char * const *reg_names;
	int num_regs;
	unsigned int double_buffer : 1;
	unsigned int legacy_regs : 1;
	unsigned int late_postproc : 1;
};
enum hantro_codec_mode {
	HANTRO_MODE_NONE = -1,
	HANTRO_MODE_JPEG_ENC,
	HANTRO_MODE_H264_DEC,
	HANTRO_MODE_MPEG2_DEC,
	HANTRO_MODE_VP8_DEC,
	HANTRO_MODE_HEVC_DEC,
	HANTRO_MODE_VP9_DEC,
	HANTRO_MODE_AV1_DEC,
};
struct hantro_ctrl {
	unsigned int codec;
	struct v4l2_ctrl_config cfg;
};
struct hantro_func {
	unsigned int id;
	struct video_device vdev;
	struct media_pad source_pad;
	struct media_entity sink;
	struct media_pad sink_pad;
	struct media_entity proc;
	struct media_pad proc_pads[2];
	struct media_intf_devnode *intf_devnode;
};
static inline struct hantro_func *
hantro_vdev_to_func(struct video_device *vdev)
{
	return container_of(vdev, struct hantro_func, vdev);
}
struct hantro_dev {
	struct v4l2_device v4l2_dev;
	struct v4l2_m2m_dev *m2m_dev;
	struct media_device mdev;
	struct hantro_func *encoder;
	struct hantro_func *decoder;
	struct platform_device *pdev;
	struct device *dev;
	struct clk_bulk_data *clocks;
	struct reset_control *resets;
	void __iomem **reg_bases;
	void __iomem *enc_base;
	void __iomem *dec_base;
	void __iomem *ctrl_base;
	struct mutex vpu_mutex;	 
	spinlock_t irqlock;
	const struct hantro_variant *variant;
	struct delayed_work watchdog_work;
};
struct hantro_ctx {
	struct hantro_dev *dev;
	struct v4l2_fh fh;
	bool is_encoder;
	u32 sequence_cap;
	u32 sequence_out;
	const struct hantro_fmt *vpu_src_fmt;
	struct v4l2_pix_format_mplane src_fmt;
	const struct hantro_fmt *vpu_dst_fmt;
	struct v4l2_pix_format_mplane dst_fmt;
	struct v4l2_ctrl_handler ctrl_handler;
	int jpeg_quality;
	int bit_depth;
	const struct hantro_codec_ops *codec_ops;
	struct hantro_postproc_ctx postproc;
	bool need_postproc;
	union {
		struct hantro_h264_dec_hw_ctx h264_dec;
		struct hantro_mpeg2_dec_hw_ctx mpeg2_dec;
		struct hantro_vp8_dec_hw_ctx vp8_dec;
		struct hantro_hevc_dec_hw_ctx hevc_dec;
		struct hantro_vp9_dec_hw_ctx vp9_dec;
		struct hantro_av1_dec_hw_ctx av1_dec;
	};
};
struct hantro_fmt {
	char *name;
	u32 fourcc;
	enum hantro_codec_mode codec_mode;
	int header_size;
	int max_depth;
	enum hantro_enc_fmt enc_fmt;
	struct v4l2_frmsize_stepwise frmsize;
	bool postprocessed;
	bool match_depth;
};
struct hantro_reg {
	u32 base;
	u32 shift;
	u32 mask;
};
struct hantro_postproc_regs {
	struct hantro_reg pipeline_en;
	struct hantro_reg max_burst;
	struct hantro_reg clk_gate;
	struct hantro_reg out_swap32;
	struct hantro_reg out_endian;
	struct hantro_reg out_luma_base;
	struct hantro_reg input_width;
	struct hantro_reg input_height;
	struct hantro_reg output_width;
	struct hantro_reg output_height;
	struct hantro_reg input_fmt;
	struct hantro_reg output_fmt;
	struct hantro_reg orig_width;
	struct hantro_reg display_width;
};
struct hantro_vp9_decoded_buffer_info {
	unsigned short width;
	unsigned short height;
	u32 bit_depth : 4;
};
struct hantro_decoded_buffer {
	struct v4l2_m2m_buffer base;
	union {
		struct hantro_vp9_decoded_buffer_info vp9;
	};
};
extern int hantro_debug;
#define vpu_debug(level, fmt, args...)				\
	do {							\
		if (hantro_debug & BIT(level))		\
			pr_info("%s:%d: " fmt,	                \
				 __func__, __LINE__, ##args);	\
	} while (0)
#define vpu_err(fmt, args...)					\
	pr_err("%s:%d: " fmt, __func__, __LINE__, ##args)
static __always_inline struct hantro_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct hantro_ctx, fh);
}
static __always_inline void vepu_write_relaxed(struct hantro_dev *vpu,
					       u32 val, u32 reg)
{
	vpu_debug(6, "0x%04x = 0x%08x\n", reg / 4, val);
	writel_relaxed(val, vpu->enc_base + reg);
}
static __always_inline void vepu_write(struct hantro_dev *vpu, u32 val, u32 reg)
{
	vpu_debug(6, "0x%04x = 0x%08x\n", reg / 4, val);
	writel(val, vpu->enc_base + reg);
}
static __always_inline u32 vepu_read(struct hantro_dev *vpu, u32 reg)
{
	u32 val = readl(vpu->enc_base + reg);
	vpu_debug(6, "0x%04x = 0x%08x\n", reg / 4, val);
	return val;
}
static __always_inline void vdpu_write_relaxed(struct hantro_dev *vpu,
					       u32 val, u32 reg)
{
	vpu_debug(6, "0x%04x = 0x%08x\n", reg / 4, val);
	writel_relaxed(val, vpu->dec_base + reg);
}
static __always_inline void vdpu_write(struct hantro_dev *vpu, u32 val, u32 reg)
{
	vpu_debug(6, "0x%04x = 0x%08x\n", reg / 4, val);
	writel(val, vpu->dec_base + reg);
}
static __always_inline void hantro_write_addr(struct hantro_dev *vpu,
					      unsigned long offset,
					      dma_addr_t addr)
{
	vdpu_write(vpu, addr & 0xffffffff, offset);
}
static __always_inline u32 vdpu_read(struct hantro_dev *vpu, u32 reg)
{
	u32 val = readl(vpu->dec_base + reg);
	vpu_debug(6, "0x%04x = 0x%08x\n", reg / 4, val);
	return val;
}
static __always_inline u32 vdpu_read_mask(struct hantro_dev *vpu,
					  const struct hantro_reg *reg,
					  u32 val)
{
	u32 v;
	v = vdpu_read(vpu, reg->base);
	v &= ~(reg->mask << reg->shift);
	v |= ((val & reg->mask) << reg->shift);
	return v;
}
static __always_inline void hantro_reg_write(struct hantro_dev *vpu,
					     const struct hantro_reg *reg,
					     u32 val)
{
	vdpu_write(vpu, vdpu_read_mask(vpu, reg, val), reg->base);
}
static __always_inline void hantro_reg_write_relaxed(struct hantro_dev *vpu,
						     const struct hantro_reg *reg,
						     u32 val)
{
	vdpu_write_relaxed(vpu, vdpu_read_mask(vpu, reg, val), reg->base);
}
void *hantro_get_ctrl(struct hantro_ctx *ctx, u32 id);
dma_addr_t hantro_get_ref(struct hantro_ctx *ctx, u64 ts);
static inline struct vb2_v4l2_buffer *
hantro_get_src_buf(struct hantro_ctx *ctx)
{
	return v4l2_m2m_next_src_buf(ctx->fh.m2m_ctx);
}
static inline struct vb2_v4l2_buffer *
hantro_get_dst_buf(struct hantro_ctx *ctx)
{
	return v4l2_m2m_next_dst_buf(ctx->fh.m2m_ctx);
}
bool hantro_needs_postproc(const struct hantro_ctx *ctx,
			   const struct hantro_fmt *fmt);
static inline dma_addr_t
hantro_get_dec_buf_addr(struct hantro_ctx *ctx, struct vb2_buffer *vb)
{
	if (hantro_needs_postproc(ctx, ctx->vpu_dst_fmt))
		return ctx->postproc.dec_q[vb->index].dma;
	return vb2_dma_contig_plane_dma_addr(vb, 0);
}
static inline struct hantro_decoded_buffer *
vb2_to_hantro_decoded_buf(struct vb2_buffer *buf)
{
	return container_of(buf, struct hantro_decoded_buffer, base.vb.vb2_buf);
}
void hantro_postproc_disable(struct hantro_ctx *ctx);
void hantro_postproc_enable(struct hantro_ctx *ctx);
void hantro_postproc_free(struct hantro_ctx *ctx);
int hantro_postproc_alloc(struct hantro_ctx *ctx);
int hanto_postproc_enum_framesizes(struct hantro_ctx *ctx,
				   struct v4l2_frmsizeenum *fsize);
#endif  
