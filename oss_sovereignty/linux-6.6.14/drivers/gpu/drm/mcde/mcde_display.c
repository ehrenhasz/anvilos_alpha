
 
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/dma-buf.h>
#include <linux/regulator/consumer.h>
#include <linux/media-bus-format.h>

#include <drm/drm_device.h>
#include <drm/drm_fb_dma_helper.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem_atomic_helper.h>
#include <drm/drm_gem_dma_helper.h>
#include <drm/drm_mipi_dsi.h>
#include <drm/drm_simple_kms_helper.h>
#include <drm/drm_bridge.h>
#include <drm/drm_vblank.h>
#include <video/mipi_display.h>

#include "mcde_drm.h"
#include "mcde_display_regs.h"

enum mcde_fifo {
	MCDE_FIFO_A,
	MCDE_FIFO_B,
	 
};

enum mcde_channel {
	MCDE_CHANNEL_0 = 0,
	MCDE_CHANNEL_1,
	MCDE_CHANNEL_2,
	MCDE_CHANNEL_3,
};

enum mcde_extsrc {
	MCDE_EXTSRC_0 = 0,
	MCDE_EXTSRC_1,
	MCDE_EXTSRC_2,
	MCDE_EXTSRC_3,
	MCDE_EXTSRC_4,
	MCDE_EXTSRC_5,
	MCDE_EXTSRC_6,
	MCDE_EXTSRC_7,
	MCDE_EXTSRC_8,
	MCDE_EXTSRC_9,
};

enum mcde_overlay {
	MCDE_OVERLAY_0 = 0,
	MCDE_OVERLAY_1,
	MCDE_OVERLAY_2,
	MCDE_OVERLAY_3,
	MCDE_OVERLAY_4,
	MCDE_OVERLAY_5,
};

enum mcde_formatter {
	MCDE_DSI_FORMATTER_0 = 0,
	MCDE_DSI_FORMATTER_1,
	MCDE_DSI_FORMATTER_2,
	MCDE_DSI_FORMATTER_3,
	MCDE_DSI_FORMATTER_4,
	MCDE_DSI_FORMATTER_5,
	MCDE_DPI_FORMATTER_0,
	MCDE_DPI_FORMATTER_1,
};

void mcde_display_irq(struct mcde *mcde)
{
	u32 mispp, misovl, mischnl;
	bool vblank = false;

	 
	mispp = readl(mcde->regs + MCDE_MISPP);
	misovl = readl(mcde->regs + MCDE_MISOVL);
	mischnl = readl(mcde->regs + MCDE_MISCHNL);

	 
	if (!mcde->dpi_output && mcde_dsi_irq(mcde->mdsi)) {
		u32 val;

		 
		if (mcde->flow_mode == MCDE_COMMAND_ONESHOT_FLOW) {
			spin_lock(&mcde->flow_lock);
			if (--mcde->flow_active == 0) {
				dev_dbg(mcde->dev, "TE0 IRQ\n");
				 
				val = readl(mcde->regs + MCDE_CRA0);
				val &= ~MCDE_CRX0_FLOEN;
				writel(val, mcde->regs + MCDE_CRA0);
			}
			spin_unlock(&mcde->flow_lock);
		}
	}

	 
	if (mispp & MCDE_PP_VCMPA) {
		dev_dbg(mcde->dev, "chnl A vblank IRQ\n");
		vblank = true;
	}
	if (mispp & MCDE_PP_VCMPB) {
		dev_dbg(mcde->dev, "chnl B vblank IRQ\n");
		vblank = true;
	}
	if (mispp & MCDE_PP_VCMPC0)
		dev_dbg(mcde->dev, "chnl C0 vblank IRQ\n");
	if (mispp & MCDE_PP_VCMPC1)
		dev_dbg(mcde->dev, "chnl C1 vblank IRQ\n");
	if (mispp & MCDE_PP_VSCC0)
		dev_dbg(mcde->dev, "chnl C0 TE IRQ\n");
	if (mispp & MCDE_PP_VSCC1)
		dev_dbg(mcde->dev, "chnl C1 TE IRQ\n");
	writel(mispp, mcde->regs + MCDE_RISPP);

	if (vblank)
		drm_crtc_handle_vblank(&mcde->pipe.crtc);

	if (misovl)
		dev_info(mcde->dev, "some stray overlay IRQ %08x\n", misovl);
	writel(misovl, mcde->regs + MCDE_RISOVL);

	if (mischnl)
		dev_info(mcde->dev, "some stray channel error IRQ %08x\n",
			 mischnl);
	writel(mischnl, mcde->regs + MCDE_RISCHNL);
}

void mcde_display_disable_irqs(struct mcde *mcde)
{
	 
	writel(0, mcde->regs + MCDE_IMSCPP);
	writel(0, mcde->regs + MCDE_IMSCOVL);
	writel(0, mcde->regs + MCDE_IMSCCHNL);

	 
	writel(0xFFFFFFFF, mcde->regs + MCDE_RISPP);
	writel(0xFFFFFFFF, mcde->regs + MCDE_RISOVL);
	writel(0xFFFFFFFF, mcde->regs + MCDE_RISCHNL);
}

static int mcde_display_check(struct drm_simple_display_pipe *pipe,
			      struct drm_plane_state *pstate,
			      struct drm_crtc_state *cstate)
{
	const struct drm_display_mode *mode = &cstate->mode;
	struct drm_framebuffer *old_fb = pipe->plane.state->fb;
	struct drm_framebuffer *fb = pstate->fb;

	if (fb) {
		u32 offset = drm_fb_dma_get_gem_addr(fb, pstate, 0);

		 
		if (offset & 3) {
			DRM_DEBUG_KMS("FB not 32-bit aligned\n");
			return -EINVAL;
		}

		 
		if (fb->pitches[0] != mode->hdisplay * fb->format->cpp[0]) {
			DRM_DEBUG_KMS("can't handle pitches\n");
			return -EINVAL;
		}

		 
		if (old_fb && old_fb->format != fb->format)
			cstate->mode_changed = true;
	}

	return 0;
}

static int mcde_configure_extsrc(struct mcde *mcde, enum mcde_extsrc src,
				 u32 format)
{
	u32 val;
	u32 conf;
	u32 cr;

	switch (src) {
	case MCDE_EXTSRC_0:
		conf = MCDE_EXTSRC0CONF;
		cr = MCDE_EXTSRC0CR;
		break;
	case MCDE_EXTSRC_1:
		conf = MCDE_EXTSRC1CONF;
		cr = MCDE_EXTSRC1CR;
		break;
	case MCDE_EXTSRC_2:
		conf = MCDE_EXTSRC2CONF;
		cr = MCDE_EXTSRC2CR;
		break;
	case MCDE_EXTSRC_3:
		conf = MCDE_EXTSRC3CONF;
		cr = MCDE_EXTSRC3CR;
		break;
	case MCDE_EXTSRC_4:
		conf = MCDE_EXTSRC4CONF;
		cr = MCDE_EXTSRC4CR;
		break;
	case MCDE_EXTSRC_5:
		conf = MCDE_EXTSRC5CONF;
		cr = MCDE_EXTSRC5CR;
		break;
	case MCDE_EXTSRC_6:
		conf = MCDE_EXTSRC6CONF;
		cr = MCDE_EXTSRC6CR;
		break;
	case MCDE_EXTSRC_7:
		conf = MCDE_EXTSRC7CONF;
		cr = MCDE_EXTSRC7CR;
		break;
	case MCDE_EXTSRC_8:
		conf = MCDE_EXTSRC8CONF;
		cr = MCDE_EXTSRC8CR;
		break;
	case MCDE_EXTSRC_9:
		conf = MCDE_EXTSRC9CONF;
		cr = MCDE_EXTSRC9CR;
		break;
	}

	 
	val = 0 << MCDE_EXTSRCXCONF_BUF_ID_SHIFT;
	val |= 1 << MCDE_EXTSRCXCONF_BUF_NB_SHIFT;
	val |= 0 << MCDE_EXTSRCXCONF_PRI_OVLID_SHIFT;

	switch (format) {
	case DRM_FORMAT_ARGB8888:
		val |= MCDE_EXTSRCXCONF_BPP_ARGB8888 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		break;
	case DRM_FORMAT_ABGR8888:
		val |= MCDE_EXTSRCXCONF_BPP_ARGB8888 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		val |= MCDE_EXTSRCXCONF_BGR;
		break;
	case DRM_FORMAT_XRGB8888:
		val |= MCDE_EXTSRCXCONF_BPP_XRGB8888 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		break;
	case DRM_FORMAT_XBGR8888:
		val |= MCDE_EXTSRCXCONF_BPP_XRGB8888 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		val |= MCDE_EXTSRCXCONF_BGR;
		break;
	case DRM_FORMAT_RGB888:
		val |= MCDE_EXTSRCXCONF_BPP_RGB888 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		break;
	case DRM_FORMAT_BGR888:
		val |= MCDE_EXTSRCXCONF_BPP_RGB888 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		val |= MCDE_EXTSRCXCONF_BGR;
		break;
	case DRM_FORMAT_ARGB4444:
		val |= MCDE_EXTSRCXCONF_BPP_ARGB4444 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		break;
	case DRM_FORMAT_ABGR4444:
		val |= MCDE_EXTSRCXCONF_BPP_ARGB4444 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		val |= MCDE_EXTSRCXCONF_BGR;
		break;
	case DRM_FORMAT_XRGB4444:
		val |= MCDE_EXTSRCXCONF_BPP_RGB444 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		break;
	case DRM_FORMAT_XBGR4444:
		val |= MCDE_EXTSRCXCONF_BPP_RGB444 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		val |= MCDE_EXTSRCXCONF_BGR;
		break;
	case DRM_FORMAT_XRGB1555:
		val |= MCDE_EXTSRCXCONF_BPP_IRGB1555 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		break;
	case DRM_FORMAT_XBGR1555:
		val |= MCDE_EXTSRCXCONF_BPP_IRGB1555 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		val |= MCDE_EXTSRCXCONF_BGR;
		break;
	case DRM_FORMAT_RGB565:
		val |= MCDE_EXTSRCXCONF_BPP_RGB565 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		break;
	case DRM_FORMAT_BGR565:
		val |= MCDE_EXTSRCXCONF_BPP_RGB565 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		val |= MCDE_EXTSRCXCONF_BGR;
		break;
	case DRM_FORMAT_YUV422:
		val |= MCDE_EXTSRCXCONF_BPP_YCBCR422 <<
			MCDE_EXTSRCXCONF_BPP_SHIFT;
		break;
	default:
		dev_err(mcde->dev, "Unknown pixel format 0x%08x\n",
			format);
		return -EINVAL;
	}
	writel(val, mcde->regs + conf);

	 
	val = MCDE_EXTSRCXCR_SEL_MOD_SOFTWARE_SEL;
	val |= MCDE_EXTSRCXCR_MULTIOVL_CTRL_PRIMARY;
	writel(val, mcde->regs + cr);

	return 0;
}

static void mcde_configure_overlay(struct mcde *mcde, enum mcde_overlay ovl,
				   enum mcde_extsrc src,
				   enum mcde_channel ch,
				   const struct drm_display_mode *mode,
				   u32 format, int cpp)
{
	u32 val;
	u32 conf1;
	u32 conf2;
	u32 crop;
	u32 ljinc;
	u32 cr;
	u32 comp;
	u32 pixel_fetcher_watermark;

	switch (ovl) {
	case MCDE_OVERLAY_0:
		conf1 = MCDE_OVL0CONF;
		conf2 = MCDE_OVL0CONF2;
		crop = MCDE_OVL0CROP;
		ljinc = MCDE_OVL0LJINC;
		cr = MCDE_OVL0CR;
		comp = MCDE_OVL0COMP;
		break;
	case MCDE_OVERLAY_1:
		conf1 = MCDE_OVL1CONF;
		conf2 = MCDE_OVL1CONF2;
		crop = MCDE_OVL1CROP;
		ljinc = MCDE_OVL1LJINC;
		cr = MCDE_OVL1CR;
		comp = MCDE_OVL1COMP;
		break;
	case MCDE_OVERLAY_2:
		conf1 = MCDE_OVL2CONF;
		conf2 = MCDE_OVL2CONF2;
		crop = MCDE_OVL2CROP;
		ljinc = MCDE_OVL2LJINC;
		cr = MCDE_OVL2CR;
		comp = MCDE_OVL2COMP;
		break;
	case MCDE_OVERLAY_3:
		conf1 = MCDE_OVL3CONF;
		conf2 = MCDE_OVL3CONF2;
		crop = MCDE_OVL3CROP;
		ljinc = MCDE_OVL3LJINC;
		cr = MCDE_OVL3CR;
		comp = MCDE_OVL3COMP;
		break;
	case MCDE_OVERLAY_4:
		conf1 = MCDE_OVL4CONF;
		conf2 = MCDE_OVL4CONF2;
		crop = MCDE_OVL4CROP;
		ljinc = MCDE_OVL4LJINC;
		cr = MCDE_OVL4CR;
		comp = MCDE_OVL4COMP;
		break;
	case MCDE_OVERLAY_5:
		conf1 = MCDE_OVL5CONF;
		conf2 = MCDE_OVL5CONF2;
		crop = MCDE_OVL5CROP;
		ljinc = MCDE_OVL5LJINC;
		cr = MCDE_OVL5CR;
		comp = MCDE_OVL5COMP;
		break;
	}

	val = mode->hdisplay << MCDE_OVLXCONF_PPL_SHIFT;
	val |= mode->vdisplay << MCDE_OVLXCONF_LPF_SHIFT;
	 
	val |= src << MCDE_OVLXCONF_EXTSRC_ID_SHIFT;
	writel(val, mcde->regs + conf1);

	val = MCDE_OVLXCONF2_BP_PER_PIXEL_ALPHA;
	val |= 0xff << MCDE_OVLXCONF2_ALPHAVALUE_SHIFT;
	 
	switch (format) {
	case DRM_FORMAT_ARGB8888:
	case DRM_FORMAT_ABGR8888:
	case DRM_FORMAT_ARGB4444:
	case DRM_FORMAT_ABGR4444:
	case DRM_FORMAT_XRGB1555:
	case DRM_FORMAT_XBGR1555:
		 
		break;
	case DRM_FORMAT_XRGB8888:
	case DRM_FORMAT_XBGR8888:
	case DRM_FORMAT_RGB888:
	case DRM_FORMAT_BGR888:
	case DRM_FORMAT_RGB565:
	case DRM_FORMAT_BGR565:
	case DRM_FORMAT_YUV422:
		val |= MCDE_OVLXCONF2_OPQ;
		break;
	default:
		dev_err(mcde->dev, "Unknown pixel format 0x%08x\n",
			format);
		break;
	}

	 
	switch (cpp) {
	case 2:
		pixel_fetcher_watermark = 128;
		break;
	case 3:
		pixel_fetcher_watermark = 96;
		break;
	case 4:
		pixel_fetcher_watermark = 48;
		break;
	default:
		pixel_fetcher_watermark = 48;
		break;
	}
	dev_dbg(mcde->dev, "pixel fetcher watermark level %d pixels\n",
		pixel_fetcher_watermark);
	val |= pixel_fetcher_watermark << MCDE_OVLXCONF2_PIXELFETCHERWATERMARKLEVEL_SHIFT;
	writel(val, mcde->regs + conf2);

	 
	writel(mcde->stride, mcde->regs + ljinc);
	 
	writel(0, mcde->regs + crop);

	 
	val = MCDE_OVLXCR_OVLEN;
	val |= MCDE_OVLXCR_COLCCTRL_DISABLED;
	val |= MCDE_OVLXCR_BURSTSIZE_8W <<
		MCDE_OVLXCR_BURSTSIZE_SHIFT;
	val |= MCDE_OVLXCR_MAXOUTSTANDING_8_REQ <<
		MCDE_OVLXCR_MAXOUTSTANDING_SHIFT;
	 
	val |= MCDE_OVLXCR_ROTBURSTSIZE_8W <<
		MCDE_OVLXCR_ROTBURSTSIZE_SHIFT;
	writel(val, mcde->regs + cr);

	 
	val = ch << MCDE_OVLXCOMP_CH_ID_SHIFT;
	writel(val, mcde->regs + comp);
}

static void mcde_configure_channel(struct mcde *mcde, enum mcde_channel ch,
				   enum mcde_fifo fifo,
				   const struct drm_display_mode *mode)
{
	u32 val;
	u32 conf;
	u32 sync;
	u32 stat;
	u32 bgcol;
	u32 mux;

	switch (ch) {
	case MCDE_CHANNEL_0:
		conf = MCDE_CHNL0CONF;
		sync = MCDE_CHNL0SYNCHMOD;
		stat = MCDE_CHNL0STAT;
		bgcol = MCDE_CHNL0BCKGNDCOL;
		mux = MCDE_CHNL0MUXING;
		break;
	case MCDE_CHANNEL_1:
		conf = MCDE_CHNL1CONF;
		sync = MCDE_CHNL1SYNCHMOD;
		stat = MCDE_CHNL1STAT;
		bgcol = MCDE_CHNL1BCKGNDCOL;
		mux = MCDE_CHNL1MUXING;
		break;
	case MCDE_CHANNEL_2:
		conf = MCDE_CHNL2CONF;
		sync = MCDE_CHNL2SYNCHMOD;
		stat = MCDE_CHNL2STAT;
		bgcol = MCDE_CHNL2BCKGNDCOL;
		mux = MCDE_CHNL2MUXING;
		break;
	case MCDE_CHANNEL_3:
		conf = MCDE_CHNL3CONF;
		sync = MCDE_CHNL3SYNCHMOD;
		stat = MCDE_CHNL3STAT;
		bgcol = MCDE_CHNL3BCKGNDCOL;
		mux = MCDE_CHNL3MUXING;
		return;
	}

	 
	switch (mcde->flow_mode) {
	case MCDE_COMMAND_ONESHOT_FLOW:
		 
		val = MCDE_CHNLXSYNCHMOD_SRC_SYNCH_SOFTWARE
			<< MCDE_CHNLXSYNCHMOD_SRC_SYNCH_SHIFT;
		break;
	case MCDE_COMMAND_TE_FLOW:
		val = MCDE_CHNLXSYNCHMOD_SRC_SYNCH_HARDWARE
			<< MCDE_CHNLXSYNCHMOD_SRC_SYNCH_SHIFT;
		val |= MCDE_CHNLXSYNCHMOD_OUT_SYNCH_SRC_TE0
			<< MCDE_CHNLXSYNCHMOD_OUT_SYNCH_SRC_SHIFT;
		break;
	case MCDE_COMMAND_BTA_TE_FLOW:
		val = MCDE_CHNLXSYNCHMOD_SRC_SYNCH_HARDWARE
			<< MCDE_CHNLXSYNCHMOD_SRC_SYNCH_SHIFT;
		 
		val |= MCDE_CHNLXSYNCHMOD_OUT_SYNCH_SRC_FORMATTER
			<< MCDE_CHNLXSYNCHMOD_OUT_SYNCH_SRC_SHIFT;
		break;
	case MCDE_VIDEO_TE_FLOW:
		val = MCDE_CHNLXSYNCHMOD_SRC_SYNCH_HARDWARE
			<< MCDE_CHNLXSYNCHMOD_SRC_SYNCH_SHIFT;
		val |= MCDE_CHNLXSYNCHMOD_OUT_SYNCH_SRC_TE0
			<< MCDE_CHNLXSYNCHMOD_OUT_SYNCH_SRC_SHIFT;
		break;
	case MCDE_VIDEO_FORMATTER_FLOW:
	case MCDE_DPI_FORMATTER_FLOW:
		val = MCDE_CHNLXSYNCHMOD_SRC_SYNCH_HARDWARE
			<< MCDE_CHNLXSYNCHMOD_SRC_SYNCH_SHIFT;
		val |= MCDE_CHNLXSYNCHMOD_OUT_SYNCH_SRC_FORMATTER
			<< MCDE_CHNLXSYNCHMOD_OUT_SYNCH_SRC_SHIFT;
		break;
	default:
		dev_err(mcde->dev, "unknown flow mode %d\n",
			mcde->flow_mode);
		return;
	}

	writel(val, mcde->regs + sync);

	 
	val = (mode->hdisplay - 1) << MCDE_CHNLXCONF_PPL_SHIFT;
	val |= (mode->vdisplay - 1) << MCDE_CHNLXCONF_LPF_SHIFT;
	writel(val, mcde->regs + conf);

	 
	val = MCDE_CHNLXSTAT_CHNLBLBCKGND_EN |
		MCDE_CHNLXSTAT_CHNLRD;
	writel(val, mcde->regs + stat);
	writel(0, mcde->regs + bgcol);

	 
	switch (fifo) {
	case MCDE_FIFO_A:
		writel(MCDE_CHNLXMUXING_FIFO_ID_FIFO_A,
		       mcde->regs + mux);
		break;
	case MCDE_FIFO_B:
		writel(MCDE_CHNLXMUXING_FIFO_ID_FIFO_B,
		       mcde->regs + mux);
		break;
	}

	 
	if (mcde->dpi_output) {
		u32 stripwidth;

		stripwidth = 0xF000 / (mode->vdisplay * 4);
		dev_info(mcde->dev, "stripwidth: %d\n", stripwidth);

		val = MCDE_SYNCHCONF_HWREQVEVENT_ACTIVE_VIDEO |
			(mode->hdisplay - 1 - stripwidth) << MCDE_SYNCHCONF_HWREQVCNT_SHIFT |
			MCDE_SYNCHCONF_SWINTVEVENT_ACTIVE_VIDEO |
			(mode->hdisplay - 1 - stripwidth) << MCDE_SYNCHCONF_SWINTVCNT_SHIFT;

		switch (fifo) {
		case MCDE_FIFO_A:
			writel(val, mcde->regs + MCDE_SYNCHCONFA);
			break;
		case MCDE_FIFO_B:
			writel(val, mcde->regs + MCDE_SYNCHCONFB);
			break;
		}
	}
}

static void mcde_configure_fifo(struct mcde *mcde, enum mcde_fifo fifo,
				enum mcde_formatter fmt,
				int fifo_wtrmrk)
{
	u32 val;
	u32 ctrl;
	u32 cr0, cr1;

	switch (fifo) {
	case MCDE_FIFO_A:
		ctrl = MCDE_CTRLA;
		cr0 = MCDE_CRA0;
		cr1 = MCDE_CRA1;
		break;
	case MCDE_FIFO_B:
		ctrl = MCDE_CTRLB;
		cr0 = MCDE_CRB0;
		cr1 = MCDE_CRB1;
		break;
	}

	val = fifo_wtrmrk << MCDE_CTRLX_FIFOWTRMRK_SHIFT;

	 
	switch (fmt) {
	case MCDE_DSI_FORMATTER_0:
		val |= MCDE_CTRLX_FORMTYPE_DSI << MCDE_CTRLX_FORMTYPE_SHIFT;
		val |= MCDE_CTRLX_FORMID_DSI0VID << MCDE_CTRLX_FORMID_SHIFT;
		break;
	case MCDE_DSI_FORMATTER_1:
		val |= MCDE_CTRLX_FORMTYPE_DSI << MCDE_CTRLX_FORMTYPE_SHIFT;
		val |= MCDE_CTRLX_FORMID_DSI0CMD << MCDE_CTRLX_FORMID_SHIFT;
		break;
	case MCDE_DSI_FORMATTER_2:
		val |= MCDE_CTRLX_FORMTYPE_DSI << MCDE_CTRLX_FORMTYPE_SHIFT;
		val |= MCDE_CTRLX_FORMID_DSI1VID << MCDE_CTRLX_FORMID_SHIFT;
		break;
	case MCDE_DSI_FORMATTER_3:
		val |= MCDE_CTRLX_FORMTYPE_DSI << MCDE_CTRLX_FORMTYPE_SHIFT;
		val |= MCDE_CTRLX_FORMID_DSI1CMD << MCDE_CTRLX_FORMID_SHIFT;
		break;
	case MCDE_DSI_FORMATTER_4:
		val |= MCDE_CTRLX_FORMTYPE_DSI << MCDE_CTRLX_FORMTYPE_SHIFT;
		val |= MCDE_CTRLX_FORMID_DSI2VID << MCDE_CTRLX_FORMID_SHIFT;
		break;
	case MCDE_DSI_FORMATTER_5:
		val |= MCDE_CTRLX_FORMTYPE_DSI << MCDE_CTRLX_FORMTYPE_SHIFT;
		val |= MCDE_CTRLX_FORMID_DSI2CMD << MCDE_CTRLX_FORMID_SHIFT;
		break;
	case MCDE_DPI_FORMATTER_0:
		val |= MCDE_CTRLX_FORMTYPE_DPITV << MCDE_CTRLX_FORMTYPE_SHIFT;
		val |= MCDE_CTRLX_FORMID_DPIA << MCDE_CTRLX_FORMID_SHIFT;
		break;
	case MCDE_DPI_FORMATTER_1:
		val |= MCDE_CTRLX_FORMTYPE_DPITV << MCDE_CTRLX_FORMTYPE_SHIFT;
		val |= MCDE_CTRLX_FORMID_DPIB << MCDE_CTRLX_FORMID_SHIFT;
		break;
	}
	writel(val, mcde->regs + ctrl);

	 
	val = MCDE_CRX0_BLENDEN |
		0xff << MCDE_CRX0_ALPHABLEND_SHIFT;
	writel(val, mcde->regs + cr0);

	spin_lock(&mcde->fifo_crx1_lock);
	val = readl(mcde->regs + cr1);
	 
	if (mcde->dpi_output) {
		struct drm_connector *connector = drm_panel_bridge_connector(mcde->bridge);
		u32 bus_format;

		 
		if (!connector->display_info.num_bus_formats) {
			dev_info(mcde->dev, "panel does not specify bus format, assume RGB888\n");
			bus_format = MEDIA_BUS_FMT_RGB888_1X24;
		} else {
			bus_format = connector->display_info.bus_formats[0];
		}

		 
		val &= ~MCDE_CRX1_CDWIN_MASK;
		val &= ~MCDE_CRX1_OUTBPP_MASK;
		switch (bus_format) {
		case MEDIA_BUS_FMT_RGB888_1X24:
			val |= MCDE_CRX1_CDWIN_24BPP << MCDE_CRX1_CDWIN_SHIFT;
			val |= MCDE_CRX1_OUTBPP_24BPP << MCDE_CRX1_OUTBPP_SHIFT;
			break;
		default:
			dev_err(mcde->dev, "unknown bus format, assume RGB888\n");
			val |= MCDE_CRX1_CDWIN_24BPP << MCDE_CRX1_CDWIN_SHIFT;
			val |= MCDE_CRX1_OUTBPP_24BPP << MCDE_CRX1_OUTBPP_SHIFT;
			break;
		}
	} else {
		 
		val &= ~MCDE_CRX1_CLKSEL_MASK;
		val |= MCDE_CRX1_CLKSEL_MCDECLK << MCDE_CRX1_CLKSEL_SHIFT;
	}
	writel(val, mcde->regs + cr1);
	spin_unlock(&mcde->fifo_crx1_lock);
};

static void mcde_configure_dsi_formatter(struct mcde *mcde,
					 enum mcde_formatter fmt,
					 u32 formatter_frame,
					 int pkt_size)
{
	u32 val;
	u32 conf0;
	u32 frame;
	u32 pkt;
	u32 sync;
	u32 cmdw;
	u32 delay0, delay1;

	switch (fmt) {
	case MCDE_DSI_FORMATTER_0:
		conf0 = MCDE_DSIVID0CONF0;
		frame = MCDE_DSIVID0FRAME;
		pkt = MCDE_DSIVID0PKT;
		sync = MCDE_DSIVID0SYNC;
		cmdw = MCDE_DSIVID0CMDW;
		delay0 = MCDE_DSIVID0DELAY0;
		delay1 = MCDE_DSIVID0DELAY1;
		break;
	case MCDE_DSI_FORMATTER_1:
		conf0 = MCDE_DSIVID1CONF0;
		frame = MCDE_DSIVID1FRAME;
		pkt = MCDE_DSIVID1PKT;
		sync = MCDE_DSIVID1SYNC;
		cmdw = MCDE_DSIVID1CMDW;
		delay0 = MCDE_DSIVID1DELAY0;
		delay1 = MCDE_DSIVID1DELAY1;
		break;
	case MCDE_DSI_FORMATTER_2:
		conf0 = MCDE_DSIVID2CONF0;
		frame = MCDE_DSIVID2FRAME;
		pkt = MCDE_DSIVID2PKT;
		sync = MCDE_DSIVID2SYNC;
		cmdw = MCDE_DSIVID2CMDW;
		delay0 = MCDE_DSIVID2DELAY0;
		delay1 = MCDE_DSIVID2DELAY1;
		break;
	default:
		dev_err(mcde->dev, "tried to configure a non-DSI formatter as DSI\n");
		return;
	}

	 
	val = MCDE_DSICONF0_CMD8 | MCDE_DSICONF0_DCSVID_NOTGEN;
	if (mcde->mdsi->mode_flags & MIPI_DSI_MODE_VIDEO)
		val |= MCDE_DSICONF0_VID_MODE_VID;
	switch (mcde->mdsi->format) {
	case MIPI_DSI_FMT_RGB888:
		val |= MCDE_DSICONF0_PACKING_RGB888 <<
			MCDE_DSICONF0_PACKING_SHIFT;
		break;
	case MIPI_DSI_FMT_RGB666:
		val |= MCDE_DSICONF0_PACKING_RGB666 <<
			MCDE_DSICONF0_PACKING_SHIFT;
		break;
	case MIPI_DSI_FMT_RGB666_PACKED:
		dev_err(mcde->dev,
			"we cannot handle the packed RGB666 format\n");
		val |= MCDE_DSICONF0_PACKING_RGB666 <<
			MCDE_DSICONF0_PACKING_SHIFT;
		break;
	case MIPI_DSI_FMT_RGB565:
		val |= MCDE_DSICONF0_PACKING_RGB565 <<
			MCDE_DSICONF0_PACKING_SHIFT;
		break;
	default:
		dev_err(mcde->dev, "unknown DSI format\n");
		return;
	}
	writel(val, mcde->regs + conf0);

	writel(formatter_frame, mcde->regs + frame);
	writel(pkt_size, mcde->regs + pkt);
	writel(0, mcde->regs + sync);
	 
	val = MIPI_DCS_WRITE_MEMORY_CONTINUE <<
		MCDE_DSIVIDXCMDW_CMDW_CONTINUE_SHIFT;
	val |= MIPI_DCS_WRITE_MEMORY_START <<
		MCDE_DSIVIDXCMDW_CMDW_START_SHIFT;
	writel(val, mcde->regs + cmdw);

	 
	writel(0, mcde->regs + delay0);
	writel(0, mcde->regs + delay1);
}

static void mcde_enable_fifo(struct mcde *mcde, enum mcde_fifo fifo)
{
	u32 val;
	u32 cr;

	switch (fifo) {
	case MCDE_FIFO_A:
		cr = MCDE_CRA0;
		break;
	case MCDE_FIFO_B:
		cr = MCDE_CRB0;
		break;
	default:
		dev_err(mcde->dev, "cannot enable FIFO %c\n",
			'A' + fifo);
		return;
	}

	spin_lock(&mcde->flow_lock);
	val = readl(mcde->regs + cr);
	val |= MCDE_CRX0_FLOEN;
	writel(val, mcde->regs + cr);
	mcde->flow_active++;
	spin_unlock(&mcde->flow_lock);
}

static void mcde_disable_fifo(struct mcde *mcde, enum mcde_fifo fifo,
			      bool wait_for_drain)
{
	int timeout = 100;
	u32 val;
	u32 cr;

	switch (fifo) {
	case MCDE_FIFO_A:
		cr = MCDE_CRA0;
		break;
	case MCDE_FIFO_B:
		cr = MCDE_CRB0;
		break;
	default:
		dev_err(mcde->dev, "cannot disable FIFO %c\n",
			'A' + fifo);
		return;
	}

	spin_lock(&mcde->flow_lock);
	val = readl(mcde->regs + cr);
	val &= ~MCDE_CRX0_FLOEN;
	writel(val, mcde->regs + cr);
	mcde->flow_active = 0;
	spin_unlock(&mcde->flow_lock);

	if (!wait_for_drain)
		return;

	 
	while (readl(mcde->regs + cr) & MCDE_CRX0_FLOEN) {
		usleep_range(1000, 1500);
		if (!--timeout) {
			dev_err(mcde->dev,
				"FIFO timeout while clearing FIFO %c\n",
				'A' + fifo);
			return;
		}
	}
}

 
static void mcde_drain_pipe(struct mcde *mcde, enum mcde_fifo fifo,
			    enum mcde_channel ch)
{
	u32 val;
	u32 ctrl;
	u32 synsw;

	switch (fifo) {
	case MCDE_FIFO_A:
		ctrl = MCDE_CTRLA;
		break;
	case MCDE_FIFO_B:
		ctrl = MCDE_CTRLB;
		break;
	}

	switch (ch) {
	case MCDE_CHANNEL_0:
		synsw = MCDE_CHNL0SYNCHSW;
		break;
	case MCDE_CHANNEL_1:
		synsw = MCDE_CHNL1SYNCHSW;
		break;
	case MCDE_CHANNEL_2:
		synsw = MCDE_CHNL2SYNCHSW;
		break;
	case MCDE_CHANNEL_3:
		synsw = MCDE_CHNL3SYNCHSW;
		return;
	}

	val = readl(mcde->regs + ctrl);
	if (!(val & MCDE_CTRLX_FIFOEMPTY)) {
		dev_err(mcde->dev, "Channel A FIFO not empty (handover)\n");
		 
		mcde_enable_fifo(mcde, fifo);
		 
		writel(MCDE_CHNLXSYNCHSW_SW_TRIG, mcde->regs + synsw);
		 
		mcde_disable_fifo(mcde, fifo, true);
	}
}

static int mcde_dsi_get_pkt_div(int ppl, int fifo_size)
{
	 
	int div;
	const int max_div = DIV_ROUND_UP(MCDE_MAX_WIDTH, fifo_size);

	for (div = 1; div < max_div; div++)
		if (ppl % div == 0 && ppl / div <= fifo_size)
			return div;
	return 1;
}

static void mcde_setup_dpi(struct mcde *mcde, const struct drm_display_mode *mode,
			   int *fifo_wtrmrk_lvl)
{
	struct drm_connector *connector = drm_panel_bridge_connector(mcde->bridge);
	u32 hsw, hfp, hbp;
	u32 vsw, vfp, vbp;
	u32 val;

	 
	hsw = mode->hsync_end - mode->hsync_start;
	hfp = mode->hsync_start - mode->hdisplay;
	hbp = mode->htotal - mode->hsync_end;
	vsw = mode->vsync_end - mode->vsync_start;
	vfp = mode->vsync_start - mode->vdisplay;
	vbp = mode->vtotal - mode->vsync_end;

	dev_info(mcde->dev, "output on DPI LCD from channel A\n");
	 
	dev_info(mcde->dev, "HSW: %d, HFP: %d, HBP: %d, VSW: %d, VFP: %d, VBP: %d\n",
		 hsw, hfp, hbp, vsw, vfp, vbp);

	 
	*fifo_wtrmrk_lvl = 640;

	 
	val = 7 << MCDE_CONF0_IFIFOCTRLWTRMRKLVL_SHIFT;

	 
	 
	val |= 0 << MCDE_CONF0_OUTMUX0_SHIFT;
	 
	val |= 1 << MCDE_CONF0_OUTMUX1_SHIFT;
	 
	val |= 0 << MCDE_CONF0_OUTMUX2_SHIFT;
	 
	val |= 0 << MCDE_CONF0_OUTMUX3_SHIFT;
	 
	val |= 2 << MCDE_CONF0_OUTMUX4_SHIFT;
	 
	writel(val, mcde->regs + MCDE_CONF0);

	 
	writel(0, mcde->regs + MCDE_TVCRA);

	 
	val = (vsw << MCDE_TVBL1_BEL1_SHIFT);
	val |= (vfp << MCDE_TVBL1_BSL1_SHIFT);
	writel(val, mcde->regs + MCDE_TVBL1A);
	 
	writel(val, mcde->regs + MCDE_TVBL2A);

	 
	val = (vbp << MCDE_TVDVO_DVO1_SHIFT);
	 
	val |= (vbp << MCDE_TVDVO_DVO2_SHIFT);
	writel(val, mcde->regs + MCDE_TVDVOA);

	 
	writel((hbp - 1), mcde->regs + MCDE_TVTIM1A);

	 
	val = ((hsw - 1) << MCDE_TVLBALW_LBW_SHIFT);
	val |= ((hfp - 1) << MCDE_TVLBALW_ALW_SHIFT);
	writel(val, mcde->regs + MCDE_TVLBALWA);

	 
	writel(0, mcde->regs + MCDE_TVISLA);
	writel(0, mcde->regs + MCDE_TVBLUA);

	 
	val = 0;
	if (mode->flags & DRM_MODE_FLAG_NHSYNC)
		val |= MCDE_LCDTIM1B_IHS;
	if (mode->flags & DRM_MODE_FLAG_NVSYNC)
		val |= MCDE_LCDTIM1B_IVS;
	if (connector->display_info.bus_flags & DRM_BUS_FLAG_DE_LOW)
		val |= MCDE_LCDTIM1B_IOE;
	if (connector->display_info.bus_flags & DRM_BUS_FLAG_PIXDATA_DRIVE_NEGEDGE)
		val |= MCDE_LCDTIM1B_IPC;
	writel(val, mcde->regs + MCDE_LCDTIM1A);
}

static void mcde_setup_dsi(struct mcde *mcde, const struct drm_display_mode *mode,
			   int cpp, int *fifo_wtrmrk_lvl, int *dsi_formatter_frame,
			   int *dsi_pkt_size)
{
	u32 formatter_ppl = mode->hdisplay;  
	u32 formatter_lpf = mode->vdisplay;  
	int formatter_frame;
	int formatter_cpp;
	int fifo_wtrmrk;
	u32 pkt_div;
	int pkt_size;
	u32 val;

	dev_info(mcde->dev, "output in %s mode, format %dbpp\n",
		 (mcde->mdsi->mode_flags & MIPI_DSI_MODE_VIDEO) ?
		 "VIDEO" : "CMD",
		 mipi_dsi_pixel_format_to_bpp(mcde->mdsi->format));
	formatter_cpp =
		mipi_dsi_pixel_format_to_bpp(mcde->mdsi->format) / 8;
	dev_info(mcde->dev, "Overlay CPP: %d bytes, DSI formatter CPP %d bytes\n",
		 cpp, formatter_cpp);

	 
	val = 7 << MCDE_CONF0_IFIFOCTRLWTRMRKLVL_SHIFT;

	 
	val |= 3 << MCDE_CONF0_OUTMUX0_SHIFT;
	val |= 3 << MCDE_CONF0_OUTMUX1_SHIFT;
	val |= 0 << MCDE_CONF0_OUTMUX2_SHIFT;
	val |= 4 << MCDE_CONF0_OUTMUX3_SHIFT;
	val |= 5 << MCDE_CONF0_OUTMUX4_SHIFT;
	writel(val, mcde->regs + MCDE_CONF0);

	 

	 
	fifo_wtrmrk = mode->hdisplay;
	if (mcde->mdsi->mode_flags & MIPI_DSI_MODE_VIDEO) {
		fifo_wtrmrk = min(fifo_wtrmrk, 128);
		pkt_div = 1;
	} else {
		fifo_wtrmrk = min(fifo_wtrmrk, 48);
		 
		pkt_div = mcde_dsi_get_pkt_div(mode->hdisplay, 640);
	}
	dev_dbg(mcde->dev, "FIFO watermark after flooring: %d bytes\n",
		fifo_wtrmrk);
	dev_dbg(mcde->dev, "Packet divisor: %d bytes\n", pkt_div);

	 
	pkt_size = (formatter_ppl * formatter_cpp) / pkt_div;
	 
	if (!(mcde->mdsi->mode_flags & MIPI_DSI_MODE_VIDEO))
		pkt_size++;

	dev_dbg(mcde->dev, "DSI packet size: %d * %d bytes per line\n",
		pkt_size, pkt_div);
	dev_dbg(mcde->dev, "Overlay frame size: %u bytes\n",
		mode->hdisplay * mode->vdisplay * cpp);
	 
	formatter_frame = pkt_size * pkt_div * formatter_lpf;
	dev_dbg(mcde->dev, "Formatter frame size: %u bytes\n", formatter_frame);

	*fifo_wtrmrk_lvl = fifo_wtrmrk;
	*dsi_pkt_size = pkt_size;
	*dsi_formatter_frame = formatter_frame;
}

static void mcde_display_enable(struct drm_simple_display_pipe *pipe,
				struct drm_crtc_state *cstate,
				struct drm_plane_state *plane_state)
{
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_plane *plane = &pipe->plane;
	struct drm_device *drm = crtc->dev;
	struct mcde *mcde = to_mcde(drm);
	const struct drm_display_mode *mode = &cstate->mode;
	struct drm_framebuffer *fb = plane->state->fb;
	u32 format = fb->format->format;
	int dsi_pkt_size;
	int fifo_wtrmrk;
	int cpp = fb->format->cpp[0];
	u32 dsi_formatter_frame;
	u32 val;
	int ret;

	 
	ret = regulator_enable(mcde->epod);
	if (ret) {
		dev_err(drm->dev, "can't re-enable EPOD regulator\n");
		return;
	}

	dev_info(drm->dev, "enable MCDE, %d x %d format %p4cc\n",
		 mode->hdisplay, mode->vdisplay, &format);


	 
	mcde_display_disable_irqs(mcde);
	writel(0, mcde->regs + MCDE_IMSCERR);
	writel(0xFFFFFFFF, mcde->regs + MCDE_RISERR);

	if (mcde->dpi_output)
		mcde_setup_dpi(mcde, mode, &fifo_wtrmrk);
	else
		mcde_setup_dsi(mcde, mode, cpp, &fifo_wtrmrk,
			       &dsi_formatter_frame, &dsi_pkt_size);

	mcde->stride = mode->hdisplay * cpp;
	dev_dbg(drm->dev, "Overlay line stride: %u bytes\n",
		 mcde->stride);

	 
	mcde_drain_pipe(mcde, MCDE_FIFO_A, MCDE_CHANNEL_0);

	 
	mcde_configure_extsrc(mcde, MCDE_EXTSRC_0, format);

	 
	mcde_configure_overlay(mcde, MCDE_OVERLAY_0, MCDE_EXTSRC_0,
			       MCDE_CHANNEL_0, mode, format, cpp);

	 
	mcde_configure_channel(mcde, MCDE_CHANNEL_0, MCDE_FIFO_A, mode);

	if (mcde->dpi_output) {
		unsigned long lcd_freq;

		 
		mcde_configure_fifo(mcde, MCDE_FIFO_A, MCDE_DPI_FORMATTER_0,
				    fifo_wtrmrk);

		 
		lcd_freq = clk_round_rate(mcde->fifoa_clk, mode->clock * 1000);
		ret = clk_set_rate(mcde->fifoa_clk, lcd_freq);
		if (ret)
			dev_err(mcde->dev, "failed to set LCD clock rate %lu Hz\n",
				lcd_freq);
		ret = clk_prepare_enable(mcde->fifoa_clk);
		if (ret) {
			dev_err(mcde->dev, "failed to enable FIFO A DPI clock\n");
			return;
		}
		dev_info(mcde->dev, "LCD FIFO A clk rate %lu Hz\n",
			 clk_get_rate(mcde->fifoa_clk));
	} else {
		 
		mcde_configure_fifo(mcde, MCDE_FIFO_A, MCDE_DSI_FORMATTER_0,
				    fifo_wtrmrk);

		 
		mcde_dsi_enable(mcde->bridge);

		 
		mcde_configure_dsi_formatter(mcde, MCDE_DSI_FORMATTER_0,
					     dsi_formatter_frame, dsi_pkt_size);
	}

	switch (mcde->flow_mode) {
	case MCDE_COMMAND_TE_FLOW:
	case MCDE_COMMAND_BTA_TE_FLOW:
	case MCDE_VIDEO_TE_FLOW:
		 
		if (mode->flags & DRM_MODE_FLAG_NVSYNC)
			val = MCDE_VSCRC_VSPOL;
		else
			val = 0;
		writel(val, mcde->regs + MCDE_VSCRC0);
		 
		val = readl(mcde->regs + MCDE_CRC);
		val |= MCDE_CRC_SYCEN0;
		writel(val, mcde->regs + MCDE_CRC);
		break;
	default:
		 
		break;
	}

	drm_crtc_vblank_on(crtc);

	 
	if (mcde->flow_mode != MCDE_COMMAND_ONESHOT_FLOW) {
		mcde_enable_fifo(mcde, MCDE_FIFO_A);
		dev_dbg(mcde->dev, "started MCDE video FIFO flow\n");
	}

	 
	val = readl(mcde->regs + MCDE_CR);
	val |= MCDE_CR_MCDEEN | MCDE_CR_AUTOCLKG_EN;
	writel(val, mcde->regs + MCDE_CR);

	dev_info(drm->dev, "MCDE display is enabled\n");
}

static void mcde_display_disable(struct drm_simple_display_pipe *pipe)
{
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_device *drm = crtc->dev;
	struct mcde *mcde = to_mcde(drm);
	struct drm_pending_vblank_event *event;
	int ret;

	drm_crtc_vblank_off(crtc);

	 
	mcde_disable_fifo(mcde, MCDE_FIFO_A, true);

	if (mcde->dpi_output) {
		clk_disable_unprepare(mcde->fifoa_clk);
	} else {
		 
		mcde_dsi_disable(mcde->bridge);
	}

	event = crtc->state->event;
	if (event) {
		crtc->state->event = NULL;

		spin_lock_irq(&crtc->dev->event_lock);
		drm_crtc_send_vblank_event(crtc, event);
		spin_unlock_irq(&crtc->dev->event_lock);
	}

	ret = regulator_disable(mcde->epod);
	if (ret)
		dev_err(drm->dev, "can't disable EPOD regulator\n");
	 
	usleep_range(50000, 70000);

	dev_info(drm->dev, "MCDE display is disabled\n");
}

static void mcde_start_flow(struct mcde *mcde)
{
	 
	if (mcde->flow_mode == MCDE_COMMAND_BTA_TE_FLOW)
		mcde_dsi_te_request(mcde->mdsi);

	 
	mcde_enable_fifo(mcde, MCDE_FIFO_A);

	 

	if (mcde->flow_mode == MCDE_COMMAND_ONESHOT_FLOW) {
		 
		writel(MCDE_CHNLXSYNCHSW_SW_TRIG,
		       mcde->regs + MCDE_CHNL0SYNCHSW);

		 
		mcde_disable_fifo(mcde, MCDE_FIFO_A, true);
	}

	dev_dbg(mcde->dev, "started MCDE FIFO flow\n");
}

static void mcde_set_extsrc(struct mcde *mcde, u32 buffer_address)
{
	 
	writel(buffer_address, mcde->regs + MCDE_EXTSRCXA0);
	 
	writel(buffer_address + mcde->stride, mcde->regs + MCDE_EXTSRCXA1);
}

static void mcde_display_update(struct drm_simple_display_pipe *pipe,
				struct drm_plane_state *old_pstate)
{
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_device *drm = crtc->dev;
	struct mcde *mcde = to_mcde(drm);
	struct drm_pending_vblank_event *event = crtc->state->event;
	struct drm_plane *plane = &pipe->plane;
	struct drm_plane_state *pstate = plane->state;
	struct drm_framebuffer *fb = pstate->fb;

	 
	if (event) {
		crtc->state->event = NULL;

		spin_lock_irq(&crtc->dev->event_lock);
		 
		if (crtc->state->active && drm_crtc_vblank_get(crtc) == 0) {
			dev_dbg(mcde->dev, "arm vblank event\n");
			drm_crtc_arm_vblank_event(crtc, event);
		} else {
			dev_dbg(mcde->dev, "insert fake vblank event\n");
			drm_crtc_send_vblank_event(crtc, event);
		}

		spin_unlock_irq(&crtc->dev->event_lock);
	}

	 
	if (fb) {
		mcde_set_extsrc(mcde, drm_fb_dma_get_gem_addr(fb, pstate, 0));
		dev_info_once(mcde->dev, "first update of display contents\n");
		 
		if (mcde->flow_active == 0)
			mcde_start_flow(mcde);
	} else {
		 
		dev_info(mcde->dev, "ignored a display update\n");
	}
}

static int mcde_display_enable_vblank(struct drm_simple_display_pipe *pipe)
{
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_device *drm = crtc->dev;
	struct mcde *mcde = to_mcde(drm);
	u32 val;

	 
	val = MCDE_PP_VCMPA |
		MCDE_PP_VCMPB |
		MCDE_PP_VSCC0 |
		MCDE_PP_VSCC1 |
		MCDE_PP_VCMPC0 |
		MCDE_PP_VCMPC1;
	writel(val, mcde->regs + MCDE_IMSCPP);

	return 0;
}

static void mcde_display_disable_vblank(struct drm_simple_display_pipe *pipe)
{
	struct drm_crtc *crtc = &pipe->crtc;
	struct drm_device *drm = crtc->dev;
	struct mcde *mcde = to_mcde(drm);

	 
	writel(0, mcde->regs + MCDE_IMSCPP);
	 
	writel(0xFFFFFFFF, mcde->regs + MCDE_RISPP);
}

static struct drm_simple_display_pipe_funcs mcde_display_funcs = {
	.check = mcde_display_check,
	.enable = mcde_display_enable,
	.disable = mcde_display_disable,
	.update = mcde_display_update,
	.enable_vblank = mcde_display_enable_vblank,
	.disable_vblank = mcde_display_disable_vblank,
};

int mcde_display_init(struct drm_device *drm)
{
	struct mcde *mcde = to_mcde(drm);
	int ret;
	static const u32 formats[] = {
		DRM_FORMAT_ARGB8888,
		DRM_FORMAT_ABGR8888,
		DRM_FORMAT_XRGB8888,
		DRM_FORMAT_XBGR8888,
		DRM_FORMAT_RGB888,
		DRM_FORMAT_BGR888,
		DRM_FORMAT_ARGB4444,
		DRM_FORMAT_ABGR4444,
		DRM_FORMAT_XRGB4444,
		DRM_FORMAT_XBGR4444,
		 
		DRM_FORMAT_XRGB1555,
		DRM_FORMAT_XBGR1555,
		DRM_FORMAT_RGB565,
		DRM_FORMAT_BGR565,
		DRM_FORMAT_YUV422,
	};

	ret = mcde_init_clock_divider(mcde);
	if (ret)
		return ret;

	ret = drm_simple_display_pipe_init(drm, &mcde->pipe,
					   &mcde_display_funcs,
					   formats, ARRAY_SIZE(formats),
					   NULL,
					   mcde->connector);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(mcde_display_init);
