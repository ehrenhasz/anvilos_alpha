
 

#include <linux/clk.h>
#include <linux/of_address.h>
#include <linux/of_mdio.h>
#include <linux/jiffies.h>
#include <linux/iopoll.h>

#include "xilinx_axienet.h"

#define DEFAULT_MDIO_FREQ	2500000  
#define DEFAULT_HOST_CLOCK	150000000  

 
static int axienet_mdio_wait_until_ready(struct axienet_local *lp)
{
	u32 val;

	return readx_poll_timeout(axinet_ior_read_mcr, lp,
				  val, val & XAE_MDIO_MCR_READY_MASK,
				  1, 20000);
}

 
static void axienet_mdio_mdc_enable(struct axienet_local *lp)
{
	axienet_iow(lp, XAE_MDIO_MC_OFFSET,
		    ((u32)lp->mii_clk_div | XAE_MDIO_MC_MDIOEN_MASK));
}

 
static void axienet_mdio_mdc_disable(struct axienet_local *lp)
{
	u32 mc_reg;

	mc_reg = axienet_ior(lp, XAE_MDIO_MC_OFFSET);
	axienet_iow(lp, XAE_MDIO_MC_OFFSET,
		    (mc_reg & ~XAE_MDIO_MC_MDIOEN_MASK));
}

 
static int axienet_mdio_read(struct mii_bus *bus, int phy_id, int reg)
{
	u32 rc;
	int ret;
	struct axienet_local *lp = bus->priv;

	axienet_mdio_mdc_enable(lp);

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0) {
		axienet_mdio_mdc_disable(lp);
		return ret;
	}

	axienet_iow(lp, XAE_MDIO_MCR_OFFSET,
		    (((phy_id << XAE_MDIO_MCR_PHYAD_SHIFT) &
		      XAE_MDIO_MCR_PHYAD_MASK) |
		     ((reg << XAE_MDIO_MCR_REGAD_SHIFT) &
		      XAE_MDIO_MCR_REGAD_MASK) |
		     XAE_MDIO_MCR_INITIATE_MASK |
		     XAE_MDIO_MCR_OP_READ_MASK));

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0) {
		axienet_mdio_mdc_disable(lp);
		return ret;
	}

	rc = axienet_ior(lp, XAE_MDIO_MRD_OFFSET) & 0x0000FFFF;

	dev_dbg(lp->dev, "axienet_mdio_read(phy_id=%i, reg=%x) == %x\n",
		phy_id, reg, rc);

	axienet_mdio_mdc_disable(lp);
	return rc;
}

 
static int axienet_mdio_write(struct mii_bus *bus, int phy_id, int reg,
			      u16 val)
{
	int ret;
	struct axienet_local *lp = bus->priv;

	dev_dbg(lp->dev, "axienet_mdio_write(phy_id=%i, reg=%x, val=%x)\n",
		phy_id, reg, val);

	axienet_mdio_mdc_enable(lp);

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0) {
		axienet_mdio_mdc_disable(lp);
		return ret;
	}

	axienet_iow(lp, XAE_MDIO_MWD_OFFSET, (u32)val);
	axienet_iow(lp, XAE_MDIO_MCR_OFFSET,
		    (((phy_id << XAE_MDIO_MCR_PHYAD_SHIFT) &
		      XAE_MDIO_MCR_PHYAD_MASK) |
		     ((reg << XAE_MDIO_MCR_REGAD_SHIFT) &
		      XAE_MDIO_MCR_REGAD_MASK) |
		     XAE_MDIO_MCR_INITIATE_MASK |
		     XAE_MDIO_MCR_OP_WRITE_MASK));

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret < 0) {
		axienet_mdio_mdc_disable(lp);
		return ret;
	}
	axienet_mdio_mdc_disable(lp);
	return 0;
}

 
static int axienet_mdio_enable(struct axienet_local *lp, struct device_node *np)
{
	u32 mdio_freq = DEFAULT_MDIO_FREQ;
	u32 host_clock;
	u32 clk_div;
	int ret;

	lp->mii_clk_div = 0;

	if (lp->axi_clk) {
		host_clock = clk_get_rate(lp->axi_clk);
	} else {
		struct device_node *np1;

		 
		np1 = of_find_node_by_name(NULL, "cpu");
		if (!np1) {
			netdev_warn(lp->ndev, "Could not find CPU device node.\n");
			host_clock = DEFAULT_HOST_CLOCK;
		} else {
			int ret = of_property_read_u32(np1, "clock-frequency",
						       &host_clock);
			if (ret) {
				netdev_warn(lp->ndev, "CPU clock-frequency property not found.\n");
				host_clock = DEFAULT_HOST_CLOCK;
			}
			of_node_put(np1);
		}
		netdev_info(lp->ndev, "Setting assumed host clock to %u\n",
			    host_clock);
	}

	if (np)
		of_property_read_u32(np, "clock-frequency", &mdio_freq);
	if (mdio_freq != DEFAULT_MDIO_FREQ)
		netdev_info(lp->ndev, "Setting non-standard mdio bus frequency to %u Hz\n",
			    mdio_freq);

	 

	clk_div = (host_clock / (mdio_freq * 2)) - 1;
	 
	if (host_clock % (mdio_freq * 2))
		clk_div++;

	 
	if (clk_div & ~XAE_MDIO_MC_CLOCK_DIVIDE_MAX) {
		netdev_warn(lp->ndev, "MDIO clock divisor overflow\n");
		return -EOVERFLOW;
	}
	lp->mii_clk_div = (u8)clk_div;

	netdev_dbg(lp->ndev,
		   "Setting MDIO clock divisor to %u/%u Hz host clock.\n",
		   lp->mii_clk_div, host_clock);

	axienet_mdio_mdc_enable(lp);

	ret = axienet_mdio_wait_until_ready(lp);
	if (ret)
		axienet_mdio_mdc_disable(lp);

	return ret;
}

 
int axienet_mdio_setup(struct axienet_local *lp)
{
	struct device_node *mdio_node;
	struct mii_bus *bus;
	int ret;

	bus = mdiobus_alloc();
	if (!bus)
		return -ENOMEM;

	snprintf(bus->id, MII_BUS_ID_SIZE, "axienet-%.8llx",
		 (unsigned long long)lp->regs_start);

	bus->priv = lp;
	bus->name = "Xilinx Axi Ethernet MDIO";
	bus->read = axienet_mdio_read;
	bus->write = axienet_mdio_write;
	bus->parent = lp->dev;
	lp->mii_bus = bus;

	mdio_node = of_get_child_by_name(lp->dev->of_node, "mdio");
	ret = axienet_mdio_enable(lp, mdio_node);
	if (ret < 0)
		goto unregister;
	ret = of_mdiobus_register(bus, mdio_node);
	if (ret)
		goto unregister_mdio_enabled;
	of_node_put(mdio_node);
	axienet_mdio_mdc_disable(lp);
	return 0;

unregister_mdio_enabled:
	axienet_mdio_mdc_disable(lp);
unregister:
	of_node_put(mdio_node);
	mdiobus_free(bus);
	lp->mii_bus = NULL;
	return ret;
}

 
void axienet_mdio_teardown(struct axienet_local *lp)
{
	mdiobus_unregister(lp->mii_bus);
	mdiobus_free(lp->mii_bus);
	lp->mii_bus = NULL;
}
