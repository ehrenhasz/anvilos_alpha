
 

#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/usb/of.h>
#include <linux/usb/otg.h>

#define UTMI_PHY_PORTS				2

 
#define SYSCON_USB_CFG_REG			0x420
#define   USB_CFG_DEVICE_EN_MASK		BIT(0)
#define   USB_CFG_DEVICE_MUX_OFFSET		1
#define   USB_CFG_DEVICE_MUX_MASK		BIT(1)
#define   USB_CFG_PLL_MASK			BIT(25)

#define SYSCON_UTMI_CFG_REG(id)			(0x440 + (id) * 4)
#define   UTMI_PHY_CFG_PU_MASK			BIT(5)

#define UTMI_PLL_CTRL_REG			0x0
#define   PLL_REFDIV_OFFSET			0
#define   PLL_REFDIV_MASK			GENMASK(6, 0)
#define   PLL_REFDIV_VAL			0x5
#define   PLL_FBDIV_OFFSET			16
#define   PLL_FBDIV_MASK			GENMASK(24, 16)
#define   PLL_FBDIV_VAL				0x60
#define   PLL_SEL_LPFR_MASK			GENMASK(29, 28)
#define   PLL_RDY				BIT(31)
#define UTMI_CAL_CTRL_REG			0x8
#define   IMPCAL_VTH_OFFSET			8
#define   IMPCAL_VTH_MASK			GENMASK(10, 8)
#define   IMPCAL_VTH_VAL			0x7
#define   IMPCAL_DONE				BIT(23)
#define   PLLCAL_DONE				BIT(31)
#define UTMI_TX_CH_CTRL_REG			0xC
#define   DRV_EN_LS_OFFSET			12
#define   DRV_EN_LS_MASK			GENMASK(15, 12)
#define   IMP_SEL_LS_OFFSET			16
#define   IMP_SEL_LS_MASK			GENMASK(19, 16)
#define   TX_AMP_OFFSET				20
#define   TX_AMP_MASK				GENMASK(22, 20)
#define   TX_AMP_VAL				0x4
#define UTMI_RX_CH_CTRL0_REG			0x14
#define   SQ_DET_EN				BIT(15)
#define   SQ_ANA_DTC_SEL			BIT(28)
#define UTMI_RX_CH_CTRL1_REG			0x18
#define   SQ_AMP_CAL_OFFSET			0
#define   SQ_AMP_CAL_MASK			GENMASK(2, 0)
#define   SQ_AMP_CAL_VAL			1
#define   SQ_AMP_CAL_EN				BIT(3)
#define UTMI_CTRL_STATUS0_REG			0x24
#define   SUSPENDM				BIT(22)
#define   TEST_SEL				BIT(25)
#define UTMI_CHGDTC_CTRL_REG			0x38
#define   VDAT_OFFSET				8
#define   VDAT_MASK				GENMASK(9, 8)
#define   VDAT_VAL				1
#define   VSRC_OFFSET				10
#define   VSRC_MASK				GENMASK(11, 10)
#define   VSRC_VAL				1

#define PLL_LOCK_DELAY_US			10000
#define PLL_LOCK_TIMEOUT_US			1000000

#define PORT_REGS(p)				((p)->priv->regs + (p)->id * 0x1000)

 
struct mvebu_cp110_utmi {
	void __iomem *regs;
	struct regmap *syscon;
	struct device *dev;
	const struct phy_ops *ops;
};

 
struct mvebu_cp110_utmi_port {
	struct mvebu_cp110_utmi *priv;
	u32 id;
	enum usb_dr_mode dr_mode;
};

static void mvebu_cp110_utmi_port_setup(struct mvebu_cp110_utmi_port *port)
{
	u32 reg;

	 
	reg = readl(PORT_REGS(port) + UTMI_PLL_CTRL_REG);
	reg &= ~(PLL_REFDIV_MASK | PLL_FBDIV_MASK | PLL_SEL_LPFR_MASK);
	reg |= (PLL_REFDIV_VAL << PLL_REFDIV_OFFSET) |
	       (PLL_FBDIV_VAL << PLL_FBDIV_OFFSET);
	writel(reg, PORT_REGS(port) + UTMI_PLL_CTRL_REG);

	 
	reg = readl(PORT_REGS(port) + UTMI_CAL_CTRL_REG);
	reg &= ~IMPCAL_VTH_MASK;
	reg |= IMPCAL_VTH_VAL << IMPCAL_VTH_OFFSET;
	writel(reg, PORT_REGS(port) + UTMI_CAL_CTRL_REG);

	 
	reg = readl(PORT_REGS(port) + UTMI_TX_CH_CTRL_REG);
	reg &= ~TX_AMP_MASK;
	reg |= TX_AMP_VAL << TX_AMP_OFFSET;
	writel(reg, PORT_REGS(port) + UTMI_TX_CH_CTRL_REG);

	 
	reg = readl(PORT_REGS(port) + UTMI_RX_CH_CTRL0_REG);
	reg &= ~SQ_DET_EN;
	reg |= SQ_ANA_DTC_SEL;
	writel(reg, PORT_REGS(port) + UTMI_RX_CH_CTRL0_REG);

	 
	reg = readl(PORT_REGS(port) + UTMI_RX_CH_CTRL1_REG);
	reg &= ~SQ_AMP_CAL_MASK;
	reg |= (SQ_AMP_CAL_VAL << SQ_AMP_CAL_OFFSET) | SQ_AMP_CAL_EN;
	writel(reg, PORT_REGS(port) + UTMI_RX_CH_CTRL1_REG);

	 
	reg = readl(PORT_REGS(port) + UTMI_CHGDTC_CTRL_REG);
	reg &= ~(VDAT_MASK | VSRC_MASK);
	reg |= (VDAT_VAL << VDAT_OFFSET) | (VSRC_VAL << VSRC_OFFSET);
	writel(reg, PORT_REGS(port) + UTMI_CHGDTC_CTRL_REG);
}

static int mvebu_cp110_utmi_phy_power_off(struct phy *phy)
{
	struct mvebu_cp110_utmi_port *port = phy_get_drvdata(phy);
	struct mvebu_cp110_utmi *utmi = port->priv;
	int i;

	 
	regmap_clear_bits(utmi->syscon, SYSCON_UTMI_CFG_REG(port->id),
			  UTMI_PHY_CFG_PU_MASK);

	for (i = 0; i < UTMI_PHY_PORTS; i++) {
		int test = regmap_test_bits(utmi->syscon,
					    SYSCON_UTMI_CFG_REG(i),
					    UTMI_PHY_CFG_PU_MASK);
		 
		if (test != 0)
			return 0;
	}

	 
	regmap_clear_bits(utmi->syscon, SYSCON_USB_CFG_REG, USB_CFG_PLL_MASK);

	return 0;
}

static int mvebu_cp110_utmi_phy_power_on(struct phy *phy)
{
	struct mvebu_cp110_utmi_port *port = phy_get_drvdata(phy);
	struct mvebu_cp110_utmi *utmi = port->priv;
	struct device *dev = &phy->dev;
	int ret;
	u32 reg;

	 
	ret = mvebu_cp110_utmi_phy_power_off(phy);
	if (ret) {
		dev_err(dev, "UTMI power OFF before power ON failed\n");
		return ret;
	}

	 
	if (port->dr_mode == USB_DR_MODE_PERIPHERAL) {
		regmap_update_bits(utmi->syscon, SYSCON_USB_CFG_REG,
				   USB_CFG_DEVICE_EN_MASK | USB_CFG_DEVICE_MUX_MASK,
				   USB_CFG_DEVICE_EN_MASK |
				   (port->id << USB_CFG_DEVICE_MUX_OFFSET));
	}

	 
	reg = readl(PORT_REGS(port) + UTMI_CTRL_STATUS0_REG);
	reg |= SUSPENDM | TEST_SEL;
	writel(reg, PORT_REGS(port) + UTMI_CTRL_STATUS0_REG);

	 
	mdelay(1);

	 
	mvebu_cp110_utmi_port_setup(port);

	 
	regmap_set_bits(utmi->syscon, SYSCON_UTMI_CFG_REG(port->id),
			UTMI_PHY_CFG_PU_MASK);

	 
	reg = readl(PORT_REGS(port) + UTMI_CTRL_STATUS0_REG);
	reg &= ~TEST_SEL;
	writel(reg, PORT_REGS(port) + UTMI_CTRL_STATUS0_REG);

	 
	ret = readl_poll_timeout(PORT_REGS(port) + UTMI_CAL_CTRL_REG, reg,
				 reg & IMPCAL_DONE,
				 PLL_LOCK_DELAY_US, PLL_LOCK_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "Failed to end UTMI impedance calibration\n");
		return ret;
	}

	 
	ret = readl_poll_timeout(PORT_REGS(port) + UTMI_CAL_CTRL_REG, reg,
				 reg & PLLCAL_DONE,
				 PLL_LOCK_DELAY_US, PLL_LOCK_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "Failed to end UTMI PLL calibration\n");
		return ret;
	}

	 
	ret = readl_poll_timeout(PORT_REGS(port) + UTMI_PLL_CTRL_REG, reg,
				 reg & PLL_RDY,
				 PLL_LOCK_DELAY_US, PLL_LOCK_TIMEOUT_US);
	if (ret) {
		dev_err(dev, "PLL is not ready\n");
		return ret;
	}

	 
	regmap_set_bits(utmi->syscon, SYSCON_USB_CFG_REG, USB_CFG_PLL_MASK);

	return 0;
}

static const struct phy_ops mvebu_cp110_utmi_phy_ops = {
	.power_on = mvebu_cp110_utmi_phy_power_on,
	.power_off = mvebu_cp110_utmi_phy_power_off,
	.owner = THIS_MODULE,
};

static const struct of_device_id mvebu_cp110_utmi_of_match[] = {
	{ .compatible = "marvell,cp110-utmi-phy" },
	{},
};
MODULE_DEVICE_TABLE(of, mvebu_cp110_utmi_of_match);

static int mvebu_cp110_utmi_phy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mvebu_cp110_utmi *utmi;
	struct phy_provider *provider;
	struct device_node *child;
	u32 usb_devices = 0;

	utmi = devm_kzalloc(dev, sizeof(*utmi), GFP_KERNEL);
	if (!utmi)
		return -ENOMEM;

	utmi->dev = dev;

	 
	utmi->syscon = syscon_regmap_lookup_by_phandle(dev->of_node,
						       "marvell,system-controller");
	if (IS_ERR(utmi->syscon)) {
		dev_err(dev, "Missing UTMI system controller\n");
		return PTR_ERR(utmi->syscon);
	}

	 
	utmi->regs = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(utmi->regs))
		return PTR_ERR(utmi->regs);

	for_each_available_child_of_node(dev->of_node, child) {
		struct mvebu_cp110_utmi_port *port;
		struct phy *phy;
		int ret;
		u32 port_id;

		ret = of_property_read_u32(child, "reg", &port_id);
		if ((ret < 0) || (port_id >= UTMI_PHY_PORTS)) {
			dev_err(dev,
				"invalid 'reg' property on child %pOF\n",
				child);
			continue;
		}

		port = devm_kzalloc(dev, sizeof(*port), GFP_KERNEL);
		if (!port) {
			of_node_put(child);
			return -ENOMEM;
		}

		port->dr_mode = of_usb_get_dr_mode_by_phy(child, -1);
		if ((port->dr_mode != USB_DR_MODE_HOST) &&
		    (port->dr_mode != USB_DR_MODE_PERIPHERAL)) {
			dev_err(&pdev->dev,
				"Missing dual role setting of the port%d, will use HOST mode\n",
				port_id);
			port->dr_mode = USB_DR_MODE_HOST;
		}

		if (port->dr_mode == USB_DR_MODE_PERIPHERAL) {
			usb_devices++;
			if (usb_devices > 1) {
				dev_err(dev,
					"Single USB device allowed! Port%d will use HOST mode\n",
					port_id);
				port->dr_mode = USB_DR_MODE_HOST;
			}
		}

		 
		utmi->ops = &mvebu_cp110_utmi_phy_ops;

		 
		phy = devm_phy_create(dev, child, utmi->ops);
		if (IS_ERR(phy)) {
			dev_err(dev, "Failed to create the UTMI PHY\n");
			of_node_put(child);
			return PTR_ERR(phy);
		}

		port->priv = utmi;
		port->id = port_id;
		phy_set_drvdata(phy, port);

		 
		mvebu_cp110_utmi_phy_power_off(phy);
	}

	dev_set_drvdata(dev, utmi);
	provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(provider);
}

static struct platform_driver mvebu_cp110_utmi_driver = {
	.probe	= mvebu_cp110_utmi_phy_probe,
	.driver	= {
		.name		= "mvebu-cp110-utmi-phy",
		.of_match_table	= mvebu_cp110_utmi_of_match,
	 },
};
module_platform_driver(mvebu_cp110_utmi_driver);

MODULE_AUTHOR("Konstatin Porotchkin <kostap@marvell.com>");
MODULE_DESCRIPTION("Marvell Armada CP110 UTMI PHY driver");
MODULE_LICENSE("GPL v2");
