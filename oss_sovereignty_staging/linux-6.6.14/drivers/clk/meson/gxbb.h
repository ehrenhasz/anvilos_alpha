 
 

#ifndef __GXBB_H
#define __GXBB_H

 
#define SCR				0x2C  
#define TIMEOUT_VALUE			0x3c  

#define HHI_GP0_PLL_CNTL		0x40  
#define HHI_GP0_PLL_CNTL2		0x44  
#define HHI_GP0_PLL_CNTL3		0x48  
#define HHI_GP0_PLL_CNTL4		0x4c  
#define	HHI_GP0_PLL_CNTL5		0x50  
#define	HHI_GP0_PLL_CNTL1		0x58  

#define HHI_XTAL_DIVN_CNTL		0xbc  
#define HHI_TIMER90K			0xec  

#define HHI_MEM_PD_REG0			0x100  
#define HHI_MEM_PD_REG1			0x104  
#define HHI_VPU_MEM_PD_REG1		0x108  
#define HHI_VIID_CLK_DIV		0x128  
#define HHI_VIID_CLK_CNTL		0x12c  

#define HHI_GCLK_MPEG0			0x140  
#define HHI_GCLK_MPEG1			0x144  
#define HHI_GCLK_MPEG2			0x148  
#define HHI_GCLK_OTHER			0x150  
#define HHI_GCLK_AO			0x154  
#define HHI_SYS_OSCIN_CNTL		0x158  
#define HHI_SYS_CPU_CLK_CNTL1		0x15c  
#define HHI_SYS_CPU_RESET_CNTL		0x160  
#define HHI_VID_CLK_DIV			0x164  

#define HHI_MPEG_CLK_CNTL		0x174  
#define HHI_AUD_CLK_CNTL		0x178  
#define HHI_VID_CLK_CNTL		0x17c  
#define HHI_AUD_CLK_CNTL2		0x190  
#define HHI_VID_CLK_CNTL2		0x194  
#define HHI_SYS_CPU_CLK_CNTL0		0x19c  
#define HHI_VID_PLL_CLK_DIV		0x1a0  
#define HHI_AUD_CLK_CNTL3		0x1a4  
#define HHI_MALI_CLK_CNTL		0x1b0  
#define HHI_VPU_CLK_CNTL		0x1bC  

#define HHI_HDMI_CLK_CNTL		0x1CC  
#define HHI_VDEC_CLK_CNTL		0x1E0  
#define HHI_VDEC2_CLK_CNTL		0x1E4  
#define HHI_VDEC3_CLK_CNTL		0x1E8  
#define HHI_VDEC4_CLK_CNTL		0x1EC  
#define HHI_HDCP22_CLK_CNTL		0x1F0  
#define HHI_VAPBCLK_CNTL		0x1F4  

#define HHI_VPU_CLKB_CNTL		0x20C  
#define HHI_USB_CLK_CNTL		0x220  
#define HHI_32K_CLK_CNTL		0x224  
#define HHI_GEN_CLK_CNTL		0x228  

#define HHI_PCM_CLK_CNTL		0x258  
#define HHI_NAND_CLK_CNTL		0x25C  
#define HHI_SD_EMMC_CLK_CNTL		0x264  

#define HHI_MPLL_CNTL			0x280  
#define HHI_MPLL_CNTL2			0x284  
#define HHI_MPLL_CNTL3			0x288  
#define HHI_MPLL_CNTL4			0x28C  
#define HHI_MPLL_CNTL5			0x290  
#define HHI_MPLL_CNTL6			0x294  
#define HHI_MPLL_CNTL7			0x298  
#define HHI_MPLL_CNTL8			0x29C  
#define HHI_MPLL_CNTL9			0x2A0  
#define HHI_MPLL_CNTL10			0x2A4  

#define HHI_MPLL3_CNTL0			0x2E0  
#define HHI_MPLL3_CNTL1			0x2E4  
#define HHI_VDAC_CNTL0			0x2F4  
#define HHI_VDAC_CNTL1			0x2F8  

#define HHI_SYS_PLL_CNTL		0x300  
#define HHI_SYS_PLL_CNTL2		0x304  
#define HHI_SYS_PLL_CNTL3		0x308  
#define HHI_SYS_PLL_CNTL4		0x30c  
#define HHI_SYS_PLL_CNTL5		0x310  
#define HHI_DPLL_TOP_I			0x318  
#define HHI_DPLL_TOP2_I			0x31C  
#define HHI_HDMI_PLL_CNTL		0x320  
#define HHI_HDMI_PLL_CNTL2		0x324  
#define HHI_HDMI_PLL_CNTL3		0x328  
#define HHI_HDMI_PLL_CNTL4		0x32C  
#define HHI_HDMI_PLL_CNTL5		0x330  
#define HHI_HDMI_PLL_CNTL6		0x334  
#define HHI_HDMI_PLL_CNTL_I		0x338  
#define HHI_HDMI_PLL_CNTL7		0x33C  

#define HHI_HDMI_PHY_CNTL0		0x3A0  
#define HHI_HDMI_PHY_CNTL1		0x3A4  
#define HHI_HDMI_PHY_CNTL2		0x3A8  
#define HHI_HDMI_PHY_CNTL3		0x3AC  

#define HHI_VID_LOCK_CLK_CNTL		0x3C8  
#define HHI_BT656_CLK_CNTL		0x3D4  
#define HHI_SAR_CLK_CNTL		0x3D8  

#endif  
