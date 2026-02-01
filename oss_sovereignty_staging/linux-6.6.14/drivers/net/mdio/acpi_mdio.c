
 

#include <linux/acpi.h>
#include <linux/acpi_mdio.h>
#include <linux/bits.h>
#include <linux/dev_printk.h>
#include <linux/fwnode_mdio.h>
#include <linux/module.h>
#include <linux/types.h>

MODULE_AUTHOR("Calvin Johnson <calvin.johnson@oss.nxp.com>");
MODULE_LICENSE("GPL");

 
int __acpi_mdiobus_register(struct mii_bus *mdio, struct fwnode_handle *fwnode,
			    struct module *owner)
{
	struct fwnode_handle *child;
	u32 addr;
	int ret;

	 
	mdio->phy_mask = GENMASK(31, 0);
	ret = __mdiobus_register(mdio, owner);
	if (ret)
		return ret;

	ACPI_COMPANION_SET(&mdio->dev, to_acpi_device_node(fwnode));

	 
	fwnode_for_each_child_node(fwnode, child) {
		ret = acpi_get_local_address(ACPI_HANDLE_FWNODE(child), &addr);
		if (ret || addr >= PHY_MAX_ADDR)
			continue;

		ret = fwnode_mdiobus_register_phy(mdio, child, addr);
		if (ret == -ENODEV)
			dev_err(&mdio->dev,
				"MDIO device at address %d is missing.\n",
				addr);
	}
	return 0;
}
EXPORT_SYMBOL(__acpi_mdiobus_register);
