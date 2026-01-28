#ifndef __LSDC_REGS_H__
#define __LSDC_REGS_H__
#include <linux/bitops.h>
#include <linux/types.h>
#define LSDC_PLL_REF_CLK_KHZ            100000
#define LS7A1000_PIXPLL0_REG            0x04B0
#define LS7A1000_PIXPLL1_REG            0x04C0
#define LS7A1000_PLL_GFX_REG            0x0490
#define LS7A1000_CONF_REG_BASE          0x10010000
#define LS7A2000_PIXPLL0_REG            0x04B0
#define LS7A2000_PIXPLL1_REG            0x04C0
#define LS7A2000_PLL_GFX_REG            0x0490
#define LS7A2000_CONF_REG_BASE          0x10010000
#define CFG_PIX_FMT_MASK                GENMASK(2, 0)
enum lsdc_pixel_format {
	LSDC_PF_NONE = 0,
	LSDC_PF_XRGB444 = 1,     
	LSDC_PF_XRGB555 = 2,     
	LSDC_PF_XRGB565 = 3,     
	LSDC_PF_XRGB8888 = 4,    
};
#define CFG_PAGE_FLIP                   BIT(7)
#define CFG_OUTPUT_ENABLE               BIT(8)
#define CFG_HW_CLONE                    BIT(9)
#define FB_REG_IN_USING                 BIT(11)
#define CFG_GAMMA_EN                    BIT(12)
#define CFG_RESET_N                     BIT(20)
#define CRTC_ANCHORED                   BIT(24)
#define CFG_DMA_STEP_MASK              GENMASK(17, 16)
#define CFG_DMA_STEP_SHIFT             16
enum lsdc_dma_steps {
	LSDC_DMA_STEP_256_BYTES = 0,
	LSDC_DMA_STEP_128_BYTES = 1,
	LSDC_DMA_STEP_64_BYTES = 2,
	LSDC_DMA_STEP_32_BYTES = 3,
};
#define CFG_VALID_BITS_MASK             GENMASK(20, 0)
#define HSYNC_INV                       BIT(31)
#define HSYNC_EN                        BIT(30)
#define HSYNC_END_MASK                  GENMASK(28, 16)
#define HSYNC_END_SHIFT                 16
#define HSYNC_START_MASK                GENMASK(12, 0)
#define HSYNC_START_SHIFT               0
#define VSYNC_INV                       BIT(31)
#define VSYNC_EN                        BIT(30)
#define VSYNC_END_MASK                  GENMASK(27, 16)
#define VSYNC_END_SHIFT                 16
#define VSYNC_START_MASK                GENMASK(11, 0)
#define VSYNC_START_SHIFT               0
#define LSDC_CRTC0_CFG_REG              0x1240
#define LSDC_CRTC0_FB0_ADDR_LO_REG      0x1260
#define LSDC_CRTC0_FB0_ADDR_HI_REG      0x15A0
#define LSDC_CRTC0_STRIDE_REG           0x1280
#define LSDC_CRTC0_FB_ORIGIN_REG        0x1300
#define LSDC_CRTC0_HDISPLAY_REG         0x1400
#define LSDC_CRTC0_HSYNC_REG            0x1420
#define LSDC_CRTC0_VDISPLAY_REG         0x1480
#define LSDC_CRTC0_VSYNC_REG            0x14A0
#define LSDC_CRTC0_GAMMA_INDEX_REG      0x14E0
#define LSDC_CRTC0_GAMMA_DATA_REG       0x1500
#define LSDC_CRTC0_FB1_ADDR_LO_REG      0x1580
#define LSDC_CRTC0_FB1_ADDR_HI_REG      0x15C0
#define LSDC_CRTC1_CFG_REG              0x1250
#define LSDC_CRTC1_FB0_ADDR_LO_REG      0x1270
#define LSDC_CRTC1_FB0_ADDR_HI_REG      0x15B0
#define LSDC_CRTC1_STRIDE_REG           0x1290
#define LSDC_CRTC1_FB_ORIGIN_REG        0x1310
#define LSDC_CRTC1_HDISPLAY_REG         0x1410
#define LSDC_CRTC1_HSYNC_REG            0x1430
#define LSDC_CRTC1_VDISPLAY_REG         0x1490
#define LSDC_CRTC1_VSYNC_REG            0x14B0
#define LSDC_CRTC1_GAMMA_INDEX_REG      0x14F0
#define LSDC_CRTC1_GAMMA_DATA_REG       0x1510
#define LSDC_CRTC1_FB1_ADDR_LO_REG      0x1590
#define LSDC_CRTC1_FB1_ADDR_HI_REG      0x15D0
#define PHY_CLOCK_POL                   BIT(9)
#define PHY_CLOCK_EN                    BIT(8)
#define PHY_DE_POL                      BIT(1)
#define PHY_DATA_EN                     BIT(0)
#define LSDC_CRTC0_DVO_CONF_REG         0x13C0
#define LSDC_CRTC1_DVO_CONF_REG         0x13D0
#define LSDC_CRTC0_SCAN_POS_REG         0x14C0
#define LSDC_CRTC1_SCAN_POS_REG         0x14D0
#define SYNC_DEVIATION_EN               BIT(31)
#define SYNC_DEVIATION_NUM              GENMASK(12, 0)
#define LSDC_CRTC0_SYNC_DEVIATION_REG   0x1B80
#define LSDC_CRTC1_SYNC_DEVIATION_REG   0x1B90
#define CRTC_PIPE_OFFSET                0x10
#define CURSOR_FORMAT_MASK              GENMASK(1, 0)
#define CURSOR_FORMAT_SHIFT             0
enum lsdc_cursor_format {
	CURSOR_FORMAT_DISABLE = 0,
	CURSOR_FORMAT_MONOCHROME = 1,    
	CURSOR_FORMAT_ARGB8888 = 2,      
};
#define CURSOR_SIZE_SHIFT               2
enum lsdc_cursor_size {
	CURSOR_SIZE_32X32 = 0,
	CURSOR_SIZE_64X64 = 1,
};
#define CURSOR_LOCATION_SHIFT           4
enum lsdc_cursor_location {
	CURSOR_ON_CRTC0 = 0,
	CURSOR_ON_CRTC1 = 1,
};
#define LSDC_CURSOR0_CFG_REG            0x1520
#define LSDC_CURSOR0_ADDR_LO_REG        0x1530
#define LSDC_CURSOR0_ADDR_HI_REG        0x15e0
#define LSDC_CURSOR0_POSITION_REG       0x1540   
#define LSDC_CURSOR0_BG_COLOR_REG       0x1550   
#define LSDC_CURSOR0_FG_COLOR_REG       0x1560   
#define LSDC_CURSOR1_CFG_REG            0x1670
#define LSDC_CURSOR1_ADDR_LO_REG        0x1680
#define LSDC_CURSOR1_ADDR_HI_REG        0x16e0
#define LSDC_CURSOR1_POSITION_REG       0x1690   
#define LSDC_CURSOR1_BG_COLOR_REG       0x16A0   
#define LSDC_CURSOR1_FG_COLOR_REG       0x16B0   
#define LSDC_INT_REG                    0x1570
#define INT_CRTC0_VSYNC                 BIT(2)
#define INT_CRTC0_HSYNC                 BIT(3)
#define INT_CRTC0_RF                    BIT(6)
#define INT_CRTC0_IDBU                  BIT(8)
#define INT_CRTC0_IDBFU                 BIT(10)
#define INT_CRTC1_VSYNC                 BIT(0)
#define INT_CRTC1_HSYNC                 BIT(1)
#define INT_CRTC1_RF                    BIT(5)
#define INT_CRTC1_IDBU                  BIT(7)
#define INT_CRTC1_IDBFU                 BIT(9)
#define INT_CRTC0_VSYNC_EN              BIT(18)
#define INT_CRTC0_HSYNC_EN              BIT(19)
#define INT_CRTC0_RF_EN                 BIT(22)
#define INT_CRTC0_IDBU_EN               BIT(24)
#define INT_CRTC0_IDBFU_EN              BIT(26)
#define INT_CRTC1_VSYNC_EN              BIT(16)
#define INT_CRTC1_HSYNC_EN              BIT(17)
#define INT_CRTC1_RF_EN                 BIT(21)
#define INT_CRTC1_IDBU_EN               BIT(23)
#define INT_CRTC1_IDBFU_EN              BIT(25)
#define INT_STATUS_MASK                 GENMASK(15, 0)
#define LS7A_DC_GPIO_DAT_REG            0x1650
#define LS7A_DC_GPIO_DIR_REG            0x1660
#define LSDC_HDMI0_ZONE_REG             0x1700
#define LSDC_HDMI1_ZONE_REG             0x1710
#define HDMI_H_ZONE_IDLE_SHIFT          0
#define HDMI_V_ZONE_IDLE_SHIFT          16
#define HDMI_INTERFACE_EN               BIT(0)
#define HDMI_PACKET_EN                  BIT(1)
#define HDMI_AUDIO_EN                   BIT(2)
#define HDMI_VIDEO_PREAMBLE_MASK        GENMASK(7, 4)
#define HDMI_VIDEO_PREAMBLE_SHIFT       4
#define HW_I2C_EN                       BIT(8)
#define HDMI_CTL_PERIOD_MODE            BIT(9)
#define LSDC_HDMI0_INTF_CTRL_REG        0x1720
#define LSDC_HDMI1_INTF_CTRL_REG        0x1730
#define HDMI_PHY_EN                     BIT(0)
#define HDMI_PHY_RESET_N                BIT(1)
#define HDMI_PHY_TERM_L_EN              BIT(8)
#define HDMI_PHY_TERM_H_EN              BIT(9)
#define HDMI_PHY_TERM_DET_EN            BIT(10)
#define HDMI_PHY_TERM_STATUS            BIT(11)
#define LSDC_HDMI0_PHY_CTRL_REG         0x1800
#define LSDC_HDMI1_PHY_CTRL_REG         0x1810
#define HDMI_PLL_ENABLE                 BIT(0)
#define HDMI_PLL_LOCKED                 BIT(16)
#define HDMI_PLL_BYPASS                 BIT(17)
#define HDMI_PLL_IDF_SHIFT              1
#define HDMI_PLL_IDF_MASK               GENMASK(5, 1)
#define HDMI_PLL_LF_SHIFT               6
#define HDMI_PLL_LF_MASK                GENMASK(12, 6)
#define HDMI_PLL_ODF_SHIFT              13
#define HDMI_PLL_ODF_MASK               GENMASK(15, 13)
#define LSDC_HDMI0_PHY_PLL_REG          0x1820
#define LSDC_HDMI1_PHY_PLL_REG          0x1830
#define LSDC_HDMI_HPD_STATUS_REG        0x1BA0
#define HDMI0_HPD_FLAG                  BIT(0)
#define HDMI1_HPD_FLAG                  BIT(1)
#define LSDC_HDMI0_PHY_CAL_REG          0x18C0
#define LSDC_HDMI1_PHY_CAL_REG          0x18D0
#define LSDC_HDMI0_AVI_CONTENT0         0x18E0
#define LSDC_HDMI1_AVI_CONTENT0         0x18D0
#define LSDC_HDMI0_AVI_CONTENT1         0x1900
#define LSDC_HDMI1_AVI_CONTENT1         0x1910
#define LSDC_HDMI0_AVI_CONTENT2         0x1920
#define LSDC_HDMI1_AVI_CONTENT2         0x1930
#define LSDC_HDMI0_AVI_CONTENT3         0x1940
#define LSDC_HDMI1_AVI_CONTENT3         0x1950
#define AVI_PKT_ENABLE                  BIT(0)
#define AVI_PKT_SEND_FREQ               BIT(1)
#define AVI_PKT_UPDATE                  BIT(2)
#define LSDC_HDMI0_AVI_INFO_CRTL_REG    0x1960
#define LSDC_HDMI1_AVI_INFO_CRTL_REG    0x1970
#define LSDC_CRTC0_VSYNC_COUNTER_REG    0x1A00
#define LSDC_CRTC1_VSYNC_COUNTER_REG    0x1A10
#define LSDC_HDMI0_AUDIO_PLL_LO_REG     0x1A20
#define LSDC_HDMI1_AUDIO_PLL_LO_REG     0x1A30
#define LSDC_HDMI0_AUDIO_PLL_HI_REG     0x1A40
#define LSDC_HDMI1_AUDIO_PLL_HI_REG     0x1A50
#endif
