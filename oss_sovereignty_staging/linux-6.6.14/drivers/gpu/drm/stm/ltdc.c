
 

#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/media-bus-format.h>
#include <linux/module.h>
#include <linux/of_graph.h>
#include <linux/pinctrl/consumer.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include <drm/drm_atomic.h>
#include <drm/drm_atomic_helper.h>
#include <drm/drm_blend.h>
#include <drm/drm_bridge.h>
#include <drm/drm_device.h>
#include <drm/drm_edid.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_of.h>
#include <drm/drm_probe_helper.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_vblank.h>

#include <video/videomode.h>

#include "ltdc.h"

#define NB_CRTC 1
#define CRTC_MASK GENMASK(NB_CRTC - 1, 0)

#define MAX_IRQ 4

#define HWVER_10200 0x010200
#define HWVER_10300 0x010300
#define HWVER_20101 0x020101
#define HWVER_40100 0x040100

 
#define LAY_OFS_0	0x80
#define LAY_OFS_1	0x100
#define LAY_OFS	(ldev->caps.layer_ofs)

 
#define LTDC_IDR	0x0000		 
#define LTDC_LCR	0x0004		 
#define LTDC_SSCR	0x0008		 
#define LTDC_BPCR	0x000C		 
#define LTDC_AWCR	0x0010		 
#define LTDC_TWCR	0x0014		 
#define LTDC_GCR	0x0018		 
#define LTDC_GC1R	0x001C		 
#define LTDC_GC2R	0x0020		 
#define LTDC_SRCR	0x0024		 
#define LTDC_GACR	0x0028		 
#define LTDC_BCCR	0x002C		 
#define LTDC_IER	0x0034		 
#define LTDC_ISR	0x0038		 
#define LTDC_ICR	0x003C		 
#define LTDC_LIPCR	0x0040		 
#define LTDC_CPSR	0x0044		 
#define LTDC_CDSR	0x0048		 
#define LTDC_EDCR	0x0060		 
#define LTDC_CCRCR	0x007C		 
#define LTDC_FUT	0x0090		 

 
#define LTDC_L1C0R	(ldev->caps.layer_regs[0])	 
#define LTDC_L1C1R	(ldev->caps.layer_regs[1])	 
#define LTDC_L1RCR	(ldev->caps.layer_regs[2])	 
#define LTDC_L1CR	(ldev->caps.layer_regs[3])	 
#define LTDC_L1WHPCR	(ldev->caps.layer_regs[4])	 
#define LTDC_L1WVPCR	(ldev->caps.layer_regs[5])	 
#define LTDC_L1CKCR	(ldev->caps.layer_regs[6])	 
#define LTDC_L1PFCR	(ldev->caps.layer_regs[7])	 
#define LTDC_L1CACR	(ldev->caps.layer_regs[8])	 
#define LTDC_L1DCCR	(ldev->caps.layer_regs[9])	 
#define LTDC_L1BFCR	(ldev->caps.layer_regs[10])	 
#define LTDC_L1BLCR	(ldev->caps.layer_regs[11])	 
#define LTDC_L1PCR	(ldev->caps.layer_regs[12])	 
#define LTDC_L1CFBAR	(ldev->caps.layer_regs[13])	 
#define LTDC_L1CFBLR	(ldev->caps.layer_regs[14])	 
#define LTDC_L1CFBLNR	(ldev->caps.layer_regs[15])	 
#define LTDC_L1AFBA0R	(ldev->caps.layer_regs[16])	 
#define LTDC_L1AFBA1R	(ldev->caps.layer_regs[17])	 
#define LTDC_L1AFBLR	(ldev->caps.layer_regs[18])	 
#define LTDC_L1AFBLNR	(ldev->caps.layer_regs[19])	 
#define LTDC_L1CLUTWR	(ldev->caps.layer_regs[20])	 
#define LTDC_L1CYR0R	(ldev->caps.layer_regs[21])	 
#define LTDC_L1CYR1R	(ldev->caps.layer_regs[22])	 
#define LTDC_L1FPF0R	(ldev->caps.layer_regs[23])	 
#define LTDC_L1FPF1R	(ldev->caps.layer_regs[24])	 

 
#define SSCR_VSH	GENMASK(10, 0)	 
#define SSCR_HSW	GENMASK(27, 16)	 

#define BPCR_AVBP	GENMASK(10, 0)	 
#define BPCR_AHBP	GENMASK(27, 16)	 

#define AWCR_AAH	GENMASK(10, 0)	 
#define AWCR_AAW	GENMASK(27, 16)	 

#define TWCR_TOTALH	GENMASK(10, 0)	 
#define TWCR_TOTALW	GENMASK(27, 16)	 

#define GCR_LTDCEN	BIT(0)		 
#define GCR_DEN		BIT(16)		 
#define GCR_CRCEN	BIT(19)		 
#define GCR_PCPOL	BIT(28)		 
#define GCR_DEPOL	BIT(29)		 
#define GCR_VSPOL	BIT(30)		 
#define GCR_HSPOL	BIT(31)		 

#define GC1R_WBCH	GENMASK(3, 0)	 
#define GC1R_WGCH	GENMASK(7, 4)	 
#define GC1R_WRCH	GENMASK(11, 8)	 
#define GC1R_PBEN	BIT(12)		 
#define GC1R_DT		GENMASK(15, 14)	 
#define GC1R_GCT	GENMASK(19, 17)	 
#define GC1R_SHREN	BIT(21)		 
#define GC1R_BCP	BIT(22)		 
#define GC1R_BBEN	BIT(23)		 
#define GC1R_LNIP	BIT(24)		 
#define GC1R_TP		BIT(25)		 
#define GC1R_IPP	BIT(26)		 
#define GC1R_SPP	BIT(27)		 
#define GC1R_DWP	BIT(28)		 
#define GC1R_STREN	BIT(29)		 
#define GC1R_BMEN	BIT(31)		 

#define GC2R_EDCA	BIT(0)		 
#define GC2R_STSAEN	BIT(1)		 
#define GC2R_DVAEN	BIT(2)		 
#define GC2R_DPAEN	BIT(3)		 
#define GC2R_BW		GENMASK(6, 4)	 
#define GC2R_EDCEN	BIT(7)		 

#define SRCR_IMR	BIT(0)		 
#define SRCR_VBR	BIT(1)		 

#define BCCR_BCBLACK	0x00		 
#define BCCR_BCBLUE	GENMASK(7, 0)	 
#define BCCR_BCGREEN	GENMASK(15, 8)	 
#define BCCR_BCRED	GENMASK(23, 16)	 
#define BCCR_BCWHITE	GENMASK(23, 0)	 

#define IER_LIE		BIT(0)		 
#define IER_FUWIE	BIT(1)		 
#define IER_TERRIE	BIT(2)		 
#define IER_RRIE	BIT(3)		 
#define IER_FUEIE	BIT(6)		 
#define IER_CRCIE	BIT(7)		 

#define CPSR_CYPOS	GENMASK(15, 0)	 

#define ISR_LIF		BIT(0)		 
#define ISR_FUWIF	BIT(1)		 
#define ISR_TERRIF	BIT(2)		 
#define ISR_RRIF	BIT(3)		 
#define ISR_FUEIF	BIT(6)		 
#define ISR_CRCIF	BIT(7)		 

#define EDCR_OCYEN	BIT(25)		 
#define EDCR_OCYSEL	BIT(26)		 
#define EDCR_OCYCO	BIT(27)		 

#define LXCR_LEN	BIT(0)		 
#define LXCR_COLKEN	BIT(1)		 
#define LXCR_CLUTEN	BIT(4)		 
#define LXCR_HMEN	BIT(8)		 

#define LXWHPCR_WHSTPOS	GENMASK(11, 0)	 
#define LXWHPCR_WHSPPOS	GENMASK(27, 16)	 

#define LXWVPCR_WVSTPOS	GENMASK(10, 0)	 
#define LXWVPCR_WVSPPOS	GENMASK(26, 16)	 

#define LXPFCR_PF	GENMASK(2, 0)	 
#define PF_FLEXIBLE	0x7		 

#define LXCACR_CONSTA	GENMASK(7, 0)	 

#define LXBFCR_BF2	GENMASK(2, 0)	 
#define LXBFCR_BF1	GENMASK(10, 8)	 
#define LXBFCR_BOR	GENMASK(18, 16)  

#define LXCFBLR_CFBLL	GENMASK(12, 0)	 
#define LXCFBLR_CFBP	GENMASK(31, 16)  

#define LXCFBLNR_CFBLN	GENMASK(10, 0)	 

#define LXCR_C1R_YIA	BIT(0)		 
#define LXCR_C1R_YSPA	BIT(1)		 
#define LXCR_C1R_YFPA	BIT(2)		 
#define LXCR_C1R_SCA	BIT(31)		 

#define LxPCR_YREN	BIT(9)		 
#define LxPCR_OF	BIT(8)		 
#define LxPCR_CBF	BIT(7)		 
#define LxPCR_YF	BIT(6)		 
#define LxPCR_YCM	GENMASK(5, 4)	 
#define YCM_I		0x0		 
#define YCM_SP		0x1		 
#define YCM_FP		0x2		 
#define LxPCR_YCEN	BIT(3)		 

#define LXRCR_IMR	BIT(0)		 
#define LXRCR_VBR	BIT(1)		 
#define LXRCR_GRMSK	BIT(2)		 

#define CLUT_SIZE	256

#define CONSTA_MAX	0xFF		 
#define BF1_PAXCA	0x600		 
#define BF1_CA		0x400		 
#define BF2_1PAXCA	0x007		 
#define BF2_1CA		0x005		 

#define NB_PF		8		 

#define FUT_DFT		128		 

 
#define CRC_SKIP_FRAMES 2

enum ltdc_pix_fmt {
	PF_NONE,
	 
	PF_ARGB8888,		 
	PF_RGBA8888,		 
	PF_ABGR8888,		 
	PF_BGRA8888,		 
	PF_RGB888,		 
	PF_BGR888,		 
	PF_RGB565,		 
	PF_BGR565,		 
	PF_ARGB1555,		 
	PF_ARGB4444,		 
	 
	PF_L8,			 
	PF_AL44,		 
	PF_AL88			 
};

 
static const enum ltdc_pix_fmt ltdc_pix_fmt_a0[NB_PF] = {
	PF_ARGB8888,		 
	PF_RGB888,		 
	PF_RGB565,		 
	PF_ARGB1555,		 
	PF_ARGB4444,		 
	PF_L8,			 
	PF_AL44,		 
	PF_AL88			 
};

static const enum ltdc_pix_fmt ltdc_pix_fmt_a1[NB_PF] = {
	PF_ARGB8888,		 
	PF_RGB888,		 
	PF_RGB565,		 
	PF_RGBA8888,		 
	PF_AL44,		 
	PF_L8,			 
	PF_ARGB1555,		 
	PF_ARGB4444		 
};

static const enum ltdc_pix_fmt ltdc_pix_fmt_a2[NB_PF] = {
	PF_ARGB8888,		 
	PF_ABGR8888,		 
	PF_RGBA8888,		 
	PF_BGRA8888,		 
	PF_RGB565,		 
	PF_BGR565,		 
	PF_RGB888,		 
	PF_NONE			 
};

static const u32 ltdc_drm_fmt_a0[] = {
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_C8
};

static const u32 ltdc_drm_fmt_a1[] = {
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_C8
};

static const u32 ltdc_drm_fmt_a2[] = {
	DRM_FORMAT_ARGB8888,
	DRM_FORMAT_XRGB8888,
	DRM_FORMAT_ABGR8888,
	DRM_FORMAT_XBGR8888,
	DRM_FORMAT_RGBA8888,
	DRM_FORMAT_RGBX8888,
	DRM_FORMAT_BGRA8888,
	DRM_FORMAT_BGRX8888,
	DRM_FORMAT_RGB565,
	DRM_FORMAT_BGR565,
	DRM_FORMAT_RGB888,
	DRM_FORMAT_BGR888,
	DRM_FORMAT_ARGB1555,
	DRM_FORMAT_XRGB1555,
	DRM_FORMAT_ARGB4444,
	DRM_FORMAT_XRGB4444,
	DRM_FORMAT_C8
};

static const u32 ltdc_drm_fmt_ycbcr_cp[] = {
	DRM_FORMAT_YUYV,
	DRM_FORMAT_YVYU,
	DRM_FORMAT_UYVY,
	DRM_FORMAT_VYUY
};

static const u32 ltdc_drm_fmt_ycbcr_sp[] = {
	DRM_FORMAT_NV12,
	DRM_FORMAT_NV21
};

static const u32 ltdc_drm_fmt_ycbcr_fp[] = {
	DRM_FORMAT_YUV420,
	DRM_FORMAT_YVU420
};

 
static const u32 ltdc_layer_regs_a0[] = {
	0x80,	 
	0x00,	 
	0x00,	 
	0x84,	 
	0x88,	 
	0x8c,	 
	0x90,	 
	0x94,	 
	0x98,	 
	0x9c,	 
	0xa0,	 
	0x00,	 
	0x00,	 
	0xac,	 
	0xb0,	 
	0xb4,	 
	0x00,	 
	0x00,	 
	0x00,	 
	0x00,	 
	0xc4,	 
	0x00,	 
	0x00,	 
	0x00,	 
	0x00	 
};

static const u32 ltdc_layer_regs_a1[] = {
	0x80,	 
	0x84,	 
	0x00,	 
	0x88,	 
	0x8c,	 
	0x90,	 
	0x94,	 
	0x98,	 
	0x9c,	 
	0xa0,	 
	0xa4,	 
	0xa8,	 
	0x00,	 
	0xac,	 
	0xb0,	 
	0xb4,	 
	0xb8,	 
	0xbc,	 
	0xc0,	 
	0xc4,	 
	0xc8,	 
	0x00,	 
	0x00,	 
	0x00,	 
	0x00	 
};

static const u32 ltdc_layer_regs_a2[] = {
	0x100,	 
	0x104,	 
	0x108,	 
	0x10c,	 
	0x110,	 
	0x114,	 
	0x118,	 
	0x11c,	 
	0x120,	 
	0x124,	 
	0x128,	 
	0x12c,	 
	0x130,	 
	0x134,	 
	0x138,	 
	0x13c,	 
	0x140,	 
	0x144,	 
	0x148,	 
	0x14c,	 
	0x150,	 
	0x16c,	 
	0x170,	 
	0x174,	 
	0x178	 
};

static const u64 ltdc_format_modifiers[] = {
	DRM_FORMAT_MOD_LINEAR,
	DRM_FORMAT_MOD_INVALID
};

static const struct regmap_config stm32_ltdc_regmap_cfg = {
	.reg_bits = 32,
	.val_bits = 32,
	.reg_stride = sizeof(u32),
	.max_register = 0x400,
	.use_relaxed_mmio = true,
	.cache_type = REGCACHE_NONE,
};

static const u32 ltdc_ycbcr2rgb_coeffs[DRM_COLOR_ENCODING_MAX][DRM_COLOR_RANGE_MAX][2] = {
	[DRM_COLOR_YCBCR_BT601][DRM_COLOR_YCBCR_LIMITED_RANGE] = {
		0x02040199,	 
		0x006400D0	 
	},
	[DRM_COLOR_YCBCR_BT601][DRM_COLOR_YCBCR_FULL_RANGE] = {
		0x01C60167,	 
		0x005800B7	 
	},
	[DRM_COLOR_YCBCR_BT709][DRM_COLOR_YCBCR_LIMITED_RANGE] = {
		0x021D01CB,	 
		0x00370089	 
	},
	[DRM_COLOR_YCBCR_BT709][DRM_COLOR_YCBCR_FULL_RANGE] = {
		0x01DB0193,	 
		0x00300078	 
	}
	 
};

static inline struct ltdc_device *crtc_to_ltdc(struct drm_crtc *crtc)
{
	return (struct ltdc_device *)crtc->dev->dev_private;
}

static inline struct ltdc_device *plane_to_ltdc(struct drm_plane *plane)
{
	return (struct ltdc_device *)plane->dev->dev_private;
}

static inline struct ltdc_device *encoder_to_ltdc(struct drm_encoder *enc)
{
	return (struct ltdc_device *)enc->dev->dev_private;
}

static inline enum ltdc_pix_fmt to_ltdc_pixelformat(u32 drm_fmt)
{
	enum ltdc_pix_fmt pf;

	switch (drm_fmt) {
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_XRGB8888:
		pf = PF_ARGB8888;
		break;
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_XBGR8888:
		pf = PF_ABGR8888;
		break;
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
		pf = PF_RGBA8888;
		break;
	case DRM_FORMAT_BGRA8888:
	case DRM_FORMAT_BGRX8888:
		pf = PF_BGRA8888;
		break;
	case DRM_FORMAT_RGB888:
		pf = PF_RGB888;
		break;
	case DRM_FORMAT_BGR888:
		pf = PF_BGR888;
		break;
	case DRM_FORMAT_RGB565:
		pf = PF_RGB565;
		break;
	case DRM_FORMAT_BGR565:
		pf = PF_BGR565;
		break;
	case DRM_FORMAT_ARGB1555:
	case DRM_FORMAT_XRGB1555:
		pf = PF_ARGB1555;
		break;
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_XRGB4444:
		pf = PF_ARGB4444;
		break;
	case DRM_FORMAT_C8:
		pf = PF_L8;
		break;
	default:
		pf = PF_NONE;
		break;
		 
	}

	return pf;
}

static inline u32 ltdc_set_flexible_pixel_format(struct drm_plane *plane, enum ltdc_pix_fmt pix_fmt)
{
	struct ltdc_device *ldev = plane_to_ltdc(plane);
	u32 lofs = plane->index * LAY_OFS, ret = PF_FLEXIBLE;
	int psize, alen, apos, rlen, rpos, glen, gpos, blen, bpos;

	switch (pix_fmt) {
	case PF_BGR888:
		psize = 3;
		alen = 0; apos = 0; rlen = 8; rpos = 0;
		glen = 8; gpos = 8; blen = 8; bpos = 16;
	break;
	case PF_ARGB1555:
		psize = 2;
		alen = 1; apos = 15; rlen = 5; rpos = 10;
		glen = 5; gpos = 5;  blen = 5; bpos = 0;
	break;
	case PF_ARGB4444:
		psize = 2;
		alen = 4; apos = 12; rlen = 4; rpos = 8;
		glen = 4; gpos = 4; blen = 4; bpos = 0;
	break;
	case PF_L8:
		psize = 1;
		alen = 0; apos = 0; rlen = 8; rpos = 0;
		glen = 8; gpos = 0; blen = 8; bpos = 0;
	break;
	case PF_AL44:
		psize = 1;
		alen = 4; apos = 4; rlen = 4; rpos = 0;
		glen = 4; gpos = 0; blen = 4; bpos = 0;
	break;
	case PF_AL88:
		psize = 2;
		alen = 8; apos = 8; rlen = 8; rpos = 0;
		glen = 8; gpos = 0; blen = 8; bpos = 0;
	break;
	default:
		ret = NB_PF;  
	break;
	}

	if (ret == PF_FLEXIBLE) {
		regmap_write(ldev->regmap, LTDC_L1FPF0R + lofs,
			     (rlen << 14)  + (rpos << 9) + (alen << 5) + apos);

		regmap_write(ldev->regmap, LTDC_L1FPF1R + lofs,
			     (psize << 18) + (blen << 14)  + (bpos << 9) + (glen << 5) + gpos);
	}

	return ret;
}

 
static inline u32 is_xrgb(u32 drm)
{
	return ((drm & 0xFF) == 'X' || ((drm >> 8) & 0xFF) == 'X');
}

static inline void ltdc_set_ycbcr_config(struct drm_plane *plane, u32 drm_pix_fmt)
{
	struct ltdc_device *ldev = plane_to_ltdc(plane);
	struct drm_plane_state *state = plane->state;
	u32 lofs = plane->index * LAY_OFS;
	u32 val;

	switch (drm_pix_fmt) {
	case DRM_FORMAT_YUYV:
		val = (YCM_I << 4) | LxPCR_YF | LxPCR_CBF;
		break;
	case DRM_FORMAT_YVYU:
		val = (YCM_I << 4) | LxPCR_YF;
		break;
	case DRM_FORMAT_UYVY:
		val = (YCM_I << 4) | LxPCR_CBF;
		break;
	case DRM_FORMAT_VYUY:
		val = (YCM_I << 4);
		break;
	case DRM_FORMAT_NV12:
		val = (YCM_SP << 4) | LxPCR_CBF;
		break;
	case DRM_FORMAT_NV21:
		val = (YCM_SP << 4);
		break;
	case DRM_FORMAT_YUV420:
	case DRM_FORMAT_YVU420:
		val = (YCM_FP << 4);
		break;
	default:
		 
		DRM_ERROR("Unsupported pixel format: %u\n", drm_pix_fmt);
		return;
	}

	 
	if (state->color_range == DRM_COLOR_YCBCR_LIMITED_RANGE)
		val |= LxPCR_YREN;

	 
	val |= LxPCR_YCEN;

	regmap_write(ldev->regmap, LTDC_L1PCR + lofs, val);
}

static inline void ltdc_set_ycbcr_coeffs(struct drm_plane *plane)
{
	struct ltdc_device *ldev = plane_to_ltdc(plane);
	struct drm_plane_state *state = plane->state;
	enum drm_color_encoding enc = state->color_encoding;
	enum drm_color_range ran = state->color_range;
	u32 lofs = plane->index * LAY_OFS;

	if (enc != DRM_COLOR_YCBCR_BT601 && enc != DRM_COLOR_YCBCR_BT709) {
		DRM_ERROR("color encoding %d not supported, use bt601 by default\n", enc);
		 
		enc = DRM_COLOR_YCBCR_BT601;
	}

	if (ran != DRM_COLOR_YCBCR_LIMITED_RANGE && ran != DRM_COLOR_YCBCR_FULL_RANGE) {
		DRM_ERROR("color range %d not supported, use limited range by default\n", ran);
		 
		ran = DRM_COLOR_YCBCR_LIMITED_RANGE;
	}

	DRM_DEBUG_DRIVER("Color encoding=%d, range=%d\n", enc, ran);
	regmap_write(ldev->regmap, LTDC_L1CYR0R + lofs,
		     ltdc_ycbcr2rgb_coeffs[enc][ran][0]);
	regmap_write(ldev->regmap, LTDC_L1CYR1R + lofs,
		     ltdc_ycbcr2rgb_coeffs[enc][ran][1]);
}

static inline void ltdc_irq_crc_handle(struct ltdc_device *ldev,
				       struct drm_crtc *crtc)
{
	u32 crc;
	int ret;

	if (ldev->crc_skip_count < CRC_SKIP_FRAMES) {
		ldev->crc_skip_count++;
		return;
	}

	 
	ret = regmap_read(ldev->regmap, LTDC_CCRCR, &crc);
	if (ret)
		return;

	 
	drm_crtc_add_crc_entry(crtc, true, drm_crtc_accurate_vblank_count(crtc), &crc);
}

static irqreturn_t ltdc_irq_thread(int irq, void *arg)
{
	struct drm_device *ddev = arg;
	struct ltdc_device *ldev = ddev->dev_private;
	struct drm_crtc *crtc = drm_crtc_from_index(ddev, 0);

	 
	if (ldev->irq_status & ISR_LIF) {
		drm_crtc_handle_vblank(crtc);

		 
		if (ldev->crc_active)
			ltdc_irq_crc_handle(ldev, crtc);
	}

	mutex_lock(&ldev->err_lock);
	if (ldev->irq_status & ISR_TERRIF)
		ldev->transfer_err++;
	if (ldev->irq_status & ISR_FUEIF)
		ldev->fifo_err++;
	if (ldev->irq_status & ISR_FUWIF)
		ldev->fifo_warn++;
	mutex_unlock(&ldev->err_lock);

	return IRQ_HANDLED;
}

static irqreturn_t ltdc_irq(int irq, void *arg)
{
	struct drm_device *ddev = arg;
	struct ltdc_device *ldev = ddev->dev_private;

	 
	ldev->irq_status = readl_relaxed(ldev->regs + LTDC_ISR);
	writel_relaxed(ldev->irq_status, ldev->regs + LTDC_ICR);

	return IRQ_WAKE_THREAD;
}

 

static void ltdc_crtc_update_clut(struct drm_crtc *crtc)
{
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);
	struct drm_color_lut *lut;
	u32 val;
	int i;

	if (!crtc->state->color_mgmt_changed || !crtc->state->gamma_lut)
		return;

	lut = (struct drm_color_lut *)crtc->state->gamma_lut->data;

	for (i = 0; i < CLUT_SIZE; i++, lut++) {
		val = ((lut->red << 8) & 0xff0000) | (lut->green & 0xff00) |
			(lut->blue >> 8) | (i << 24);
		regmap_write(ldev->regmap, LTDC_L1CLUTWR, val);
	}
}

static void ltdc_crtc_atomic_enable(struct drm_crtc *crtc,
				    struct drm_atomic_state *state)
{
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);
	struct drm_device *ddev = crtc->dev;

	DRM_DEBUG_DRIVER("\n");

	pm_runtime_get_sync(ddev->dev);

	 
	regmap_write(ldev->regmap, LTDC_BCCR, BCCR_BCBLACK);

	 
	regmap_set_bits(ldev->regmap, LTDC_IER, IER_FUWIE | IER_FUEIE | IER_RRIE | IER_TERRIE);

	 
	if (!ldev->caps.plane_reg_shadow)
		regmap_set_bits(ldev->regmap, LTDC_SRCR, SRCR_VBR);

	drm_crtc_vblank_on(crtc);
}

static void ltdc_crtc_atomic_disable(struct drm_crtc *crtc,
				     struct drm_atomic_state *state)
{
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);
	struct drm_device *ddev = crtc->dev;
	int layer_index = 0;

	DRM_DEBUG_DRIVER("\n");

	drm_crtc_vblank_off(crtc);

	 
	for (layer_index = 0; layer_index < ldev->caps.nb_layers; layer_index++)
		regmap_write_bits(ldev->regmap, LTDC_L1CR + layer_index * LAY_OFS,
				  LXCR_CLUTEN | LXCR_LEN, 0);

	 
	regmap_clear_bits(ldev->regmap, LTDC_IER, IER_FUWIE | IER_FUEIE | IER_RRIE | IER_TERRIE);

	 
	if (!ldev->caps.plane_reg_shadow)
		regmap_set_bits(ldev->regmap, LTDC_SRCR, SRCR_IMR);

	pm_runtime_put_sync(ddev->dev);

	 
	mutex_lock(&ldev->err_lock);
	ldev->transfer_err = 0;
	ldev->fifo_err = 0;
	ldev->fifo_warn = 0;
	mutex_unlock(&ldev->err_lock);
}

#define CLK_TOLERANCE_HZ 50

static enum drm_mode_status
ltdc_crtc_mode_valid(struct drm_crtc *crtc,
		     const struct drm_display_mode *mode)
{
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);
	int target = mode->clock * 1000;
	int target_min = target - CLK_TOLERANCE_HZ;
	int target_max = target + CLK_TOLERANCE_HZ;
	int result;

	result = clk_round_rate(ldev->pixel_clk, target);

	DRM_DEBUG_DRIVER("clk rate target %d, available %d\n", target, result);

	 
	if (result > ldev->caps.pad_max_freq_hz)
		return MODE_CLOCK_HIGH;

	 
	if (mode->type & DRM_MODE_TYPE_PREFERRED)
		return MODE_OK;

	 
	if (result < target_min || result > target_max)
		return MODE_CLOCK_RANGE;

	return MODE_OK;
}

static bool ltdc_crtc_mode_fixup(struct drm_crtc *crtc,
				 const struct drm_display_mode *mode,
				 struct drm_display_mode *adjusted_mode)
{
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);
	int rate = mode->clock * 1000;

	if (clk_set_rate(ldev->pixel_clk, rate) < 0) {
		DRM_ERROR("Cannot set rate (%dHz) for pixel clk\n", rate);
		return false;
	}

	adjusted_mode->clock = clk_get_rate(ldev->pixel_clk) / 1000;

	DRM_DEBUG_DRIVER("requested clock %dkHz, adjusted clock %dkHz\n",
			 mode->clock, adjusted_mode->clock);

	return true;
}

static void ltdc_crtc_mode_set_nofb(struct drm_crtc *crtc)
{
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);
	struct drm_device *ddev = crtc->dev;
	struct drm_connector_list_iter iter;
	struct drm_connector *connector = NULL;
	struct drm_encoder *encoder = NULL, *en_iter;
	struct drm_bridge *bridge = NULL, *br_iter;
	struct drm_display_mode *mode = &crtc->state->adjusted_mode;
	u32 hsync, vsync, accum_hbp, accum_vbp, accum_act_w, accum_act_h;
	u32 total_width, total_height;
	u32 bus_formats = MEDIA_BUS_FMT_RGB888_1X24;
	u32 bus_flags = 0;
	u32 val;
	int ret;

	 
	drm_for_each_encoder(en_iter, ddev)
		if (en_iter->crtc == crtc) {
			encoder = en_iter;
			break;
		}

	if (encoder) {
		 
		list_for_each_entry(br_iter, &encoder->bridge_chain, chain_node)
			if (br_iter->encoder == encoder) {
				bridge = br_iter;
				break;
			}

		 
		drm_connector_list_iter_begin(ddev, &iter);
		drm_for_each_connector_iter(connector, &iter)
			if (connector->encoder == encoder)
				break;
		drm_connector_list_iter_end(&iter);
	}

	if (bridge && bridge->timings) {
		bus_flags = bridge->timings->input_bus_flags;
	} else if (connector) {
		bus_flags = connector->display_info.bus_flags;
		if (connector->display_info.num_bus_formats)
			bus_formats = connector->display_info.bus_formats[0];
	}

	if (!pm_runtime_active(ddev->dev)) {
		ret = pm_runtime_get_sync(ddev->dev);
		if (ret) {
			DRM_ERROR("Failed to set mode, cannot get sync\n");
			return;
		}
	}

	DRM_DEBUG_DRIVER("CRTC:%d mode:%s\n", crtc->base.id, mode->name);
	DRM_DEBUG_DRIVER("Video mode: %dx%d", mode->hdisplay, mode->vdisplay);
	DRM_DEBUG_DRIVER(" hfp %d hbp %d hsl %d vfp %d vbp %d vsl %d\n",
			 mode->hsync_start - mode->hdisplay,
			 mode->htotal - mode->hsync_end,
			 mode->hsync_end - mode->hsync_start,
			 mode->vsync_start - mode->vdisplay,
			 mode->vtotal - mode->vsync_end,
			 mode->vsync_end - mode->vsync_start);

	 
	hsync = mode->hsync_end - mode->hsync_start - 1;
	vsync = mode->vsync_end - mode->vsync_start - 1;
	accum_hbp = mode->htotal - mode->hsync_start - 1;
	accum_vbp = mode->vtotal - mode->vsync_start - 1;
	accum_act_w = accum_hbp + mode->hdisplay;
	accum_act_h = accum_vbp + mode->vdisplay;
	total_width = mode->htotal - 1;
	total_height = mode->vtotal - 1;

	 
	val = 0;

	if (mode->flags & DRM_MODE_FLAG_PHSYNC)
		val |= GCR_HSPOL;

	if (mode->flags & DRM_MODE_FLAG_PVSYNC)
		val |= GCR_VSPOL;

	if (bus_flags & DRM_BUS_FLAG_DE_LOW)
		val |= GCR_DEPOL;

	if (bus_flags & DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE)
		val |= GCR_PCPOL;

	regmap_update_bits(ldev->regmap, LTDC_GCR,
			   GCR_HSPOL | GCR_VSPOL | GCR_DEPOL | GCR_PCPOL, val);

	 
	val = (hsync << 16) | vsync;
	regmap_update_bits(ldev->regmap, LTDC_SSCR, SSCR_VSH | SSCR_HSW, val);

	 
	val = (accum_hbp << 16) | accum_vbp;
	regmap_update_bits(ldev->regmap, LTDC_BPCR, BPCR_AVBP | BPCR_AHBP, val);

	 
	val = (accum_act_w << 16) | accum_act_h;
	regmap_update_bits(ldev->regmap, LTDC_AWCR, AWCR_AAW | AWCR_AAH, val);

	 
	val = (total_width << 16) | total_height;
	regmap_update_bits(ldev->regmap, LTDC_TWCR, TWCR_TOTALH | TWCR_TOTALW, val);

	regmap_write(ldev->regmap, LTDC_LIPCR, (accum_act_h + 1));

	 
	if (ldev->caps.ycbcr_output) {
		 
		int vic = drm_match_cea_mode(mode);
		u32 val;

		if (vic == 6 || vic == 7 || vic == 21 || vic == 22 ||
		    vic == 2 || vic == 3 || vic == 17 || vic == 18)
			 
			val = 0;
		else
			 
			val = EDCR_OCYSEL;

		switch (bus_formats) {
		case MEDIA_BUS_FMT_YUYV8_1X16:
			 
			regmap_write(ldev->regmap, LTDC_EDCR, EDCR_OCYEN | val);
			break;
		case MEDIA_BUS_FMT_YVYU8_1X16:
			 
			regmap_write(ldev->regmap, LTDC_EDCR, EDCR_OCYEN | EDCR_OCYCO | val);
			break;
		default:
			 
			regmap_write(ldev->regmap, LTDC_EDCR, 0);
			break;
		}
	}
}

static void ltdc_crtc_atomic_flush(struct drm_crtc *crtc,
				   struct drm_atomic_state *state)
{
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);
	struct drm_device *ddev = crtc->dev;
	struct drm_pending_vblank_event *event = crtc->state->event;

	DRM_DEBUG_ATOMIC("\n");

	ltdc_crtc_update_clut(crtc);

	 
	if (!ldev->caps.plane_reg_shadow)
		regmap_set_bits(ldev->regmap, LTDC_SRCR, SRCR_VBR);

	if (event) {
		crtc->state->event = NULL;

		spin_lock_irq(&ddev->event_lock);
		if (drm_crtc_vblank_get(crtc) == 0)
			drm_crtc_arm_vblank_event(crtc, event);
		else
			drm_crtc_send_vblank_event(crtc, event);
		spin_unlock_irq(&ddev->event_lock);
	}
}

static bool ltdc_crtc_get_scanout_position(struct drm_crtc *crtc,
					   bool in_vblank_irq,
					   int *vpos, int *hpos,
					   ktime_t *stime, ktime_t *etime,
					   const struct drm_display_mode *mode)
{
	struct drm_device *ddev = crtc->dev;
	struct ltdc_device *ldev = ddev->dev_private;
	int line, vactive_start, vactive_end, vtotal;

	if (stime)
		*stime = ktime_get();

	 
	if (pm_runtime_active(ddev->dev)) {
		regmap_read(ldev->regmap, LTDC_CPSR, &line);
		line &= CPSR_CYPOS;
		regmap_read(ldev->regmap, LTDC_BPCR, &vactive_start);
		vactive_start &= BPCR_AVBP;
		regmap_read(ldev->regmap, LTDC_AWCR, &vactive_end);
		vactive_end &= AWCR_AAH;
		regmap_read(ldev->regmap, LTDC_TWCR, &vtotal);
		vtotal &= TWCR_TOTALH;

		if (line > vactive_end)
			*vpos = line - vtotal - vactive_start;
		else
			*vpos = line - vactive_start;
	} else {
		*vpos = 0;
	}

	*hpos = 0;

	if (etime)
		*etime = ktime_get();

	return true;
}

static const struct drm_crtc_helper_funcs ltdc_crtc_helper_funcs = {
	.mode_valid = ltdc_crtc_mode_valid,
	.mode_fixup = ltdc_crtc_mode_fixup,
	.mode_set_nofb = ltdc_crtc_mode_set_nofb,
	.atomic_flush = ltdc_crtc_atomic_flush,
	.atomic_enable = ltdc_crtc_atomic_enable,
	.atomic_disable = ltdc_crtc_atomic_disable,
	.get_scanout_position = ltdc_crtc_get_scanout_position,
};

static int ltdc_crtc_enable_vblank(struct drm_crtc *crtc)
{
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);
	struct drm_crtc_state *state = crtc->state;

	DRM_DEBUG_DRIVER("\n");

	if (state->enable)
		regmap_set_bits(ldev->regmap, LTDC_IER, IER_LIE);
	else
		return -EPERM;

	return 0;
}

static void ltdc_crtc_disable_vblank(struct drm_crtc *crtc)
{
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);

	DRM_DEBUG_DRIVER("\n");
	regmap_clear_bits(ldev->regmap, LTDC_IER, IER_LIE);
}

static int ltdc_crtc_set_crc_source(struct drm_crtc *crtc, const char *source)
{
	struct ltdc_device *ldev;
	int ret;

	DRM_DEBUG_DRIVER("\n");

	if (!crtc)
		return -ENODEV;

	ldev = crtc_to_ltdc(crtc);

	if (source && strcmp(source, "auto") == 0) {
		ldev->crc_active = true;
		ret = regmap_set_bits(ldev->regmap, LTDC_GCR, GCR_CRCEN);
	} else if (!source) {
		ldev->crc_active = false;
		ret = regmap_clear_bits(ldev->regmap, LTDC_GCR, GCR_CRCEN);
	} else {
		ret = -EINVAL;
	}

	ldev->crc_skip_count = 0;
	return ret;
}

static int ltdc_crtc_verify_crc_source(struct drm_crtc *crtc,
				       const char *source, size_t *values_cnt)
{
	DRM_DEBUG_DRIVER("\n");

	if (!crtc)
		return -ENODEV;

	if (source && strcmp(source, "auto") != 0) {
		DRM_DEBUG_DRIVER("Unknown CRC source %s for %s\n",
				 source, crtc->name);
		return -EINVAL;
	}

	*values_cnt = 1;
	return 0;
}

static void ltdc_crtc_atomic_print_state(struct drm_printer *p,
					 const struct drm_crtc_state *state)
{
	struct drm_crtc *crtc = state->crtc;
	struct ltdc_device *ldev = crtc_to_ltdc(crtc);

	drm_printf(p, "\ttransfer_error=%d\n", ldev->transfer_err);
	drm_printf(p, "\tfifo_underrun_error=%d\n", ldev->fifo_err);
	drm_printf(p, "\tfifo_underrun_warning=%d\n", ldev->fifo_warn);
	drm_printf(p, "\tfifo_underrun_threshold=%d\n", ldev->fifo_threshold);
}

static const struct drm_crtc_funcs ltdc_crtc_funcs = {
	.destroy = drm_crtc_cleanup,
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
	.enable_vblank = ltdc_crtc_enable_vblank,
	.disable_vblank = ltdc_crtc_disable_vblank,
	.get_vblank_timestamp = drm_crtc_vblank_helper_get_vblank_timestamp,
	.atomic_print_state = ltdc_crtc_atomic_print_state,
};

static const struct drm_crtc_funcs ltdc_crtc_with_crc_support_funcs = {
	.destroy = drm_crtc_cleanup,
	.set_config = drm_atomic_helper_set_config,
	.page_flip = drm_atomic_helper_page_flip,
	.reset = drm_atomic_helper_crtc_reset,
	.atomic_duplicate_state = drm_atomic_helper_crtc_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_crtc_destroy_state,
	.enable_vblank = ltdc_crtc_enable_vblank,
	.disable_vblank = ltdc_crtc_disable_vblank,
	.get_vblank_timestamp = drm_crtc_vblank_helper_get_vblank_timestamp,
	.set_crc_source = ltdc_crtc_set_crc_source,
	.verify_crc_source = ltdc_crtc_verify_crc_source,
	.atomic_print_state = ltdc_crtc_atomic_print_state,
};

 

static int ltdc_plane_atomic_check(struct drm_plane *plane,
				   struct drm_atomic_state *state)
{
	struct drm_plane_state *new_plane_state = drm_atomic_get_new_plane_state(state,
										 plane);
	struct drm_framebuffer *fb = new_plane_state->fb;
	u32 src_w, src_h;

	DRM_DEBUG_DRIVER("\n");

	if (!fb)
		return 0;

	 
	src_w = new_plane_state->src_w >> 16;
	src_h = new_plane_state->src_h >> 16;

	 
	if (src_w != new_plane_state->crtc_w || src_h != new_plane_state->crtc_h) {
		DRM_DEBUG_DRIVER("Scaling is not supported");

		return -EINVAL;
	}

	return 0;
}

static void ltdc_plane_atomic_update(struct drm_plane *plane,
				     struct drm_atomic_state *state)
{
	struct ltdc_device *ldev = plane_to_ltdc(plane);
	struct drm_plane_state *newstate = drm_atomic_get_new_plane_state(state,
									  plane);
	struct drm_framebuffer *fb = newstate->fb;
	u32 lofs = plane->index * LAY_OFS;
	u32 x0 = newstate->crtc_x;
	u32 x1 = newstate->crtc_x + newstate->crtc_w - 1;
	u32 y0 = newstate->crtc_y;
	u32 y1 = newstate->crtc_y + newstate->crtc_h - 1;
	u32 src_x, src_y, src_w, src_h;
	u32 val, pitch_in_bytes, line_length, line_number, ahbp, avbp, bpcr;
	u32 paddr, paddr1, paddr2;
	enum ltdc_pix_fmt pf;

	if (!newstate->crtc || !fb) {
		DRM_DEBUG_DRIVER("fb or crtc NULL");
		return;
	}

	 
	src_x = newstate->src_x >> 16;
	src_y = newstate->src_y >> 16;
	src_w = newstate->src_w >> 16;
	src_h = newstate->src_h >> 16;

	DRM_DEBUG_DRIVER("plane:%d fb:%d (%dx%d)@(%d,%d) -> (%dx%d)@(%d,%d)\n",
			 plane->base.id, fb->base.id,
			 src_w, src_h, src_x, src_y,
			 newstate->crtc_w, newstate->crtc_h,
			 newstate->crtc_x, newstate->crtc_y);

	regmap_read(ldev->regmap, LTDC_BPCR, &bpcr);

	ahbp = (bpcr & BPCR_AHBP) >> 16;
	avbp = bpcr & BPCR_AVBP;

	 
	val = ((x1 + 1 + ahbp) << 16) + (x0 + 1 + ahbp);
	regmap_write_bits(ldev->regmap, LTDC_L1WHPCR + lofs,
			  LXWHPCR_WHSTPOS | LXWHPCR_WHSPPOS, val);

	 
	val = ((y1 + 1 + avbp) << 16) + (y0 + 1 + avbp);
	regmap_write_bits(ldev->regmap, LTDC_L1WVPCR + lofs,
			  LXWVPCR_WVSTPOS | LXWVPCR_WVSPPOS, val);

	 
	pf = to_ltdc_pixelformat(fb->format->format);
	for (val = 0; val < NB_PF; val++)
		if (ldev->caps.pix_fmt_hw[val] == pf)
			break;

	 
	if (ldev->caps.pix_fmt_flex && val == NB_PF)
		val = ltdc_set_flexible_pixel_format(plane, pf);

	if (val == NB_PF) {
		DRM_ERROR("Pixel format %.4s not supported\n",
			  (char *)&fb->format->format);
		val = 0;	 
	}
	regmap_write_bits(ldev->regmap, LTDC_L1PFCR + lofs, LXPFCR_PF, val);

	 
	val = newstate->alpha >> 8;
	regmap_write_bits(ldev->regmap, LTDC_L1CACR + lofs, LXCACR_CONSTA, val);

	 
	val = BF1_PAXCA | BF2_1PAXCA;
	if (!fb->format->has_alpha)
		val = BF1_CA | BF2_1CA;

	 
	if (ldev->caps.non_alpha_only_l1 &&
	    plane->type != DRM_PLANE_TYPE_PRIMARY)
		val = BF1_PAXCA | BF2_1PAXCA;

	if (ldev->caps.dynamic_zorder) {
		val |= (newstate->normalized_zpos << 16);
		regmap_write_bits(ldev->regmap, LTDC_L1BFCR + lofs,
				  LXBFCR_BF2 | LXBFCR_BF1 | LXBFCR_BOR, val);
	} else {
		regmap_write_bits(ldev->regmap, LTDC_L1BFCR + lofs,
				  LXBFCR_BF2 | LXBFCR_BF1, val);
	}

	 
	paddr = (u32)drm_fb_dma_get_gem_addr(fb, newstate, 0);

	if (newstate->rotation & DRM_MODE_REFLECT_X)
		paddr += (fb->format->cpp[0] * (x1 - x0 + 1)) - 1;

	if (newstate->rotation & DRM_MODE_REFLECT_Y)
		paddr += (fb->pitches[0] * (y1 - y0));

	DRM_DEBUG_DRIVER("fb: phys 0x%08x", paddr);
	regmap_write(ldev->regmap, LTDC_L1CFBAR + lofs, paddr);

	 
	line_length = fb->format->cpp[0] *
		      (x1 - x0 + 1) + (ldev->caps.bus_width >> 3) - 1;

	if (newstate->rotation & DRM_MODE_REFLECT_Y)
		 
		pitch_in_bytes = 0x10000 - fb->pitches[0];
	else
		pitch_in_bytes = fb->pitches[0];

	val = (pitch_in_bytes << 16) | line_length;
	regmap_write_bits(ldev->regmap, LTDC_L1CFBLR + lofs, LXCFBLR_CFBLL | LXCFBLR_CFBP, val);

	 
	line_number = y1 - y0 + 1;
	regmap_write_bits(ldev->regmap, LTDC_L1CFBLNR + lofs, LXCFBLNR_CFBLN, line_number);

	if (ldev->caps.ycbcr_input) {
		if (fb->format->is_yuv) {
			switch (fb->format->format) {
			case DRM_FORMAT_NV12:
			case DRM_FORMAT_NV21:
			 
			paddr1 = (u32)drm_fb_dma_get_gem_addr(fb, newstate, 1);

			if (newstate->rotation & DRM_MODE_REFLECT_X)
				paddr1 += ((fb->format->cpp[1] * (x1 - x0 + 1)) >> 1) - 1;

			if (newstate->rotation & DRM_MODE_REFLECT_Y)
				paddr1 += (fb->pitches[1] * (y1 - y0 - 1)) >> 1;

			regmap_write(ldev->regmap, LTDC_L1AFBA0R + lofs, paddr1);
			break;
			case DRM_FORMAT_YUV420:
			 
			paddr1 = (u32)drm_fb_dma_get_gem_addr(fb, newstate, 1);
			paddr2 = (u32)drm_fb_dma_get_gem_addr(fb, newstate, 2);

			if (newstate->rotation & DRM_MODE_REFLECT_X) {
				paddr1 += ((fb->format->cpp[1] * (x1 - x0 + 1)) >> 1) - 1;
				paddr2 += ((fb->format->cpp[2] * (x1 - x0 + 1)) >> 1) - 1;
			}

			if (newstate->rotation & DRM_MODE_REFLECT_Y) {
				paddr1 += (fb->pitches[1] * (y1 - y0 - 1)) >> 1;
				paddr2 += (fb->pitches[2] * (y1 - y0 - 1)) >> 1;
			}

			regmap_write(ldev->regmap, LTDC_L1AFBA0R + lofs, paddr1);
			regmap_write(ldev->regmap, LTDC_L1AFBA1R + lofs, paddr2);
			break;
			case DRM_FORMAT_YVU420:
			 
			paddr1 = (u32)drm_fb_dma_get_gem_addr(fb, newstate, 2);
			paddr2 = (u32)drm_fb_dma_get_gem_addr(fb, newstate, 1);

			if (newstate->rotation & DRM_MODE_REFLECT_X) {
				paddr1 += ((fb->format->cpp[1] * (x1 - x0 + 1)) >> 1) - 1;
				paddr2 += ((fb->format->cpp[2] * (x1 - x0 + 1)) >> 1) - 1;
			}

			if (newstate->rotation & DRM_MODE_REFLECT_Y) {
				paddr1 += (fb->pitches[1] * (y1 - y0 - 1)) >> 1;
				paddr2 += (fb->pitches[2] * (y1 - y0 - 1)) >> 1;
			}

			regmap_write(ldev->regmap, LTDC_L1AFBA0R + lofs, paddr1);
			regmap_write(ldev->regmap, LTDC_L1AFBA1R + lofs, paddr2);
			break;
			}

			 
			if (fb->format->num_planes > 1) {
				if (newstate->rotation & DRM_MODE_REFLECT_Y)
					 
					pitch_in_bytes = 0x10000 - fb->pitches[1];
				else
					pitch_in_bytes = fb->pitches[1];

				line_length = ((fb->format->cpp[1] * (x1 - x0 + 1)) >> 1) +
					      (ldev->caps.bus_width >> 3) - 1;

				 
				val = (pitch_in_bytes << 16) | line_length;
				regmap_write(ldev->regmap, LTDC_L1AFBLR + lofs, val);

				 
				val = line_number >> 1;
				regmap_write(ldev->regmap, LTDC_L1AFBLNR + lofs, val);
			}

			 
			ltdc_set_ycbcr_coeffs(plane);

			 
			ltdc_set_ycbcr_config(plane, fb->format->format);
		} else {
			 
			regmap_write(ldev->regmap, LTDC_L1PCR + lofs, 0);
		}
	}

	 
	val = fb->format->format == DRM_FORMAT_C8 ? LXCR_CLUTEN : 0;
	val |= LXCR_LEN;

	 
	if (newstate->rotation & DRM_MODE_REFLECT_X)
		val |= LXCR_HMEN;

	regmap_write_bits(ldev->regmap, LTDC_L1CR + lofs, LXCR_LEN | LXCR_CLUTEN | LXCR_HMEN, val);

	 
	if (ldev->caps.plane_reg_shadow)
		regmap_write_bits(ldev->regmap, LTDC_L1RCR + lofs,
				  LXRCR_IMR | LXRCR_VBR | LXRCR_GRMSK, LXRCR_VBR);

	ldev->plane_fpsi[plane->index].counter++;

	mutex_lock(&ldev->err_lock);
	if (ldev->transfer_err) {
		DRM_WARN("ltdc transfer error: %d\n", ldev->transfer_err);
		ldev->transfer_err = 0;
	}

	if (ldev->caps.fifo_threshold) {
		if (ldev->fifo_err) {
			DRM_WARN("ltdc fifo underrun: please verify display mode\n");
			ldev->fifo_err = 0;
		}
	} else {
		if (ldev->fifo_warn >= ldev->fifo_threshold) {
			DRM_WARN("ltdc fifo underrun: please verify display mode\n");
			ldev->fifo_warn = 0;
		}
	}
	mutex_unlock(&ldev->err_lock);
}

static void ltdc_plane_atomic_disable(struct drm_plane *plane,
				      struct drm_atomic_state *state)
{
	struct drm_plane_state *oldstate = drm_atomic_get_old_plane_state(state,
									  plane);
	struct ltdc_device *ldev = plane_to_ltdc(plane);
	u32 lofs = plane->index * LAY_OFS;

	 
	regmap_write_bits(ldev->regmap, LTDC_L1CR + lofs, LXCR_LEN | LXCR_CLUTEN |  LXCR_HMEN, 0);

	 
	if (ldev->caps.plane_reg_shadow)
		regmap_write_bits(ldev->regmap, LTDC_L1RCR + lofs,
				  LXRCR_IMR | LXRCR_VBR | LXRCR_GRMSK, LXRCR_VBR);

	DRM_DEBUG_DRIVER("CRTC:%d plane:%d\n",
			 oldstate->crtc->base.id, plane->base.id);
}

static void ltdc_plane_atomic_print_state(struct drm_printer *p,
					  const struct drm_plane_state *state)
{
	struct drm_plane *plane = state->plane;
	struct ltdc_device *ldev = plane_to_ltdc(plane);
	struct fps_info *fpsi = &ldev->plane_fpsi[plane->index];
	int ms_since_last;
	ktime_t now;

	now = ktime_get();
	ms_since_last = ktime_to_ms(ktime_sub(now, fpsi->last_timestamp));

	drm_printf(p, "\tuser_updates=%dfps\n",
		   DIV_ROUND_CLOSEST(fpsi->counter * 1000, ms_since_last));

	fpsi->last_timestamp = now;
	fpsi->counter = 0;
}

static const struct drm_plane_funcs ltdc_plane_funcs = {
	.update_plane = drm_atomic_helper_update_plane,
	.disable_plane = drm_atomic_helper_disable_plane,
	.destroy = drm_plane_cleanup,
	.reset = drm_atomic_helper_plane_reset,
	.atomic_duplicate_state = drm_atomic_helper_plane_duplicate_state,
	.atomic_destroy_state = drm_atomic_helper_plane_destroy_state,
	.atomic_print_state = ltdc_plane_atomic_print_state,
};

static const struct drm_plane_helper_funcs ltdc_plane_helper_funcs = {
	.atomic_check = ltdc_plane_atomic_check,
	.atomic_update = ltdc_plane_atomic_update,
	.atomic_disable = ltdc_plane_atomic_disable,
};

static struct drm_plane *ltdc_plane_create(struct drm_device *ddev,
					   enum drm_plane_type type,
					   int index)
{
	unsigned long possible_crtcs = CRTC_MASK;
	struct ltdc_device *ldev = ddev->dev_private;
	struct device *dev = ddev->dev;
	struct drm_plane *plane;
	unsigned int i, nb_fmt = 0;
	u32 *formats;
	u32 drm_fmt;
	const u64 *modifiers = ltdc_format_modifiers;
	u32 lofs = index * LAY_OFS;
	u32 val;
	int ret;

	 
	formats = devm_kzalloc(dev, (ldev->caps.pix_fmt_nb +
			       ARRAY_SIZE(ltdc_drm_fmt_ycbcr_cp) +
			       ARRAY_SIZE(ltdc_drm_fmt_ycbcr_sp) +
			       ARRAY_SIZE(ltdc_drm_fmt_ycbcr_fp)) *
			       sizeof(*formats), GFP_KERNEL);

	for (i = 0; i < ldev->caps.pix_fmt_nb; i++) {
		drm_fmt = ldev->caps.pix_fmt_drm[i];

		 
		if (ldev->caps.non_alpha_only_l1)
			 
			if (type != DRM_PLANE_TYPE_PRIMARY && is_xrgb(drm_fmt))
				continue;

		formats[nb_fmt++] = drm_fmt;
	}

	 
	if (ldev->caps.ycbcr_input) {
		regmap_read(ldev->regmap, LTDC_L1C1R + lofs, &val);
		if (val & LXCR_C1R_YIA) {
			memcpy(&formats[nb_fmt], ltdc_drm_fmt_ycbcr_cp,
			       ARRAY_SIZE(ltdc_drm_fmt_ycbcr_cp) * sizeof(*formats));
			nb_fmt += ARRAY_SIZE(ltdc_drm_fmt_ycbcr_cp);
		}
		if (val & LXCR_C1R_YSPA) {
			memcpy(&formats[nb_fmt], ltdc_drm_fmt_ycbcr_sp,
			       ARRAY_SIZE(ltdc_drm_fmt_ycbcr_sp) * sizeof(*formats));
			nb_fmt += ARRAY_SIZE(ltdc_drm_fmt_ycbcr_sp);
		}
		if (val & LXCR_C1R_YFPA) {
			memcpy(&formats[nb_fmt], ltdc_drm_fmt_ycbcr_fp,
			       ARRAY_SIZE(ltdc_drm_fmt_ycbcr_fp) * sizeof(*formats));
			nb_fmt += ARRAY_SIZE(ltdc_drm_fmt_ycbcr_fp);
		}
	}

	plane = devm_kzalloc(dev, sizeof(*plane), GFP_KERNEL);
	if (!plane)
		return NULL;

	ret = drm_universal_plane_init(ddev, plane, possible_crtcs,
				       &ltdc_plane_funcs, formats, nb_fmt,
				       modifiers, type, NULL);
	if (ret < 0)
		return NULL;

	if (ldev->caps.ycbcr_input) {
		if (val & (LXCR_C1R_YIA | LXCR_C1R_YSPA | LXCR_C1R_YFPA))
			drm_plane_create_color_properties(plane,
							  BIT(DRM_COLOR_YCBCR_BT601) |
							  BIT(DRM_COLOR_YCBCR_BT709),
							  BIT(DRM_COLOR_YCBCR_LIMITED_RANGE) |
							  BIT(DRM_COLOR_YCBCR_FULL_RANGE),
							  DRM_COLOR_YCBCR_BT601,
							  DRM_COLOR_YCBCR_LIMITED_RANGE);
	}

	drm_plane_helper_add(plane, &ltdc_plane_helper_funcs);

	drm_plane_create_alpha_property(plane);

	DRM_DEBUG_DRIVER("plane:%d created\n", plane->base.id);

	return plane;
}

static void ltdc_plane_destroy_all(struct drm_device *ddev)
{
	struct drm_plane *plane, *plane_temp;

	list_for_each_entry_safe(plane, plane_temp,
				 &ddev->mode_config.plane_list, head)
		drm_plane_cleanup(plane);
}

static int ltdc_crtc_init(struct drm_device *ddev, struct drm_crtc *crtc)
{
	struct ltdc_device *ldev = ddev->dev_private;
	struct drm_plane *primary, *overlay;
	int supported_rotations = DRM_MODE_ROTATE_0 | DRM_MODE_REFLECT_X | DRM_MODE_REFLECT_Y;
	unsigned int i;
	int ret;

	primary = ltdc_plane_create(ddev, DRM_PLANE_TYPE_PRIMARY, 0);
	if (!primary) {
		DRM_ERROR("Can not create primary plane\n");
		return -EINVAL;
	}

	if (ldev->caps.dynamic_zorder)
		drm_plane_create_zpos_property(primary, 0, 0, ldev->caps.nb_layers - 1);
	else
		drm_plane_create_zpos_immutable_property(primary, 0);

	if (ldev->caps.plane_rotation)
		drm_plane_create_rotation_property(primary, DRM_MODE_ROTATE_0,
						   supported_rotations);

	 
	if (ldev->caps.crc)
		ret = drm_crtc_init_with_planes(ddev, crtc, primary, NULL,
						&ltdc_crtc_with_crc_support_funcs, NULL);
	else
		ret = drm_crtc_init_with_planes(ddev, crtc, primary, NULL,
						&ltdc_crtc_funcs, NULL);
	if (ret) {
		DRM_ERROR("Can not initialize CRTC\n");
		goto cleanup;
	}

	drm_crtc_helper_add(crtc, &ltdc_crtc_helper_funcs);

	drm_mode_crtc_set_gamma_size(crtc, CLUT_SIZE);
	drm_crtc_enable_color_mgmt(crtc, 0, false, CLUT_SIZE);

	DRM_DEBUG_DRIVER("CRTC:%d created\n", crtc->base.id);

	 
	for (i = 1; i < ldev->caps.nb_layers; i++) {
		overlay = ltdc_plane_create(ddev, DRM_PLANE_TYPE_OVERLAY, i);
		if (!overlay) {
			ret = -ENOMEM;
			DRM_ERROR("Can not create overlay plane %d\n", i);
			goto cleanup;
		}
		if (ldev->caps.dynamic_zorder)
			drm_plane_create_zpos_property(overlay, i, 0, ldev->caps.nb_layers - 1);
		else
			drm_plane_create_zpos_immutable_property(overlay, i);

		if (ldev->caps.plane_rotation)
			drm_plane_create_rotation_property(overlay, DRM_MODE_ROTATE_0,
							   supported_rotations);
	}

	return 0;

cleanup:
	ltdc_plane_destroy_all(ddev);
	return ret;
}

static void ltdc_encoder_disable(struct drm_encoder *encoder)
{
	struct drm_device *ddev = encoder->dev;
	struct ltdc_device *ldev = ddev->dev_private;

	DRM_DEBUG_DRIVER("\n");

	 
	regmap_clear_bits(ldev->regmap, LTDC_GCR, GCR_LTDCEN);

	 
	pinctrl_pm_select_sleep_state(ddev->dev);
}

static void ltdc_encoder_enable(struct drm_encoder *encoder)
{
	struct drm_device *ddev = encoder->dev;
	struct ltdc_device *ldev = ddev->dev_private;

	DRM_DEBUG_DRIVER("\n");

	 
	if (ldev->caps.fifo_threshold)
		regmap_write(ldev->regmap, LTDC_FUT, ldev->fifo_threshold);

	 
	regmap_set_bits(ldev->regmap, LTDC_GCR, GCR_LTDCEN);
}

static void ltdc_encoder_mode_set(struct drm_encoder *encoder,
				  struct drm_display_mode *mode,
				  struct drm_display_mode *adjusted_mode)
{
	struct drm_device *ddev = encoder->dev;

	DRM_DEBUG_DRIVER("\n");

	 
	if (encoder->encoder_type == DRM_MODE_ENCODER_DPI)
		pinctrl_pm_select_default_state(ddev->dev);
}

static const struct drm_encoder_helper_funcs ltdc_encoder_helper_funcs = {
	.disable = ltdc_encoder_disable,
	.enable = ltdc_encoder_enable,
	.mode_set = ltdc_encoder_mode_set,
};

static int ltdc_encoder_init(struct drm_device *ddev, struct drm_bridge *bridge)
{
	struct drm_encoder *encoder;
	int ret;

	encoder = devm_kzalloc(ddev->dev, sizeof(*encoder), GFP_KERNEL);
	if (!encoder)
		return -ENOMEM;

	encoder->possible_crtcs = CRTC_MASK;
	encoder->possible_clones = 0;	 

	drm_simple_encoder_init(ddev, encoder, DRM_MODE_ENCODER_DPI);

	drm_encoder_helper_add(encoder, &ltdc_encoder_helper_funcs);

	ret = drm_bridge_attach(encoder, bridge, NULL, 0);
	if (ret) {
		if (ret != -EPROBE_DEFER)
			drm_encoder_cleanup(encoder);
		return ret;
	}

	DRM_DEBUG_DRIVER("Bridge encoder:%d created\n", encoder->base.id);

	return 0;
}

static int ltdc_get_caps(struct drm_device *ddev)
{
	struct ltdc_device *ldev = ddev->dev_private;
	u32 bus_width_log2, lcr, gc2r;

	 
	regmap_read(ldev->regmap, LTDC_LCR, &lcr);

	ldev->caps.nb_layers = clamp((int)lcr, 1, LTDC_MAX_LAYER);

	 
	regmap_read(ldev->regmap, LTDC_GC2R, &gc2r);
	bus_width_log2 = (gc2r & GC2R_BW) >> 4;
	ldev->caps.bus_width = 8 << bus_width_log2;
	regmap_read(ldev->regmap, LTDC_IDR, &ldev->caps.hw_version);

	switch (ldev->caps.hw_version) {
	case HWVER_10200:
	case HWVER_10300:
		ldev->caps.layer_ofs = LAY_OFS_0;
		ldev->caps.layer_regs = ltdc_layer_regs_a0;
		ldev->caps.pix_fmt_hw = ltdc_pix_fmt_a0;
		ldev->caps.pix_fmt_drm = ltdc_drm_fmt_a0;
		ldev->caps.pix_fmt_nb = ARRAY_SIZE(ltdc_drm_fmt_a0);
		ldev->caps.pix_fmt_flex = false;
		 
		ldev->caps.non_alpha_only_l1 = true;
		ldev->caps.pad_max_freq_hz = 90000000;
		if (ldev->caps.hw_version == HWVER_10200)
			ldev->caps.pad_max_freq_hz = 65000000;
		ldev->caps.nb_irq = 2;
		ldev->caps.ycbcr_input = false;
		ldev->caps.ycbcr_output = false;
		ldev->caps.plane_reg_shadow = false;
		ldev->caps.crc = false;
		ldev->caps.dynamic_zorder = false;
		ldev->caps.plane_rotation = false;
		ldev->caps.fifo_threshold = false;
		break;
	case HWVER_20101:
		ldev->caps.layer_ofs = LAY_OFS_0;
		ldev->caps.layer_regs = ltdc_layer_regs_a1;
		ldev->caps.pix_fmt_hw = ltdc_pix_fmt_a1;
		ldev->caps.pix_fmt_drm = ltdc_drm_fmt_a1;
		ldev->caps.pix_fmt_nb = ARRAY_SIZE(ltdc_drm_fmt_a1);
		ldev->caps.pix_fmt_flex = false;
		ldev->caps.non_alpha_only_l1 = false;
		ldev->caps.pad_max_freq_hz = 150000000;
		ldev->caps.nb_irq = 4;
		ldev->caps.ycbcr_input = false;
		ldev->caps.ycbcr_output = false;
		ldev->caps.plane_reg_shadow = false;
		ldev->caps.crc = false;
		ldev->caps.dynamic_zorder = false;
		ldev->caps.plane_rotation = false;
		ldev->caps.fifo_threshold = false;
		break;
	case HWVER_40100:
		ldev->caps.layer_ofs = LAY_OFS_1;
		ldev->caps.layer_regs = ltdc_layer_regs_a2;
		ldev->caps.pix_fmt_hw = ltdc_pix_fmt_a2;
		ldev->caps.pix_fmt_drm = ltdc_drm_fmt_a2;
		ldev->caps.pix_fmt_nb = ARRAY_SIZE(ltdc_drm_fmt_a2);
		ldev->caps.pix_fmt_flex = true;
		ldev->caps.non_alpha_only_l1 = false;
		ldev->caps.pad_max_freq_hz = 90000000;
		ldev->caps.nb_irq = 2;
		ldev->caps.ycbcr_input = true;
		ldev->caps.ycbcr_output = true;
		ldev->caps.plane_reg_shadow = true;
		ldev->caps.crc = true;
		ldev->caps.dynamic_zorder = true;
		ldev->caps.plane_rotation = true;
		ldev->caps.fifo_threshold = true;
		break;
	default:
		return -ENODEV;
	}

	return 0;
}

void ltdc_suspend(struct drm_device *ddev)
{
	struct ltdc_device *ldev = ddev->dev_private;

	DRM_DEBUG_DRIVER("\n");
	clk_disable_unprepare(ldev->pixel_clk);
}

int ltdc_resume(struct drm_device *ddev)
{
	struct ltdc_device *ldev = ddev->dev_private;
	int ret;

	DRM_DEBUG_DRIVER("\n");

	ret = clk_prepare_enable(ldev->pixel_clk);
	if (ret) {
		DRM_ERROR("failed to enable pixel clock (%d)\n", ret);
		return ret;
	}

	return 0;
}

int ltdc_load(struct drm_device *ddev)
{
	struct platform_device *pdev = to_platform_device(ddev->dev);
	struct ltdc_device *ldev = ddev->dev_private;
	struct device *dev = ddev->dev;
	struct device_node *np = dev->of_node;
	struct drm_bridge *bridge;
	struct drm_panel *panel;
	struct drm_crtc *crtc;
	struct reset_control *rstc;
	struct resource *res;
	int irq, i, nb_endpoints;
	int ret = -ENODEV;

	DRM_DEBUG_DRIVER("\n");

	 
	nb_endpoints = of_graph_get_endpoint_count(np);
	if (!nb_endpoints)
		return -ENODEV;

	ldev->pixel_clk = devm_clk_get(dev, "lcd");
	if (IS_ERR(ldev->pixel_clk)) {
		if (PTR_ERR(ldev->pixel_clk) != -EPROBE_DEFER)
			DRM_ERROR("Unable to get lcd clock\n");
		return PTR_ERR(ldev->pixel_clk);
	}

	if (clk_prepare_enable(ldev->pixel_clk)) {
		DRM_ERROR("Unable to prepare pixel clock\n");
		return -ENODEV;
	}

	 
	for (i = 0; i < nb_endpoints; i++) {
		ret = drm_of_find_panel_or_bridge(np, 0, i, &panel, &bridge);

		 
		if (ret == -ENODEV)
			continue;
		else if (ret)
			goto err;

		if (panel) {
			bridge = drm_panel_bridge_add_typed(panel,
							    DRM_MODE_CONNECTOR_DPI);
			if (IS_ERR(bridge)) {
				DRM_ERROR("panel-bridge endpoint %d\n", i);
				ret = PTR_ERR(bridge);
				goto err;
			}
		}

		if (bridge) {
			ret = ltdc_encoder_init(ddev, bridge);
			if (ret) {
				if (ret != -EPROBE_DEFER)
					DRM_ERROR("init encoder endpoint %d\n", i);
				goto err;
			}
		}
	}

	rstc = devm_reset_control_get_exclusive(dev, NULL);

	mutex_init(&ldev->err_lock);

	if (!IS_ERR(rstc)) {
		reset_control_assert(rstc);
		usleep_range(10, 20);
		reset_control_deassert(rstc);
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	ldev->regs = devm_ioremap_resource(dev, res);
	if (IS_ERR(ldev->regs)) {
		DRM_ERROR("Unable to get ltdc registers\n");
		ret = PTR_ERR(ldev->regs);
		goto err;
	}

	ldev->regmap = devm_regmap_init_mmio(&pdev->dev, ldev->regs, &stm32_ltdc_regmap_cfg);
	if (IS_ERR(ldev->regmap)) {
		DRM_ERROR("Unable to regmap ltdc registers\n");
		ret = PTR_ERR(ldev->regmap);
		goto err;
	}

	ret = ltdc_get_caps(ddev);
	if (ret) {
		DRM_ERROR("hardware identifier (0x%08x) not supported!\n",
			  ldev->caps.hw_version);
		goto err;
	}

	 
	if (ldev->caps.fifo_threshold)
		regmap_clear_bits(ldev->regmap, LTDC_IER, IER_LIE | IER_RRIE | IER_FUWIE |
				  IER_TERRIE);
	else
		regmap_clear_bits(ldev->regmap, LTDC_IER, IER_LIE | IER_RRIE | IER_FUWIE |
				  IER_TERRIE | IER_FUEIE);

	DRM_DEBUG_DRIVER("ltdc hw version 0x%08x\n", ldev->caps.hw_version);

	 
	ldev->transfer_err = 0;
	ldev->fifo_err = 0;
	ldev->fifo_warn = 0;
	ldev->fifo_threshold = FUT_DFT;

	for (i = 0; i < ldev->caps.nb_irq; i++) {
		irq = platform_get_irq(pdev, i);
		if (irq < 0) {
			ret = irq;
			goto err;
		}

		ret = devm_request_threaded_irq(dev, irq, ltdc_irq,
						ltdc_irq_thread, IRQF_ONESHOT,
						dev_name(dev), ddev);
		if (ret) {
			DRM_ERROR("Failed to register LTDC interrupt\n");
			goto err;
		}
	}

	crtc = devm_kzalloc(dev, sizeof(*crtc), GFP_KERNEL);
	if (!crtc) {
		DRM_ERROR("Failed to allocate crtc\n");
		ret = -ENOMEM;
		goto err;
	}

	ret = ltdc_crtc_init(ddev, crtc);
	if (ret) {
		DRM_ERROR("Failed to init crtc\n");
		goto err;
	}

	ret = drm_vblank_init(ddev, NB_CRTC);
	if (ret) {
		DRM_ERROR("Failed calling drm_vblank_init()\n");
		goto err;
	}

	clk_disable_unprepare(ldev->pixel_clk);

	pinctrl_pm_select_sleep_state(ddev->dev);

	pm_runtime_enable(ddev->dev);

	return 0;
err:
	for (i = 0; i < nb_endpoints; i++)
		drm_of_panel_bridge_remove(ddev->dev->of_node, 0, i);

	clk_disable_unprepare(ldev->pixel_clk);

	return ret;
}

void ltdc_unload(struct drm_device *ddev)
{
	struct device *dev = ddev->dev;
	int nb_endpoints, i;

	DRM_DEBUG_DRIVER("\n");

	nb_endpoints = of_graph_get_endpoint_count(dev->of_node);

	for (i = 0; i < nb_endpoints; i++)
		drm_of_panel_bridge_remove(ddev->dev->of_node, 0, i);

	pm_runtime_disable(ddev->dev);
}

MODULE_AUTHOR("Philippe Cornu <philippe.cornu@st.com>");
MODULE_AUTHOR("Yannick Fertre <yannick.fertre@st.com>");
MODULE_AUTHOR("Fabien Dessenne <fabien.dessenne@st.com>");
MODULE_AUTHOR("Mickael Reulier <mickael.reulier@st.com>");
MODULE_DESCRIPTION("STMicroelectronics ST DRM LTDC driver");
MODULE_LICENSE("GPL v2");
