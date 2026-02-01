
 

#include <linux/delay.h>
#include <linux/of_mdio.h>
#include <linux/platform_device.h>
#include <linux/gpio/consumer.h>

#include "emac.h"

 
#define ARC_MDIO_COMPLETE_POLL_COUNT	1

 
static int arc_mdio_complete_wait(struct arc_emac_priv *priv)
{
	unsigned int i;

	for (i = 0; i < ARC_MDIO_COMPLETE_POLL_COUNT * 40; i++) {
		unsigned int status = arc_reg_get(priv, R_STATUS);

		status &= MDIO_MASK;

		if (status) {
			 
			arc_reg_set(priv, R_STATUS, status);
			return 0;
		}

		msleep(25);
	}

	return -ETIMEDOUT;
}

 
static int arc_mdio_read(struct mii_bus *bus, int phy_addr, int reg_num)
{
	struct arc_emac_priv *priv = bus->priv;
	unsigned int value;
	int error;

	arc_reg_set(priv, R_MDIO,
		    0x60020000 | (phy_addr << 23) | (reg_num << 18));

	error = arc_mdio_complete_wait(priv);
	if (error < 0)
		return error;

	value = arc_reg_get(priv, R_MDIO) & 0xffff;

	dev_dbg(priv->dev, "arc_mdio_read(phy_addr=%i, reg_num=%x) = %x\n",
		phy_addr, reg_num, value);

	return value;
}

 
static int arc_mdio_write(struct mii_bus *bus, int phy_addr,
			  int reg_num, u16 value)
{
	struct arc_emac_priv *priv = bus->priv;

	dev_dbg(priv->dev,
		"arc_mdio_write(phy_addr=%i, reg_num=%x, value=%x)\n",
		phy_addr, reg_num, value);

	arc_reg_set(priv, R_MDIO,
		    0x50020000 | (phy_addr << 23) | (reg_num << 18) | value);

	return arc_mdio_complete_wait(priv);
}

 
static int arc_mdio_reset(struct mii_bus *bus)
{
	struct arc_emac_priv *priv = bus->priv;
	struct arc_emac_mdio_bus_data *data = &priv->bus_data;

	if (data->reset_gpio) {
		gpiod_set_value_cansleep(data->reset_gpio, 1);
		msleep(data->msec);
		gpiod_set_value_cansleep(data->reset_gpio, 0);
	}

	return 0;
}

 
int arc_mdio_probe(struct arc_emac_priv *priv)
{
	struct arc_emac_mdio_bus_data *data = &priv->bus_data;
	struct device_node *np = priv->dev->of_node;
	const char *name = "Synopsys MII Bus";
	struct mii_bus *bus;
	int error;

	bus = mdiobus_alloc();
	if (!bus)
		return -ENOMEM;

	priv->bus = bus;
	bus->priv = priv;
	bus->parent = priv->dev;
	bus->name = name;
	bus->read = &arc_mdio_read;
	bus->write = &arc_mdio_write;
	bus->reset = &arc_mdio_reset;

	 
	data->reset_gpio = devm_gpiod_get_optional(priv->dev, "phy-reset",
						   GPIOD_OUT_LOW);
	if (IS_ERR(data->reset_gpio)) {
		mdiobus_free(bus);
		return dev_err_probe(priv->dev, PTR_ERR(data->reset_gpio),
				     "Failed to request gpio\n");
	}

	of_property_read_u32(np, "phy-reset-duration", &data->msec);
	 
	if (data->msec > 1000)
		data->msec = 1;

	snprintf(bus->id, MII_BUS_ID_SIZE, "%s", bus->name);

	error = of_mdiobus_register(bus, priv->dev->of_node);
	if (error) {
		mdiobus_free(bus);
		return dev_err_probe(priv->dev, error,
				     "cannot register MDIO bus %s\n", name);
	}

	return 0;
}

 
int arc_mdio_remove(struct arc_emac_priv *priv)
{
	mdiobus_unregister(priv->bus);
	mdiobus_free(priv->bus);
	priv->bus = NULL;

	return 0;
}
