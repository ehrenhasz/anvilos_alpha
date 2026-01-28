#ifndef __OMAP3ISP_H__
#define __OMAP3ISP_H__
enum isp_interface_type {
	ISP_INTERFACE_PARALLEL,
	ISP_INTERFACE_CSI2A_PHY2,
	ISP_INTERFACE_CCP2B_PHY1,
	ISP_INTERFACE_CCP2B_PHY2,
	ISP_INTERFACE_CSI2C_PHY1,
};
struct isp_parallel_cfg {
	unsigned int data_lane_shift:3;
	unsigned int clk_pol:1;
	unsigned int hs_pol:1;
	unsigned int vs_pol:1;
	unsigned int fld_pol:1;
	unsigned int data_pol:1;
	unsigned int bt656:1;
};
enum {
	ISP_CCP2_PHY_DATA_CLOCK = 0,
	ISP_CCP2_PHY_DATA_STROBE = 1,
};
enum {
	ISP_CCP2_MODE_MIPI = 0,
	ISP_CCP2_MODE_CCP2 = 1,
};
struct isp_csiphy_lane {
	u8 pos;
	u8 pol;
};
#define ISP_CSIPHY1_NUM_DATA_LANES	1
#define ISP_CSIPHY2_NUM_DATA_LANES	2
struct isp_csiphy_lanes_cfg {
	struct isp_csiphy_lane data[ISP_CSIPHY2_NUM_DATA_LANES];
	struct isp_csiphy_lane clk;
};
struct isp_ccp2_cfg {
	unsigned int strobe_clk_pol:1;
	unsigned int crc:1;
	unsigned int ccp2_mode:1;
	unsigned int phy_layer:1;
	unsigned int vpclk_div:2;
	unsigned int vp_clk_pol:1;
	struct isp_csiphy_lanes_cfg lanecfg;
};
struct isp_csi2_cfg {
	unsigned crc:1;
	struct isp_csiphy_lanes_cfg lanecfg;
	u8 num_data_lanes;
};
struct isp_bus_cfg {
	enum isp_interface_type interface;
	union {
		struct isp_parallel_cfg parallel;
		struct isp_ccp2_cfg ccp2;
		struct isp_csi2_cfg csi2;
	} bus;  
};
#endif	 
