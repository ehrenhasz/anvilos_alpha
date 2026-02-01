 
 

#ifndef __MESON_DW_MIPI_DSI_H
#define __MESON_DW_MIPI_DSI_H

 
 
#define MIPI_DSI_TOP_SW_RESET                      0x3c0

#define MIPI_DSI_TOP_SW_RESET_DWC	BIT(0)
#define MIPI_DSI_TOP_SW_RESET_INTR	BIT(1)
#define MIPI_DSI_TOP_SW_RESET_DPI	BIT(2)
#define MIPI_DSI_TOP_SW_RESET_TIMING	BIT(3)

 
#define MIPI_DSI_TOP_CLK_CNTL                      0x3c4

#define MIPI_DSI_TOP_CLK_SYSCLK_EN	BIT(0)
#define MIPI_DSI_TOP_CLK_PIXCLK_EN	BIT(1)

 
#define MIPI_DSI_TOP_CNTL                          0x3c8

 
#define VENC_IN_COLOR_30B   0x0
#define VENC_IN_COLOR_24B   0x1
#define VENC_IN_COLOR_18B   0x2
#define VENC_IN_COLOR_16B   0x3

 
#define DPI_COLOR_16BIT_CFG_1		0
#define DPI_COLOR_16BIT_CFG_2		1
#define DPI_COLOR_16BIT_CFG_3		2
#define DPI_COLOR_18BIT_CFG_1		3
#define DPI_COLOR_18BIT_CFG_2		4
#define DPI_COLOR_24BIT			5
#define DPI_COLOR_20BIT_YCBCR_422	6
#define DPI_COLOR_24BIT_YCBCR_422	7
#define DPI_COLOR_16BIT_YCBCR_422	8
#define DPI_COLOR_30BIT			9
#define DPI_COLOR_36BIT			10
#define DPI_COLOR_12BIT_YCBCR_420	11

#define MIPI_DSI_TOP_DPI_COLOR_MODE	GENMASK(23, 20)
#define MIPI_DSI_TOP_IN_COLOR_MODE	GENMASK(18, 16)
#define MIPI_DSI_TOP_CHROMA_SUBSAMPLE	GENMASK(15, 14)
#define MIPI_DSI_TOP_COMP2_SEL		GENMASK(13, 12)
#define MIPI_DSI_TOP_COMP1_SEL		GENMASK(11, 10)
#define MIPI_DSI_TOP_COMP0_SEL		GENMASK(9, 8)
#define MIPI_DSI_TOP_DE_INVERT		BIT(6)
#define MIPI_DSI_TOP_HSYNC_INVERT	BIT(5)
#define MIPI_DSI_TOP_VSYNC_INVERT	BIT(4)
#define MIPI_DSI_TOP_DPICOLORM		BIT(3)
#define MIPI_DSI_TOP_DPISHUTDN		BIT(2)

#define MIPI_DSI_TOP_SUSPEND_CNTL                  0x3cc
#define MIPI_DSI_TOP_SUSPEND_LINE                  0x3d0
#define MIPI_DSI_TOP_SUSPEND_PIX                   0x3d4
#define MIPI_DSI_TOP_MEAS_CNTL                     0x3d8
 
#define MIPI_DSI_TOP_STAT                          0x3dc
#define MIPI_DSI_TOP_MEAS_STAT_TE0                 0x3e0
#define MIPI_DSI_TOP_MEAS_STAT_TE1                 0x3e4
#define MIPI_DSI_TOP_MEAS_STAT_VS0                 0x3e8
#define MIPI_DSI_TOP_MEAS_STAT_VS1                 0x3ec
 
#define MIPI_DSI_TOP_INTR_CNTL_STAT                0x3f0


#define MIPI_DSI_TOP_MEM_PD                        0x3f4

#endif  
