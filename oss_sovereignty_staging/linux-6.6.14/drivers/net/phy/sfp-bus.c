
#include <linux/export.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/phylink.h>
#include <linux/property.h>
#include <linux/rtnetlink.h>
#include <linux/slab.h>

#include "sfp.h"

 
struct sfp_bus {
	 
	struct kref kref;
	struct list_head node;
	const struct fwnode_handle *fwnode;

	const struct sfp_socket_ops *socket_ops;
	struct device *sfp_dev;
	struct sfp *sfp;
	const struct sfp_quirk *sfp_quirk;

	const struct sfp_upstream_ops *upstream_ops;
	void *upstream;
	struct phy_device *phydev;

	bool registered;
	bool started;
};

 
int sfp_parse_port(struct sfp_bus *bus, const struct sfp_eeprom_id *id,
		   unsigned long *support)
{
	int port;

	 
	switch (id->base.connector) {
	case SFF8024_CONNECTOR_SC:
	case SFF8024_CONNECTOR_FIBERJACK:
	case SFF8024_CONNECTOR_LC:
	case SFF8024_CONNECTOR_MT_RJ:
	case SFF8024_CONNECTOR_MU:
	case SFF8024_CONNECTOR_OPTICAL_PIGTAIL:
	case SFF8024_CONNECTOR_MPO_1X12:
	case SFF8024_CONNECTOR_MPO_2X16:
		port = PORT_FIBRE;
		break;

	case SFF8024_CONNECTOR_RJ45:
		port = PORT_TP;
		break;

	case SFF8024_CONNECTOR_COPPER_PIGTAIL:
		port = PORT_DA;
		break;

	case SFF8024_CONNECTOR_UNSPEC:
		if (id->base.e1000_base_t) {
			port = PORT_TP;
			break;
		}
		fallthrough;
	case SFF8024_CONNECTOR_SG:  
	case SFF8024_CONNECTOR_HSSDC_II:
	case SFF8024_CONNECTOR_NOSEPARATE:
	case SFF8024_CONNECTOR_MXC_2X16:
		port = PORT_OTHER;
		break;
	default:
		dev_warn(bus->sfp_dev, "SFP: unknown connector id 0x%02x\n",
			 id->base.connector);
		port = PORT_OTHER;
		break;
	}

	if (support) {
		switch (port) {
		case PORT_FIBRE:
			phylink_set(support, FIBRE);
			break;

		case PORT_TP:
			phylink_set(support, TP);
			break;
		}
	}

	return port;
}
EXPORT_SYMBOL_GPL(sfp_parse_port);

 
bool sfp_may_have_phy(struct sfp_bus *bus, const struct sfp_eeprom_id *id)
{
	if (id->base.e1000_base_t)
		return true;

	if (id->base.phys_id != SFF8024_ID_DWDM_SFP) {
		switch (id->base.extended_cc) {
		case SFF8024_ECC_10GBASE_T_SFI:
		case SFF8024_ECC_10GBASE_T_SR:
		case SFF8024_ECC_5GBASE_T:
		case SFF8024_ECC_2_5GBASE_T:
			return true;
		}
	}

	return false;
}
EXPORT_SYMBOL_GPL(sfp_may_have_phy);

 
void sfp_parse_support(struct sfp_bus *bus, const struct sfp_eeprom_id *id,
		       unsigned long *support, unsigned long *interfaces)
{
	unsigned int br_min, br_nom, br_max;
	__ETHTOOL_DECLARE_LINK_MODE_MASK(modes) = { 0, };

	phylink_set(modes, Autoneg);
	phylink_set(modes, Pause);
	phylink_set(modes, Asym_Pause);

	 
	br_min = br_nom = br_max = 0;
	if (id->base.br_nominal) {
		if (id->base.br_nominal != 255) {
			br_nom = id->base.br_nominal * 100;
			br_min = br_nom - id->base.br_nominal * id->ext.br_min;
			br_max = br_nom + id->base.br_nominal * id->ext.br_max;
		} else if (id->ext.br_max) {
			br_nom = 250 * id->ext.br_max;
			br_max = br_nom + br_nom * id->ext.br_min / 100;
			br_min = br_nom - br_nom * id->ext.br_min / 100;
		}

		 
		if (br_min == br_max && id->base.sfp_ct_passive)
			br_min = 0;
	}

	 
	if (id->base.e10g_base_sr) {
		phylink_set(modes, 10000baseSR_Full);
		__set_bit(PHY_INTERFACE_MODE_10GBASER, interfaces);
	}
	if (id->base.e10g_base_lr) {
		phylink_set(modes, 10000baseLR_Full);
		__set_bit(PHY_INTERFACE_MODE_10GBASER, interfaces);
	}
	if (id->base.e10g_base_lrm) {
		phylink_set(modes, 10000baseLRM_Full);
		__set_bit(PHY_INTERFACE_MODE_10GBASER, interfaces);
	}
	if (id->base.e10g_base_er) {
		phylink_set(modes, 10000baseER_Full);
		__set_bit(PHY_INTERFACE_MODE_10GBASER, interfaces);
	}
	if (id->base.e1000_base_sx ||
	    id->base.e1000_base_lx ||
	    id->base.e1000_base_cx) {
		phylink_set(modes, 1000baseX_Full);
		__set_bit(PHY_INTERFACE_MODE_1000BASEX, interfaces);
	}
	if (id->base.e1000_base_t) {
		phylink_set(modes, 1000baseT_Half);
		phylink_set(modes, 1000baseT_Full);
		__set_bit(PHY_INTERFACE_MODE_1000BASEX, interfaces);
		__set_bit(PHY_INTERFACE_MODE_SGMII, interfaces);
	}

	 
	if ((id->base.e_base_px || id->base.e_base_bx10) &&
	    br_min <= 1300 && br_max >= 1200) {
		phylink_set(modes, 1000baseX_Full);
		__set_bit(PHY_INTERFACE_MODE_1000BASEX, interfaces);
	}

	 
	if (id->base.e100_base_fx || id->base.e100_base_lx) {
		phylink_set(modes, 100baseFX_Full);
		__set_bit(PHY_INTERFACE_MODE_100BASEX, interfaces);
	}
	if ((id->base.e_base_px || id->base.e_base_bx10) && br_nom == 100) {
		phylink_set(modes, 100baseFX_Full);
		__set_bit(PHY_INTERFACE_MODE_100BASEX, interfaces);
	}

	 
	if ((id->base.sfp_ct_passive || id->base.sfp_ct_active) && br_nom) {
		 
		if (br_min <= 12000 && br_max >= 10300) {
			phylink_set(modes, 10000baseCR_Full);
			__set_bit(PHY_INTERFACE_MODE_10GBASER, interfaces);
		}
		if (br_min <= 3200 && br_max >= 3100) {
			phylink_set(modes, 2500baseX_Full);
			__set_bit(PHY_INTERFACE_MODE_2500BASEX, interfaces);
		}
		if (br_min <= 1300 && br_max >= 1200) {
			phylink_set(modes, 1000baseX_Full);
			__set_bit(PHY_INTERFACE_MODE_1000BASEX, interfaces);
		}
	}
	if (id->base.sfp_ct_passive) {
		if (id->base.passive.sff8431_app_e) {
			phylink_set(modes, 10000baseCR_Full);
			__set_bit(PHY_INTERFACE_MODE_10GBASER, interfaces);
		}
	}
	if (id->base.sfp_ct_active) {
		if (id->base.active.sff8431_app_e ||
		    id->base.active.sff8431_lim) {
			phylink_set(modes, 10000baseCR_Full);
			__set_bit(PHY_INTERFACE_MODE_10GBASER, interfaces);
		}
	}

	switch (id->base.extended_cc) {
	case SFF8024_ECC_UNSPEC:
		break;
	case SFF8024_ECC_100G_25GAUI_C2M_AOC:
		if (br_min <= 28000 && br_max >= 25000) {
			 
			__set_bit(PHY_INTERFACE_MODE_25GBASER, interfaces);
			 
			phylink_set(modes, 25000baseSR_Full);
		}
		break;
	case SFF8024_ECC_100GBASE_SR4_25GBASE_SR:
		phylink_set(modes, 100000baseSR4_Full);
		phylink_set(modes, 25000baseSR_Full);
		__set_bit(PHY_INTERFACE_MODE_25GBASER, interfaces);
		break;
	case SFF8024_ECC_100GBASE_LR4_25GBASE_LR:
	case SFF8024_ECC_100GBASE_ER4_25GBASE_ER:
		phylink_set(modes, 100000baseLR4_ER4_Full);
		break;
	case SFF8024_ECC_100GBASE_CR4:
		phylink_set(modes, 100000baseCR4_Full);
		fallthrough;
	case SFF8024_ECC_25GBASE_CR_S:
	case SFF8024_ECC_25GBASE_CR_N:
		phylink_set(modes, 25000baseCR_Full);
		__set_bit(PHY_INTERFACE_MODE_25GBASER, interfaces);
		break;
	case SFF8024_ECC_10GBASE_T_SFI:
	case SFF8024_ECC_10GBASE_T_SR:
		phylink_set(modes, 10000baseT_Full);
		__set_bit(PHY_INTERFACE_MODE_10GBASER, interfaces);
		break;
	case SFF8024_ECC_5GBASE_T:
		phylink_set(modes, 5000baseT_Full);
		__set_bit(PHY_INTERFACE_MODE_5GBASER, interfaces);
		break;
	case SFF8024_ECC_2_5GBASE_T:
		phylink_set(modes, 2500baseT_Full);
		__set_bit(PHY_INTERFACE_MODE_2500BASEX, interfaces);
		break;
	default:
		dev_warn(bus->sfp_dev,
			 "Unknown/unsupported extended compliance code: 0x%02x\n",
			 id->base.extended_cc);
		break;
	}

	 
	if (id->base.fc_speed_100 ||
	    id->base.fc_speed_200 ||
	    id->base.fc_speed_400) {
		if (id->base.br_nominal >= 31) {
			phylink_set(modes, 2500baseX_Full);
			__set_bit(PHY_INTERFACE_MODE_2500BASEX, interfaces);
		}
		if (id->base.br_nominal >= 12) {
			phylink_set(modes, 1000baseX_Full);
			__set_bit(PHY_INTERFACE_MODE_1000BASEX, interfaces);
		}
	}

	 
	if (bitmap_empty(modes, __ETHTOOL_LINK_MODE_MASK_NBITS) && br_nom) {
		if (br_min <= 1300 && br_max >= 1200) {
			phylink_set(modes, 1000baseX_Full);
			__set_bit(PHY_INTERFACE_MODE_1000BASEX, interfaces);
		}
		if (br_min <= 3200 && br_max >= 2500) {
			phylink_set(modes, 2500baseX_Full);
			__set_bit(PHY_INTERFACE_MODE_2500BASEX, interfaces);
		}
	}

	if (bus->sfp_quirk && bus->sfp_quirk->modes)
		bus->sfp_quirk->modes(id, modes, interfaces);

	linkmode_or(support, support, modes);
}
EXPORT_SYMBOL_GPL(sfp_parse_support);

 
phy_interface_t sfp_select_interface(struct sfp_bus *bus,
				     unsigned long *link_modes)
{
	if (phylink_test(link_modes, 25000baseCR_Full) ||
	    phylink_test(link_modes, 25000baseKR_Full) ||
	    phylink_test(link_modes, 25000baseSR_Full))
		return PHY_INTERFACE_MODE_25GBASER;

	if (phylink_test(link_modes, 10000baseCR_Full) ||
	    phylink_test(link_modes, 10000baseSR_Full) ||
	    phylink_test(link_modes, 10000baseLR_Full) ||
	    phylink_test(link_modes, 10000baseLRM_Full) ||
	    phylink_test(link_modes, 10000baseER_Full) ||
	    phylink_test(link_modes, 10000baseT_Full))
		return PHY_INTERFACE_MODE_10GBASER;

	if (phylink_test(link_modes, 5000baseT_Full))
		return PHY_INTERFACE_MODE_5GBASER;

	if (phylink_test(link_modes, 2500baseX_Full))
		return PHY_INTERFACE_MODE_2500BASEX;

	if (phylink_test(link_modes, 1000baseT_Half) ||
	    phylink_test(link_modes, 1000baseT_Full))
		return PHY_INTERFACE_MODE_SGMII;

	if (phylink_test(link_modes, 1000baseX_Full))
		return PHY_INTERFACE_MODE_1000BASEX;

	if (phylink_test(link_modes, 100baseFX_Full))
		return PHY_INTERFACE_MODE_100BASEX;

	dev_warn(bus->sfp_dev, "Unable to ascertain link mode\n");

	return PHY_INTERFACE_MODE_NA;
}
EXPORT_SYMBOL_GPL(sfp_select_interface);

static LIST_HEAD(sfp_buses);
static DEFINE_MUTEX(sfp_mutex);

static const struct sfp_upstream_ops *sfp_get_upstream_ops(struct sfp_bus *bus)
{
	return bus->registered ? bus->upstream_ops : NULL;
}

static struct sfp_bus *sfp_bus_get(const struct fwnode_handle *fwnode)
{
	struct sfp_bus *sfp, *new, *found = NULL;

	new = kzalloc(sizeof(*new), GFP_KERNEL);

	mutex_lock(&sfp_mutex);

	list_for_each_entry(sfp, &sfp_buses, node) {
		if (sfp->fwnode == fwnode) {
			kref_get(&sfp->kref);
			found = sfp;
			break;
		}
	}

	if (!found && new) {
		kref_init(&new->kref);
		new->fwnode = fwnode;
		list_add(&new->node, &sfp_buses);
		found = new;
		new = NULL;
	}

	mutex_unlock(&sfp_mutex);

	kfree(new);

	return found;
}

static void sfp_bus_release(struct kref *kref)
{
	struct sfp_bus *bus = container_of(kref, struct sfp_bus, kref);

	list_del(&bus->node);
	mutex_unlock(&sfp_mutex);
	kfree(bus);
}

 
void sfp_bus_put(struct sfp_bus *bus)
{
	if (bus)
		kref_put_mutex(&bus->kref, sfp_bus_release, &sfp_mutex);
}
EXPORT_SYMBOL_GPL(sfp_bus_put);

static int sfp_register_bus(struct sfp_bus *bus)
{
	const struct sfp_upstream_ops *ops = bus->upstream_ops;
	int ret;

	if (ops) {
		if (ops->link_down)
			ops->link_down(bus->upstream);
		if (ops->connect_phy && bus->phydev) {
			ret = ops->connect_phy(bus->upstream, bus->phydev);
			if (ret)
				return ret;
		}
	}
	bus->registered = true;
	bus->socket_ops->attach(bus->sfp);
	if (bus->started)
		bus->socket_ops->start(bus->sfp);
	bus->upstream_ops->attach(bus->upstream, bus);
	return 0;
}

static void sfp_unregister_bus(struct sfp_bus *bus)
{
	const struct sfp_upstream_ops *ops = bus->upstream_ops;

	if (bus->registered) {
		bus->upstream_ops->detach(bus->upstream, bus);
		if (bus->started)
			bus->socket_ops->stop(bus->sfp);
		bus->socket_ops->detach(bus->sfp);
		if (bus->phydev && ops && ops->disconnect_phy)
			ops->disconnect_phy(bus->upstream);
	}
	bus->registered = false;
}

 
int sfp_get_module_info(struct sfp_bus *bus, struct ethtool_modinfo *modinfo)
{
	return bus->socket_ops->module_info(bus->sfp, modinfo);
}
EXPORT_SYMBOL_GPL(sfp_get_module_info);

 
int sfp_get_module_eeprom(struct sfp_bus *bus, struct ethtool_eeprom *ee,
			  u8 *data)
{
	return bus->socket_ops->module_eeprom(bus->sfp, ee, data);
}
EXPORT_SYMBOL_GPL(sfp_get_module_eeprom);

 
int sfp_get_module_eeprom_by_page(struct sfp_bus *bus,
				  const struct ethtool_module_eeprom *page,
				  struct netlink_ext_ack *extack)
{
	return bus->socket_ops->module_eeprom_by_page(bus->sfp, page, extack);
}
EXPORT_SYMBOL_GPL(sfp_get_module_eeprom_by_page);

 
void sfp_upstream_start(struct sfp_bus *bus)
{
	if (bus->registered)
		bus->socket_ops->start(bus->sfp);
	bus->started = true;
}
EXPORT_SYMBOL_GPL(sfp_upstream_start);

 
void sfp_upstream_stop(struct sfp_bus *bus)
{
	if (bus->registered)
		bus->socket_ops->stop(bus->sfp);
	bus->started = false;
}
EXPORT_SYMBOL_GPL(sfp_upstream_stop);

static void sfp_upstream_clear(struct sfp_bus *bus)
{
	bus->upstream_ops = NULL;
	bus->upstream = NULL;
}

 
void sfp_upstream_set_signal_rate(struct sfp_bus *bus, unsigned int rate_kbd)
{
	if (bus->registered)
		bus->socket_ops->set_signal_rate(bus->sfp, rate_kbd);
}
EXPORT_SYMBOL_GPL(sfp_upstream_set_signal_rate);

 
struct sfp_bus *sfp_bus_find_fwnode(const struct fwnode_handle *fwnode)
{
	struct fwnode_reference_args ref;
	struct sfp_bus *bus;
	int ret;

	ret = fwnode_property_get_reference_args(fwnode, "sfp", NULL,
						 0, 0, &ref);
	if (ret == -ENOENT)
		return NULL;
	else if (ret < 0)
		return ERR_PTR(ret);

	if (!fwnode_device_is_available(ref.fwnode)) {
		fwnode_handle_put(ref.fwnode);
		return NULL;
	}

	bus = sfp_bus_get(ref.fwnode);
	fwnode_handle_put(ref.fwnode);
	if (!bus)
		return ERR_PTR(-ENOMEM);

	return bus;
}
EXPORT_SYMBOL_GPL(sfp_bus_find_fwnode);

 
int sfp_bus_add_upstream(struct sfp_bus *bus, void *upstream,
			 const struct sfp_upstream_ops *ops)
{
	int ret;

	 
	if (!bus)
		return 0;

	rtnl_lock();
	kref_get(&bus->kref);
	bus->upstream_ops = ops;
	bus->upstream = upstream;

	if (bus->sfp) {
		ret = sfp_register_bus(bus);
		if (ret)
			sfp_upstream_clear(bus);
	} else {
		ret = 0;
	}
	rtnl_unlock();

	if (ret)
		sfp_bus_put(bus);

	return ret;
}
EXPORT_SYMBOL_GPL(sfp_bus_add_upstream);

 
void sfp_bus_del_upstream(struct sfp_bus *bus)
{
	if (bus) {
		rtnl_lock();
		if (bus->sfp)
			sfp_unregister_bus(bus);
		sfp_upstream_clear(bus);
		rtnl_unlock();

		sfp_bus_put(bus);
	}
}
EXPORT_SYMBOL_GPL(sfp_bus_del_upstream);

 
int sfp_add_phy(struct sfp_bus *bus, struct phy_device *phydev)
{
	const struct sfp_upstream_ops *ops = sfp_get_upstream_ops(bus);
	int ret = 0;

	if (ops && ops->connect_phy)
		ret = ops->connect_phy(bus->upstream, phydev);

	if (ret == 0)
		bus->phydev = phydev;

	return ret;
}
EXPORT_SYMBOL_GPL(sfp_add_phy);

void sfp_remove_phy(struct sfp_bus *bus)
{
	const struct sfp_upstream_ops *ops = sfp_get_upstream_ops(bus);

	if (ops && ops->disconnect_phy)
		ops->disconnect_phy(bus->upstream);
	bus->phydev = NULL;
}
EXPORT_SYMBOL_GPL(sfp_remove_phy);

void sfp_link_up(struct sfp_bus *bus)
{
	const struct sfp_upstream_ops *ops = sfp_get_upstream_ops(bus);

	if (ops && ops->link_up)
		ops->link_up(bus->upstream);
}
EXPORT_SYMBOL_GPL(sfp_link_up);

void sfp_link_down(struct sfp_bus *bus)
{
	const struct sfp_upstream_ops *ops = sfp_get_upstream_ops(bus);

	if (ops && ops->link_down)
		ops->link_down(bus->upstream);
}
EXPORT_SYMBOL_GPL(sfp_link_down);

int sfp_module_insert(struct sfp_bus *bus, const struct sfp_eeprom_id *id,
		      const struct sfp_quirk *quirk)
{
	const struct sfp_upstream_ops *ops = sfp_get_upstream_ops(bus);
	int ret = 0;

	bus->sfp_quirk = quirk;

	if (ops && ops->module_insert)
		ret = ops->module_insert(bus->upstream, id);

	return ret;
}
EXPORT_SYMBOL_GPL(sfp_module_insert);

void sfp_module_remove(struct sfp_bus *bus)
{
	const struct sfp_upstream_ops *ops = sfp_get_upstream_ops(bus);

	if (ops && ops->module_remove)
		ops->module_remove(bus->upstream);

	bus->sfp_quirk = NULL;
}
EXPORT_SYMBOL_GPL(sfp_module_remove);

int sfp_module_start(struct sfp_bus *bus)
{
	const struct sfp_upstream_ops *ops = sfp_get_upstream_ops(bus);
	int ret = 0;

	if (ops && ops->module_start)
		ret = ops->module_start(bus->upstream);

	return ret;
}
EXPORT_SYMBOL_GPL(sfp_module_start);

void sfp_module_stop(struct sfp_bus *bus)
{
	const struct sfp_upstream_ops *ops = sfp_get_upstream_ops(bus);

	if (ops && ops->module_stop)
		ops->module_stop(bus->upstream);
}
EXPORT_SYMBOL_GPL(sfp_module_stop);

static void sfp_socket_clear(struct sfp_bus *bus)
{
	bus->sfp_dev = NULL;
	bus->sfp = NULL;
	bus->socket_ops = NULL;
}

struct sfp_bus *sfp_register_socket(struct device *dev, struct sfp *sfp,
				    const struct sfp_socket_ops *ops)
{
	struct sfp_bus *bus = sfp_bus_get(dev->fwnode);
	int ret = 0;

	if (bus) {
		rtnl_lock();
		bus->sfp_dev = dev;
		bus->sfp = sfp;
		bus->socket_ops = ops;

		if (bus->upstream_ops) {
			ret = sfp_register_bus(bus);
			if (ret)
				sfp_socket_clear(bus);
		}
		rtnl_unlock();
	}

	if (ret) {
		sfp_bus_put(bus);
		bus = NULL;
	}

	return bus;
}
EXPORT_SYMBOL_GPL(sfp_register_socket);

void sfp_unregister_socket(struct sfp_bus *bus)
{
	rtnl_lock();
	if (bus->upstream_ops)
		sfp_unregister_bus(bus);
	sfp_socket_clear(bus);
	rtnl_unlock();

	sfp_bus_put(bus);
}
EXPORT_SYMBOL_GPL(sfp_unregister_socket);
