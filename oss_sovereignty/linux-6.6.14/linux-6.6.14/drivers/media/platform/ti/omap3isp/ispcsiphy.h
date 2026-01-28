#ifndef OMAP3_ISP_CSI_PHY_H
#define OMAP3_ISP_CSI_PHY_H
#include "omap3isp.h"
struct isp_csi2_device;
struct regulator;
struct isp_csiphy {
	struct isp_device *isp;
	struct mutex mutex;	 
	struct isp_csi2_device *csi2;
	struct regulator *vdd;
	struct media_entity *entity;
	unsigned int cfg_regs;
	unsigned int phy_regs;
	u8 num_data_lanes;	 
};
int omap3isp_csiphy_acquire(struct isp_csiphy *phy,
			    struct media_entity *entity);
void omap3isp_csiphy_release(struct isp_csiphy *phy);
int omap3isp_csiphy_init(struct isp_device *isp);
void omap3isp_csiphy_cleanup(struct isp_device *isp);
#endif	 
