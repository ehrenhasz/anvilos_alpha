#include <drm/drm_device.h>
#include <drm/drm_simple_kms_helper.h>
struct aspeed_gfx {
	struct drm_device		drm;
	void __iomem			*base;
	struct clk			*clk;
	struct reset_control		*rst;
	struct regmap			*scu;
	u32				dac_reg;
	u32				int_clr_reg;
	u32				vga_scratch_reg;
	u32				throd_val;
	u32				scan_line_max;
	struct drm_simple_display_pipe	pipe;
	struct drm_connector		connector;
};
#define to_aspeed_gfx(x) container_of(x, struct aspeed_gfx, drm)
int aspeed_gfx_create_pipe(struct drm_device *drm);
int aspeed_gfx_create_output(struct drm_device *drm);
#define CRT_CTRL1		0x60  
#define CRT_CTRL2		0x64  
#define CRT_STATUS		0x68  
#define CRT_MISC		0x6c  
#define CRT_HORIZ0		0x70  
#define CRT_HORIZ1		0x74  
#define CRT_VERT0		0x78  
#define CRT_VERT1		0x7C  
#define CRT_ADDR		0x80  
#define CRT_OFFSET		0x84  
#define CRT_THROD		0x88  
#define CRT_XSCALE		0x8C  
#define CRT_CURSOR0		0x90  
#define CRT_CURSOR1		0x94  
#define CRT_CURSOR2		0x98  
#define CRT_9C			0x9C
#define CRT_OSD_H		0xA0  
#define CRT_OSD_V		0xA4  
#define CRT_OSD_ADDR		0xA8  
#define CRT_OSD_DISP		0xAC  
#define CRT_OSD_THRESH		0xB0  
#define CRT_B4			0xB4
#define CRT_STS_V		0xB8  
#define CRT_SCRATCH		0xBC  
#define CRT_BB0_ADDR		0xD0  
#define CRT_BB1_ADDR		0xD4  
#define CRT_BB_COUNT		0xD8  
#define OSD_COLOR1		0xE0  
#define OSD_COLOR2		0xE4  
#define OSD_COLOR3		0xE8  
#define OSD_COLOR4		0xEC  
#define OSD_COLOR5		0xF0  
#define OSD_COLOR6		0xF4  
#define OSD_COLOR7		0xF8  
#define OSD_COLOR8		0xFC  
#define CRT_CTRL_EN			BIT(0)
#define CRT_CTRL_HW_CURSOR_EN		BIT(1)
#define CRT_CTRL_OSD_EN			BIT(2)
#define CRT_CTRL_INTERLACED		BIT(3)
#define CRT_CTRL_COLOR_RGB565		(0 << 7)
#define CRT_CTRL_COLOR_YUV444		(1 << 7)
#define CRT_CTRL_COLOR_XRGB8888		(2 << 7)
#define CRT_CTRL_COLOR_RGB888		(3 << 7)
#define CRT_CTRL_COLOR_YUV444_2RGB	(5 << 7)
#define CRT_CTRL_COLOR_YUV422		(7 << 7)
#define CRT_CTRL_COLOR_MASK		GENMASK(9, 7)
#define CRT_CTRL_HSYNC_NEGATIVE		BIT(16)
#define CRT_CTRL_VSYNC_NEGATIVE		BIT(17)
#define CRT_CTRL_VERTICAL_INTR_EN	BIT(30)
#define CRT_CTRL_VERTICAL_INTR_STS	BIT(31)
#define CRT_CTRL_DAC_EN			BIT(0)
#define CRT_CTRL_VBLANK_LINE(x)		(((x) << 20) & CRT_CTRL_VBLANK_LINE_MASK)
#define CRT_CTRL_VBLANK_LINE_MASK	GENMASK(31, 20)
#define CRT_H_TOTAL(x)			(x)
#define CRT_H_DE(x)			((x) << 16)
#define CRT_H_RS_START(x)		(x)
#define CRT_H_RS_END(x)			((x) << 16)
#define CRT_V_TOTAL(x)			(x)
#define CRT_V_DE(x)			((x) << 16)
#define CRT_V_RS_START(x)		(x)
#define CRT_V_RS_END(x)			((x) << 16)
#define CRT_DISP_OFFSET(x)		(x)
#define CRT_TERM_COUNT(x)		((x) << 16)
#define CRT_THROD_LOW(x)		(x)
#define CRT_THROD_HIGH(x)		((x) << 8)
