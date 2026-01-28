#ifndef _TVE200_DRM_H_
#define _TVE200_DRM_H_
#include <linux/irqreturn.h>
#include <drm/drm_simple_kms_helper.h>
struct clk;
struct drm_bridge;
struct drm_connector;
struct drm_device;
struct drm_file;
struct drm_mode_create_dumb;
struct drm_panel;
#define TVE200_Y_FRAME_BASE_ADDR	0x00
#define TVE200_U_FRAME_BASE_ADDR	0x04
#define TVE200_V_FRAME_BASE_ADDR	0x08
#define TVE200_INT_EN			0x0C
#define TVE200_INT_CLR			0x10
#define TVE200_INT_STAT			0x14
#define TVE200_INT_BUS_ERR		BIT(7)
#define TVE200_INT_V_STATUS		BIT(6)  
#define TVE200_INT_V_NEXT_FRAME		BIT(5)
#define TVE200_INT_U_NEXT_FRAME		BIT(4)
#define TVE200_INT_Y_NEXT_FRAME		BIT(3)
#define TVE200_INT_V_FIFO_UNDERRUN	BIT(2)
#define TVE200_INT_U_FIFO_UNDERRUN	BIT(1)
#define TVE200_INT_Y_FIFO_UNDERRUN	BIT(0)
#define TVE200_FIFO_UNDERRUNS		(TVE200_INT_V_FIFO_UNDERRUN | \
					 TVE200_INT_U_FIFO_UNDERRUN | \
					 TVE200_INT_Y_FIFO_UNDERRUN)
#define TVE200_CTRL			0x18
#define TVE200_CTRL_YUV420		BIT(31)
#define TVE200_CTRL_CSMODE		BIT(30)
#define TVE200_CTRL_NONINTERLACE	BIT(28)  
#define TVE200_CTRL_TVCLKP		BIT(27)  
#define TVE200_CTRL_BURST_4_WORDS	(0 << 24)
#define TVE200_CTRL_BURST_8_WORDS	(1 << 24)
#define TVE200_CTRL_BURST_16_WORDS	(2 << 24)
#define TVE200_CTRL_BURST_32_WORDS	(3 << 24)
#define TVE200_CTRL_BURST_64_WORDS	(4 << 24)
#define TVE200_CTRL_BURST_128_WORDS	(5 << 24)
#define TVE200_CTRL_BURST_256_WORDS	(6 << 24)
#define TVE200_CTRL_BURST_0_WORDS	(7 << 24)  
#define TVE200_CTRL_RETRYCNT_MASK	GENMASK(23, 16)
#define TVE200_CTRL_RETRYCNT_16		(1 << 16)
#define TVE200_CTRL_BBBP		BIT(15)  
#define TVE200_CTRL_YCBCRODR_CB0Y0CR0Y1	(0 << 12)
#define TVE200_CTRL_YCBCRODR_Y0CB0Y1CR0	(1 << 12)
#define TVE200_CTRL_YCBCRODR_CR0Y0CB0Y1	(2 << 12)
#define TVE200_CTRL_YCBCRODR_Y1CB0Y0CR0	(3 << 12)
#define TVE200_CTRL_YCBCRODR_CR0Y1CB0Y0	(4 << 12)
#define TVE200_CTRL_YCBCRODR_Y1CR0Y0CB0	(5 << 12)
#define TVE200_CTRL_YCBCRODR_CB0Y1CR0Y0	(6 << 12)
#define TVE200_CTRL_YCBCRODR_Y0CR0Y1CB0	(7 << 12)
#define TVE200_CTRL_IPRESOL_CIF		(0 << 10)
#define TVE200_CTRL_IPRESOL_VGA		(1 << 10)
#define TVE200_CTRL_IPRESOL_D1		(2 << 10)
#define TVE200_CTRL_NTSC		BIT(9)  
#define TVE200_CTRL_INTERLACE		BIT(8)  
#define TVE200_IPDMOD_RGB555		(0 << 6)  
#define TVE200_IPDMOD_RGB565		(1 << 6)
#define TVE200_IPDMOD_RGB888		(2 << 6)
#define TVE200_IPDMOD_YUV420		(2 << 6)  
#define TVE200_IPDMOD_YUV422		(3 << 6)
#define TVE200_VSTSTYPE_VSYNC		(0 << 4)  
#define TVE200_VSTSTYPE_VBP		(1 << 4)  
#define TVE200_VSTSTYPE_VAI		(2 << 4)  
#define TVE200_VSTSTYPE_VFP		(3 << 4)  
#define TVE200_VSTSTYPE_BITS		(BIT(4) | BIT(5))
#define TVE200_BGR			BIT(1)  
#define TVE200_TVEEN			BIT(0)  
#define TVE200_CTRL_2			0x1c
#define TVE200_CTRL_3			0x20
#define TVE200_CTRL_4			0x24
#define TVE200_CTRL_4_RESET		BIT(0)  
struct tve200_drm_dev_private {
	struct drm_device *drm;
	struct drm_connector *connector;
	struct drm_panel *panel;
	struct drm_bridge *bridge;
	struct drm_simple_display_pipe pipe;
	void *regs;
	struct clk *pclk;
	struct clk *clk;
};
#define to_tve200_connector(x) \
	container_of(x, struct tve200_drm_connector, connector)
int tve200_display_init(struct drm_device *dev);
irqreturn_t tve200_irq(int irq, void *data);
int tve200_connector_init(struct drm_device *dev);
int tve200_encoder_init(struct drm_device *dev);
int tve200_dumb_create(struct drm_file *file_priv,
		      struct drm_device *dev,
		      struct drm_mode_create_dumb *args);
#endif  
