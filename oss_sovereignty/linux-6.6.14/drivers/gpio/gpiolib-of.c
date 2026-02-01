
 

#include <linux/device.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/slab.h>
#include <linux/string.h>

#include <linux/gpio/consumer.h>
#include <linux/gpio/machine.h>

#include "gpiolib.h"
#include "gpiolib-of.h"

 
enum of_gpio_flags {
	OF_GPIO_ACTIVE_LOW = 0x1,
	OF_GPIO_SINGLE_ENDED = 0x2,
	OF_GPIO_OPEN_DRAIN = 0x4,
	OF_GPIO_TRANSITORY = 0x8,
	OF_GPIO_PULL_UP = 0x10,
	OF_GPIO_PULL_DOWN = 0x20,
	OF_GPIO_PULL_DISABLE = 0x40,
};

 
static int of_gpio_named_count(const struct device_node *np,
			       const char *propname)
{
	return of_count_phandle_with_args(np, propname, "#gpio-cells");
}

 
static int of_gpio_spi_cs_get_count(struct device *dev, const char *con_id)
{
	struct device_node *np = dev->of_node;

	if (!IS_ENABLED(CONFIG_SPI_MASTER))
		return 0;
	if (!con_id || strcmp(con_id, "cs"))
		return 0;
	if (!of_device_is_compatible(np, "fsl,spi") &&
	    !of_device_is_compatible(np, "aeroflexgaisler,spictrl") &&
	    !of_device_is_compatible(np, "ibm,ppc4xx-spi"))
		return 0;
	return of_gpio_named_count(np, "gpios");
}

int of_gpio_get_count(struct device *dev, const char *con_id)
{
	int ret;
	char propname[32];
	unsigned int i;

	ret = of_gpio_spi_cs_get_count(dev, con_id);
	if (ret > 0)
		return ret;

	for (i = 0; i < ARRAY_SIZE(gpio_suffixes); i++) {
		if (con_id)
			snprintf(propname, sizeof(propname), "%s-%s",
				 con_id, gpio_suffixes[i]);
		else
			snprintf(propname, sizeof(propname), "%s",
				 gpio_suffixes[i]);

		ret = of_gpio_named_count(dev->of_node, propname);
		if (ret > 0)
			break;
	}
	return ret ? ret : -ENOENT;
}

static int of_gpiochip_match_node_and_xlate(struct gpio_chip *chip, void *data)
{
	struct of_phandle_args *gpiospec = data;

	return device_match_of_node(&chip->gpiodev->dev, gpiospec->np) &&
				chip->of_xlate &&
				chip->of_xlate(chip, gpiospec, NULL) >= 0;
}

static struct gpio_chip *of_find_gpiochip_by_xlate(
					struct of_phandle_args *gpiospec)
{
	return gpiochip_find(gpiospec, of_gpiochip_match_node_and_xlate);
}

static struct gpio_desc *of_xlate_and_get_gpiod_flags(struct gpio_chip *chip,
					struct of_phandle_args *gpiospec,
					enum of_gpio_flags *flags)
{
	int ret;

	if (chip->of_gpio_n_cells != gpiospec->args_count)
		return ERR_PTR(-EINVAL);

	ret = chip->of_xlate(chip, gpiospec, flags);
	if (ret < 0)
		return ERR_PTR(ret);

	return gpiochip_get_desc(chip, ret);
}

 
static void of_gpio_quirk_polarity(const struct device_node *np,
				   bool active_high,
				   enum of_gpio_flags *flags)
{
	if (active_high) {
		if (*flags & OF_GPIO_ACTIVE_LOW) {
			pr_warn("%s GPIO handle specifies active low - ignored\n",
				of_node_full_name(np));
			*flags &= ~OF_GPIO_ACTIVE_LOW;
		}
	} else {
		if (!(*flags & OF_GPIO_ACTIVE_LOW))
			pr_info("%s enforce active low on GPIO handle\n",
				of_node_full_name(np));
		*flags |= OF_GPIO_ACTIVE_LOW;
	}
}

 
static void of_gpio_try_fixup_polarity(const struct device_node *np,
				       const char *propname,
				       enum of_gpio_flags *flags)
{
	static const struct {
		const char *compatible;
		const char *propname;
		bool active_high;
	} gpios[] = {
#if !IS_ENABLED(CONFIG_LCD_HX8357)
		 
		{ "himax,hx8357",	"gpios-reset",	false },
		{ "himax,hx8369",	"gpios-reset",	false },
#endif
	};
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		if (of_device_is_compatible(np, gpios[i].compatible) &&
		    !strcmp(propname, gpios[i].propname)) {
			of_gpio_quirk_polarity(np, gpios[i].active_high, flags);
			break;
		}
	}
}

static void of_gpio_set_polarity_by_property(const struct device_node *np,
					     const char *propname,
					     enum of_gpio_flags *flags)
{
	const struct device_node *np_compat = np;
	const struct device_node *np_propname = np;
	static const struct {
		const char *compatible;
		const char *gpio_propname;
		const char *polarity_propname;
	} gpios[] = {
#if IS_ENABLED(CONFIG_FEC)
		 
		{ "fsl,imx25-fec",   "phy-reset-gpios", "phy-reset-active-high" },
		{ "fsl,imx27-fec",   "phy-reset-gpios", "phy-reset-active-high" },
		{ "fsl,imx28-fec",   "phy-reset-gpios", "phy-reset-active-high" },
		{ "fsl,imx6q-fec",   "phy-reset-gpios", "phy-reset-active-high" },
		{ "fsl,mvf600-fec",  "phy-reset-gpios", "phy-reset-active-high" },
		{ "fsl,imx6sx-fec",  "phy-reset-gpios", "phy-reset-active-high" },
		{ "fsl,imx6ul-fec",  "phy-reset-gpios", "phy-reset-active-high" },
		{ "fsl,imx8mq-fec",  "phy-reset-gpios", "phy-reset-active-high" },
		{ "fsl,imx8qm-fec",  "phy-reset-gpios", "phy-reset-active-high" },
		{ "fsl,s32v234-fec", "phy-reset-gpios", "phy-reset-active-high" },
#endif
#if IS_ENABLED(CONFIG_PCI_IMX6)
		{ "fsl,imx6q-pcie",  "reset-gpio", "reset-gpio-active-high" },
		{ "fsl,imx6sx-pcie", "reset-gpio", "reset-gpio-active-high" },
		{ "fsl,imx6qp-pcie", "reset-gpio", "reset-gpio-active-high" },
		{ "fsl,imx7d-pcie",  "reset-gpio", "reset-gpio-active-high" },
		{ "fsl,imx8mq-pcie", "reset-gpio", "reset-gpio-active-high" },
		{ "fsl,imx8mm-pcie", "reset-gpio", "reset-gpio-active-high" },
		{ "fsl,imx8mp-pcie", "reset-gpio", "reset-gpio-active-high" },
#endif

		 
#if IS_ENABLED(CONFIG_REGULATOR_FIXED_VOLTAGE)
		{ "regulator-fixed",   "gpios",        "enable-active-high" },
		{ "regulator-fixed",   "gpio",         "enable-active-high" },
		{ "reg-fixed-voltage", "gpios",        "enable-active-high" },
		{ "reg-fixed-voltage", "gpio",         "enable-active-high" },
#endif
#if IS_ENABLED(CONFIG_REGULATOR_GPIO)
		{ "regulator-gpio",    "enable-gpio",  "enable-active-high" },
		{ "regulator-gpio",    "enable-gpios", "enable-active-high" },
#endif
#if IS_ENABLED(CONFIG_MMC_ATMELMCI)
		{ "atmel,hsmci",       "cd-gpios",     "cd-inverted" },
#endif
	};
	unsigned int i;
	bool active_high;

#if IS_ENABLED(CONFIG_MMC_ATMELMCI)
	 
	if (of_device_is_compatible(np->parent, "atmel,hsmci")) {
		np_compat = np->parent;
		np_propname = np;
	}
#endif

	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		if (of_device_is_compatible(np_compat, gpios[i].compatible) &&
		    !strcmp(propname, gpios[i].gpio_propname)) {
			active_high = of_property_read_bool(np_propname,
						gpios[i].polarity_propname);
			of_gpio_quirk_polarity(np, active_high, flags);
			break;
		}
	}
}

static void of_gpio_flags_quirks(const struct device_node *np,
				 const char *propname,
				 enum of_gpio_flags *flags,
				 int index)
{
	of_gpio_try_fixup_polarity(np, propname, flags);
	of_gpio_set_polarity_by_property(np, propname, flags);

	 
	if (IS_ENABLED(CONFIG_REGULATOR) &&
	    of_device_is_compatible(np, "reg-fixed-voltage") &&
	    of_property_read_bool(np, "gpio-open-drain")) {
		*flags |= (OF_GPIO_SINGLE_ENDED | OF_GPIO_OPEN_DRAIN);
		pr_info("%s uses legacy open drain flag - update the DTS if you can\n",
			of_node_full_name(np));
	}

	 
	if (IS_ENABLED(CONFIG_SPI_MASTER) && !strcmp(propname, "cs-gpios") &&
	    of_property_read_bool(np, "cs-gpios")) {
		struct device_node *child;
		u32 cs;
		int ret;

		for_each_child_of_node(np, child) {
			ret = of_property_read_u32(child, "reg", &cs);
			if (ret)
				continue;
			if (cs == index) {
				 
				bool active_high = of_property_read_bool(child,
								"spi-cs-high");
				of_gpio_quirk_polarity(child, active_high,
						       flags);
				of_node_put(child);
				break;
			}
		}
	}

	 
	if (IS_ENABLED(CONFIG_STMMAC_ETH) &&
	    !strcmp(propname, "snps,reset-gpio") &&
	    of_property_read_bool(np, "snps,reset-active-low"))
		*flags |= OF_GPIO_ACTIVE_LOW;
}

 
static struct gpio_desc *of_get_named_gpiod_flags(const struct device_node *np,
		     const char *propname, int index, enum of_gpio_flags *flags)
{
	struct of_phandle_args gpiospec;
	struct gpio_chip *chip;
	struct gpio_desc *desc;
	int ret;

	ret = of_parse_phandle_with_args_map(np, propname, "gpio", index,
					     &gpiospec);
	if (ret) {
		pr_debug("%s: can't parse '%s' property of node '%pOF[%d]'\n",
			__func__, propname, np, index);
		return ERR_PTR(ret);
	}

	chip = of_find_gpiochip_by_xlate(&gpiospec);
	if (!chip) {
		desc = ERR_PTR(-EPROBE_DEFER);
		goto out;
	}

	desc = of_xlate_and_get_gpiod_flags(chip, &gpiospec, flags);
	if (IS_ERR(desc))
		goto out;

	if (flags)
		of_gpio_flags_quirks(np, propname, flags, index);

	pr_debug("%s: parsed '%s' property of node '%pOF[%d]' - status (%d)\n",
		 __func__, propname, np, index,
		 PTR_ERR_OR_ZERO(desc));

out:
	of_node_put(gpiospec.np);

	return desc;
}

 
int of_get_named_gpio(const struct device_node *np, const char *propname,
		      int index)
{
	struct gpio_desc *desc;

	desc = of_get_named_gpiod_flags(np, propname, index, NULL);

	if (IS_ERR(desc))
		return PTR_ERR(desc);
	else
		return desc_to_gpio(desc);
}
EXPORT_SYMBOL_GPL(of_get_named_gpio);

 
static unsigned long of_convert_gpio_flags(enum of_gpio_flags flags)
{
	unsigned long lflags = GPIO_LOOKUP_FLAGS_DEFAULT;

	if (flags & OF_GPIO_ACTIVE_LOW)
		lflags |= GPIO_ACTIVE_LOW;

	if (flags & OF_GPIO_SINGLE_ENDED) {
		if (flags & OF_GPIO_OPEN_DRAIN)
			lflags |= GPIO_OPEN_DRAIN;
		else
			lflags |= GPIO_OPEN_SOURCE;
	}

	if (flags & OF_GPIO_TRANSITORY)
		lflags |= GPIO_TRANSITORY;

	if (flags & OF_GPIO_PULL_UP)
		lflags |= GPIO_PULL_UP;

	if (flags & OF_GPIO_PULL_DOWN)
		lflags |= GPIO_PULL_DOWN;

	if (flags & OF_GPIO_PULL_DISABLE)
		lflags |= GPIO_PULL_DISABLE;

	return lflags;
}

static struct gpio_desc *of_find_gpio_rename(struct device_node *np,
					     const char *con_id,
					     unsigned int idx,
					     enum of_gpio_flags *of_flags)
{
	static const struct of_rename_gpio {
		const char *con_id;
		const char *legacy_id;	 
		 
		const char *compatible;
	} gpios[] = {
#if !IS_ENABLED(CONFIG_LCD_HX8357)
		 
		{ "reset",	"gpios-reset",	"himax,hx8357" },
		{ "reset",	"gpios-reset",	"himax,hx8369" },
#endif
#if IS_ENABLED(CONFIG_MFD_ARIZONA)
		{ "wlf,reset",	NULL,		NULL },
#endif
#if IS_ENABLED(CONFIG_RTC_DRV_MOXART)
		{ "rtc-data",	"gpio-rtc-data",	"moxa,moxart-rtc" },
		{ "rtc-sclk",	"gpio-rtc-sclk",	"moxa,moxart-rtc" },
		{ "rtc-reset",	"gpio-rtc-reset",	"moxa,moxart-rtc" },
#endif
#if IS_ENABLED(CONFIG_NFC_MRVL_I2C)
		{ "reset",	"reset-n-io",	"marvell,nfc-i2c" },
#endif
#if IS_ENABLED(CONFIG_NFC_MRVL_SPI)
		{ "reset",	"reset-n-io",	"marvell,nfc-spi" },
#endif
#if IS_ENABLED(CONFIG_NFC_MRVL_UART)
		{ "reset",	"reset-n-io",	"marvell,nfc-uart" },
		{ "reset",	"reset-n-io",	"mrvl,nfc-uart" },
#endif
#if !IS_ENABLED(CONFIG_PCI_LANTIQ)
		 
		{ "reset",	"gpios-reset",	"lantiq,pci-xway" },
#endif

		 
#if IS_ENABLED(CONFIG_REGULATOR_ARIZONA_LDO1)
		{ "wlf,ldoena",  NULL,		NULL },  
#endif
#if IS_ENABLED(CONFIG_REGULATOR_WM8994)
		{ "wlf,ldo1ena", NULL,		NULL },  
		{ "wlf,ldo2ena", NULL,		NULL },  
#endif

#if IS_ENABLED(CONFIG_SND_SOC_CS42L56)
		{ "reset",	"cirrus,gpio-nreset",	"cirrus,cs42l56" },
#endif
#if IS_ENABLED(CONFIG_SND_SOC_MT2701_CS42448)
		{ "i2s1-in-sel-gpio1",	NULL,	"mediatek,mt2701-cs42448-machine" },
		{ "i2s1-in-sel-gpio2",	NULL,	"mediatek,mt2701-cs42448-machine" },
#endif
#if IS_ENABLED(CONFIG_SND_SOC_TLV320AIC3X)
		{ "reset",	"gpio-reset",	"ti,tlv320aic3x" },
		{ "reset",	"gpio-reset",	"ti,tlv320aic33" },
		{ "reset",	"gpio-reset",	"ti,tlv320aic3007" },
		{ "reset",	"gpio-reset",	"ti,tlv320aic3104" },
		{ "reset",	"gpio-reset",	"ti,tlv320aic3106" },
#endif
#if IS_ENABLED(CONFIG_SPI_GPIO)
		 
		{ "miso",	"gpio-miso",	"spi-gpio" },
		{ "mosi",	"gpio-mosi",	"spi-gpio" },
		{ "sck",	"gpio-sck",	"spi-gpio" },
#endif

		 
#if IS_ENABLED(CONFIG_SPI_FSL_SPI)
		{ "cs",		"gpios",	"fsl,spi" },
		{ "cs",		"gpios",	"aeroflexgaisler,spictrl" },
#endif
#if IS_ENABLED(CONFIG_SPI_PPC4xx)
		{ "cs",		"gpios",	"ibm,ppc4xx-spi" },
#endif

#if IS_ENABLED(CONFIG_TYPEC_FUSB302)
		 
		{ "fcs,int_n",	NULL,		"fcs,fusb302" },
#endif
	};
	struct gpio_desc *desc;
	const char *legacy_id;
	unsigned int i;

	if (!con_id)
		return ERR_PTR(-ENOENT);

	for (i = 0; i < ARRAY_SIZE(gpios); i++) {
		if (strcmp(con_id, gpios[i].con_id))
			continue;

		if (gpios[i].compatible &&
		    !of_device_is_compatible(np, gpios[i].compatible))
			continue;

		legacy_id = gpios[i].legacy_id ?: gpios[i].con_id;
		desc = of_get_named_gpiod_flags(np, legacy_id, idx, of_flags);
		if (!gpiod_not_found(desc)) {
			pr_info("%s uses legacy gpio name '%s' instead of '%s-gpios'\n",
				of_node_full_name(np), legacy_id, con_id);
			return desc;
		}
	}

	return ERR_PTR(-ENOENT);
}

static struct gpio_desc *of_find_mt2701_gpio(struct device_node *np,
					     const char *con_id,
					     unsigned int idx,
					     enum of_gpio_flags *of_flags)
{
	struct gpio_desc *desc;
	const char *legacy_id;

	if (!IS_ENABLED(CONFIG_SND_SOC_MT2701_CS42448))
		return ERR_PTR(-ENOENT);

	if (!of_device_is_compatible(np, "mediatek,mt2701-cs42448-machine"))
		return ERR_PTR(-ENOENT);

	if (!con_id || strcmp(con_id, "i2s1-in-sel"))
		return ERR_PTR(-ENOENT);

	if (idx == 0)
		legacy_id = "i2s1-in-sel-gpio1";
	else if (idx == 1)
		legacy_id = "i2s1-in-sel-gpio2";
	else
		return ERR_PTR(-ENOENT);

	desc = of_get_named_gpiod_flags(np, legacy_id, 0, of_flags);
	if (!gpiod_not_found(desc))
		pr_info("%s is using legacy gpio name '%s' instead of '%s-gpios'\n",
			of_node_full_name(np), legacy_id, con_id);

	return desc;
}

typedef struct gpio_desc *(*of_find_gpio_quirk)(struct device_node *np,
						const char *con_id,
						unsigned int idx,
						enum of_gpio_flags *of_flags);
static const of_find_gpio_quirk of_find_gpio_quirks[] = {
	of_find_gpio_rename,
	of_find_mt2701_gpio,
	NULL
};

struct gpio_desc *of_find_gpio(struct device_node *np, const char *con_id,
			       unsigned int idx, unsigned long *flags)
{
	char prop_name[32];  
	enum of_gpio_flags of_flags;
	const of_find_gpio_quirk *q;
	struct gpio_desc *desc;
	unsigned int i;

	 
	for (i = 0; i < ARRAY_SIZE(gpio_suffixes); i++) {
		if (con_id)
			snprintf(prop_name, sizeof(prop_name), "%s-%s", con_id,
				 gpio_suffixes[i]);
		else
			snprintf(prop_name, sizeof(prop_name), "%s",
				 gpio_suffixes[i]);

		desc = of_get_named_gpiod_flags(np, prop_name, idx, &of_flags);

		if (!gpiod_not_found(desc))
			break;
	}

	 
	for (q = of_find_gpio_quirks; gpiod_not_found(desc) && *q; q++)
		desc = (*q)(np, con_id, idx, &of_flags);

	if (IS_ERR(desc))
		return desc;

	*flags = of_convert_gpio_flags(of_flags);

	return desc;
}

 
static struct gpio_desc *of_parse_own_gpio(struct device_node *np,
					   struct gpio_chip *chip,
					   unsigned int idx, const char **name,
					   unsigned long *lflags,
					   enum gpiod_flags *dflags)
{
	struct device_node *chip_np;
	enum of_gpio_flags xlate_flags;
	struct of_phandle_args gpiospec;
	struct gpio_desc *desc;
	unsigned int i;
	u32 tmp;
	int ret;

	chip_np = dev_of_node(&chip->gpiodev->dev);
	if (!chip_np)
		return ERR_PTR(-EINVAL);

	xlate_flags = 0;
	*lflags = GPIO_LOOKUP_FLAGS_DEFAULT;
	*dflags = GPIOD_ASIS;

	ret = of_property_read_u32(chip_np, "#gpio-cells", &tmp);
	if (ret)
		return ERR_PTR(ret);

	gpiospec.np = chip_np;
	gpiospec.args_count = tmp;

	for (i = 0; i < tmp; i++) {
		ret = of_property_read_u32_index(np, "gpios", idx * tmp + i,
						 &gpiospec.args[i]);
		if (ret)
			return ERR_PTR(ret);
	}

	desc = of_xlate_and_get_gpiod_flags(chip, &gpiospec, &xlate_flags);
	if (IS_ERR(desc))
		return desc;

	*lflags = of_convert_gpio_flags(xlate_flags);

	if (of_property_read_bool(np, "input"))
		*dflags |= GPIOD_IN;
	else if (of_property_read_bool(np, "output-low"))
		*dflags |= GPIOD_OUT_LOW;
	else if (of_property_read_bool(np, "output-high"))
		*dflags |= GPIOD_OUT_HIGH;
	else {
		pr_warn("GPIO line %d (%pOFn): no hogging state specified, bailing out\n",
			desc_to_gpio(desc), np);
		return ERR_PTR(-EINVAL);
	}

	if (name && of_property_read_string(np, "line-name", name))
		*name = np->name;

	return desc;
}

 
static int of_gpiochip_add_hog(struct gpio_chip *chip, struct device_node *hog)
{
	enum gpiod_flags dflags;
	struct gpio_desc *desc;
	unsigned long lflags;
	const char *name;
	unsigned int i;
	int ret;

	for (i = 0;; i++) {
		desc = of_parse_own_gpio(hog, chip, i, &name, &lflags, &dflags);
		if (IS_ERR(desc))
			break;

		ret = gpiod_hog(desc, name, lflags, dflags);
		if (ret < 0)
			return ret;

#ifdef CONFIG_OF_DYNAMIC
		desc->hog = hog;
#endif
	}

	return 0;
}

 
static int of_gpiochip_scan_gpios(struct gpio_chip *chip)
{
	struct device_node *np;
	int ret;

	for_each_available_child_of_node(dev_of_node(&chip->gpiodev->dev), np) {
		if (!of_property_read_bool(np, "gpio-hog"))
			continue;

		ret = of_gpiochip_add_hog(chip, np);
		if (ret < 0) {
			of_node_put(np);
			return ret;
		}

		of_node_set_flag(np, OF_POPULATED);
	}

	return 0;
}

#ifdef CONFIG_OF_DYNAMIC
 
static void of_gpiochip_remove_hog(struct gpio_chip *chip,
				   struct device_node *hog)
{
	struct gpio_desc *desc;

	for_each_gpio_desc_with_flag(chip, desc, FLAG_IS_HOGGED)
		if (desc->hog == hog)
			gpiochip_free_own_desc(desc);
}

static int of_gpiochip_match_node(struct gpio_chip *chip, void *data)
{
	return device_match_of_node(&chip->gpiodev->dev, data);
}

static struct gpio_chip *of_find_gpiochip_by_node(struct device_node *np)
{
	return gpiochip_find(np, of_gpiochip_match_node);
}

static int of_gpio_notify(struct notifier_block *nb, unsigned long action,
			  void *arg)
{
	struct of_reconfig_data *rd = arg;
	struct gpio_chip *chip;
	int ret;

	 
	switch (of_reconfig_get_state_change(action, arg)) {
	case OF_RECONFIG_CHANGE_ADD:
		if (!of_property_read_bool(rd->dn, "gpio-hog"))
			return NOTIFY_OK;	 

		if (of_node_test_and_set_flag(rd->dn, OF_POPULATED))
			return NOTIFY_OK;

		chip = of_find_gpiochip_by_node(rd->dn->parent);
		if (chip == NULL)
			return NOTIFY_OK;	 

		ret = of_gpiochip_add_hog(chip, rd->dn);
		if (ret < 0) {
			pr_err("%s: failed to add hogs for %pOF\n", __func__,
			       rd->dn);
			of_node_clear_flag(rd->dn, OF_POPULATED);
			return notifier_from_errno(ret);
		}
		break;

	case OF_RECONFIG_CHANGE_REMOVE:
		if (!of_node_check_flag(rd->dn, OF_POPULATED))
			return NOTIFY_OK;	 

		chip = of_find_gpiochip_by_node(rd->dn->parent);
		if (chip == NULL)
			return NOTIFY_OK;	 

		of_gpiochip_remove_hog(chip, rd->dn);
		of_node_clear_flag(rd->dn, OF_POPULATED);
		break;
	}

	return NOTIFY_OK;
}

struct notifier_block gpio_of_notifier = {
	.notifier_call = of_gpio_notify,
};
#endif  

 
static int of_gpio_simple_xlate(struct gpio_chip *gc,
				const struct of_phandle_args *gpiospec,
				u32 *flags)
{
	 
	if (gc->of_gpio_n_cells < 2) {
		WARN_ON(1);
		return -EINVAL;
	}

	if (WARN_ON(gpiospec->args_count < gc->of_gpio_n_cells))
		return -EINVAL;

	if (gpiospec->args[0] >= gc->ngpio)
		return -EINVAL;

	if (flags)
		*flags = gpiospec->args[1];

	return gpiospec->args[0];
}

#if IS_ENABLED(CONFIG_OF_GPIO_MM_GPIOCHIP)
#include <linux/gpio/legacy-of-mm-gpiochip.h>
 
int of_mm_gpiochip_add_data(struct device_node *np,
			    struct of_mm_gpio_chip *mm_gc,
			    void *data)
{
	int ret = -ENOMEM;
	struct gpio_chip *gc = &mm_gc->gc;

	gc->label = kasprintf(GFP_KERNEL, "%pOF", np);
	if (!gc->label)
		goto err0;

	mm_gc->regs = of_iomap(np, 0);
	if (!mm_gc->regs)
		goto err1;

	gc->base = -1;

	if (mm_gc->save_regs)
		mm_gc->save_regs(mm_gc);

	fwnode_handle_put(mm_gc->gc.fwnode);
	mm_gc->gc.fwnode = fwnode_handle_get(of_fwnode_handle(np));

	ret = gpiochip_add_data(gc, data);
	if (ret)
		goto err2;

	return 0;
err2:
	of_node_put(np);
	iounmap(mm_gc->regs);
err1:
	kfree(gc->label);
err0:
	pr_err("%pOF: GPIO chip registration failed with status %d\n", np, ret);
	return ret;
}
EXPORT_SYMBOL_GPL(of_mm_gpiochip_add_data);

 
void of_mm_gpiochip_remove(struct of_mm_gpio_chip *mm_gc)
{
	struct gpio_chip *gc = &mm_gc->gc;

	gpiochip_remove(gc);
	iounmap(mm_gc->regs);
	kfree(gc->label);
}
EXPORT_SYMBOL_GPL(of_mm_gpiochip_remove);
#endif

#ifdef CONFIG_PINCTRL
static int of_gpiochip_add_pin_range(struct gpio_chip *chip)
{
	struct of_phandle_args pinspec;
	struct pinctrl_dev *pctldev;
	struct device_node *np;
	int index = 0, ret;
	const char *name;
	static const char group_names_propname[] = "gpio-ranges-group-names";
	struct property *group_names;

	np = dev_of_node(&chip->gpiodev->dev);
	if (!np)
		return 0;

	group_names = of_find_property(np, group_names_propname, NULL);

	for (;; index++) {
		ret = of_parse_phandle_with_fixed_args(np, "gpio-ranges", 3,
				index, &pinspec);
		if (ret)
			break;

		pctldev = of_pinctrl_get(pinspec.np);
		of_node_put(pinspec.np);
		if (!pctldev)
			return -EPROBE_DEFER;

		if (pinspec.args[2]) {
			if (group_names) {
				of_property_read_string_index(np,
						group_names_propname,
						index, &name);
				if (strlen(name)) {
					pr_err("%pOF: Group name of numeric GPIO ranges must be the empty string.\n",
						np);
					break;
				}
			}
			 
			ret = gpiochip_add_pin_range(chip,
					pinctrl_dev_get_devname(pctldev),
					pinspec.args[0],
					pinspec.args[1],
					pinspec.args[2]);
			if (ret)
				return ret;
		} else {
			 
			if (pinspec.args[1]) {
				pr_err("%pOF: Illegal gpio-range format.\n",
					np);
				break;
			}

			if (!group_names) {
				pr_err("%pOF: GPIO group range requested but no %s property.\n",
					np, group_names_propname);
				break;
			}

			ret = of_property_read_string_index(np,
						group_names_propname,
						index, &name);
			if (ret)
				break;

			if (!strlen(name)) {
				pr_err("%pOF: Group name of GPIO group range cannot be the empty string.\n",
				np);
				break;
			}

			ret = gpiochip_add_pingroup_range(chip, pctldev,
						pinspec.args[0], name);
			if (ret)
				return ret;
		}
	}

	return 0;
}

#else
static int of_gpiochip_add_pin_range(struct gpio_chip *chip) { return 0; }
#endif

int of_gpiochip_add(struct gpio_chip *chip)
{
	struct device_node *np;
	int ret;

	np = dev_of_node(&chip->gpiodev->dev);
	if (!np)
		return 0;

	if (!chip->of_xlate) {
		chip->of_gpio_n_cells = 2;
		chip->of_xlate = of_gpio_simple_xlate;
	}

	if (chip->of_gpio_n_cells > MAX_PHANDLE_ARGS)
		return -EINVAL;

	ret = of_gpiochip_add_pin_range(chip);
	if (ret)
		return ret;

	of_node_get(np);

	ret = of_gpiochip_scan_gpios(chip);
	if (ret)
		of_node_put(np);

	return ret;
}

void of_gpiochip_remove(struct gpio_chip *chip)
{
	of_node_put(dev_of_node(&chip->gpiodev->dev));
}
