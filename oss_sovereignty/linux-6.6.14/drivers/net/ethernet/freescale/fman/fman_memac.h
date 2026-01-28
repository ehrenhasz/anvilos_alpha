#ifndef __MEMAC_H
#define __MEMAC_H
#include "fman_mac.h"
#include <linux/netdevice.h>
#include <linux/phy_fixed.h>
struct mac_device;
int memac_initialization(struct mac_device *mac_dev,
			 struct device_node *mac_node,
			 struct fman_mac_params *params);
#endif  
