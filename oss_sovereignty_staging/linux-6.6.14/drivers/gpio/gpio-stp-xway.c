
 

#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/mutex.h>
#include <linux/gpio/driver.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/err.h>

 

 
#define XWAY_STP_CON0		0x00
 
#define XWAY_STP_CON1		0x04
 
#define XWAY_STP_CPU0		0x08
 
#define XWAY_STP_CPU1		0x0C
 
#define XWAY_STP_AR		0x10

 
#define XWAY_STP_CON_SWU	BIT(31)

 
#define XWAY_STP_2HZ		0
#define XWAY_STP_4HZ		BIT(23)
#define XWAY_STP_8HZ		BIT(24)
#define XWAY_STP_10HZ		(BIT(24) | BIT(23))
#define XWAY_STP_SPEED_MASK	(BIT(23) | BIT(24) | BIT(25) | BIT(26) | BIT(27))

#define XWAY_STP_FPIS_VALUE	BIT(21)
#define XWAY_STP_FPIS_MASK	(BIT(20) | BIT(21))

 
#define XWAY_STP_UPD_FPI	BIT(31)
#define XWAY_STP_UPD_MASK	(BIT(31) | BIT(30))

 
#define XWAY_STP_ADSL_SHIFT	24
#define XWAY_STP_ADSL_MASK	0x3

 
#define XWAY_STP_PHY_MASK	0x7
#define XWAY_STP_PHY1_SHIFT	27
#define XWAY_STP_PHY2_SHIFT	3
#define XWAY_STP_PHY3_SHIFT	6
#define XWAY_STP_PHY4_SHIFT	15

 
#define XWAY_STP_GROUP0		BIT(0)
#define XWAY_STP_GROUP1		BIT(1)
#define XWAY_STP_GROUP2		BIT(2)
#define XWAY_STP_GROUP_MASK	(0x7)

 
#define XWAY_STP_FALLING	BIT(26)
#define XWAY_STP_EDGE_MASK	BIT(26)

#define xway_stp_r32(m, reg)		__raw_readl(m + reg)
#define xway_stp_w32(m, val, reg)	__raw_writel(val, m + reg)
#define xway_stp_w32_mask(m, clear, set, reg) \
		xway_stp_w32(m, (xway_stp_r32(m, reg) & ~(clear)) | (set), reg)

struct xway_stp {
	struct gpio_chip gc;
	void __iomem *virt;
	u32 edge;	 
	u32 shadow;	 
	u8 groups;	 
	u8 dsl;		 
	u8 phy1;	 
	u8 phy2;	 
	u8 phy3;	 
	u8 phy4;	 
	u8 reserved;	 
};

 
static int xway_stp_get(struct gpio_chip *gc, unsigned int gpio)
{
	struct xway_stp *chip = gpiochip_get_data(gc);

	return (xway_stp_r32(chip->virt, XWAY_STP_CPU0) & BIT(gpio));
}

 
static void xway_stp_set(struct gpio_chip *gc, unsigned gpio, int val)
{
	struct xway_stp *chip = gpiochip_get_data(gc);

	if (val)
		chip->shadow |= BIT(gpio);
	else
		chip->shadow &= ~BIT(gpio);
	xway_stp_w32(chip->virt, chip->shadow, XWAY_STP_CPU0);
	if (!chip->reserved)
		xway_stp_w32_mask(chip->virt, 0, XWAY_STP_CON_SWU, XWAY_STP_CON0);
}

 
static int xway_stp_dir_out(struct gpio_chip *gc, unsigned gpio, int val)
{
	xway_stp_set(gc, gpio, val);

	return 0;
}

 
static int xway_stp_request(struct gpio_chip *gc, unsigned gpio)
{
	struct xway_stp *chip = gpiochip_get_data(gc);

	if ((gpio < 8) && (chip->reserved & BIT(gpio))) {
		dev_err(gc->parent, "GPIO %d is driven by hardware\n", gpio);
		return -ENODEV;
	}

	return 0;
}

 
static void xway_stp_hw_init(struct xway_stp *chip)
{
	 
	xway_stp_w32(chip->virt, 0, XWAY_STP_AR);
	xway_stp_w32(chip->virt, 0, XWAY_STP_CPU0);
	xway_stp_w32(chip->virt, 0, XWAY_STP_CPU1);
	xway_stp_w32(chip->virt, XWAY_STP_CON_SWU, XWAY_STP_CON0);
	xway_stp_w32(chip->virt, 0, XWAY_STP_CON1);

	 
	xway_stp_w32_mask(chip->virt, XWAY_STP_EDGE_MASK,
				chip->edge, XWAY_STP_CON0);

	 
	xway_stp_w32_mask(chip->virt, XWAY_STP_GROUP_MASK,
				chip->groups, XWAY_STP_CON1);

	 
	xway_stp_w32_mask(chip->virt,
			XWAY_STP_ADSL_MASK << XWAY_STP_ADSL_SHIFT,
			chip->dsl << XWAY_STP_ADSL_SHIFT,
			XWAY_STP_CON0);

	 
	xway_stp_w32_mask(chip->virt,
			XWAY_STP_PHY_MASK << XWAY_STP_PHY1_SHIFT,
			chip->phy1 << XWAY_STP_PHY1_SHIFT,
			XWAY_STP_CON0);
	xway_stp_w32_mask(chip->virt,
			XWAY_STP_PHY_MASK << XWAY_STP_PHY2_SHIFT,
			chip->phy2 << XWAY_STP_PHY2_SHIFT,
			XWAY_STP_CON1);

	if (of_machine_is_compatible("lantiq,grx390")
	    || of_machine_is_compatible("lantiq,ar10")) {
		xway_stp_w32_mask(chip->virt,
				XWAY_STP_PHY_MASK << XWAY_STP_PHY3_SHIFT,
				chip->phy3 << XWAY_STP_PHY3_SHIFT,
				XWAY_STP_CON1);
	}

	if (of_machine_is_compatible("lantiq,grx390")) {
		xway_stp_w32_mask(chip->virt,
				XWAY_STP_PHY_MASK << XWAY_STP_PHY4_SHIFT,
				chip->phy4 << XWAY_STP_PHY4_SHIFT,
				XWAY_STP_CON1);
	}

	 
	chip->reserved = (chip->phy4 << 11) | (chip->phy3 << 8) | (chip->phy2 << 5)
		| (chip->phy1 << 2) | chip->dsl;

	 
	if (chip->reserved) {
		xway_stp_w32_mask(chip->virt, XWAY_STP_UPD_MASK,
			XWAY_STP_UPD_FPI, XWAY_STP_CON1);
		xway_stp_w32_mask(chip->virt, XWAY_STP_SPEED_MASK,
			XWAY_STP_10HZ, XWAY_STP_CON1);
		xway_stp_w32_mask(chip->virt, XWAY_STP_FPIS_MASK,
			XWAY_STP_FPIS_VALUE, XWAY_STP_CON1);
	}
}

static int xway_stp_probe(struct platform_device *pdev)
{
	u32 shadow, groups, dsl, phy;
	struct xway_stp *chip;
	struct clk *clk;
	int ret = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->virt = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(chip->virt))
		return PTR_ERR(chip->virt);

	chip->gc.parent = &pdev->dev;
	chip->gc.label = "stp-xway";
	chip->gc.direction_output = xway_stp_dir_out;
	chip->gc.get = xway_stp_get;
	chip->gc.set = xway_stp_set;
	chip->gc.request = xway_stp_request;
	chip->gc.base = -1;
	chip->gc.owner = THIS_MODULE;

	 
	if (!of_property_read_u32(pdev->dev.of_node, "lantiq,shadow", &shadow))
		chip->shadow = shadow;

	 
	if (!of_property_read_u32(pdev->dev.of_node, "lantiq,groups", &groups))
		chip->groups = groups & XWAY_STP_GROUP_MASK;
	else
		chip->groups = XWAY_STP_GROUP0;
	chip->gc.ngpio = fls(chip->groups) * 8;

	 
	if (!of_property_read_u32(pdev->dev.of_node, "lantiq,dsl", &dsl))
		chip->dsl = dsl & XWAY_STP_ADSL_MASK;

	 
	if (of_machine_is_compatible("lantiq,ar9") ||
			of_machine_is_compatible("lantiq,gr9") ||
			of_machine_is_compatible("lantiq,vr9") ||
			of_machine_is_compatible("lantiq,ar10") ||
			of_machine_is_compatible("lantiq,grx390")) {
		if (!of_property_read_u32(pdev->dev.of_node, "lantiq,phy1", &phy))
			chip->phy1 = phy & XWAY_STP_PHY_MASK;
		if (!of_property_read_u32(pdev->dev.of_node, "lantiq,phy2", &phy))
			chip->phy2 = phy & XWAY_STP_PHY_MASK;
	}

	if (of_machine_is_compatible("lantiq,ar10") ||
			of_machine_is_compatible("lantiq,grx390")) {
		if (!of_property_read_u32(pdev->dev.of_node, "lantiq,phy3", &phy))
			chip->phy3 = phy & XWAY_STP_PHY_MASK;
	}

	if (of_machine_is_compatible("lantiq,grx390")) {
		if (!of_property_read_u32(pdev->dev.of_node, "lantiq,phy4", &phy))
			chip->phy4 = phy & XWAY_STP_PHY_MASK;
	}

	 
	if (!of_property_read_bool(pdev->dev.of_node, "lantiq,rising"))
		chip->edge = XWAY_STP_FALLING;

	clk = devm_clk_get(&pdev->dev, NULL);
	if (IS_ERR(clk)) {
		dev_err(&pdev->dev, "Failed to get clock\n");
		return PTR_ERR(clk);
	}

	ret = clk_prepare_enable(clk);
	if (ret)
		return ret;

	xway_stp_hw_init(chip);

	ret = devm_gpiochip_add_data(&pdev->dev, &chip->gc, chip);
	if (ret) {
		clk_disable_unprepare(clk);
		return ret;
	}

	dev_info(&pdev->dev, "Init done\n");

	return 0;
}

static const struct of_device_id xway_stp_match[] = {
	{ .compatible = "lantiq,gpio-stp-xway" },
	{},
};
MODULE_DEVICE_TABLE(of, xway_stp_match);

static struct platform_driver xway_stp_driver = {
	.probe = xway_stp_probe,
	.driver = {
		.name = "gpio-stp-xway",
		.of_match_table = xway_stp_match,
	},
};

static int __init xway_stp_init(void)
{
	return platform_driver_register(&xway_stp_driver);
}

subsys_initcall(xway_stp_init);
