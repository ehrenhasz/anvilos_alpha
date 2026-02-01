
 

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/acpi.h>
#include <linux/bitmap.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/ethtool.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mdio.h>
#include <linux/mii.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/netdevice.h>
#include <linux/phy.h>
#include <linux/phylib_stubs.h>
#include <linux/phy_led_triggers.h>
#include <linux/pse-pd/pse.h>
#include <linux/property.h>
#include <linux/rtnetlink.h>
#include <linux/sfp.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>

MODULE_DESCRIPTION("PHY library");
MODULE_AUTHOR("Andy Fleming");
MODULE_LICENSE("GPL");

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_basic_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_basic_features);

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_basic_t1_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_basic_t1_features);

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_basic_t1s_p2mp_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_basic_t1s_p2mp_features);

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_gbit_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_gbit_features);

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_gbit_fibre_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_gbit_fibre_features);

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_gbit_all_ports_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_gbit_all_ports_features);

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_10gbit_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_10gbit_features);

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_10gbit_fec_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_10gbit_fec_features);

const int phy_basic_ports_array[3] = {
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_TP_BIT,
	ETHTOOL_LINK_MODE_MII_BIT,
};
EXPORT_SYMBOL_GPL(phy_basic_ports_array);

const int phy_fibre_port_array[1] = {
	ETHTOOL_LINK_MODE_FIBRE_BIT,
};
EXPORT_SYMBOL_GPL(phy_fibre_port_array);

const int phy_all_ports_features_array[7] = {
	ETHTOOL_LINK_MODE_Autoneg_BIT,
	ETHTOOL_LINK_MODE_TP_BIT,
	ETHTOOL_LINK_MODE_MII_BIT,
	ETHTOOL_LINK_MODE_FIBRE_BIT,
	ETHTOOL_LINK_MODE_AUI_BIT,
	ETHTOOL_LINK_MODE_BNC_BIT,
	ETHTOOL_LINK_MODE_Backplane_BIT,
};
EXPORT_SYMBOL_GPL(phy_all_ports_features_array);

const int phy_10_100_features_array[4] = {
	ETHTOOL_LINK_MODE_10baseT_Half_BIT,
	ETHTOOL_LINK_MODE_10baseT_Full_BIT,
	ETHTOOL_LINK_MODE_100baseT_Half_BIT,
	ETHTOOL_LINK_MODE_100baseT_Full_BIT,
};
EXPORT_SYMBOL_GPL(phy_10_100_features_array);

const int phy_basic_t1_features_array[3] = {
	ETHTOOL_LINK_MODE_TP_BIT,
	ETHTOOL_LINK_MODE_10baseT1L_Full_BIT,
	ETHTOOL_LINK_MODE_100baseT1_Full_BIT,
};
EXPORT_SYMBOL_GPL(phy_basic_t1_features_array);

const int phy_basic_t1s_p2mp_features_array[2] = {
	ETHTOOL_LINK_MODE_TP_BIT,
	ETHTOOL_LINK_MODE_10baseT1S_P2MP_Half_BIT,
};
EXPORT_SYMBOL_GPL(phy_basic_t1s_p2mp_features_array);

const int phy_gbit_features_array[2] = {
	ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
};
EXPORT_SYMBOL_GPL(phy_gbit_features_array);

const int phy_10gbit_features_array[1] = {
	ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
};
EXPORT_SYMBOL_GPL(phy_10gbit_features_array);

static const int phy_10gbit_fec_features_array[1] = {
	ETHTOOL_LINK_MODE_10000baseR_FEC_BIT,
};

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_10gbit_full_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_10gbit_full_features);

static const int phy_10gbit_full_features_array[] = {
	ETHTOOL_LINK_MODE_10baseT_Full_BIT,
	ETHTOOL_LINK_MODE_100baseT_Full_BIT,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
};

static const int phy_eee_cap1_features_array[] = {
	ETHTOOL_LINK_MODE_100baseT_Full_BIT,
	ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseT_Full_BIT,
	ETHTOOL_LINK_MODE_1000baseKX_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKX4_Full_BIT,
	ETHTOOL_LINK_MODE_10000baseKR_Full_BIT,
};

__ETHTOOL_DECLARE_LINK_MODE_MASK(phy_eee_cap1_features) __ro_after_init;
EXPORT_SYMBOL_GPL(phy_eee_cap1_features);

static void features_init(void)
{
	 
	linkmode_set_bit_array(phy_basic_ports_array,
			       ARRAY_SIZE(phy_basic_ports_array),
			       phy_basic_features);
	linkmode_set_bit_array(phy_10_100_features_array,
			       ARRAY_SIZE(phy_10_100_features_array),
			       phy_basic_features);

	 
	linkmode_set_bit_array(phy_basic_t1_features_array,
			       ARRAY_SIZE(phy_basic_t1_features_array),
			       phy_basic_t1_features);

	 
	linkmode_set_bit_array(phy_basic_t1s_p2mp_features_array,
			       ARRAY_SIZE(phy_basic_t1s_p2mp_features_array),
			       phy_basic_t1s_p2mp_features);

	 
	linkmode_set_bit_array(phy_basic_ports_array,
			       ARRAY_SIZE(phy_basic_ports_array),
			       phy_gbit_features);
	linkmode_set_bit_array(phy_10_100_features_array,
			       ARRAY_SIZE(phy_10_100_features_array),
			       phy_gbit_features);
	linkmode_set_bit_array(phy_gbit_features_array,
			       ARRAY_SIZE(phy_gbit_features_array),
			       phy_gbit_features);

	 
	linkmode_set_bit_array(phy_basic_ports_array,
			       ARRAY_SIZE(phy_basic_ports_array),
			       phy_gbit_fibre_features);
	linkmode_set_bit_array(phy_10_100_features_array,
			       ARRAY_SIZE(phy_10_100_features_array),
			       phy_gbit_fibre_features);
	linkmode_set_bit_array(phy_gbit_features_array,
			       ARRAY_SIZE(phy_gbit_features_array),
			       phy_gbit_fibre_features);
	linkmode_set_bit_array(phy_fibre_port_array,
			       ARRAY_SIZE(phy_fibre_port_array),
			       phy_gbit_fibre_features);

	 
	linkmode_set_bit_array(phy_all_ports_features_array,
			       ARRAY_SIZE(phy_all_ports_features_array),
			       phy_gbit_all_ports_features);
	linkmode_set_bit_array(phy_10_100_features_array,
			       ARRAY_SIZE(phy_10_100_features_array),
			       phy_gbit_all_ports_features);
	linkmode_set_bit_array(phy_gbit_features_array,
			       ARRAY_SIZE(phy_gbit_features_array),
			       phy_gbit_all_ports_features);

	 
	linkmode_set_bit_array(phy_all_ports_features_array,
			       ARRAY_SIZE(phy_all_ports_features_array),
			       phy_10gbit_features);
	linkmode_set_bit_array(phy_10_100_features_array,
			       ARRAY_SIZE(phy_10_100_features_array),
			       phy_10gbit_features);
	linkmode_set_bit_array(phy_gbit_features_array,
			       ARRAY_SIZE(phy_gbit_features_array),
			       phy_10gbit_features);
	linkmode_set_bit_array(phy_10gbit_features_array,
			       ARRAY_SIZE(phy_10gbit_features_array),
			       phy_10gbit_features);

	 
	linkmode_set_bit_array(phy_all_ports_features_array,
			       ARRAY_SIZE(phy_all_ports_features_array),
			       phy_10gbit_full_features);
	linkmode_set_bit_array(phy_10gbit_full_features_array,
			       ARRAY_SIZE(phy_10gbit_full_features_array),
			       phy_10gbit_full_features);
	 
	linkmode_set_bit_array(phy_10gbit_fec_features_array,
			       ARRAY_SIZE(phy_10gbit_fec_features_array),
			       phy_10gbit_fec_features);
	linkmode_set_bit_array(phy_eee_cap1_features_array,
			       ARRAY_SIZE(phy_eee_cap1_features_array),
			       phy_eee_cap1_features);

}

void phy_device_free(struct phy_device *phydev)
{
	put_device(&phydev->mdio.dev);
}
EXPORT_SYMBOL(phy_device_free);

static void phy_mdio_device_free(struct mdio_device *mdiodev)
{
	struct phy_device *phydev;

	phydev = container_of(mdiodev, struct phy_device, mdio);
	phy_device_free(phydev);
}

static void phy_device_release(struct device *dev)
{
	fwnode_handle_put(dev->fwnode);
	kfree(to_phy_device(dev));
}

static void phy_mdio_device_remove(struct mdio_device *mdiodev)
{
	struct phy_device *phydev;

	phydev = container_of(mdiodev, struct phy_device, mdio);
	phy_device_remove(phydev);
}

static struct phy_driver genphy_driver;

static LIST_HEAD(phy_fixup_list);
static DEFINE_MUTEX(phy_fixup_lock);

static bool mdio_bus_phy_may_suspend(struct phy_device *phydev)
{
	struct device_driver *drv = phydev->mdio.dev.driver;
	struct phy_driver *phydrv = to_phy_driver(drv);
	struct net_device *netdev = phydev->attached_dev;

	if (!drv || !phydrv->suspend)
		return false;

	 
	if (!netdev)
		goto out;

	if (netdev->wol_enabled)
		return false;

	 
	if (netdev->dev.parent && device_may_wakeup(netdev->dev.parent))
		return false;

	 
	if (device_may_wakeup(&netdev->dev))
		return false;

out:
	return !phydev->suspended;
}

static __maybe_unused int mdio_bus_phy_suspend(struct device *dev)
{
	struct phy_device *phydev = to_phy_device(dev);

	if (phydev->mac_managed_pm)
		return 0;

	 
	if (phy_interrupt_is_valid(phydev)) {
		phydev->irq_suspended = 1;
		synchronize_irq(phydev->irq);
	}

	 
	if (phydev->attached_dev && phydev->adjust_link)
		phy_stop_machine(phydev);

	if (!mdio_bus_phy_may_suspend(phydev))
		return 0;

	phydev->suspended_by_mdio_bus = 1;

	return phy_suspend(phydev);
}

static __maybe_unused int mdio_bus_phy_resume(struct device *dev)
{
	struct phy_device *phydev = to_phy_device(dev);
	int ret;

	if (phydev->mac_managed_pm)
		return 0;

	if (!phydev->suspended_by_mdio_bus)
		goto no_resume;

	phydev->suspended_by_mdio_bus = 0;

	 
	WARN_ON(phydev->state != PHY_HALTED && phydev->state != PHY_READY &&
		phydev->state != PHY_UP);

	ret = phy_init_hw(phydev);
	if (ret < 0)
		return ret;

	ret = phy_resume(phydev);
	if (ret < 0)
		return ret;
no_resume:
	if (phy_interrupt_is_valid(phydev)) {
		phydev->irq_suspended = 0;
		synchronize_irq(phydev->irq);

		 
		if (phydev->irq_rerun) {
			phydev->irq_rerun = 0;
			enable_irq(phydev->irq);
			irq_wake_thread(phydev->irq, phydev);
		}
	}

	if (phydev->attached_dev && phydev->adjust_link)
		phy_start_machine(phydev);

	return 0;
}

static SIMPLE_DEV_PM_OPS(mdio_bus_phy_pm_ops, mdio_bus_phy_suspend,
			 mdio_bus_phy_resume);

 
int phy_register_fixup(const char *bus_id, u32 phy_uid, u32 phy_uid_mask,
		       int (*run)(struct phy_device *))
{
	struct phy_fixup *fixup = kzalloc(sizeof(*fixup), GFP_KERNEL);

	if (!fixup)
		return -ENOMEM;

	strscpy(fixup->bus_id, bus_id, sizeof(fixup->bus_id));
	fixup->phy_uid = phy_uid;
	fixup->phy_uid_mask = phy_uid_mask;
	fixup->run = run;

	mutex_lock(&phy_fixup_lock);
	list_add_tail(&fixup->list, &phy_fixup_list);
	mutex_unlock(&phy_fixup_lock);

	return 0;
}
EXPORT_SYMBOL(phy_register_fixup);

 
int phy_register_fixup_for_uid(u32 phy_uid, u32 phy_uid_mask,
			       int (*run)(struct phy_device *))
{
	return phy_register_fixup(PHY_ANY_ID, phy_uid, phy_uid_mask, run);
}
EXPORT_SYMBOL(phy_register_fixup_for_uid);

 
int phy_register_fixup_for_id(const char *bus_id,
			      int (*run)(struct phy_device *))
{
	return phy_register_fixup(bus_id, PHY_ANY_UID, 0xffffffff, run);
}
EXPORT_SYMBOL(phy_register_fixup_for_id);

 
int phy_unregister_fixup(const char *bus_id, u32 phy_uid, u32 phy_uid_mask)
{
	struct list_head *pos, *n;
	struct phy_fixup *fixup;
	int ret;

	ret = -ENODEV;

	mutex_lock(&phy_fixup_lock);
	list_for_each_safe(pos, n, &phy_fixup_list) {
		fixup = list_entry(pos, struct phy_fixup, list);

		if ((!strcmp(fixup->bus_id, bus_id)) &&
		    phy_id_compare(fixup->phy_uid, phy_uid, phy_uid_mask)) {
			list_del(&fixup->list);
			kfree(fixup);
			ret = 0;
			break;
		}
	}
	mutex_unlock(&phy_fixup_lock);

	return ret;
}
EXPORT_SYMBOL(phy_unregister_fixup);

 
int phy_unregister_fixup_for_uid(u32 phy_uid, u32 phy_uid_mask)
{
	return phy_unregister_fixup(PHY_ANY_ID, phy_uid, phy_uid_mask);
}
EXPORT_SYMBOL(phy_unregister_fixup_for_uid);

 
int phy_unregister_fixup_for_id(const char *bus_id)
{
	return phy_unregister_fixup(bus_id, PHY_ANY_UID, 0xffffffff);
}
EXPORT_SYMBOL(phy_unregister_fixup_for_id);

 
static int phy_needs_fixup(struct phy_device *phydev, struct phy_fixup *fixup)
{
	if (strcmp(fixup->bus_id, phydev_name(phydev)) != 0)
		if (strcmp(fixup->bus_id, PHY_ANY_ID) != 0)
			return 0;

	if (!phy_id_compare(phydev->phy_id, fixup->phy_uid,
			    fixup->phy_uid_mask))
		if (fixup->phy_uid != PHY_ANY_UID)
			return 0;

	return 1;
}

 
static int phy_scan_fixups(struct phy_device *phydev)
{
	struct phy_fixup *fixup;

	mutex_lock(&phy_fixup_lock);
	list_for_each_entry(fixup, &phy_fixup_list, list) {
		if (phy_needs_fixup(phydev, fixup)) {
			int err = fixup->run(phydev);

			if (err < 0) {
				mutex_unlock(&phy_fixup_lock);
				return err;
			}
			phydev->has_fixups = true;
		}
	}
	mutex_unlock(&phy_fixup_lock);

	return 0;
}

static int phy_bus_match(struct device *dev, struct device_driver *drv)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct phy_driver *phydrv = to_phy_driver(drv);
	const int num_ids = ARRAY_SIZE(phydev->c45_ids.device_ids);
	int i;

	if (!(phydrv->mdiodrv.flags & MDIO_DEVICE_IS_PHY))
		return 0;

	if (phydrv->match_phy_device)
		return phydrv->match_phy_device(phydev);

	if (phydev->is_c45) {
		for (i = 1; i < num_ids; i++) {
			if (phydev->c45_ids.device_ids[i] == 0xffffffff)
				continue;

			if (phy_id_compare(phydev->c45_ids.device_ids[i],
					   phydrv->phy_id, phydrv->phy_id_mask))
				return 1;
		}
		return 0;
	} else {
		return phy_id_compare(phydev->phy_id, phydrv->phy_id,
				      phydrv->phy_id_mask);
	}
}

static ssize_t
phy_id_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);

	return sysfs_emit(buf, "0x%.8lx\n", (unsigned long)phydev->phy_id);
}
static DEVICE_ATTR_RO(phy_id);

static ssize_t
phy_interface_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);
	const char *mode = NULL;

	if (phy_is_internal(phydev))
		mode = "internal";
	else
		mode = phy_modes(phydev->interface);

	return sysfs_emit(buf, "%s\n", mode);
}
static DEVICE_ATTR_RO(phy_interface);

static ssize_t
phy_has_fixups_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);

	return sysfs_emit(buf, "%d\n", phydev->has_fixups);
}
static DEVICE_ATTR_RO(phy_has_fixups);

static ssize_t phy_dev_flags_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);

	return sysfs_emit(buf, "0x%08x\n", phydev->dev_flags);
}
static DEVICE_ATTR_RO(phy_dev_flags);

static struct attribute *phy_dev_attrs[] = {
	&dev_attr_phy_id.attr,
	&dev_attr_phy_interface.attr,
	&dev_attr_phy_has_fixups.attr,
	&dev_attr_phy_dev_flags.attr,
	NULL,
};
ATTRIBUTE_GROUPS(phy_dev);

static const struct device_type mdio_bus_phy_type = {
	.name = "PHY",
	.groups = phy_dev_groups,
	.release = phy_device_release,
	.pm = pm_ptr(&mdio_bus_phy_pm_ops),
};

static int phy_request_driver_module(struct phy_device *dev, u32 phy_id)
{
	int ret;

	ret = request_module(MDIO_MODULE_PREFIX MDIO_ID_FMT,
			     MDIO_ID_ARGS(phy_id));
	 
	if (IS_ENABLED(CONFIG_MODULES) && ret < 0 && ret != -ENOENT) {
		phydev_err(dev, "error %d loading PHY driver module for ID 0x%08lx\n",
			   ret, (unsigned long)phy_id);
		return ret;
	}

	return 0;
}

struct phy_device *phy_device_create(struct mii_bus *bus, int addr, u32 phy_id,
				     bool is_c45,
				     struct phy_c45_device_ids *c45_ids)
{
	struct phy_device *dev;
	struct mdio_device *mdiodev;
	int ret = 0;

	 
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return ERR_PTR(-ENOMEM);

	mdiodev = &dev->mdio;
	mdiodev->dev.parent = &bus->dev;
	mdiodev->dev.bus = &mdio_bus_type;
	mdiodev->dev.type = &mdio_bus_phy_type;
	mdiodev->bus = bus;
	mdiodev->bus_match = phy_bus_match;
	mdiodev->addr = addr;
	mdiodev->flags = MDIO_DEVICE_FLAG_PHY;
	mdiodev->device_free = phy_mdio_device_free;
	mdiodev->device_remove = phy_mdio_device_remove;

	dev->speed = SPEED_UNKNOWN;
	dev->duplex = DUPLEX_UNKNOWN;
	dev->pause = 0;
	dev->asym_pause = 0;
	dev->link = 0;
	dev->port = PORT_TP;
	dev->interface = PHY_INTERFACE_MODE_GMII;

	dev->autoneg = AUTONEG_ENABLE;

	dev->pma_extable = -ENODATA;
	dev->is_c45 = is_c45;
	dev->phy_id = phy_id;
	if (c45_ids)
		dev->c45_ids = *c45_ids;
	dev->irq = bus->irq[addr];

	dev_set_name(&mdiodev->dev, PHY_ID_FMT, bus->id, addr);
	device_initialize(&mdiodev->dev);

	dev->state = PHY_DOWN;
	INIT_LIST_HEAD(&dev->leds);

	mutex_init(&dev->lock);
	INIT_DELAYED_WORK(&dev->state_queue, phy_state_machine);

	 
	if (is_c45 && c45_ids) {
		const int num_ids = ARRAY_SIZE(c45_ids->device_ids);
		int i;

		for (i = 1; i < num_ids; i++) {
			if (c45_ids->device_ids[i] == 0xffffffff)
				continue;

			ret = phy_request_driver_module(dev,
						c45_ids->device_ids[i]);
			if (ret)
				break;
		}
	} else {
		ret = phy_request_driver_module(dev, phy_id);
	}

	if (ret) {
		put_device(&mdiodev->dev);
		dev = ERR_PTR(ret);
	}

	return dev;
}
EXPORT_SYMBOL(phy_device_create);

 
static int phy_c45_probe_present(struct mii_bus *bus, int prtad, int devad)
{
	int stat2;

	stat2 = mdiobus_c45_read(bus, prtad, devad, MDIO_STAT2);
	if (stat2 < 0)
		return stat2;

	return (stat2 & MDIO_STAT2_DEVPRST) == MDIO_STAT2_DEVPRST_VAL;
}

 
static int get_phy_c45_devs_in_pkg(struct mii_bus *bus, int addr, int dev_addr,
				   u32 *devices_in_package)
{
	int phy_reg;

	phy_reg = mdiobus_c45_read(bus, addr, dev_addr, MDIO_DEVS2);
	if (phy_reg < 0)
		return -EIO;
	*devices_in_package = phy_reg << 16;

	phy_reg = mdiobus_c45_read(bus, addr, dev_addr, MDIO_DEVS1);
	if (phy_reg < 0)
		return -EIO;
	*devices_in_package |= phy_reg;

	return 0;
}

 
static int get_phy_c45_ids(struct mii_bus *bus, int addr,
			   struct phy_c45_device_ids *c45_ids)
{
	const int num_ids = ARRAY_SIZE(c45_ids->device_ids);
	u32 devs_in_pkg = 0;
	int i, ret, phy_reg;

	 
	for (i = 1; i < MDIO_MMD_NUM && (devs_in_pkg == 0 ||
	     (devs_in_pkg & 0x1fffffff) == 0x1fffffff); i++) {
		if (i == MDIO_MMD_VEND1 || i == MDIO_MMD_VEND2) {
			 
			ret = phy_c45_probe_present(bus, addr, i);
			if (ret < 0)
				return -EIO;

			if (!ret)
				continue;
		}
		phy_reg = get_phy_c45_devs_in_pkg(bus, addr, i, &devs_in_pkg);
		if (phy_reg < 0)
			return -EIO;
	}

	if ((devs_in_pkg & 0x1fffffff) == 0x1fffffff) {
		 
		phy_reg = get_phy_c45_devs_in_pkg(bus, addr, 0, &devs_in_pkg);
		if (phy_reg < 0)
			return -EIO;

		 
		if ((devs_in_pkg & 0x1fffffff) == 0x1fffffff)
			return -ENODEV;
	}

	 
	for (i = 1; i < num_ids; i++) {
		if (!(devs_in_pkg & (1 << i)))
			continue;

		if (i == MDIO_MMD_VEND1 || i == MDIO_MMD_VEND2) {
			 
			ret = phy_c45_probe_present(bus, addr, i);
			if (ret < 0)
				return ret;

			if (!ret)
				continue;
		}

		phy_reg = mdiobus_c45_read(bus, addr, i, MII_PHYSID1);
		if (phy_reg < 0)
			return -EIO;
		c45_ids->device_ids[i] = phy_reg << 16;

		phy_reg = mdiobus_c45_read(bus, addr, i, MII_PHYSID2);
		if (phy_reg < 0)
			return -EIO;
		c45_ids->device_ids[i] |= phy_reg;
	}

	c45_ids->devices_in_package = devs_in_pkg;
	 
	c45_ids->mmds_present = devs_in_pkg & ~BIT(0);

	return 0;
}

 
static int get_phy_c22_id(struct mii_bus *bus, int addr, u32 *phy_id)
{
	int phy_reg;

	 
	phy_reg = mdiobus_read(bus, addr, MII_PHYSID1);
	if (phy_reg < 0) {
		 
		return (phy_reg == -EIO || phy_reg == -ENODEV) ? -ENODEV : -EIO;
	}

	*phy_id = phy_reg << 16;

	 
	phy_reg = mdiobus_read(bus, addr, MII_PHYSID2);
	if (phy_reg < 0) {
		 
		return (phy_reg == -EIO || phy_reg == -ENODEV) ? -ENODEV : -EIO;
	}

	*phy_id |= phy_reg;

	 
	if ((*phy_id & 0x1fffffff) == 0x1fffffff)
		return -ENODEV;

	return 0;
}

 
int fwnode_get_phy_id(struct fwnode_handle *fwnode, u32 *phy_id)
{
	unsigned int upper, lower;
	const char *cp;
	int ret;

	ret = fwnode_property_read_string(fwnode, "compatible", &cp);
	if (ret)
		return ret;

	if (sscanf(cp, "ethernet-phy-id%4x.%4x", &upper, &lower) != 2)
		return -EINVAL;

	*phy_id = ((upper & GENMASK(15, 0)) << 16) | (lower & GENMASK(15, 0));
	return 0;
}
EXPORT_SYMBOL(fwnode_get_phy_id);

 
struct phy_device *get_phy_device(struct mii_bus *bus, int addr, bool is_c45)
{
	struct phy_c45_device_ids c45_ids;
	u32 phy_id = 0;
	int r;

	c45_ids.devices_in_package = 0;
	c45_ids.mmds_present = 0;
	memset(c45_ids.device_ids, 0xff, sizeof(c45_ids.device_ids));

	if (is_c45)
		r = get_phy_c45_ids(bus, addr, &c45_ids);
	else
		r = get_phy_c22_id(bus, addr, &phy_id);

	if (r)
		return ERR_PTR(r);

	 
	if (!is_c45 && phy_id == 0 && bus->read_c45) {
		r = get_phy_c45_ids(bus, addr, &c45_ids);
		if (!r)
			return phy_device_create(bus, addr, phy_id,
						 true, &c45_ids);
	}

	return phy_device_create(bus, addr, phy_id, is_c45, &c45_ids);
}
EXPORT_SYMBOL(get_phy_device);

 
int phy_device_register(struct phy_device *phydev)
{
	int err;

	err = mdiobus_register_device(&phydev->mdio);
	if (err)
		return err;

	 
	phy_device_reset(phydev, 0);

	 
	err = phy_scan_fixups(phydev);
	if (err) {
		phydev_err(phydev, "failed to initialize\n");
		goto out;
	}

	err = device_add(&phydev->mdio.dev);
	if (err) {
		phydev_err(phydev, "failed to add\n");
		goto out;
	}

	return 0;

 out:
	 
	phy_device_reset(phydev, 1);

	mdiobus_unregister_device(&phydev->mdio);
	return err;
}
EXPORT_SYMBOL(phy_device_register);

 
void phy_device_remove(struct phy_device *phydev)
{
	unregister_mii_timestamper(phydev->mii_ts);
	pse_control_put(phydev->psec);

	device_del(&phydev->mdio.dev);

	 
	phy_device_reset(phydev, 1);

	mdiobus_unregister_device(&phydev->mdio);
}
EXPORT_SYMBOL(phy_device_remove);

 
int phy_get_c45_ids(struct phy_device *phydev)
{
	return get_phy_c45_ids(phydev->mdio.bus, phydev->mdio.addr,
			       &phydev->c45_ids);
}
EXPORT_SYMBOL(phy_get_c45_ids);

 
struct phy_device *phy_find_first(struct mii_bus *bus)
{
	struct phy_device *phydev;
	int addr;

	for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
		phydev = mdiobus_get_phy(bus, addr);
		if (phydev)
			return phydev;
	}
	return NULL;
}
EXPORT_SYMBOL(phy_find_first);

static void phy_link_change(struct phy_device *phydev, bool up)
{
	struct net_device *netdev = phydev->attached_dev;

	if (up)
		netif_carrier_on(netdev);
	else
		netif_carrier_off(netdev);
	phydev->adjust_link(netdev);
	if (phydev->mii_ts && phydev->mii_ts->link_state)
		phydev->mii_ts->link_state(phydev->mii_ts, phydev);
}

 
static void phy_prepare_link(struct phy_device *phydev,
			     void (*handler)(struct net_device *))
{
	phydev->adjust_link = handler;
}

 
int phy_connect_direct(struct net_device *dev, struct phy_device *phydev,
		       void (*handler)(struct net_device *),
		       phy_interface_t interface)
{
	int rc;

	if (!dev)
		return -EINVAL;

	rc = phy_attach_direct(dev, phydev, phydev->dev_flags, interface);
	if (rc)
		return rc;

	phy_prepare_link(phydev, handler);
	if (phy_interrupt_is_valid(phydev))
		phy_request_interrupt(phydev);

	return 0;
}
EXPORT_SYMBOL(phy_connect_direct);

 
struct phy_device *phy_connect(struct net_device *dev, const char *bus_id,
			       void (*handler)(struct net_device *),
			       phy_interface_t interface)
{
	struct phy_device *phydev;
	struct device *d;
	int rc;

	 
	d = bus_find_device_by_name(&mdio_bus_type, NULL, bus_id);
	if (!d) {
		pr_err("PHY %s not found\n", bus_id);
		return ERR_PTR(-ENODEV);
	}
	phydev = to_phy_device(d);

	rc = phy_connect_direct(dev, phydev, handler, interface);
	put_device(d);
	if (rc)
		return ERR_PTR(rc);

	return phydev;
}
EXPORT_SYMBOL(phy_connect);

 
void phy_disconnect(struct phy_device *phydev)
{
	if (phy_is_started(phydev))
		phy_stop(phydev);

	if (phy_interrupt_is_valid(phydev))
		phy_free_interrupt(phydev);

	phydev->adjust_link = NULL;

	phy_detach(phydev);
}
EXPORT_SYMBOL(phy_disconnect);

 
static int phy_poll_reset(struct phy_device *phydev)
{
	 
	int ret, val;

	ret = phy_read_poll_timeout(phydev, MII_BMCR, val, !(val & BMCR_RESET),
				    50000, 600000, true);
	if (ret)
		return ret;
	 
	msleep(1);
	return 0;
}

int phy_init_hw(struct phy_device *phydev)
{
	int ret = 0;

	 
	phy_device_reset(phydev, 0);

	if (!phydev->drv)
		return 0;

	if (phydev->drv->soft_reset) {
		ret = phydev->drv->soft_reset(phydev);
		 
		if (!ret)
			phydev->suspended = 0;
	}

	if (ret < 0)
		return ret;

	ret = phy_scan_fixups(phydev);
	if (ret < 0)
		return ret;

	if (phydev->drv->config_init) {
		ret = phydev->drv->config_init(phydev);
		if (ret < 0)
			return ret;
	}

	if (phydev->drv->config_intr) {
		ret = phydev->drv->config_intr(phydev);
		if (ret < 0)
			return ret;
	}

	return 0;
}
EXPORT_SYMBOL(phy_init_hw);

void phy_attached_info(struct phy_device *phydev)
{
	phy_attached_print(phydev, NULL);
}
EXPORT_SYMBOL(phy_attached_info);

#define ATTACHED_FMT "attached PHY driver %s(mii_bus:phy_addr=%s, irq=%s)"
char *phy_attached_info_irq(struct phy_device *phydev)
{
	char *irq_str;
	char irq_num[8];

	switch(phydev->irq) {
	case PHY_POLL:
		irq_str = "POLL";
		break;
	case PHY_MAC_INTERRUPT:
		irq_str = "MAC";
		break;
	default:
		snprintf(irq_num, sizeof(irq_num), "%d", phydev->irq);
		irq_str = irq_num;
		break;
	}

	return kasprintf(GFP_KERNEL, "%s", irq_str);
}
EXPORT_SYMBOL(phy_attached_info_irq);

void phy_attached_print(struct phy_device *phydev, const char *fmt, ...)
{
	const char *unbound = phydev->drv ? "" : "[unbound] ";
	char *irq_str = phy_attached_info_irq(phydev);

	if (!fmt) {
		phydev_info(phydev, ATTACHED_FMT "\n", unbound,
			    phydev_name(phydev), irq_str);
	} else {
		va_list ap;

		phydev_info(phydev, ATTACHED_FMT, unbound,
			    phydev_name(phydev), irq_str);

		va_start(ap, fmt);
		vprintk(fmt, ap);
		va_end(ap);
	}
	kfree(irq_str);
}
EXPORT_SYMBOL(phy_attached_print);

static void phy_sysfs_create_links(struct phy_device *phydev)
{
	struct net_device *dev = phydev->attached_dev;
	int err;

	if (!dev)
		return;

	err = sysfs_create_link(&phydev->mdio.dev.kobj, &dev->dev.kobj,
				"attached_dev");
	if (err)
		return;

	err = sysfs_create_link_nowarn(&dev->dev.kobj,
				       &phydev->mdio.dev.kobj,
				       "phydev");
	if (err) {
		dev_err(&dev->dev, "could not add device link to %s err %d\n",
			kobject_name(&phydev->mdio.dev.kobj),
			err);
		 
	}

	phydev->sysfs_links = true;
}

static ssize_t
phy_standalone_show(struct device *dev, struct device_attribute *attr,
		    char *buf)
{
	struct phy_device *phydev = to_phy_device(dev);

	return sysfs_emit(buf, "%d\n", !phydev->attached_dev);
}
static DEVICE_ATTR_RO(phy_standalone);

 
void phy_sfp_attach(void *upstream, struct sfp_bus *bus)
{
	struct phy_device *phydev = upstream;

	if (phydev->attached_dev)
		phydev->attached_dev->sfp_bus = bus;
	phydev->sfp_bus_attached = true;
}
EXPORT_SYMBOL(phy_sfp_attach);

 
void phy_sfp_detach(void *upstream, struct sfp_bus *bus)
{
	struct phy_device *phydev = upstream;

	if (phydev->attached_dev)
		phydev->attached_dev->sfp_bus = NULL;
	phydev->sfp_bus_attached = false;
}
EXPORT_SYMBOL(phy_sfp_detach);

 
int phy_sfp_probe(struct phy_device *phydev,
		  const struct sfp_upstream_ops *ops)
{
	struct sfp_bus *bus;
	int ret = 0;

	if (phydev->mdio.dev.fwnode) {
		bus = sfp_bus_find_fwnode(phydev->mdio.dev.fwnode);
		if (IS_ERR(bus))
			return PTR_ERR(bus);

		phydev->sfp_bus = bus;

		ret = sfp_bus_add_upstream(bus, phydev, ops);
		sfp_bus_put(bus);
	}
	return ret;
}
EXPORT_SYMBOL(phy_sfp_probe);

 
int phy_attach_direct(struct net_device *dev, struct phy_device *phydev,
		      u32 flags, phy_interface_t interface)
{
	struct mii_bus *bus = phydev->mdio.bus;
	struct device *d = &phydev->mdio.dev;
	struct module *ndev_owner = NULL;
	bool using_genphy = false;
	int err;

	 
	if (dev)
		ndev_owner = dev->dev.parent->driver->owner;
	if (ndev_owner != bus->owner && !try_module_get(bus->owner)) {
		phydev_err(phydev, "failed to get the bus module\n");
		return -EIO;
	}

	get_device(d);

	 
	if (!d->driver) {
		if (phydev->is_c45)
			d->driver = &genphy_c45_driver.mdiodrv.driver;
		else
			d->driver = &genphy_driver.mdiodrv.driver;

		using_genphy = true;
	}

	if (!try_module_get(d->driver->owner)) {
		phydev_err(phydev, "failed to get the device driver module\n");
		err = -EIO;
		goto error_put_device;
	}

	if (using_genphy) {
		err = d->driver->probe(d);
		if (err >= 0)
			err = device_bind_driver(d);

		if (err)
			goto error_module_put;
	}

	if (phydev->attached_dev) {
		dev_err(&dev->dev, "PHY already attached\n");
		err = -EBUSY;
		goto error;
	}

	phydev->phy_link_change = phy_link_change;
	if (dev) {
		phydev->attached_dev = dev;
		dev->phydev = phydev;

		if (phydev->sfp_bus_attached)
			dev->sfp_bus = phydev->sfp_bus;
	}

	 
	phydev->sysfs_links = false;

	phy_sysfs_create_links(phydev);

	if (!phydev->attached_dev) {
		err = sysfs_create_file(&phydev->mdio.dev.kobj,
					&dev_attr_phy_standalone.attr);
		if (err)
			phydev_err(phydev, "error creating 'phy_standalone' sysfs entry\n");
	}

	phydev->dev_flags |= flags;

	phydev->interface = interface;

	phydev->state = PHY_READY;

	phydev->interrupts = PHY_INTERRUPT_DISABLED;

	 
	if (phydev->dev_flags & PHY_F_NO_IRQ)
		phydev->irq = PHY_POLL;

	 
	if (using_genphy)
		phydev->port = PORT_MII;

	 
	if (dev)
		netif_carrier_off(phydev->attached_dev);

	 
	err = phy_init_hw(phydev);
	if (err)
		goto error;

	phy_resume(phydev);
	if (!phydev->is_on_sfp_module)
		phy_led_triggers_register(phydev);

	 
	if (dev && phydev->mdio.bus->parent && dev->dev.parent != phydev->mdio.bus->parent)
		phydev->devlink = device_link_add(dev->dev.parent, &phydev->mdio.dev,
						  DL_FLAG_PM_RUNTIME | DL_FLAG_STATELESS);

	return err;

error:
	 
	phy_detach(phydev);
	return err;

error_module_put:
	module_put(d->driver->owner);
	d->driver = NULL;
error_put_device:
	put_device(d);
	if (ndev_owner != bus->owner)
		module_put(bus->owner);
	return err;
}
EXPORT_SYMBOL(phy_attach_direct);

 
struct phy_device *phy_attach(struct net_device *dev, const char *bus_id,
			      phy_interface_t interface)
{
	struct bus_type *bus = &mdio_bus_type;
	struct phy_device *phydev;
	struct device *d;
	int rc;

	if (!dev)
		return ERR_PTR(-EINVAL);

	 
	d = bus_find_device_by_name(bus, NULL, bus_id);
	if (!d) {
		pr_err("PHY %s not found\n", bus_id);
		return ERR_PTR(-ENODEV);
	}
	phydev = to_phy_device(d);

	rc = phy_attach_direct(dev, phydev, phydev->dev_flags, interface);
	put_device(d);
	if (rc)
		return ERR_PTR(rc);

	return phydev;
}
EXPORT_SYMBOL(phy_attach);

static bool phy_driver_is_genphy_kind(struct phy_device *phydev,
				      struct device_driver *driver)
{
	struct device *d = &phydev->mdio.dev;
	bool ret = false;

	if (!phydev->drv)
		return ret;

	get_device(d);
	ret = d->driver == driver;
	put_device(d);

	return ret;
}

bool phy_driver_is_genphy(struct phy_device *phydev)
{
	return phy_driver_is_genphy_kind(phydev,
					 &genphy_driver.mdiodrv.driver);
}
EXPORT_SYMBOL_GPL(phy_driver_is_genphy);

bool phy_driver_is_genphy_10g(struct phy_device *phydev)
{
	return phy_driver_is_genphy_kind(phydev,
					 &genphy_c45_driver.mdiodrv.driver);
}
EXPORT_SYMBOL_GPL(phy_driver_is_genphy_10g);

 
int phy_package_join(struct phy_device *phydev, int addr, size_t priv_size)
{
	struct mii_bus *bus = phydev->mdio.bus;
	struct phy_package_shared *shared;
	int ret;

	if (addr < 0 || addr >= PHY_MAX_ADDR)
		return -EINVAL;

	mutex_lock(&bus->shared_lock);
	shared = bus->shared[addr];
	if (!shared) {
		ret = -ENOMEM;
		shared = kzalloc(sizeof(*shared), GFP_KERNEL);
		if (!shared)
			goto err_unlock;
		if (priv_size) {
			shared->priv = kzalloc(priv_size, GFP_KERNEL);
			if (!shared->priv)
				goto err_free;
			shared->priv_size = priv_size;
		}
		shared->addr = addr;
		refcount_set(&shared->refcnt, 1);
		bus->shared[addr] = shared;
	} else {
		ret = -EINVAL;
		if (priv_size && priv_size != shared->priv_size)
			goto err_unlock;
		refcount_inc(&shared->refcnt);
	}
	mutex_unlock(&bus->shared_lock);

	phydev->shared = shared;

	return 0;

err_free:
	kfree(shared);
err_unlock:
	mutex_unlock(&bus->shared_lock);
	return ret;
}
EXPORT_SYMBOL_GPL(phy_package_join);

 
void phy_package_leave(struct phy_device *phydev)
{
	struct phy_package_shared *shared = phydev->shared;
	struct mii_bus *bus = phydev->mdio.bus;

	if (!shared)
		return;

	if (refcount_dec_and_mutex_lock(&shared->refcnt, &bus->shared_lock)) {
		bus->shared[shared->addr] = NULL;
		mutex_unlock(&bus->shared_lock);
		kfree(shared->priv);
		kfree(shared);
	}

	phydev->shared = NULL;
}
EXPORT_SYMBOL_GPL(phy_package_leave);

static void devm_phy_package_leave(struct device *dev, void *res)
{
	phy_package_leave(*(struct phy_device **)res);
}

 
int devm_phy_package_join(struct device *dev, struct phy_device *phydev,
			  int addr, size_t priv_size)
{
	struct phy_device **ptr;
	int ret;

	ptr = devres_alloc(devm_phy_package_leave, sizeof(*ptr),
			   GFP_KERNEL);
	if (!ptr)
		return -ENOMEM;

	ret = phy_package_join(phydev, addr, priv_size);

	if (!ret) {
		*ptr = phydev;
		devres_add(dev, ptr);
	} else {
		devres_free(ptr);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(devm_phy_package_join);

 
void phy_detach(struct phy_device *phydev)
{
	struct net_device *dev = phydev->attached_dev;
	struct module *ndev_owner = NULL;
	struct mii_bus *bus;

	if (phydev->devlink)
		device_link_del(phydev->devlink);

	if (phydev->sysfs_links) {
		if (dev)
			sysfs_remove_link(&dev->dev.kobj, "phydev");
		sysfs_remove_link(&phydev->mdio.dev.kobj, "attached_dev");
	}

	if (!phydev->attached_dev)
		sysfs_remove_file(&phydev->mdio.dev.kobj,
				  &dev_attr_phy_standalone.attr);

	phy_suspend(phydev);
	if (dev) {
		phydev->attached_dev->phydev = NULL;
		phydev->attached_dev = NULL;
	}
	phydev->phylink = NULL;

	if (!phydev->is_on_sfp_module)
		phy_led_triggers_unregister(phydev);

	if (phydev->mdio.dev.driver)
		module_put(phydev->mdio.dev.driver->owner);

	 
	if (phy_driver_is_genphy(phydev) ||
	    phy_driver_is_genphy_10g(phydev))
		device_release_driver(&phydev->mdio.dev);

	 
	phy_device_reset(phydev, 1);

	 
	bus = phydev->mdio.bus;

	put_device(&phydev->mdio.dev);
	if (dev)
		ndev_owner = dev->dev.parent->driver->owner;
	if (ndev_owner != bus->owner)
		module_put(bus->owner);
}
EXPORT_SYMBOL(phy_detach);

int phy_suspend(struct phy_device *phydev)
{
	struct ethtool_wolinfo wol = { .cmd = ETHTOOL_GWOL };
	struct net_device *netdev = phydev->attached_dev;
	struct phy_driver *phydrv = phydev->drv;
	int ret;

	if (phydev->suspended)
		return 0;

	phy_ethtool_get_wol(phydev, &wol);
	phydev->wol_enabled = wol.wolopts || (netdev && netdev->wol_enabled);
	 
	if (phydev->wol_enabled && !(phydrv->flags & PHY_ALWAYS_CALL_SUSPEND))
		return -EBUSY;

	if (!phydrv || !phydrv->suspend)
		return 0;

	ret = phydrv->suspend(phydev);
	if (!ret)
		phydev->suspended = true;

	return ret;
}
EXPORT_SYMBOL(phy_suspend);

int __phy_resume(struct phy_device *phydev)
{
	struct phy_driver *phydrv = phydev->drv;
	int ret;

	lockdep_assert_held(&phydev->lock);

	if (!phydrv || !phydrv->resume)
		return 0;

	ret = phydrv->resume(phydev);
	if (!ret)
		phydev->suspended = false;

	return ret;
}
EXPORT_SYMBOL(__phy_resume);

int phy_resume(struct phy_device *phydev)
{
	int ret;

	mutex_lock(&phydev->lock);
	ret = __phy_resume(phydev);
	mutex_unlock(&phydev->lock);

	return ret;
}
EXPORT_SYMBOL(phy_resume);

int phy_loopback(struct phy_device *phydev, bool enable)
{
	int ret = 0;

	if (!phydev->drv)
		return -EIO;

	mutex_lock(&phydev->lock);

	if (enable && phydev->loopback_enabled) {
		ret = -EBUSY;
		goto out;
	}

	if (!enable && !phydev->loopback_enabled) {
		ret = -EINVAL;
		goto out;
	}

	if (phydev->drv->set_loopback)
		ret = phydev->drv->set_loopback(phydev, enable);
	else
		ret = genphy_loopback(phydev, enable);

	if (ret)
		goto out;

	phydev->loopback_enabled = enable;

out:
	mutex_unlock(&phydev->lock);
	return ret;
}
EXPORT_SYMBOL(phy_loopback);

 
int phy_reset_after_clk_enable(struct phy_device *phydev)
{
	if (!phydev || !phydev->drv)
		return -ENODEV;

	if (phydev->drv->flags & PHY_RST_AFTER_CLK_EN) {
		phy_device_reset(phydev, 1);
		phy_device_reset(phydev, 0);
		return 1;
	}

	return 0;
}
EXPORT_SYMBOL(phy_reset_after_clk_enable);

 

 
static int genphy_config_advert(struct phy_device *phydev)
{
	int err, bmsr, changed = 0;
	u32 adv;

	 
	linkmode_and(phydev->advertising, phydev->advertising,
		     phydev->supported);

	adv = linkmode_adv_to_mii_adv_t(phydev->advertising);

	 
	err = phy_modify_changed(phydev, MII_ADVERTISE,
				 ADVERTISE_ALL | ADVERTISE_100BASE4 |
				 ADVERTISE_PAUSE_CAP | ADVERTISE_PAUSE_ASYM,
				 adv);
	if (err < 0)
		return err;
	if (err > 0)
		changed = 1;

	bmsr = phy_read(phydev, MII_BMSR);
	if (bmsr < 0)
		return bmsr;

	 
	if (!(bmsr & BMSR_ESTATEN))
		return changed;

	adv = linkmode_adv_to_mii_ctrl1000_t(phydev->advertising);

	err = phy_modify_changed(phydev, MII_CTRL1000,
				 ADVERTISE_1000FULL | ADVERTISE_1000HALF,
				 adv);
	if (err < 0)
		return err;
	if (err > 0)
		changed = 1;

	return changed;
}

 
static int genphy_c37_config_advert(struct phy_device *phydev)
{
	u16 adv = 0;

	 
	linkmode_and(phydev->advertising, phydev->advertising,
		     phydev->supported);

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseX_Full_BIT,
			      phydev->advertising))
		adv |= ADVERTISE_1000XFULL;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT,
			      phydev->advertising))
		adv |= ADVERTISE_1000XPAUSE;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT,
			      phydev->advertising))
		adv |= ADVERTISE_1000XPSE_ASYM;

	return phy_modify_changed(phydev, MII_ADVERTISE,
				  ADVERTISE_1000XFULL | ADVERTISE_1000XPAUSE |
				  ADVERTISE_1000XHALF | ADVERTISE_1000XPSE_ASYM,
				  adv);
}

 
int genphy_config_eee_advert(struct phy_device *phydev)
{
	int err;

	 
	if (!phydev->eee_broken_modes)
		return 0;

	err = phy_modify_mmd_changed(phydev, MDIO_MMD_AN, MDIO_AN_EEE_ADV,
				     phydev->eee_broken_modes, 0);
	 
	return err < 0 ? 0 : err;
}
EXPORT_SYMBOL(genphy_config_eee_advert);

 
int genphy_setup_forced(struct phy_device *phydev)
{
	u16 ctl;

	phydev->pause = 0;
	phydev->asym_pause = 0;

	ctl = mii_bmcr_encode_fixed(phydev->speed, phydev->duplex);

	return phy_modify(phydev, MII_BMCR,
			  ~(BMCR_LOOPBACK | BMCR_ISOLATE | BMCR_PDOWN), ctl);
}
EXPORT_SYMBOL(genphy_setup_forced);

static int genphy_setup_master_slave(struct phy_device *phydev)
{
	u16 ctl = 0;

	if (!phydev->is_gigabit_capable)
		return 0;

	switch (phydev->master_slave_set) {
	case MASTER_SLAVE_CFG_MASTER_PREFERRED:
		ctl |= CTL1000_PREFER_MASTER;
		break;
	case MASTER_SLAVE_CFG_SLAVE_PREFERRED:
		break;
	case MASTER_SLAVE_CFG_MASTER_FORCE:
		ctl |= CTL1000_AS_MASTER;
		fallthrough;
	case MASTER_SLAVE_CFG_SLAVE_FORCE:
		ctl |= CTL1000_ENABLE_MASTER;
		break;
	case MASTER_SLAVE_CFG_UNKNOWN:
	case MASTER_SLAVE_CFG_UNSUPPORTED:
		return 0;
	default:
		phydev_warn(phydev, "Unsupported Master/Slave mode\n");
		return -EOPNOTSUPP;
	}

	return phy_modify_changed(phydev, MII_CTRL1000,
				  (CTL1000_ENABLE_MASTER | CTL1000_AS_MASTER |
				   CTL1000_PREFER_MASTER), ctl);
}

int genphy_read_master_slave(struct phy_device *phydev)
{
	int cfg, state;
	int val;

	phydev->master_slave_get = MASTER_SLAVE_CFG_UNKNOWN;
	phydev->master_slave_state = MASTER_SLAVE_STATE_UNKNOWN;

	val = phy_read(phydev, MII_CTRL1000);
	if (val < 0)
		return val;

	if (val & CTL1000_ENABLE_MASTER) {
		if (val & CTL1000_AS_MASTER)
			cfg = MASTER_SLAVE_CFG_MASTER_FORCE;
		else
			cfg = MASTER_SLAVE_CFG_SLAVE_FORCE;
	} else {
		if (val & CTL1000_PREFER_MASTER)
			cfg = MASTER_SLAVE_CFG_MASTER_PREFERRED;
		else
			cfg = MASTER_SLAVE_CFG_SLAVE_PREFERRED;
	}

	val = phy_read(phydev, MII_STAT1000);
	if (val < 0)
		return val;

	if (val & LPA_1000MSFAIL) {
		state = MASTER_SLAVE_STATE_ERR;
	} else if (phydev->link) {
		 
		if (val & LPA_1000MSRES)
			state = MASTER_SLAVE_STATE_MASTER;
		else
			state = MASTER_SLAVE_STATE_SLAVE;
	} else {
		state = MASTER_SLAVE_STATE_UNKNOWN;
	}

	phydev->master_slave_get = cfg;
	phydev->master_slave_state = state;

	return 0;
}
EXPORT_SYMBOL(genphy_read_master_slave);

 
int genphy_restart_aneg(struct phy_device *phydev)
{
	 
	return phy_modify(phydev, MII_BMCR, BMCR_ISOLATE,
			  BMCR_ANENABLE | BMCR_ANRESTART);
}
EXPORT_SYMBOL(genphy_restart_aneg);

 
int genphy_check_and_restart_aneg(struct phy_device *phydev, bool restart)
{
	int ret;

	if (!restart) {
		 
		ret = phy_read(phydev, MII_BMCR);
		if (ret < 0)
			return ret;

		if (!(ret & BMCR_ANENABLE) || (ret & BMCR_ISOLATE))
			restart = true;
	}

	if (restart)
		return genphy_restart_aneg(phydev);

	return 0;
}
EXPORT_SYMBOL(genphy_check_and_restart_aneg);

 
int __genphy_config_aneg(struct phy_device *phydev, bool changed)
{
	int err;

	err = genphy_c45_an_config_eee_aneg(phydev);
	if (err < 0)
		return err;
	else if (err)
		changed = true;

	err = genphy_setup_master_slave(phydev);
	if (err < 0)
		return err;
	else if (err)
		changed = true;

	if (AUTONEG_ENABLE != phydev->autoneg)
		return genphy_setup_forced(phydev);

	err = genphy_config_advert(phydev);
	if (err < 0)  
		return err;
	else if (err)
		changed = true;

	return genphy_check_and_restart_aneg(phydev, changed);
}
EXPORT_SYMBOL(__genphy_config_aneg);

 
int genphy_c37_config_aneg(struct phy_device *phydev)
{
	int err, changed;

	if (phydev->autoneg != AUTONEG_ENABLE)
		return genphy_setup_forced(phydev);

	err = phy_modify(phydev, MII_BMCR, BMCR_SPEED1000 | BMCR_SPEED100,
			 BMCR_SPEED1000);
	if (err)
		return err;

	changed = genphy_c37_config_advert(phydev);
	if (changed < 0)  
		return changed;

	if (!changed) {
		 
		int ctl = phy_read(phydev, MII_BMCR);

		if (ctl < 0)
			return ctl;

		if (!(ctl & BMCR_ANENABLE) || (ctl & BMCR_ISOLATE))
			changed = 1;  
	}

	 
	if (changed > 0)
		return genphy_restart_aneg(phydev);

	return 0;
}
EXPORT_SYMBOL(genphy_c37_config_aneg);

 
int genphy_aneg_done(struct phy_device *phydev)
{
	int retval = phy_read(phydev, MII_BMSR);

	return (retval < 0) ? retval : (retval & BMSR_ANEGCOMPLETE);
}
EXPORT_SYMBOL(genphy_aneg_done);

 
int genphy_update_link(struct phy_device *phydev)
{
	int status = 0, bmcr;

	bmcr = phy_read(phydev, MII_BMCR);
	if (bmcr < 0)
		return bmcr;

	 
	if (bmcr & BMCR_ANRESTART)
		goto done;

	 
	if (!phy_polling_mode(phydev) || !phydev->link) {
		status = phy_read(phydev, MII_BMSR);
		if (status < 0)
			return status;
		else if (status & BMSR_LSTATUS)
			goto done;
	}

	 
	status = phy_read(phydev, MII_BMSR);
	if (status < 0)
		return status;
done:
	phydev->link = status & BMSR_LSTATUS ? 1 : 0;
	phydev->autoneg_complete = status & BMSR_ANEGCOMPLETE ? 1 : 0;

	 
	if (phydev->autoneg == AUTONEG_ENABLE && !phydev->autoneg_complete)
		phydev->link = 0;

	return 0;
}
EXPORT_SYMBOL(genphy_update_link);

int genphy_read_lpa(struct phy_device *phydev)
{
	int lpa, lpagb;

	if (phydev->autoneg == AUTONEG_ENABLE) {
		if (!phydev->autoneg_complete) {
			mii_stat1000_mod_linkmode_lpa_t(phydev->lp_advertising,
							0);
			mii_lpa_mod_linkmode_lpa_t(phydev->lp_advertising, 0);
			return 0;
		}

		if (phydev->is_gigabit_capable) {
			lpagb = phy_read(phydev, MII_STAT1000);
			if (lpagb < 0)
				return lpagb;

			if (lpagb & LPA_1000MSFAIL) {
				int adv = phy_read(phydev, MII_CTRL1000);

				if (adv < 0)
					return adv;

				if (adv & CTL1000_ENABLE_MASTER)
					phydev_err(phydev, "Master/Slave resolution failed, maybe conflicting manual settings?\n");
				else
					phydev_err(phydev, "Master/Slave resolution failed\n");
				return -ENOLINK;
			}

			mii_stat1000_mod_linkmode_lpa_t(phydev->lp_advertising,
							lpagb);
		}

		lpa = phy_read(phydev, MII_LPA);
		if (lpa < 0)
			return lpa;

		mii_lpa_mod_linkmode_lpa_t(phydev->lp_advertising, lpa);
	} else {
		linkmode_zero(phydev->lp_advertising);
	}

	return 0;
}
EXPORT_SYMBOL(genphy_read_lpa);

 
int genphy_read_status_fixed(struct phy_device *phydev)
{
	int bmcr = phy_read(phydev, MII_BMCR);

	if (bmcr < 0)
		return bmcr;

	if (bmcr & BMCR_FULLDPLX)
		phydev->duplex = DUPLEX_FULL;
	else
		phydev->duplex = DUPLEX_HALF;

	if (bmcr & BMCR_SPEED1000)
		phydev->speed = SPEED_1000;
	else if (bmcr & BMCR_SPEED100)
		phydev->speed = SPEED_100;
	else
		phydev->speed = SPEED_10;

	return 0;
}
EXPORT_SYMBOL(genphy_read_status_fixed);

 
int genphy_read_status(struct phy_device *phydev)
{
	int err, old_link = phydev->link;

	 
	err = genphy_update_link(phydev);
	if (err)
		return err;

	 
	if (phydev->autoneg == AUTONEG_ENABLE && old_link && phydev->link)
		return 0;

	phydev->master_slave_get = MASTER_SLAVE_CFG_UNSUPPORTED;
	phydev->master_slave_state = MASTER_SLAVE_STATE_UNSUPPORTED;
	phydev->speed = SPEED_UNKNOWN;
	phydev->duplex = DUPLEX_UNKNOWN;
	phydev->pause = 0;
	phydev->asym_pause = 0;

	if (phydev->is_gigabit_capable) {
		err = genphy_read_master_slave(phydev);
		if (err < 0)
			return err;
	}

	err = genphy_read_lpa(phydev);
	if (err < 0)
		return err;

	if (phydev->autoneg == AUTONEG_ENABLE && phydev->autoneg_complete) {
		phy_resolve_aneg_linkmode(phydev);
	} else if (phydev->autoneg == AUTONEG_DISABLE) {
		err = genphy_read_status_fixed(phydev);
		if (err < 0)
			return err;
	}

	return 0;
}
EXPORT_SYMBOL(genphy_read_status);

 
int genphy_c37_read_status(struct phy_device *phydev)
{
	int lpa, err, old_link = phydev->link;

	 
	err = genphy_update_link(phydev);
	if (err)
		return err;

	 
	if (phydev->autoneg == AUTONEG_ENABLE && old_link && phydev->link)
		return 0;

	phydev->duplex = DUPLEX_UNKNOWN;
	phydev->pause = 0;
	phydev->asym_pause = 0;

	if (phydev->autoneg == AUTONEG_ENABLE && phydev->autoneg_complete) {
		lpa = phy_read(phydev, MII_LPA);
		if (lpa < 0)
			return lpa;

		linkmode_mod_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
				 phydev->lp_advertising, lpa & LPA_LPACK);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseX_Full_BIT,
				 phydev->lp_advertising, lpa & LPA_1000XFULL);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_Pause_BIT,
				 phydev->lp_advertising, lpa & LPA_1000XPAUSE);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT,
				 phydev->lp_advertising,
				 lpa & LPA_1000XPAUSE_ASYM);

		phy_resolve_aneg_linkmode(phydev);
	} else if (phydev->autoneg == AUTONEG_DISABLE) {
		int bmcr = phy_read(phydev, MII_BMCR);

		if (bmcr < 0)
			return bmcr;

		if (bmcr & BMCR_FULLDPLX)
			phydev->duplex = DUPLEX_FULL;
		else
			phydev->duplex = DUPLEX_HALF;
	}

	return 0;
}
EXPORT_SYMBOL(genphy_c37_read_status);

 
int genphy_soft_reset(struct phy_device *phydev)
{
	u16 res = BMCR_RESET;
	int ret;

	if (phydev->autoneg == AUTONEG_ENABLE)
		res |= BMCR_ANRESTART;

	ret = phy_modify(phydev, MII_BMCR, BMCR_ISOLATE, res);
	if (ret < 0)
		return ret;

	 
	phydev->suspended = 0;

	ret = phy_poll_reset(phydev);
	if (ret)
		return ret;

	 
	if (phydev->autoneg == AUTONEG_DISABLE)
		ret = genphy_setup_forced(phydev);

	return ret;
}
EXPORT_SYMBOL(genphy_soft_reset);

irqreturn_t genphy_handle_interrupt_no_ack(struct phy_device *phydev)
{
	 
	phy_trigger_machine(phydev);

	return 0;
}
EXPORT_SYMBOL(genphy_handle_interrupt_no_ack);

 
int genphy_read_abilities(struct phy_device *phydev)
{
	int val;

	linkmode_set_bit_array(phy_basic_ports_array,
			       ARRAY_SIZE(phy_basic_ports_array),
			       phydev->supported);

	val = phy_read(phydev, MII_BMSR);
	if (val < 0)
		return val;

	linkmode_mod_bit(ETHTOOL_LINK_MODE_Autoneg_BIT, phydev->supported,
			 val & BMSR_ANEGCAPABLE);

	linkmode_mod_bit(ETHTOOL_LINK_MODE_100baseT_Full_BIT, phydev->supported,
			 val & BMSR_100FULL);
	linkmode_mod_bit(ETHTOOL_LINK_MODE_100baseT_Half_BIT, phydev->supported,
			 val & BMSR_100HALF);
	linkmode_mod_bit(ETHTOOL_LINK_MODE_10baseT_Full_BIT, phydev->supported,
			 val & BMSR_10FULL);
	linkmode_mod_bit(ETHTOOL_LINK_MODE_10baseT_Half_BIT, phydev->supported,
			 val & BMSR_10HALF);

	if (val & BMSR_ESTATEN) {
		val = phy_read(phydev, MII_ESTATUS);
		if (val < 0)
			return val;

		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
				 phydev->supported, val & ESTATUS_1000_TFULL);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
				 phydev->supported, val & ESTATUS_1000_THALF);
		linkmode_mod_bit(ETHTOOL_LINK_MODE_1000baseX_Full_BIT,
				 phydev->supported, val & ESTATUS_1000_XFULL);
	}

	 
	genphy_c45_read_eee_abilities(phydev);

	return 0;
}
EXPORT_SYMBOL(genphy_read_abilities);

 
int genphy_read_mmd_unsupported(struct phy_device *phdev, int devad, u16 regnum)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(genphy_read_mmd_unsupported);

int genphy_write_mmd_unsupported(struct phy_device *phdev, int devnum,
				 u16 regnum, u16 val)
{
	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(genphy_write_mmd_unsupported);

int genphy_suspend(struct phy_device *phydev)
{
	return phy_set_bits(phydev, MII_BMCR, BMCR_PDOWN);
}
EXPORT_SYMBOL(genphy_suspend);

int genphy_resume(struct phy_device *phydev)
{
	return phy_clear_bits(phydev, MII_BMCR, BMCR_PDOWN);
}
EXPORT_SYMBOL(genphy_resume);

int genphy_loopback(struct phy_device *phydev, bool enable)
{
	if (enable) {
		u16 val, ctl = BMCR_LOOPBACK;
		int ret;

		ctl |= mii_bmcr_encode_fixed(phydev->speed, phydev->duplex);

		phy_modify(phydev, MII_BMCR, ~0, ctl);

		ret = phy_read_poll_timeout(phydev, MII_BMSR, val,
					    val & BMSR_LSTATUS,
				    5000, 500000, true);
		if (ret)
			return ret;
	} else {
		phy_modify(phydev, MII_BMCR, BMCR_LOOPBACK, 0);

		phy_config_aneg(phydev);
	}

	return 0;
}
EXPORT_SYMBOL(genphy_loopback);

 
void phy_remove_link_mode(struct phy_device *phydev, u32 link_mode)
{
	linkmode_clear_bit(link_mode, phydev->supported);
	phy_advertise_supported(phydev);
}
EXPORT_SYMBOL(phy_remove_link_mode);

static void phy_copy_pause_bits(unsigned long *dst, unsigned long *src)
{
	linkmode_mod_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, dst,
		linkmode_test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, src));
	linkmode_mod_bit(ETHTOOL_LINK_MODE_Pause_BIT, dst,
		linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT, src));
}

 
void phy_advertise_supported(struct phy_device *phydev)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(new);

	linkmode_copy(new, phydev->supported);
	phy_copy_pause_bits(new, phydev->advertising);
	linkmode_copy(phydev->advertising, new);
}
EXPORT_SYMBOL(phy_advertise_supported);

 
void phy_support_sym_pause(struct phy_device *phydev)
{
	linkmode_clear_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, phydev->supported);
	phy_copy_pause_bits(phydev->advertising, phydev->supported);
}
EXPORT_SYMBOL(phy_support_sym_pause);

 
void phy_support_asym_pause(struct phy_device *phydev)
{
	phy_copy_pause_bits(phydev->advertising, phydev->supported);
}
EXPORT_SYMBOL(phy_support_asym_pause);

 
void phy_set_sym_pause(struct phy_device *phydev, bool rx, bool tx,
		       bool autoneg)
{
	linkmode_clear_bit(ETHTOOL_LINK_MODE_Pause_BIT, phydev->supported);

	if (rx && tx && autoneg)
		linkmode_set_bit(ETHTOOL_LINK_MODE_Pause_BIT,
				 phydev->supported);

	linkmode_copy(phydev->advertising, phydev->supported);
}
EXPORT_SYMBOL(phy_set_sym_pause);

 
void phy_set_asym_pause(struct phy_device *phydev, bool rx, bool tx)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(oldadv);

	linkmode_copy(oldadv, phydev->advertising);
	linkmode_set_pause(phydev->advertising, tx, rx);

	if (!linkmode_equal(oldadv, phydev->advertising) &&
	    phydev->autoneg)
		phy_start_aneg(phydev);
}
EXPORT_SYMBOL(phy_set_asym_pause);

 
bool phy_validate_pause(struct phy_device *phydev,
			struct ethtool_pauseparam *pp)
{
	if (!linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT,
			       phydev->supported) && pp->rx_pause)
		return false;

	if (!linkmode_test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT,
			       phydev->supported) &&
	    pp->rx_pause != pp->tx_pause)
		return false;

	return true;
}
EXPORT_SYMBOL(phy_validate_pause);

 
void phy_get_pause(struct phy_device *phydev, bool *tx_pause, bool *rx_pause)
{
	if (phydev->duplex != DUPLEX_FULL) {
		*tx_pause = false;
		*rx_pause = false;
		return;
	}

	return linkmode_resolve_pause(phydev->advertising,
				      phydev->lp_advertising,
				      tx_pause, rx_pause);
}
EXPORT_SYMBOL(phy_get_pause);

#if IS_ENABLED(CONFIG_OF_MDIO)
static int phy_get_int_delay_property(struct device *dev, const char *name)
{
	s32 int_delay;
	int ret;

	ret = device_property_read_u32(dev, name, &int_delay);
	if (ret)
		return ret;

	return int_delay;
}
#else
static int phy_get_int_delay_property(struct device *dev, const char *name)
{
	return -EINVAL;
}
#endif

 
s32 phy_get_internal_delay(struct phy_device *phydev, struct device *dev,
			   const int *delay_values, int size, bool is_rx)
{
	s32 delay;
	int i;

	if (is_rx) {
		delay = phy_get_int_delay_property(dev, "rx-internal-delay-ps");
		if (delay < 0 && size == 0) {
			if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
			    phydev->interface == PHY_INTERFACE_MODE_RGMII_RXID)
				return 1;
			else
				return 0;
		}

	} else {
		delay = phy_get_int_delay_property(dev, "tx-internal-delay-ps");
		if (delay < 0 && size == 0) {
			if (phydev->interface == PHY_INTERFACE_MODE_RGMII_ID ||
			    phydev->interface == PHY_INTERFACE_MODE_RGMII_TXID)
				return 1;
			else
				return 0;
		}
	}

	if (delay < 0)
		return delay;

	if (delay && size == 0)
		return delay;

	if (delay < delay_values[0] || delay > delay_values[size - 1]) {
		phydev_err(phydev, "Delay %d is out of range\n", delay);
		return -EINVAL;
	}

	if (delay == delay_values[0])
		return 0;

	for (i = 1; i < size; i++) {
		if (delay == delay_values[i])
			return i;

		 
		if (delay > delay_values[i - 1] &&
		    delay < delay_values[i]) {
			if (delay - delay_values[i - 1] <
			    delay_values[i] - delay)
				return i - 1;
			else
				return i;
		}
	}

	phydev_err(phydev, "error finding internal delay index for %d\n",
		   delay);

	return -EINVAL;
}
EXPORT_SYMBOL(phy_get_internal_delay);

static bool phy_drv_supports_irq(struct phy_driver *phydrv)
{
	return phydrv->config_intr && phydrv->handle_interrupt;
}

static int phy_led_set_brightness(struct led_classdev *led_cdev,
				  enum led_brightness value)
{
	struct phy_led *phyled = to_phy_led(led_cdev);
	struct phy_device *phydev = phyled->phydev;
	int err;

	mutex_lock(&phydev->lock);
	err = phydev->drv->led_brightness_set(phydev, phyled->index, value);
	mutex_unlock(&phydev->lock);

	return err;
}

static int phy_led_blink_set(struct led_classdev *led_cdev,
			     unsigned long *delay_on,
			     unsigned long *delay_off)
{
	struct phy_led *phyled = to_phy_led(led_cdev);
	struct phy_device *phydev = phyled->phydev;
	int err;

	mutex_lock(&phydev->lock);
	err = phydev->drv->led_blink_set(phydev, phyled->index,
					 delay_on, delay_off);
	mutex_unlock(&phydev->lock);

	return err;
}

static __maybe_unused struct device *
phy_led_hw_control_get_device(struct led_classdev *led_cdev)
{
	struct phy_led *phyled = to_phy_led(led_cdev);
	struct phy_device *phydev = phyled->phydev;

	if (phydev->attached_dev)
		return &phydev->attached_dev->dev;
	return NULL;
}

static int __maybe_unused
phy_led_hw_control_get(struct led_classdev *led_cdev,
		       unsigned long *rules)
{
	struct phy_led *phyled = to_phy_led(led_cdev);
	struct phy_device *phydev = phyled->phydev;
	int err;

	mutex_lock(&phydev->lock);
	err = phydev->drv->led_hw_control_get(phydev, phyled->index, rules);
	mutex_unlock(&phydev->lock);

	return err;
}

static int __maybe_unused
phy_led_hw_control_set(struct led_classdev *led_cdev,
		       unsigned long rules)
{
	struct phy_led *phyled = to_phy_led(led_cdev);
	struct phy_device *phydev = phyled->phydev;
	int err;

	mutex_lock(&phydev->lock);
	err = phydev->drv->led_hw_control_set(phydev, phyled->index, rules);
	mutex_unlock(&phydev->lock);

	return err;
}

static __maybe_unused int phy_led_hw_is_supported(struct led_classdev *led_cdev,
						  unsigned long rules)
{
	struct phy_led *phyled = to_phy_led(led_cdev);
	struct phy_device *phydev = phyled->phydev;
	int err;

	mutex_lock(&phydev->lock);
	err = phydev->drv->led_hw_is_supported(phydev, phyled->index, rules);
	mutex_unlock(&phydev->lock);

	return err;
}

static void phy_leds_unregister(struct phy_device *phydev)
{
	struct phy_led *phyled;

	list_for_each_entry(phyled, &phydev->leds, list) {
		led_classdev_unregister(&phyled->led_cdev);
	}
}

static int of_phy_led(struct phy_device *phydev,
		      struct device_node *led)
{
	struct device *dev = &phydev->mdio.dev;
	struct led_init_data init_data = {};
	struct led_classdev *cdev;
	struct phy_led *phyled;
	u32 index;
	int err;

	phyled = devm_kzalloc(dev, sizeof(*phyled), GFP_KERNEL);
	if (!phyled)
		return -ENOMEM;

	cdev = &phyled->led_cdev;
	phyled->phydev = phydev;

	err = of_property_read_u32(led, "reg", &index);
	if (err)
		return err;
	if (index > U8_MAX)
		return -EINVAL;

	phyled->index = index;
	if (phydev->drv->led_brightness_set)
		cdev->brightness_set_blocking = phy_led_set_brightness;
	if (phydev->drv->led_blink_set)
		cdev->blink_set = phy_led_blink_set;

#ifdef CONFIG_LEDS_TRIGGERS
	if (phydev->drv->led_hw_is_supported &&
	    phydev->drv->led_hw_control_set &&
	    phydev->drv->led_hw_control_get) {
		cdev->hw_control_is_supported = phy_led_hw_is_supported;
		cdev->hw_control_set = phy_led_hw_control_set;
		cdev->hw_control_get = phy_led_hw_control_get;
		cdev->hw_control_trigger = "netdev";
	}

	cdev->hw_control_get_device = phy_led_hw_control_get_device;
#endif
	cdev->max_brightness = 1;
	init_data.devicename = dev_name(&phydev->mdio.dev);
	init_data.fwnode = of_fwnode_handle(led);
	init_data.devname_mandatory = true;

	err = led_classdev_register_ext(dev, cdev, &init_data);
	if (err)
		return err;

	list_add(&phyled->list, &phydev->leds);

	return 0;
}

static int of_phy_leds(struct phy_device *phydev)
{
	struct device_node *node = phydev->mdio.dev.of_node;
	struct device_node *leds, *led;
	int err;

	if (!IS_ENABLED(CONFIG_OF_MDIO))
		return 0;

	if (!node)
		return 0;

	leds = of_get_child_by_name(node, "leds");
	if (!leds)
		return 0;

	for_each_available_child_of_node(leds, led) {
		err = of_phy_led(phydev, led);
		if (err) {
			of_node_put(led);
			phy_leds_unregister(phydev);
			return err;
		}
	}

	return 0;
}

 
struct mdio_device *fwnode_mdio_find_device(struct fwnode_handle *fwnode)
{
	struct device *d;

	if (!fwnode)
		return NULL;

	d = bus_find_device_by_fwnode(&mdio_bus_type, fwnode);
	if (!d)
		return NULL;

	return to_mdio_device(d);
}
EXPORT_SYMBOL(fwnode_mdio_find_device);

 
struct phy_device *fwnode_phy_find_device(struct fwnode_handle *phy_fwnode)
{
	struct mdio_device *mdiodev;

	mdiodev = fwnode_mdio_find_device(phy_fwnode);
	if (!mdiodev)
		return NULL;

	if (mdiodev->flags & MDIO_DEVICE_FLAG_PHY)
		return to_phy_device(&mdiodev->dev);

	put_device(&mdiodev->dev);

	return NULL;
}
EXPORT_SYMBOL(fwnode_phy_find_device);

 
struct phy_device *device_phy_find_device(struct device *dev)
{
	return fwnode_phy_find_device(dev_fwnode(dev));
}
EXPORT_SYMBOL_GPL(device_phy_find_device);

 
struct fwnode_handle *fwnode_get_phy_node(const struct fwnode_handle *fwnode)
{
	struct fwnode_handle *phy_node;

	 
	phy_node = fwnode_find_reference(fwnode, "phy-handle", 0);
	if (is_acpi_node(fwnode) || !IS_ERR(phy_node))
		return phy_node;
	phy_node = fwnode_find_reference(fwnode, "phy", 0);
	if (IS_ERR(phy_node))
		phy_node = fwnode_find_reference(fwnode, "phy-device", 0);
	return phy_node;
}
EXPORT_SYMBOL_GPL(fwnode_get_phy_node);

 
static int phy_probe(struct device *dev)
{
	struct phy_device *phydev = to_phy_device(dev);
	struct device_driver *drv = phydev->mdio.dev.driver;
	struct phy_driver *phydrv = to_phy_driver(drv);
	int err = 0;

	phydev->drv = phydrv;

	 
	if (!phy_drv_supports_irq(phydrv) && phy_interrupt_is_valid(phydev))
		phydev->irq = PHY_POLL;

	if (phydrv->flags & PHY_IS_INTERNAL)
		phydev->is_internal = true;

	 
	phy_device_reset(phydev, 0);

	if (phydev->drv->probe) {
		err = phydev->drv->probe(phydev);
		if (err)
			goto out;
	}

	phy_disable_interrupts(phydev);

	 
	if (phydrv->features) {
		linkmode_copy(phydev->supported, phydrv->features);
		genphy_c45_read_eee_abilities(phydev);
	}
	else if (phydrv->get_features)
		err = phydrv->get_features(phydev);
	else if (phydev->is_c45)
		err = genphy_c45_pma_read_abilities(phydev);
	else
		err = genphy_read_abilities(phydev);

	if (err)
		goto out;

	if (!linkmode_test_bit(ETHTOOL_LINK_MODE_Autoneg_BIT,
			       phydev->supported))
		phydev->autoneg = 0;

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT_Half_BIT,
			      phydev->supported))
		phydev->is_gigabit_capable = 1;
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_1000baseT_Full_BIT,
			      phydev->supported))
		phydev->is_gigabit_capable = 1;

	of_set_phy_supported(phydev);
	phy_advertise_supported(phydev);

	 
	err = genphy_c45_read_eee_adv(phydev, phydev->advertising_eee);
	if (err)
		goto out;

	 
	phydev->eee_enabled = !linkmode_empty(phydev->advertising_eee);

	 
	if (phydev->eee_enabled)
		linkmode_and(phydev->advertising_eee, phydev->supported_eee,
			     phydev->advertising_eee);

	 
	of_set_phy_eee_broken(phydev);

	 
	if (!test_bit(ETHTOOL_LINK_MODE_Pause_BIT, phydev->supported) &&
	    !test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, phydev->supported)) {
		linkmode_set_bit(ETHTOOL_LINK_MODE_Pause_BIT,
				 phydev->supported);
		linkmode_set_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT,
				 phydev->supported);
	}

	 
	phydev->state = PHY_READY;

	 
	if (IS_ENABLED(CONFIG_PHYLIB_LEDS))
		err = of_phy_leds(phydev);

out:
	 
	if (err)
		phy_device_reset(phydev, 1);

	return err;
}

static int phy_remove(struct device *dev)
{
	struct phy_device *phydev = to_phy_device(dev);

	cancel_delayed_work_sync(&phydev->state_queue);

	if (IS_ENABLED(CONFIG_PHYLIB_LEDS))
		phy_leds_unregister(phydev);

	phydev->state = PHY_DOWN;

	sfp_bus_del_upstream(phydev->sfp_bus);
	phydev->sfp_bus = NULL;

	if (phydev->drv && phydev->drv->remove)
		phydev->drv->remove(phydev);

	 
	phy_device_reset(phydev, 1);

	phydev->drv = NULL;

	return 0;
}

 
int phy_driver_register(struct phy_driver *new_driver, struct module *owner)
{
	int retval;

	 
	if (WARN_ON(new_driver->features && new_driver->get_features)) {
		pr_err("%s: features and get_features must not both be set\n",
		       new_driver->name);
		return -EINVAL;
	}

	 
	if (WARN(new_driver->mdiodrv.driver.of_match_table,
		 "%s: driver must not provide a DT match table\n",
		 new_driver->name))
		return -EINVAL;

	new_driver->mdiodrv.flags |= MDIO_DEVICE_IS_PHY;
	new_driver->mdiodrv.driver.name = new_driver->name;
	new_driver->mdiodrv.driver.bus = &mdio_bus_type;
	new_driver->mdiodrv.driver.probe = phy_probe;
	new_driver->mdiodrv.driver.remove = phy_remove;
	new_driver->mdiodrv.driver.owner = owner;
	new_driver->mdiodrv.driver.probe_type = PROBE_FORCE_SYNCHRONOUS;

	retval = driver_register(&new_driver->mdiodrv.driver);
	if (retval) {
		pr_err("%s: Error %d in registering driver\n",
		       new_driver->name, retval);

		return retval;
	}

	pr_debug("%s: Registered new driver\n", new_driver->name);

	return 0;
}
EXPORT_SYMBOL(phy_driver_register);

int phy_drivers_register(struct phy_driver *new_driver, int n,
			 struct module *owner)
{
	int i, ret = 0;

	for (i = 0; i < n; i++) {
		ret = phy_driver_register(new_driver + i, owner);
		if (ret) {
			while (i-- > 0)
				phy_driver_unregister(new_driver + i);
			break;
		}
	}
	return ret;
}
EXPORT_SYMBOL(phy_drivers_register);

void phy_driver_unregister(struct phy_driver *drv)
{
	driver_unregister(&drv->mdiodrv.driver);
}
EXPORT_SYMBOL(phy_driver_unregister);

void phy_drivers_unregister(struct phy_driver *drv, int n)
{
	int i;

	for (i = 0; i < n; i++)
		phy_driver_unregister(drv + i);
}
EXPORT_SYMBOL(phy_drivers_unregister);

static struct phy_driver genphy_driver = {
	.phy_id		= 0xffffffff,
	.phy_id_mask	= 0xffffffff,
	.name		= "Generic PHY",
	.get_features	= genphy_read_abilities,
	.suspend	= genphy_suspend,
	.resume		= genphy_resume,
	.set_loopback   = genphy_loopback,
};

static const struct ethtool_phy_ops phy_ethtool_phy_ops = {
	.get_sset_count		= phy_ethtool_get_sset_count,
	.get_strings		= phy_ethtool_get_strings,
	.get_stats		= phy_ethtool_get_stats,
	.get_plca_cfg		= phy_ethtool_get_plca_cfg,
	.set_plca_cfg		= phy_ethtool_set_plca_cfg,
	.get_plca_status	= phy_ethtool_get_plca_status,
	.start_cable_test	= phy_start_cable_test,
	.start_cable_test_tdr	= phy_start_cable_test_tdr,
};

static const struct phylib_stubs __phylib_stubs = {
	.hwtstamp_get = __phy_hwtstamp_get,
	.hwtstamp_set = __phy_hwtstamp_set,
};

static void phylib_register_stubs(void)
{
	phylib_stubs = &__phylib_stubs;
}

static void phylib_unregister_stubs(void)
{
	phylib_stubs = NULL;
}

static int __init phy_init(void)
{
	int rc;

	rtnl_lock();
	ethtool_set_ethtool_phy_ops(&phy_ethtool_phy_ops);
	phylib_register_stubs();
	rtnl_unlock();

	rc = mdio_bus_init();
	if (rc)
		goto err_ethtool_phy_ops;

	features_init();

	rc = phy_driver_register(&genphy_c45_driver, THIS_MODULE);
	if (rc)
		goto err_mdio_bus;

	rc = phy_driver_register(&genphy_driver, THIS_MODULE);
	if (rc)
		goto err_c45;

	return 0;

err_c45:
	phy_driver_unregister(&genphy_c45_driver);
err_mdio_bus:
	mdio_bus_exit();
err_ethtool_phy_ops:
	rtnl_lock();
	phylib_unregister_stubs();
	ethtool_set_ethtool_phy_ops(NULL);
	rtnl_unlock();

	return rc;
}

static void __exit phy_exit(void)
{
	phy_driver_unregister(&genphy_c45_driver);
	phy_driver_unregister(&genphy_driver);
	mdio_bus_exit();
	rtnl_lock();
	phylib_unregister_stubs();
	ethtool_set_ethtool_phy_ops(NULL);
	rtnl_unlock();
}

subsys_initcall(phy_init);
module_exit(phy_exit);
