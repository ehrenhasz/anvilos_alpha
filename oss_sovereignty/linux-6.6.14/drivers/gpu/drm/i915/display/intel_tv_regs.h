 
 

#ifndef __INTEL_TV_REGS_H__
#define __INTEL_TV_REGS_H__

#include "intel_display_reg_defs.h"

 
#define TV_CTL			_MMIO(0x68000)
 
# define TV_ENC_ENABLE			(1 << 31)
 
# define TV_ENC_PIPE_SEL_SHIFT		30
# define TV_ENC_PIPE_SEL_MASK		(1 << 30)
# define TV_ENC_PIPE_SEL(pipe)		((pipe) << 30)
 
# define TV_ENC_OUTPUT_COMPOSITE	(0 << 28)
 
# define TV_ENC_OUTPUT_SVIDEO		(1 << 28)
 
# define TV_ENC_OUTPUT_COMPONENT	(2 << 28)
 
# define TV_ENC_OUTPUT_SVIDEO_COMPOSITE	(3 << 28)
# define TV_TRILEVEL_SYNC		(1 << 21)
 
# define TV_SLOW_SYNC			(1 << 20)
 
# define TV_OVERSAMPLE_4X		(0 << 18)
 
# define TV_OVERSAMPLE_2X		(1 << 18)
 
# define TV_OVERSAMPLE_NONE		(2 << 18)
 
# define TV_OVERSAMPLE_8X		(3 << 18)
# define TV_OVERSAMPLE_MASK		(3 << 18)
 
# define TV_PROGRESSIVE			(1 << 17)
 
# define TV_PAL_BURST			(1 << 16)
 
# define TV_YC_SKEW_MASK		(7 << 12)
 
# define TV_ENC_SDP_FIX			(1 << 11)
 
# define TV_ENC_C0_FIX			(1 << 10)
 
# define TV_CTL_SAVE			((1 << 11) | (3 << 9) | (7 << 6) | 0xf)
# define TV_FUSE_STATE_MASK		(3 << 4)
 
# define TV_FUSE_STATE_ENABLED		(0 << 4)
 
# define TV_FUSE_STATE_NO_MACROVISION	(1 << 4)
 
# define TV_FUSE_STATE_DISABLED		(2 << 4)
 
# define TV_TEST_MODE_NORMAL		(0 << 0)
 
# define TV_TEST_MODE_PATTERN_1		(1 << 0)
 
# define TV_TEST_MODE_PATTERN_2		(2 << 0)
 
# define TV_TEST_MODE_PATTERN_3		(3 << 0)
 
# define TV_TEST_MODE_PATTERN_4		(4 << 0)
 
# define TV_TEST_MODE_PATTERN_5		(5 << 0)
 
# define TV_TEST_MODE_MONITOR_DETECT	(7 << 0)
# define TV_TEST_MODE_MASK		(7 << 0)

#define TV_DAC			_MMIO(0x68004)
# define TV_DAC_SAVE		0x00ffff00
 
# define TVDAC_STATE_CHG		(1 << 31)
# define TVDAC_SENSE_MASK		(7 << 28)
 
# define TVDAC_A_SENSE			(1 << 30)
 
# define TVDAC_B_SENSE			(1 << 29)
 
# define TVDAC_C_SENSE			(1 << 28)
 
# define TVDAC_STATE_CHG_EN		(1 << 27)
 
# define TVDAC_A_SENSE_CTL		(1 << 26)
 
# define TVDAC_B_SENSE_CTL		(1 << 25)
 
# define TVDAC_C_SENSE_CTL		(1 << 24)
 
# define DAC_CTL_OVERRIDE		(1 << 7)
 
# define ENC_TVDAC_SLEW_FAST		(1 << 6)
# define DAC_A_1_3_V			(0 << 4)
# define DAC_A_1_1_V			(1 << 4)
# define DAC_A_0_7_V			(2 << 4)
# define DAC_A_MASK			(3 << 4)
# define DAC_B_1_3_V			(0 << 2)
# define DAC_B_1_1_V			(1 << 2)
# define DAC_B_0_7_V			(2 << 2)
# define DAC_B_MASK			(3 << 2)
# define DAC_C_1_3_V			(0 << 0)
# define DAC_C_1_1_V			(1 << 0)
# define DAC_C_0_7_V			(2 << 0)
# define DAC_C_MASK			(3 << 0)

 
#define TV_CSC_Y		_MMIO(0x68010)
# define TV_RY_MASK			0x07ff0000
# define TV_RY_SHIFT			16
# define TV_GY_MASK			0x00000fff
# define TV_GY_SHIFT			0

#define TV_CSC_Y2		_MMIO(0x68014)
# define TV_BY_MASK			0x07ff0000
# define TV_BY_SHIFT			16
 
# define TV_AY_MASK			0x000003ff
# define TV_AY_SHIFT			0

#define TV_CSC_U		_MMIO(0x68018)
# define TV_RU_MASK			0x07ff0000
# define TV_RU_SHIFT			16
# define TV_GU_MASK			0x000007ff
# define TV_GU_SHIFT			0

#define TV_CSC_U2		_MMIO(0x6801c)
# define TV_BU_MASK			0x07ff0000
# define TV_BU_SHIFT			16
 
# define TV_AU_MASK			0x000003ff
# define TV_AU_SHIFT			0

#define TV_CSC_V		_MMIO(0x68020)
# define TV_RV_MASK			0x0fff0000
# define TV_RV_SHIFT			16
# define TV_GV_MASK			0x000007ff
# define TV_GV_SHIFT			0

#define TV_CSC_V2		_MMIO(0x68024)
# define TV_BV_MASK			0x07ff0000
# define TV_BV_SHIFT			16
 
# define TV_AV_MASK			0x000007ff
# define TV_AV_SHIFT			0

#define TV_CLR_KNOBS		_MMIO(0x68028)
 
# define TV_BRIGHTNESS_MASK		0xff000000
# define TV_BRIGHTNESS_SHIFT		24
 
# define TV_CONTRAST_MASK		0x00ff0000
# define TV_CONTRAST_SHIFT		16
 
# define TV_SATURATION_MASK		0x0000ff00
# define TV_SATURATION_SHIFT		8
 
# define TV_HUE_MASK			0x000000ff
# define TV_HUE_SHIFT			0

#define TV_CLR_LEVEL		_MMIO(0x6802c)
 
# define TV_BLACK_LEVEL_MASK		0x01ff0000
# define TV_BLACK_LEVEL_SHIFT		16
 
# define TV_BLANK_LEVEL_MASK		0x000001ff
# define TV_BLANK_LEVEL_SHIFT		0

#define TV_H_CTL_1		_MMIO(0x68030)
 
# define TV_HSYNC_END_MASK		0x1fff0000
# define TV_HSYNC_END_SHIFT		16
 
# define TV_HTOTAL_MASK			0x00001fff
# define TV_HTOTAL_SHIFT		0

#define TV_H_CTL_2		_MMIO(0x68034)
 
# define TV_BURST_ENA			(1 << 31)
 
# define TV_HBURST_START_SHIFT		16
# define TV_HBURST_START_MASK		0x1fff0000
 
# define TV_HBURST_LEN_SHIFT		0
# define TV_HBURST_LEN_MASK		0x0001fff

#define TV_H_CTL_3		_MMIO(0x68038)
 
# define TV_HBLANK_END_SHIFT		16
# define TV_HBLANK_END_MASK		0x1fff0000
 
# define TV_HBLANK_START_SHIFT		0
# define TV_HBLANK_START_MASK		0x0001fff

#define TV_V_CTL_1		_MMIO(0x6803c)
 
# define TV_NBR_END_SHIFT		16
# define TV_NBR_END_MASK		0x07ff0000
 
# define TV_VI_END_F1_SHIFT		8
# define TV_VI_END_F1_MASK		0x00003f00
 
# define TV_VI_END_F2_SHIFT		0
# define TV_VI_END_F2_MASK		0x0000003f

#define TV_V_CTL_2		_MMIO(0x68040)
 
# define TV_VSYNC_LEN_MASK		0x07ff0000
# define TV_VSYNC_LEN_SHIFT		16
 
# define TV_VSYNC_START_F1_MASK		0x00007f00
# define TV_VSYNC_START_F1_SHIFT	8
 
# define TV_VSYNC_START_F2_MASK		0x0000007f
# define TV_VSYNC_START_F2_SHIFT	0

#define TV_V_CTL_3		_MMIO(0x68044)
 
# define TV_EQUAL_ENA			(1 << 31)
 
# define TV_VEQ_LEN_MASK		0x007f0000
# define TV_VEQ_LEN_SHIFT		16
 
# define TV_VEQ_START_F1_MASK		0x0007f00
# define TV_VEQ_START_F1_SHIFT		8
 
# define TV_VEQ_START_F2_MASK		0x000007f
# define TV_VEQ_START_F2_SHIFT		0

#define TV_V_CTL_4		_MMIO(0x68048)
 
# define TV_VBURST_START_F1_MASK	0x003f0000
# define TV_VBURST_START_F1_SHIFT	16
 
# define TV_VBURST_END_F1_MASK		0x000000ff
# define TV_VBURST_END_F1_SHIFT		0

#define TV_V_CTL_5		_MMIO(0x6804c)
 
# define TV_VBURST_START_F2_MASK	0x003f0000
# define TV_VBURST_START_F2_SHIFT	16
 
# define TV_VBURST_END_F2_MASK		0x000000ff
# define TV_VBURST_END_F2_SHIFT		0

#define TV_V_CTL_6		_MMIO(0x68050)
 
# define TV_VBURST_START_F3_MASK	0x003f0000
# define TV_VBURST_START_F3_SHIFT	16
 
# define TV_VBURST_END_F3_MASK		0x000000ff
# define TV_VBURST_END_F3_SHIFT		0

#define TV_V_CTL_7		_MMIO(0x68054)
 
# define TV_VBURST_START_F4_MASK	0x003f0000
# define TV_VBURST_START_F4_SHIFT	16
 
# define TV_VBURST_END_F4_MASK		0x000000ff
# define TV_VBURST_END_F4_SHIFT		0

#define TV_SC_CTL_1		_MMIO(0x68060)
 
# define TV_SC_DDA1_EN			(1 << 31)
 
# define TV_SC_DDA2_EN			(1 << 30)
 
# define TV_SC_DDA3_EN			(1 << 29)
 
# define TV_SC_RESET_EVERY_2		(0 << 24)
 
# define TV_SC_RESET_EVERY_4		(1 << 24)
 
# define TV_SC_RESET_EVERY_8		(2 << 24)
 
# define TV_SC_RESET_NEVER		(3 << 24)
 
# define TV_BURST_LEVEL_MASK		0x00ff0000
# define TV_BURST_LEVEL_SHIFT		16
 
# define TV_SCDDA1_INC_MASK		0x00000fff
# define TV_SCDDA1_INC_SHIFT		0

#define TV_SC_CTL_2		_MMIO(0x68064)
 
# define TV_SCDDA2_SIZE_MASK		0x7fff0000
# define TV_SCDDA2_SIZE_SHIFT		16
 
# define TV_SCDDA2_INC_MASK		0x00007fff
# define TV_SCDDA2_INC_SHIFT		0

#define TV_SC_CTL_3		_MMIO(0x68068)
 
# define TV_SCDDA3_SIZE_MASK		0x7fff0000
# define TV_SCDDA3_SIZE_SHIFT		16
 
# define TV_SCDDA3_INC_MASK		0x00007fff
# define TV_SCDDA3_INC_SHIFT		0

#define TV_WIN_POS		_MMIO(0x68070)
 
# define TV_XPOS_MASK			0x1fff0000
# define TV_XPOS_SHIFT			16
 
# define TV_YPOS_MASK			0x00000fff
# define TV_YPOS_SHIFT			0

#define TV_WIN_SIZE		_MMIO(0x68074)
 
# define TV_XSIZE_MASK			0x1fff0000
# define TV_XSIZE_SHIFT			16
 
# define TV_YSIZE_MASK			0x00000fff
# define TV_YSIZE_SHIFT			0

#define TV_FILTER_CTL_1		_MMIO(0x68080)
 
# define TV_AUTO_SCALE			(1 << 31)
 
# define TV_V_FILTER_BYPASS		(1 << 29)
 
# define TV_VADAPT			(1 << 28)
# define TV_VADAPT_MODE_MASK		(3 << 26)
 
# define TV_VADAPT_MODE_LEAST		(0 << 26)
 
# define TV_VADAPT_MODE_MODERATE	(1 << 26)
 
# define TV_VADAPT_MODE_MOST		(3 << 26)
 
# define TV_HSCALE_FRAC_MASK		0x00003fff
# define TV_HSCALE_FRAC_SHIFT		0

#define TV_FILTER_CTL_2		_MMIO(0x68084)
 
# define TV_VSCALE_INT_MASK		0x00038000
# define TV_VSCALE_INT_SHIFT		15
 
# define TV_VSCALE_FRAC_MASK		0x00007fff
# define TV_VSCALE_FRAC_SHIFT		0

#define TV_FILTER_CTL_3		_MMIO(0x68088)
 
# define TV_VSCALE_IP_INT_MASK		0x00038000
# define TV_VSCALE_IP_INT_SHIFT		15
 
# define TV_VSCALE_IP_FRAC_MASK		0x00007fff
# define TV_VSCALE_IP_FRAC_SHIFT		0

#define TV_CC_CONTROL		_MMIO(0x68090)
# define TV_CC_ENABLE			(1 << 31)
 
# define TV_CC_FID_MASK			(1 << 27)
# define TV_CC_FID_SHIFT		27
 
# define TV_CC_HOFF_MASK		0x03ff0000
# define TV_CC_HOFF_SHIFT		16
 
# define TV_CC_LINE_MASK		0x0000003f
# define TV_CC_LINE_SHIFT		0

#define TV_CC_DATA		_MMIO(0x68094)
# define TV_CC_RDY			(1 << 31)
 
# define TV_CC_DATA_2_MASK		0x007f0000
# define TV_CC_DATA_2_SHIFT		16
 
# define TV_CC_DATA_1_MASK		0x0000007f
# define TV_CC_DATA_1_SHIFT		0

#define TV_H_LUMA(i)		_MMIO(0x68100 + (i) * 4)  
#define TV_H_CHROMA(i)		_MMIO(0x68200 + (i) * 4)  
#define TV_V_LUMA(i)		_MMIO(0x68300 + (i) * 4)  
#define TV_V_CHROMA(i)		_MMIO(0x68400 + (i) * 4)  

#endif  
