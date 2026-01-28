#ifndef __ASM_POWERPC_MPC5xxx_H__
#define __ASM_POWERPC_MPC5xxx_H__
#include <linux/property.h>
unsigned long mpc5xxx_fwnode_get_bus_frequency(struct fwnode_handle *fwnode);
static inline unsigned long mpc5xxx_get_bus_frequency(struct device *dev)
{
	return mpc5xxx_fwnode_get_bus_frequency(dev_fwnode(dev));
}
#endif  
