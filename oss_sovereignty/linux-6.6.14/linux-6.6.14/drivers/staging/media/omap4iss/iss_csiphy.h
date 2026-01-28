#ifndef OMAP4_ISS_CSI_PHY_H
#define OMAP4_ISS_CSI_PHY_H
#include <linux/platform_data/media/omap4iss.h>
struct iss_csi2_device;
struct iss_csiphy_dphy_cfg {
	u8 ths_term;
	u8 ths_settle;
	u8 tclk_term;
	unsigned tclk_miss:1;
	u8 tclk_settle;
};
struct iss_csiphy {
	struct iss_device *iss;
	struct mutex mutex;	 
	u8 phy_in_use;
	struct iss_csi2_device *csi2;
	unsigned int cfg_regs;
	unsigned int phy_regs;
	u8 max_data_lanes;	 
	u8 used_data_lanes;	 
	struct iss_csiphy_lanes_cfg lanes;
	struct iss_csiphy_dphy_cfg dphy;
};
int omap4iss_csiphy_config(struct iss_device *iss,
			   struct v4l2_subdev *csi2_subdev);
int omap4iss_csiphy_acquire(struct iss_csiphy *phy);
void omap4iss_csiphy_release(struct iss_csiphy *phy);
int omap4iss_csiphy_init(struct iss_device *iss);
#endif	 
