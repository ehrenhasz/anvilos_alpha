
 

#include <linux/device.h>
#include <linux/err.h>
#include <linux/fwnode_mdio.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netdevice.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_mdio.h>
#include <linux/of_net.h>
#include <linux/phy.h>
#include <linux/phy_fixed.h>

#define DEFAULT_GPIO_RESET_DELAY	10	 

MODULE_AUTHOR("Grant Likely <grant.likely@secretlab.ca>");
MODULE_LICENSE("GPL");

 
static int of_get_phy_id(struct device_node *device, u32 *phy_id)
{
	return fwnode_get_phy_id(of_fwnode_handle(device), phy_id);
}

int of_mdiobus_phy_device_register(struct mii_bus *mdio, struct phy_device *phy,
				   struct device_node *child, u32 addr)
{
	return fwnode_mdiobus_phy_device_register(mdio, phy,
						  of_fwnode_handle(child),
						  addr);
}
EXPORT_SYMBOL(of_mdiobus_phy_device_register);

static int of_mdiobus_register_phy(struct mii_bus *mdio,
				    struct device_node *child, u32 addr)
{
	return fwnode_mdiobus_register_phy(mdio, of_fwnode_handle(child), addr);
}

static int of_mdiobus_register_device(struct mii_bus *mdio,
				      struct device_node *child, u32 addr)
{
	struct fwnode_handle *fwnode = of_fwnode_handle(child);
	struct mdio_device *mdiodev;
	int rc;

	mdiodev = mdio_device_create(mdio, addr);
	if (IS_ERR(mdiodev))
		return PTR_ERR(mdiodev);

	 
	fwnode_handle_get(fwnode);
	device_set_node(&mdiodev->dev, fwnode);

	 
	rc = mdio_device_register(mdiodev);
	if (rc) {
		device_set_node(&mdiodev->dev, NULL);
		fwnode_handle_put(fwnode);
		mdio_device_free(mdiodev);
		return rc;
	}

	dev_dbg(&mdio->dev, "registered mdio device %pOFn at address %i\n",
		child, addr);
	return 0;
}

 
static const struct of_device_id whitelist_phys[] = {
	{ .compatible = "brcm,40nm-ephy" },
	{ .compatible = "broadcom,bcm5241" },
	{ .compatible = "marvell,88E1111", },
	{ .compatible = "marvell,88e1116", },
	{ .compatible = "marvell,88e1118", },
	{ .compatible = "marvell,88e1145", },
	{ .compatible = "marvell,88e1149r", },
	{ .compatible = "marvell,88e1310", },
	{ .compatible = "marvell,88E1510", },
	{ .compatible = "marvell,88E1514", },
	{ .compatible = "moxa,moxart-rtl8201cp", },
	{}
};

 
bool of_mdiobus_child_is_phy(struct device_node *child)
{
	u32 phy_id;

	if (of_get_phy_id(child, &phy_id) != -EINVAL)
		return true;

	if (of_device_is_compatible(child, "ethernet-phy-ieee802.3-c45"))
		return true;

	if (of_device_is_compatible(child, "ethernet-phy-ieee802.3-c22"))
		return true;

	if (of_match_node(whitelist_phys, child)) {
		pr_warn(FW_WARN
			"%pOF: Whitelisted compatible string. Please remove\n",
			child);
		return true;
	}

	if (!of_property_present(child, "compatible"))
		return true;

	return false;
}
EXPORT_SYMBOL(of_mdiobus_child_is_phy);

 
int __of_mdiobus_register(struct mii_bus *mdio, struct device_node *np,
			  struct module *owner)
{
	struct device_node *child;
	bool scanphys = false;
	int addr, rc;

	if (!np)
		return __mdiobus_register(mdio, owner);

	 
	if (!of_device_is_available(np))
		return -ENODEV;

	 
	mdio->phy_mask = ~0;

	device_set_node(&mdio->dev, of_fwnode_handle(np));

	 
	mdio->reset_delay_us = DEFAULT_GPIO_RESET_DELAY;
	of_property_read_u32(np, "reset-delay-us", &mdio->reset_delay_us);
	mdio->reset_post_delay_us = 0;
	of_property_read_u32(np, "reset-post-delay-us", &mdio->reset_post_delay_us);

	 
	rc = __mdiobus_register(mdio, owner);
	if (rc)
		return rc;

	 
	for_each_available_child_of_node(np, child) {
		addr = of_mdio_parse_addr(&mdio->dev, child);
		if (addr < 0) {
			scanphys = true;
			continue;
		}

		if (of_mdiobus_child_is_phy(child))
			rc = of_mdiobus_register_phy(mdio, child, addr);
		else
			rc = of_mdiobus_register_device(mdio, child, addr);

		if (rc == -ENODEV)
			dev_err(&mdio->dev,
				"MDIO device at address %d is missing.\n",
				addr);
		else if (rc)
			goto unregister;
	}

	if (!scanphys)
		return 0;

	 
	for_each_available_child_of_node(np, child) {
		 
		if (of_property_present(child, "reg"))
			continue;

		for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
			 
			if (mdiobus_is_registered_device(mdio, addr))
				continue;

			 
			dev_info(&mdio->dev, "scan phy %pOFn at address %i\n",
				 child, addr);

			if (of_mdiobus_child_is_phy(child)) {
				 
				rc = of_mdiobus_register_phy(mdio, child, addr);
				if (!rc)
					break;
				if (rc != -ENODEV)
					goto unregister;
			}
		}
	}

	return 0;

unregister:
	of_node_put(child);
	mdiobus_unregister(mdio);
	return rc;
}
EXPORT_SYMBOL(__of_mdiobus_register);

 
struct mdio_device *of_mdio_find_device(struct device_node *np)
{
	return fwnode_mdio_find_device(of_fwnode_handle(np));
}
EXPORT_SYMBOL(of_mdio_find_device);

 
struct phy_device *of_phy_find_device(struct device_node *phy_np)
{
	return fwnode_phy_find_device(of_fwnode_handle(phy_np));
}
EXPORT_SYMBOL(of_phy_find_device);

 
struct phy_device *of_phy_connect(struct net_device *dev,
				  struct device_node *phy_np,
				  void (*hndlr)(struct net_device *), u32 flags,
				  phy_interface_t iface)
{
	struct phy_device *phy = of_phy_find_device(phy_np);
	int ret;

	if (!phy)
		return NULL;

	phy->dev_flags |= flags;

	ret = phy_connect_direct(dev, phy, hndlr, iface);

	 
	put_device(&phy->mdio.dev);

	return ret ? NULL : phy;
}
EXPORT_SYMBOL(of_phy_connect);

 
struct phy_device *of_phy_get_and_connect(struct net_device *dev,
					  struct device_node *np,
					  void (*hndlr)(struct net_device *))
{
	phy_interface_t iface;
	struct device_node *phy_np;
	struct phy_device *phy;
	int ret;

	ret = of_get_phy_mode(np, &iface);
	if (ret)
		return NULL;
	if (of_phy_is_fixed_link(np)) {
		ret = of_phy_register_fixed_link(np);
		if (ret < 0) {
			netdev_err(dev, "broken fixed-link specification\n");
			return NULL;
		}
		phy_np = of_node_get(np);
	} else {
		phy_np = of_parse_phandle(np, "phy-handle", 0);
		if (!phy_np)
			return NULL;
	}

	phy = of_phy_connect(dev, phy_np, hndlr, 0, iface);

	of_node_put(phy_np);

	return phy;
}
EXPORT_SYMBOL(of_phy_get_and_connect);

 
bool of_phy_is_fixed_link(struct device_node *np)
{
	struct device_node *dn;
	int len, err;
	const char *managed;

	 
	dn = of_get_child_by_name(np, "fixed-link");
	if (dn) {
		of_node_put(dn);
		return true;
	}

	err = of_property_read_string(np, "managed", &managed);
	if (err == 0 && strcmp(managed, "auto") != 0)
		return true;

	 
	if (of_get_property(np, "fixed-link", &len) &&
	    len == (5 * sizeof(__be32)))
		return true;

	return false;
}
EXPORT_SYMBOL(of_phy_is_fixed_link);

int of_phy_register_fixed_link(struct device_node *np)
{
	struct fixed_phy_status status = {};
	struct device_node *fixed_link_node;
	u32 fixed_link_prop[5];
	const char *managed;

	if (of_property_read_string(np, "managed", &managed) == 0 &&
	    strcmp(managed, "in-band-status") == 0) {
		 
		goto register_phy;
	}

	 
	fixed_link_node = of_get_child_by_name(np, "fixed-link");
	if (fixed_link_node) {
		status.link = 1;
		status.duplex = of_property_read_bool(fixed_link_node,
						      "full-duplex");
		if (of_property_read_u32(fixed_link_node, "speed",
					 &status.speed)) {
			of_node_put(fixed_link_node);
			return -EINVAL;
		}
		status.pause = of_property_read_bool(fixed_link_node, "pause");
		status.asym_pause = of_property_read_bool(fixed_link_node,
							  "asym-pause");
		of_node_put(fixed_link_node);

		goto register_phy;
	}

	 
	if (of_property_read_u32_array(np, "fixed-link", fixed_link_prop,
				       ARRAY_SIZE(fixed_link_prop)) == 0) {
		status.link = 1;
		status.duplex = fixed_link_prop[1];
		status.speed  = fixed_link_prop[2];
		status.pause  = fixed_link_prop[3];
		status.asym_pause = fixed_link_prop[4];
		goto register_phy;
	}

	return -ENODEV;

register_phy:
	return PTR_ERR_OR_ZERO(fixed_phy_register(PHY_POLL, &status, np));
}
EXPORT_SYMBOL(of_phy_register_fixed_link);

void of_phy_deregister_fixed_link(struct device_node *np)
{
	struct phy_device *phydev;

	phydev = of_phy_find_device(np);
	if (!phydev)
		return;

	fixed_phy_unregister(phydev);

	put_device(&phydev->mdio.dev);	 
	phy_device_free(phydev);	 
}
EXPORT_SYMBOL(of_phy_deregister_fixed_link);
