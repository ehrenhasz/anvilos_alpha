 
 

#include <linux/device.h>
#include <linux/phy.h>

#if IS_REACHABLE(CONFIG_HWMON)
int aqr_hwmon_probe(struct phy_device *phydev);
#else
static inline int aqr_hwmon_probe(struct phy_device *phydev) { return 0; }
#endif
