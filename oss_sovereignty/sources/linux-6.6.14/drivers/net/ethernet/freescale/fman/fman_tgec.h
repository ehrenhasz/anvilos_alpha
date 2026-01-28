


#ifndef __TGEC_H
#define __TGEC_H

#include "fman_mac.h"

struct mac_device;

int tgec_initialization(struct mac_device *mac_dev,
			struct device_node *mac_node,
			struct fman_mac_params *params);

#endif 
