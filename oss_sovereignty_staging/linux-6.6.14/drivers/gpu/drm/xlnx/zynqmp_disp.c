
 

#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_plane.h>

#include <linux/clk.h>
#include <linux/dma/xilinx_dpdma.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "zynqmp_disp.h"
#include "zynqmp_disp_regs.h"
#include "zynqmp_dp.h"
#include "zynqmp_dpsub.h"

 

#define ZYNQMP_DISP_AV_BUF_NUM_VID_GFX_BUFFERS		4
#define ZYNQMP_DISP_AV_BUF_NUM_BUFFERS			6

#define ZYNQMP_DISP_MAX_NUM_SUB_PLANES			3

 
struct zynqmp_disp_format {
	u32 drm_fmt;
	u32 buf_fmt;
	bool swap;
	const u32 *sf;
};

 
struct zynqmp_disp_layer_dma {
	struct dma_chan *chan;
	struct dma_interleaved_template xt;
	struct data_chunk sgl;
};

 
struct zynqmp_disp_layer_info {
	const struct zynqmp_disp_format *formats;
	unsigned int num_formats;
	unsigned int num_channels;
};

 
struct zynqmp_disp_layer {
	enum zynqmp_dpsub_layer_id id;
	struct zynqmp_disp *disp;
	const struct zynqmp_disp_layer_info *info;

	struct zynqmp_disp_layer_dma dmas[ZYNQMP_DISP_MAX_NUM_SUB_PLANES];

	const struct zynqmp_disp_format *disp_fmt;
	const struct drm_format_info *drm_fmt;
	enum zynqmp_dpsub_layer_mode mode;
};

 
struct zynqmp_disp {
	struct device *dev;
	struct zynqmp_dpsub *dpsub;

	struct {
		void __iomem *base;
	} blend;
	struct {
		void __iomem *base;
	} avbuf;
	struct {
		void __iomem *base;
	} audio;

	struct zynqmp_disp_layer layers[ZYNQMP_DPSUB_NUM_LAYERS];
};

 

static const u32 scaling_factors_444[] = {
	ZYNQMP_DISP_AV_BUF_4BIT_SF,
	ZYNQMP_DISP_AV_BUF_4BIT_SF,
	ZYNQMP_DISP_AV_BUF_4BIT_SF,
};

static const u32 scaling_factors_555[] = {
	ZYNQMP_DISP_AV_BUF_5BIT_SF,
	ZYNQMP_DISP_AV_BUF_5BIT_SF,
	ZYNQMP_DISP_AV_BUF_5BIT_SF,
};

static const u32 scaling_factors_565[] = {
	ZYNQMP_DISP_AV_BUF_5BIT_SF,
	ZYNQMP_DISP_AV_BUF_6BIT_SF,
	ZYNQMP_DISP_AV_BUF_5BIT_SF,
};

static const u32 scaling_factors_888[] = {
	ZYNQMP_DISP_AV_BUF_8BIT_SF,
	ZYNQMP_DISP_AV_BUF_8BIT_SF,
	ZYNQMP_DISP_AV_BUF_8BIT_SF,
};

static const u32 scaling_factors_101010[] = {
	ZYNQMP_DISP_AV_BUF_10BIT_SF,
	ZYNQMP_DISP_AV_BUF_10BIT_SF,
	ZYNQMP_DISP_AV_BUF_10BIT_SF,
};

 
static const struct zynqmp_disp_format avbuf_vid_fmts[] = {
	{
		.drm_fmt	= DRM_FORMAT_VYUY,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_VYUY,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_UYVY,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_VYUY,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_YUYV,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YUYV,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_YVYU,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YUYV,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_YUV422,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV16,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_YVU422,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV16,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_YUV444,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV24,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_YVU444,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV24,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_NV16,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV16CI,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_NV61,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV16CI,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_BGR888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_RGB888,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_RGB888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_RGB888,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_XBGR8888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_RGBA8880,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_XRGB8888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_RGBA8880,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_XBGR2101010,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_RGB888_10,
		.swap		= false,
		.sf		= scaling_factors_101010,
	}, {
		.drm_fmt	= DRM_FORMAT_XRGB2101010,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_RGB888_10,
		.swap		= true,
		.sf		= scaling_factors_101010,
	}, {
		.drm_fmt	= DRM_FORMAT_YUV420,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV16_420,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_YVU420,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV16_420,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_NV12,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV16CI_420,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_NV21,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_VID_YV16CI_420,
		.swap		= true,
		.sf		= scaling_factors_888,
	},
};

 
static const struct zynqmp_disp_format avbuf_gfx_fmts[] = {
	{
		.drm_fmt	= DRM_FORMAT_ABGR8888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_RGBA8888,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_ARGB8888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_RGBA8888,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_RGBA8888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_ABGR8888,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_BGRA8888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_ABGR8888,
		.swap		= true,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_BGR888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_RGB888,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_RGB888,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_BGR888,
		.swap		= false,
		.sf		= scaling_factors_888,
	}, {
		.drm_fmt	= DRM_FORMAT_RGBA5551,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_RGBA5551,
		.swap		= false,
		.sf		= scaling_factors_555,
	}, {
		.drm_fmt	= DRM_FORMAT_BGRA5551,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_RGBA5551,
		.swap		= true,
		.sf		= scaling_factors_555,
	}, {
		.drm_fmt	= DRM_FORMAT_RGBA4444,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_RGBA4444,
		.swap		= false,
		.sf		= scaling_factors_444,
	}, {
		.drm_fmt	= DRM_FORMAT_BGRA4444,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_RGBA4444,
		.swap		= true,
		.sf		= scaling_factors_444,
	}, {
		.drm_fmt	= DRM_FORMAT_RGB565,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_RGB565,
		.swap		= false,
		.sf		= scaling_factors_565,
	}, {
		.drm_fmt	= DRM_FORMAT_BGR565,
		.buf_fmt	= ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_RGB565,
		.swap		= true,
		.sf		= scaling_factors_565,
	},
};

static u32 zynqmp_disp_avbuf_read(struct zynqmp_disp *disp, int reg)
{
	return readl(disp->avbuf.base + reg);
}

static void zynqmp_disp_avbuf_write(struct zynqmp_disp *disp, int reg, u32 val)
{
	writel(val, disp->avbuf.base + reg);
}

static bool zynqmp_disp_layer_is_video(const struct zynqmp_disp_layer *layer)
{
	return layer->id == ZYNQMP_DPSUB_LAYER_VID;
}

 
static void zynqmp_disp_avbuf_set_format(struct zynqmp_disp *disp,
					 struct zynqmp_disp_layer *layer,
					 const struct zynqmp_disp_format *fmt)
{
	unsigned int i;
	u32 val;

	val = zynqmp_disp_avbuf_read(disp, ZYNQMP_DISP_AV_BUF_FMT);
	val &= zynqmp_disp_layer_is_video(layer)
	    ? ~ZYNQMP_DISP_AV_BUF_FMT_NL_VID_MASK
	    : ~ZYNQMP_DISP_AV_BUF_FMT_NL_GFX_MASK;
	val |= fmt->buf_fmt;
	zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_FMT, val);

	for (i = 0; i < ZYNQMP_DISP_AV_BUF_NUM_SF; i++) {
		unsigned int reg = zynqmp_disp_layer_is_video(layer)
				 ? ZYNQMP_DISP_AV_BUF_VID_COMP_SF(i)
				 : ZYNQMP_DISP_AV_BUF_GFX_COMP_SF(i);

		zynqmp_disp_avbuf_write(disp, reg, fmt->sf[i]);
	}
}

 
static void
zynqmp_disp_avbuf_set_clocks_sources(struct zynqmp_disp *disp,
				     bool video_from_ps, bool audio_from_ps,
				     bool timings_internal)
{
	u32 val = 0;

	if (video_from_ps)
		val |= ZYNQMP_DISP_AV_BUF_CLK_SRC_VID_FROM_PS;
	if (audio_from_ps)
		val |= ZYNQMP_DISP_AV_BUF_CLK_SRC_AUD_FROM_PS;
	if (timings_internal)
		val |= ZYNQMP_DISP_AV_BUF_CLK_SRC_VID_INTERNAL_TIMING;

	zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_CLK_SRC, val);
}

 
static void zynqmp_disp_avbuf_enable_channels(struct zynqmp_disp *disp)
{
	unsigned int i;
	u32 val;

	val = ZYNQMP_DISP_AV_BUF_CHBUF_EN |
	      (ZYNQMP_DISP_AV_BUF_CHBUF_BURST_LEN_MAX <<
	       ZYNQMP_DISP_AV_BUF_CHBUF_BURST_LEN_SHIFT);

	for (i = 0; i < ZYNQMP_DISP_AV_BUF_NUM_VID_GFX_BUFFERS; i++)
		zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_CHBUF(i),
					val);

	val = ZYNQMP_DISP_AV_BUF_CHBUF_EN |
	      (ZYNQMP_DISP_AV_BUF_CHBUF_BURST_LEN_AUD_MAX <<
	       ZYNQMP_DISP_AV_BUF_CHBUF_BURST_LEN_SHIFT);

	for (; i < ZYNQMP_DISP_AV_BUF_NUM_BUFFERS; i++)
		zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_CHBUF(i),
					val);
}

 
static void zynqmp_disp_avbuf_disable_channels(struct zynqmp_disp *disp)
{
	unsigned int i;

	for (i = 0; i < ZYNQMP_DISP_AV_BUF_NUM_BUFFERS; i++)
		zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_CHBUF(i),
					ZYNQMP_DISP_AV_BUF_CHBUF_FLUSH);
}

 
static void zynqmp_disp_avbuf_enable_audio(struct zynqmp_disp *disp)
{
	u32 val;

	val = zynqmp_disp_avbuf_read(disp, ZYNQMP_DISP_AV_BUF_OUTPUT);
	val &= ~ZYNQMP_DISP_AV_BUF_OUTPUT_AUD1_MASK;
	val |= ZYNQMP_DISP_AV_BUF_OUTPUT_AUD1_MEM;
	val |= ZYNQMP_DISP_AV_BUF_OUTPUT_AUD2_EN;
	zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_OUTPUT, val);
}

 
static void zynqmp_disp_avbuf_disable_audio(struct zynqmp_disp *disp)
{
	u32 val;

	val = zynqmp_disp_avbuf_read(disp, ZYNQMP_DISP_AV_BUF_OUTPUT);
	val &= ~ZYNQMP_DISP_AV_BUF_OUTPUT_AUD1_MASK;
	val |= ZYNQMP_DISP_AV_BUF_OUTPUT_AUD1_DISABLE;
	val &= ~ZYNQMP_DISP_AV_BUF_OUTPUT_AUD2_EN;
	zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_OUTPUT, val);
}

 
static void zynqmp_disp_avbuf_enable_video(struct zynqmp_disp *disp,
					   struct zynqmp_disp_layer *layer)
{
	u32 val;

	val = zynqmp_disp_avbuf_read(disp, ZYNQMP_DISP_AV_BUF_OUTPUT);
	if (zynqmp_disp_layer_is_video(layer)) {
		val &= ~ZYNQMP_DISP_AV_BUF_OUTPUT_VID1_MASK;
		if (layer->mode == ZYNQMP_DPSUB_LAYER_NONLIVE)
			val |= ZYNQMP_DISP_AV_BUF_OUTPUT_VID1_MEM;
		else
			val |= ZYNQMP_DISP_AV_BUF_OUTPUT_VID1_LIVE;
	} else {
		val &= ~ZYNQMP_DISP_AV_BUF_OUTPUT_VID2_MASK;
		val |= ZYNQMP_DISP_AV_BUF_OUTPUT_VID2_MEM;
		if (layer->mode == ZYNQMP_DPSUB_LAYER_NONLIVE)
			val |= ZYNQMP_DISP_AV_BUF_OUTPUT_VID2_MEM;
		else
			val |= ZYNQMP_DISP_AV_BUF_OUTPUT_VID2_LIVE;
	}
	zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_OUTPUT, val);
}

 
static void zynqmp_disp_avbuf_disable_video(struct zynqmp_disp *disp,
					    struct zynqmp_disp_layer *layer)
{
	u32 val;

	val = zynqmp_disp_avbuf_read(disp, ZYNQMP_DISP_AV_BUF_OUTPUT);
	if (zynqmp_disp_layer_is_video(layer)) {
		val &= ~ZYNQMP_DISP_AV_BUF_OUTPUT_VID1_MASK;
		val |= ZYNQMP_DISP_AV_BUF_OUTPUT_VID1_NONE;
	} else {
		val &= ~ZYNQMP_DISP_AV_BUF_OUTPUT_VID2_MASK;
		val |= ZYNQMP_DISP_AV_BUF_OUTPUT_VID2_DISABLE;
	}
	zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_OUTPUT, val);
}

 
static void zynqmp_disp_avbuf_enable(struct zynqmp_disp *disp)
{
	zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_SRST_REG, 0);
}

 
static void zynqmp_disp_avbuf_disable(struct zynqmp_disp *disp)
{
	zynqmp_disp_avbuf_write(disp, ZYNQMP_DISP_AV_BUF_SRST_REG,
				ZYNQMP_DISP_AV_BUF_SRST_REG_VID_RST);
}

 

static void zynqmp_disp_blend_write(struct zynqmp_disp *disp, int reg, u32 val)
{
	writel(val, disp->blend.base + reg);
}

 
static const u16 csc_zero_matrix[] = {
	0x0,    0x0,    0x0,
	0x0,    0x0,    0x0,
	0x0,    0x0,    0x0
};

static const u16 csc_identity_matrix[] = {
	0x1000, 0x0,    0x0,
	0x0,    0x1000, 0x0,
	0x0,    0x0,    0x1000
};

static const u32 csc_zero_offsets[] = {
	0, 0, 0
};

static const u16 csc_rgb_to_sdtv_matrix[] = {
	0x4c9,  0x864,  0x1d3,
	0x7d4d, 0x7ab3, 0x800,
	0x800,  0x794d, 0x7eb3
};

static const u32 csc_rgb_to_sdtv_offsets[] = {
	0x0, 0x8000000, 0x8000000
};

static const u16 csc_sdtv_to_rgb_matrix[] = {
	0x1000, 0x166f, 0x0,
	0x1000, 0x7483, 0x7a7f,
	0x1000, 0x0,    0x1c5a
};

static const u32 csc_sdtv_to_rgb_offsets[] = {
	0x0, 0x1800, 0x1800
};

 
static void zynqmp_disp_blend_set_output_format(struct zynqmp_disp *disp,
						enum zynqmp_dpsub_format format)
{
	static const unsigned int blend_output_fmts[] = {
		[ZYNQMP_DPSUB_FORMAT_RGB] = ZYNQMP_DISP_V_BLEND_OUTPUT_VID_FMT_RGB,
		[ZYNQMP_DPSUB_FORMAT_YCRCB444] = ZYNQMP_DISP_V_BLEND_OUTPUT_VID_FMT_YCBCR444,
		[ZYNQMP_DPSUB_FORMAT_YCRCB422] = ZYNQMP_DISP_V_BLEND_OUTPUT_VID_FMT_YCBCR422
					       | ZYNQMP_DISP_V_BLEND_OUTPUT_VID_FMT_EN_DOWNSAMPLE,
		[ZYNQMP_DPSUB_FORMAT_YONLY] = ZYNQMP_DISP_V_BLEND_OUTPUT_VID_FMT_YONLY,
	};

	u32 fmt = blend_output_fmts[format];
	const u16 *coeffs;
	const u32 *offsets;
	unsigned int i;

	zynqmp_disp_blend_write(disp, ZYNQMP_DISP_V_BLEND_OUTPUT_VID_FMT, fmt);
	if (fmt == ZYNQMP_DISP_V_BLEND_OUTPUT_VID_FMT_RGB) {
		coeffs = csc_identity_matrix;
		offsets = csc_zero_offsets;
	} else {
		coeffs = csc_rgb_to_sdtv_matrix;
		offsets = csc_rgb_to_sdtv_offsets;
	}

	for (i = 0; i < ZYNQMP_DISP_V_BLEND_NUM_COEFF; i++)
		zynqmp_disp_blend_write(disp,
					ZYNQMP_DISP_V_BLEND_RGB2YCBCR_COEFF(i),
					coeffs[i]);

	for (i = 0; i < ZYNQMP_DISP_V_BLEND_NUM_OFFSET; i++)
		zynqmp_disp_blend_write(disp,
					ZYNQMP_DISP_V_BLEND_OUTCSC_OFFSET(i),
					offsets[i]);
}

 
static void zynqmp_disp_blend_set_bg_color(struct zynqmp_disp *disp,
					   u32 rcr, u32 gy, u32 bcb)
{
	zynqmp_disp_blend_write(disp, ZYNQMP_DISP_V_BLEND_BG_CLR_0, rcr);
	zynqmp_disp_blend_write(disp, ZYNQMP_DISP_V_BLEND_BG_CLR_1, gy);
	zynqmp_disp_blend_write(disp, ZYNQMP_DISP_V_BLEND_BG_CLR_2, bcb);
}

 
void zynqmp_disp_blend_set_global_alpha(struct zynqmp_disp *disp,
					bool enable, u32 alpha)
{
	zynqmp_disp_blend_write(disp, ZYNQMP_DISP_V_BLEND_SET_GLOBAL_ALPHA,
				ZYNQMP_DISP_V_BLEND_SET_GLOBAL_ALPHA_VALUE(alpha) |
				(enable ? ZYNQMP_DISP_V_BLEND_SET_GLOBAL_ALPHA_EN : 0));
}

 
static void zynqmp_disp_blend_layer_set_csc(struct zynqmp_disp *disp,
					    struct zynqmp_disp_layer *layer,
					    const u16 *coeffs,
					    const u32 *offsets)
{
	unsigned int swap[3] = { 0, 1, 2 };
	unsigned int reg;
	unsigned int i;

	if (layer->disp_fmt->swap) {
		if (layer->drm_fmt->is_yuv) {
			 
			swap[1] = 2;
			swap[2] = 1;
		} else {
			 
			swap[0] = 2;
			swap[2] = 0;
		}
	}

	if (zynqmp_disp_layer_is_video(layer))
		reg = ZYNQMP_DISP_V_BLEND_IN1CSC_COEFF(0);
	else
		reg = ZYNQMP_DISP_V_BLEND_IN2CSC_COEFF(0);

	for (i = 0; i < ZYNQMP_DISP_V_BLEND_NUM_COEFF; i += 3, reg += 12) {
		zynqmp_disp_blend_write(disp, reg + 0, coeffs[i + swap[0]]);
		zynqmp_disp_blend_write(disp, reg + 4, coeffs[i + swap[1]]);
		zynqmp_disp_blend_write(disp, reg + 8, coeffs[i + swap[2]]);
	}

	if (zynqmp_disp_layer_is_video(layer))
		reg = ZYNQMP_DISP_V_BLEND_IN1CSC_OFFSET(0);
	else
		reg = ZYNQMP_DISP_V_BLEND_IN2CSC_OFFSET(0);

	for (i = 0; i < ZYNQMP_DISP_V_BLEND_NUM_OFFSET; i++)
		zynqmp_disp_blend_write(disp, reg + i * 4, offsets[i]);
}

 
static void zynqmp_disp_blend_layer_enable(struct zynqmp_disp *disp,
					   struct zynqmp_disp_layer *layer)
{
	const u16 *coeffs;
	const u32 *offsets;
	u32 val;

	val = (layer->drm_fmt->is_yuv ?
	       0 : ZYNQMP_DISP_V_BLEND_LAYER_CONTROL_RGB) |
	      (layer->drm_fmt->hsub > 1 ?
	       ZYNQMP_DISP_V_BLEND_LAYER_CONTROL_EN_US : 0);

	zynqmp_disp_blend_write(disp,
				ZYNQMP_DISP_V_BLEND_LAYER_CONTROL(layer->id),
				val);

	if (layer->drm_fmt->is_yuv) {
		coeffs = csc_sdtv_to_rgb_matrix;
		offsets = csc_sdtv_to_rgb_offsets;
	} else {
		coeffs = csc_identity_matrix;
		offsets = csc_zero_offsets;
	}

	zynqmp_disp_blend_layer_set_csc(disp, layer, coeffs, offsets);
}

 
static void zynqmp_disp_blend_layer_disable(struct zynqmp_disp *disp,
					    struct zynqmp_disp_layer *layer)
{
	zynqmp_disp_blend_write(disp,
				ZYNQMP_DISP_V_BLEND_LAYER_CONTROL(layer->id),
				0);

	zynqmp_disp_blend_layer_set_csc(disp, layer, csc_zero_matrix,
					csc_zero_offsets);
}

 

static void zynqmp_disp_audio_write(struct zynqmp_disp *disp, int reg, u32 val)
{
	writel(val, disp->audio.base + reg);
}

 
static void zynqmp_disp_audio_enable(struct zynqmp_disp *disp)
{
	 
	zynqmp_disp_audio_write(disp, ZYNQMP_DISP_AUD_SOFT_RESET, 0);
	zynqmp_disp_audio_write(disp, ZYNQMP_DISP_AUD_MIXER_VOLUME,
				ZYNQMP_DISP_AUD_MIXER_VOLUME_NO_SCALE);
}

 
static void zynqmp_disp_audio_disable(struct zynqmp_disp *disp)
{
	zynqmp_disp_audio_write(disp, ZYNQMP_DISP_AUD_SOFT_RESET,
				ZYNQMP_DISP_AUD_SOFT_RESET_AUD_SRST);
}

 

 
static const struct zynqmp_disp_format *
zynqmp_disp_layer_find_format(struct zynqmp_disp_layer *layer,
			      u32 drm_fmt)
{
	unsigned int i;

	for (i = 0; i < layer->info->num_formats; i++) {
		if (layer->info->formats[i].drm_fmt == drm_fmt)
			return &layer->info->formats[i];
	}

	return NULL;
}

 
u32 *zynqmp_disp_layer_drm_formats(struct zynqmp_disp_layer *layer,
				   unsigned int *num_formats)
{
	unsigned int i;
	u32 *formats;

	formats = kcalloc(layer->info->num_formats, sizeof(*formats),
			  GFP_KERNEL);
	if (!formats)
		return NULL;

	for (i = 0; i < layer->info->num_formats; ++i)
		formats[i] = layer->info->formats[i].drm_fmt;

	*num_formats = layer->info->num_formats;
	return formats;
}

 
void zynqmp_disp_layer_enable(struct zynqmp_disp_layer *layer,
			      enum zynqmp_dpsub_layer_mode mode)
{
	layer->mode = mode;
	zynqmp_disp_avbuf_enable_video(layer->disp, layer);
	zynqmp_disp_blend_layer_enable(layer->disp, layer);
}

 
void zynqmp_disp_layer_disable(struct zynqmp_disp_layer *layer)
{
	unsigned int i;

	if (layer->disp->dpsub->dma_enabled) {
		for (i = 0; i < layer->drm_fmt->num_planes; i++)
			dmaengine_terminate_sync(layer->dmas[i].chan);
	}

	zynqmp_disp_avbuf_disable_video(layer->disp, layer);
	zynqmp_disp_blend_layer_disable(layer->disp, layer);
}

 
void zynqmp_disp_layer_set_format(struct zynqmp_disp_layer *layer,
				  const struct drm_format_info *info)
{
	unsigned int i;

	layer->disp_fmt = zynqmp_disp_layer_find_format(layer, info->format);
	layer->drm_fmt = info;

	zynqmp_disp_avbuf_set_format(layer->disp, layer, layer->disp_fmt);

	if (!layer->disp->dpsub->dma_enabled)
		return;

	 
	for (i = 0; i < info->num_planes; i++) {
		struct zynqmp_disp_layer_dma *dma = &layer->dmas[i];
		struct xilinx_dpdma_peripheral_config pconfig = {
			.video_group = true,
		};
		struct dma_slave_config config = {
			.direction = DMA_MEM_TO_DEV,
			.peripheral_config = &pconfig,
			.peripheral_size = sizeof(pconfig),
		};

		dmaengine_slave_config(dma->chan, &config);
	}
}

 
int zynqmp_disp_layer_update(struct zynqmp_disp_layer *layer,
			     struct drm_plane_state *state)
{
	const struct drm_format_info *info = layer->drm_fmt;
	unsigned int i;

	if (!layer->disp->dpsub->dma_enabled)
		return 0;

	for (i = 0; i < info->num_planes; i++) {
		unsigned int width = state->crtc_w / (i ? info->hsub : 1);
		unsigned int height = state->crtc_h / (i ? info->vsub : 1);
		struct zynqmp_disp_layer_dma *dma = &layer->dmas[i];
		struct dma_async_tx_descriptor *desc;
		dma_addr_t dma_addr;

		dma_addr = drm_fb_dma_get_gem_addr(state->fb, state, i);

		dma->xt.numf = height;
		dma->sgl.size = width * info->cpp[i];
		dma->sgl.icg = state->fb->pitches[i] - dma->sgl.size;
		dma->xt.src_start = dma_addr;
		dma->xt.frame_size = 1;
		dma->xt.dir = DMA_MEM_TO_DEV;
		dma->xt.src_sgl = true;
		dma->xt.dst_sgl = false;

		desc = dmaengine_prep_interleaved_dma(dma->chan, &dma->xt,
						      DMA_CTRL_ACK |
						      DMA_PREP_REPEAT |
						      DMA_PREP_LOAD_EOT);
		if (!desc) {
			dev_err(layer->disp->dev,
				"failed to prepare DMA descriptor\n");
			return -ENOMEM;
		}

		dmaengine_submit(desc);
		dma_async_issue_pending(dma->chan);
	}

	return 0;
}

 
static void zynqmp_disp_layer_release_dma(struct zynqmp_disp *disp,
					  struct zynqmp_disp_layer *layer)
{
	unsigned int i;

	if (!layer->info || !disp->dpsub->dma_enabled)
		return;

	for (i = 0; i < layer->info->num_channels; i++) {
		struct zynqmp_disp_layer_dma *dma = &layer->dmas[i];

		if (!dma->chan)
			continue;

		 
		dmaengine_terminate_sync(dma->chan);
		dma_release_channel(dma->chan);
	}
}

 
static void zynqmp_disp_destroy_layers(struct zynqmp_disp *disp)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(disp->layers); i++)
		zynqmp_disp_layer_release_dma(disp, &disp->layers[i]);
}

 
static int zynqmp_disp_layer_request_dma(struct zynqmp_disp *disp,
					 struct zynqmp_disp_layer *layer)
{
	static const char * const dma_names[] = { "vid", "gfx" };
	unsigned int i;
	int ret;

	if (!disp->dpsub->dma_enabled)
		return 0;

	for (i = 0; i < layer->info->num_channels; i++) {
		struct zynqmp_disp_layer_dma *dma = &layer->dmas[i];
		char dma_channel_name[16];

		snprintf(dma_channel_name, sizeof(dma_channel_name),
			 "%s%u", dma_names[layer->id], i);
		dma->chan = dma_request_chan(disp->dev, dma_channel_name);
		if (IS_ERR(dma->chan)) {
			ret = dev_err_probe(disp->dev, PTR_ERR(dma->chan),
					    "failed to request dma channel\n");
			dma->chan = NULL;
			return ret;
		}
	}

	return 0;
}

 
static int zynqmp_disp_create_layers(struct zynqmp_disp *disp)
{
	static const struct zynqmp_disp_layer_info layer_info[] = {
		[ZYNQMP_DPSUB_LAYER_VID] = {
			.formats = avbuf_vid_fmts,
			.num_formats = ARRAY_SIZE(avbuf_vid_fmts),
			.num_channels = 3,
		},
		[ZYNQMP_DPSUB_LAYER_GFX] = {
			.formats = avbuf_gfx_fmts,
			.num_formats = ARRAY_SIZE(avbuf_gfx_fmts),
			.num_channels = 1,
		},
	};

	unsigned int i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(disp->layers); i++) {
		struct zynqmp_disp_layer *layer = &disp->layers[i];

		layer->id = i;
		layer->disp = disp;
		layer->info = &layer_info[i];

		ret = zynqmp_disp_layer_request_dma(disp, layer);
		if (ret)
			goto err;

		disp->dpsub->layers[i] = layer;
	}

	return 0;

err:
	zynqmp_disp_destroy_layers(disp);
	return ret;
}

 

 
void zynqmp_disp_enable(struct zynqmp_disp *disp)
{
	zynqmp_disp_blend_set_output_format(disp, ZYNQMP_DPSUB_FORMAT_RGB);
	zynqmp_disp_blend_set_bg_color(disp, 0, 0, 0);

	zynqmp_disp_avbuf_enable(disp);
	 
	zynqmp_disp_avbuf_set_clocks_sources(disp, disp->dpsub->vid_clk_from_ps,
					     disp->dpsub->aud_clk_from_ps,
					     true);
	zynqmp_disp_avbuf_enable_channels(disp);
	zynqmp_disp_avbuf_enable_audio(disp);

	zynqmp_disp_audio_enable(disp);
}

 
void zynqmp_disp_disable(struct zynqmp_disp *disp)
{
	zynqmp_disp_audio_disable(disp);

	zynqmp_disp_avbuf_disable_audio(disp);
	zynqmp_disp_avbuf_disable_channels(disp);
	zynqmp_disp_avbuf_disable(disp);
}

 
int zynqmp_disp_setup_clock(struct zynqmp_disp *disp,
			    unsigned long mode_clock)
{
	unsigned long rate;
	long diff;
	int ret;

	ret = clk_set_rate(disp->dpsub->vid_clk, mode_clock);
	if (ret) {
		dev_err(disp->dev, "failed to set the video clock\n");
		return ret;
	}

	rate = clk_get_rate(disp->dpsub->vid_clk);
	diff = rate - mode_clock;
	if (abs(diff) > mode_clock / 20)
		dev_info(disp->dev,
			 "requested pixel rate: %lu actual rate: %lu\n",
			 mode_clock, rate);
	else
		dev_dbg(disp->dev,
			"requested pixel rate: %lu actual rate: %lu\n",
			mode_clock, rate);

	return 0;
}

 

int zynqmp_disp_probe(struct zynqmp_dpsub *dpsub)
{
	struct platform_device *pdev = to_platform_device(dpsub->dev);
	struct zynqmp_disp *disp;
	int ret;

	disp = kzalloc(sizeof(*disp), GFP_KERNEL);
	if (!disp)
		return -ENOMEM;

	disp->dev = &pdev->dev;
	disp->dpsub = dpsub;

	disp->blend.base = devm_platform_ioremap_resource_byname(pdev, "blend");
	if (IS_ERR(disp->blend.base)) {
		ret = PTR_ERR(disp->blend.base);
		goto error;
	}

	disp->avbuf.base = devm_platform_ioremap_resource_byname(pdev, "av_buf");
	if (IS_ERR(disp->avbuf.base)) {
		ret = PTR_ERR(disp->avbuf.base);
		goto error;
	}

	disp->audio.base = devm_platform_ioremap_resource_byname(pdev, "aud");
	if (IS_ERR(disp->audio.base)) {
		ret = PTR_ERR(disp->audio.base);
		goto error;
	}

	ret = zynqmp_disp_create_layers(disp);
	if (ret)
		goto error;

	if (disp->dpsub->dma_enabled) {
		struct zynqmp_disp_layer *layer;

		layer = &disp->layers[ZYNQMP_DPSUB_LAYER_VID];
		dpsub->dma_align = 1 << layer->dmas[0].chan->device->copy_align;
	}

	dpsub->disp = disp;

	return 0;

error:
	kfree(disp);
	return ret;
}

void zynqmp_disp_remove(struct zynqmp_dpsub *dpsub)
{
	struct zynqmp_disp *disp = dpsub->disp;

	zynqmp_disp_destroy_layers(disp);
}
