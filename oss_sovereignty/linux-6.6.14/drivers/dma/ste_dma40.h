#ifndef STE_DMA40_H
#define STE_DMA40_H
#define STEDMA40_MAX_SEG_SIZE 0xFFFF
#define STEDMA40_DEV_DST_MEMORY (-1)
#define	STEDMA40_DEV_SRC_MEMORY (-1)
enum stedma40_mode {
	STEDMA40_MODE_LOGICAL = 0,
	STEDMA40_MODE_PHYSICAL,
	STEDMA40_MODE_OPERATION,
};
enum stedma40_mode_opt {
	STEDMA40_PCHAN_BASIC_MODE = 0,
	STEDMA40_LCHAN_SRC_LOG_DST_LOG = 0,
	STEDMA40_PCHAN_MODULO_MODE,
	STEDMA40_PCHAN_DOUBLE_DST_MODE,
	STEDMA40_LCHAN_SRC_PHY_DST_LOG,
	STEDMA40_LCHAN_SRC_LOG_DST_PHY,
};
#define STEDMA40_ESIZE_8_BIT  0x0
#define STEDMA40_ESIZE_16_BIT 0x1
#define STEDMA40_ESIZE_32_BIT 0x2
#define STEDMA40_ESIZE_64_BIT 0x3
#define STEDMA40_PSIZE_PHY_1  0x4
#define STEDMA40_PSIZE_PHY_2  0x0
#define STEDMA40_PSIZE_PHY_4  0x1
#define STEDMA40_PSIZE_PHY_8  0x2
#define STEDMA40_PSIZE_PHY_16 0x3
#define STEDMA40_PSIZE_LOG_1  STEDMA40_PSIZE_PHY_2
#define STEDMA40_PSIZE_LOG_4  STEDMA40_PSIZE_PHY_4
#define STEDMA40_PSIZE_LOG_8  STEDMA40_PSIZE_PHY_8
#define STEDMA40_PSIZE_LOG_16 STEDMA40_PSIZE_PHY_16
#define STEDMA40_MAX_PHYS 32
enum stedma40_flow_ctrl {
	STEDMA40_NO_FLOW_CTRL,
	STEDMA40_FLOW_CTRL,
};
struct stedma40_half_channel_info {
	bool big_endian;
	enum dma_slave_buswidth data_width;
	int psize;
	enum stedma40_flow_ctrl flow_ctrl;
};
struct stedma40_chan_cfg {
	enum dma_transfer_direction		 dir;
	bool					 high_priority;
	bool					 realtime;
	enum stedma40_mode			 mode;
	enum stedma40_mode_opt			 mode_opt;
	int					 dev_type;
	struct stedma40_half_channel_info	 src_info;
	struct stedma40_half_channel_info	 dst_info;
	bool					 use_fixed_channel;
	int					 phy_channel;
};
#endif  
