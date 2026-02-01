
 

#include <linux/bitfield.h>
#include <linux/bitops.h>
#include <linux/bits.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/reset.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>

 
#define MIPI_DSI_PHY_CTRL				0x0

 
#define MIPI_DSI_CHAN_CTRL				0x4

 
#define MIPI_DSI_CHAN_STS				0x8

 
#define MIPI_DSI_CLK_TIM				0xc

 
#define MIPI_DSI_HS_TIM					0x10

 
#define MIPI_DSI_LP_TIM					0x14

 
#define MIPI_DSI_ANA_UP_TIM				0x18

 
#define MIPI_DSI_INIT_TIM				0x1c

 
#define MIPI_DSI_WAKEUP_TIM				0x20

 
#define MIPI_DSI_LPOK_TIM				0x24

 
#define MIPI_DSI_LP_WCHDOG				0x28

 
#define MIPI_DSI_ANA_CTRL				0x2c

 
#define MIPI_DSI_CLK_TIM1				0x30

 
#define MIPI_DSI_TURN_WCHDOG				0x34

 
#define MIPI_DSI_ULPS_CHECK				0x38
#define MIPI_DSI_TEST_CTRL0				0x3c
#define MIPI_DSI_TEST_CTRL1				0x40

struct phy_meson_axg_mipi_dphy_priv {
	struct device				*dev;
	struct regmap				*regmap;
	struct clk				*clk;
	struct reset_control			*reset;
	struct phy				*analog;
	struct phy_configure_opts_mipi_dphy	config;
};

static const struct regmap_config phy_meson_axg_mipi_dphy_regmap_conf = {
	.reg_bits = 8,
	.val_bits = 32,
	.reg_stride = 4,
	.max_register = MIPI_DSI_TEST_CTRL1,
};

static int phy_meson_axg_mipi_dphy_init(struct phy *phy)
{
	struct phy_meson_axg_mipi_dphy_priv *priv = phy_get_drvdata(phy);
	int ret;

	ret = phy_init(priv->analog);
	if (ret)
		return ret;

	ret = reset_control_reset(priv->reset);
	if (ret)
		return ret;

	return 0;
}

static int phy_meson_axg_mipi_dphy_configure(struct phy *phy,
					      union phy_configure_opts *opts)
{
	struct phy_meson_axg_mipi_dphy_priv *priv = phy_get_drvdata(phy);
	int ret;

	ret = phy_mipi_dphy_config_validate(&opts->mipi_dphy);
	if (ret)
		return ret;

	ret = phy_configure(priv->analog, opts);
	if (ret)
		return ret;

	memcpy(&priv->config, opts, sizeof(priv->config));

	return 0;
}

static int phy_meson_axg_mipi_dphy_power_on(struct phy *phy)
{
	struct phy_meson_axg_mipi_dphy_priv *priv = phy_get_drvdata(phy);
	int ret;
	unsigned long temp;

	ret = phy_power_on(priv->analog);
	if (ret)
		return ret;

	 
	regmap_write(priv->regmap, MIPI_DSI_PHY_CTRL,  0x1);
	regmap_write(priv->regmap, MIPI_DSI_PHY_CTRL,
		     BIT(0) |  
		     BIT(7) |  
		     BIT(8));  

	 
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(9), BIT(9));

	 
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(12), BIT(12));
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(31), BIT(31));
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(31), 0);

	 
	temp = (1000000 * 100) / (priv->config.hs_clk_rate / 1000);
	temp = temp * 8 * 10;

	regmap_write(priv->regmap, MIPI_DSI_CLK_TIM,
		     DIV_ROUND_UP(priv->config.clk_trail, temp) |
		     (DIV_ROUND_UP(priv->config.clk_post +
				   priv->config.hs_trail, temp) << 8) |
		     (DIV_ROUND_UP(priv->config.clk_zero, temp) << 16) |
		     (DIV_ROUND_UP(priv->config.clk_prepare, temp) << 24));
	regmap_write(priv->regmap, MIPI_DSI_CLK_TIM1,
		     DIV_ROUND_UP(priv->config.clk_pre, BITS_PER_BYTE));

	regmap_write(priv->regmap, MIPI_DSI_HS_TIM,
		     DIV_ROUND_UP(priv->config.hs_exit, temp) |
		     (DIV_ROUND_UP(priv->config.hs_trail, temp) << 8) |
		     (DIV_ROUND_UP(priv->config.hs_zero, temp) << 16) |
		     (DIV_ROUND_UP(priv->config.hs_prepare, temp) << 24));

	regmap_write(priv->regmap, MIPI_DSI_LP_TIM,
		     DIV_ROUND_UP(priv->config.lpx, temp) |
		     (DIV_ROUND_UP(priv->config.ta_sure, temp) << 8) |
		     (DIV_ROUND_UP(priv->config.ta_go, temp) << 16) |
		     (DIV_ROUND_UP(priv->config.ta_get, temp) << 24));

	regmap_write(priv->regmap, MIPI_DSI_ANA_UP_TIM, 0x0100);
	regmap_write(priv->regmap, MIPI_DSI_INIT_TIM,
		     DIV_ROUND_UP(priv->config.init * NSEC_PER_MSEC, temp));
	regmap_write(priv->regmap, MIPI_DSI_WAKEUP_TIM,
		     DIV_ROUND_UP(priv->config.wakeup * NSEC_PER_MSEC, temp));
	regmap_write(priv->regmap, MIPI_DSI_LPOK_TIM, 0x7C);
	regmap_write(priv->regmap, MIPI_DSI_ULPS_CHECK, 0x927C);
	regmap_write(priv->regmap, MIPI_DSI_LP_WCHDOG, 0x1000);
	regmap_write(priv->regmap, MIPI_DSI_TURN_WCHDOG, 0x1000);

	 
	switch (priv->config.lanes) {
	case 1:
		regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0xe);
		break;
	case 2:
		regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0xc);
		break;
	case 3:
		regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0x8);
		break;
	case 4:
	default:
		regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0);
		break;
	}

	 
	regmap_update_bits(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(1), BIT(1));

	return 0;
}

static int phy_meson_axg_mipi_dphy_power_off(struct phy *phy)
{
	struct phy_meson_axg_mipi_dphy_priv *priv = phy_get_drvdata(phy);

	regmap_write(priv->regmap, MIPI_DSI_CHAN_CTRL, 0xf);
	regmap_write(priv->regmap, MIPI_DSI_PHY_CTRL, BIT(31));

	phy_power_off(priv->analog);

	return 0;
}

static int phy_meson_axg_mipi_dphy_exit(struct phy *phy)
{
	struct phy_meson_axg_mipi_dphy_priv *priv = phy_get_drvdata(phy);
	int ret;

	ret = phy_exit(priv->analog);
	if (ret)
		return ret;

	return reset_control_reset(priv->reset);
}

static const struct phy_ops phy_meson_axg_mipi_dphy_ops = {
	.configure	= phy_meson_axg_mipi_dphy_configure,
	.init		= phy_meson_axg_mipi_dphy_init,
	.exit		= phy_meson_axg_mipi_dphy_exit,
	.power_on	= phy_meson_axg_mipi_dphy_power_on,
	.power_off	= phy_meson_axg_mipi_dphy_power_off,
	.owner		= THIS_MODULE,
};

static int phy_meson_axg_mipi_dphy_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct phy_provider *phy_provider;
	struct phy_meson_axg_mipi_dphy_priv *priv;
	struct phy *phy;
	void __iomem *base;
	int ret;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->dev = dev;
	platform_set_drvdata(pdev, priv);

	base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(base))
		return PTR_ERR(base);

	priv->regmap = devm_regmap_init_mmio(dev, base,
					&phy_meson_axg_mipi_dphy_regmap_conf);
	if (IS_ERR(priv->regmap))
		return PTR_ERR(priv->regmap);

	priv->clk = devm_clk_get(dev, "pclk");
	if (IS_ERR(priv->clk))
		return PTR_ERR(priv->clk);

	priv->reset = devm_reset_control_get(dev, "phy");
	if (IS_ERR(priv->reset))
		return PTR_ERR(priv->reset);

	priv->analog = devm_phy_get(dev, "analog");
	if (IS_ERR(priv->analog))
		return PTR_ERR(priv->analog);

	ret = clk_prepare_enable(priv->clk);
	if (ret)
		return ret;

	ret = reset_control_deassert(priv->reset);
	if (ret)
		return ret;

	phy = devm_phy_create(dev, NULL, &phy_meson_axg_mipi_dphy_ops);
	if (IS_ERR(phy)) {
		ret = PTR_ERR(phy);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "failed to create PHY\n");

		return ret;
	}

	phy_set_drvdata(phy, priv);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);
}

static const struct of_device_id phy_meson_axg_mipi_dphy_of_match[] = {
	{ .compatible = "amlogic,axg-mipi-dphy", },
	{ },
};
MODULE_DEVICE_TABLE(of, phy_meson_axg_mipi_dphy_of_match);

static struct platform_driver phy_meson_axg_mipi_dphy_driver = {
	.probe	= phy_meson_axg_mipi_dphy_probe,
	.driver	= {
		.name		= "phy-meson-axg-mipi-dphy",
		.of_match_table	= phy_meson_axg_mipi_dphy_of_match,
	},
};
module_platform_driver(phy_meson_axg_mipi_dphy_driver);

MODULE_AUTHOR("Neil Armstrong <narmstrong@baylibre.com>");
MODULE_DESCRIPTION("Meson AXG MIPI DPHY driver");
MODULE_LICENSE("GPL v2");
