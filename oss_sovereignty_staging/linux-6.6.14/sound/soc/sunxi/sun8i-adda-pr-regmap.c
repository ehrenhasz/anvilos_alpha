
 

#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "sun8i-adda-pr-regmap.h"

 
#define ADDA_PR			0x0		 
#define ADDA_PR_RESET			BIT(28)
#define ADDA_PR_WRITE			BIT(24)
#define ADDA_PR_ADDR_SHIFT		16
#define ADDA_PR_ADDR_MASK		GENMASK(4, 0)
#define ADDA_PR_DATA_IN_SHIFT		8
#define ADDA_PR_DATA_IN_MASK		GENMASK(7, 0)
#define ADDA_PR_DATA_OUT_SHIFT		0
#define ADDA_PR_DATA_OUT_MASK		GENMASK(7, 0)

 
static int adda_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	void __iomem *base = (void __iomem *)context;
	u32 tmp;

	 
	writel(readl(base) | ADDA_PR_RESET, base);

	 
	writel(readl(base) & ~ADDA_PR_WRITE, base);

	 
	tmp = readl(base);
	tmp &= ~(ADDA_PR_ADDR_MASK << ADDA_PR_ADDR_SHIFT);
	tmp |= (reg & ADDA_PR_ADDR_MASK) << ADDA_PR_ADDR_SHIFT;
	writel(tmp, base);

	 
	*val = readl(base) & ADDA_PR_DATA_OUT_MASK;

	return 0;
}

static int adda_reg_write(void *context, unsigned int reg, unsigned int val)
{
	void __iomem *base = (void __iomem *)context;
	u32 tmp;

	 
	writel(readl(base) | ADDA_PR_RESET, base);

	 
	tmp = readl(base);
	tmp &= ~(ADDA_PR_ADDR_MASK << ADDA_PR_ADDR_SHIFT);
	tmp |= (reg & ADDA_PR_ADDR_MASK) << ADDA_PR_ADDR_SHIFT;
	writel(tmp, base);

	 
	tmp = readl(base);
	tmp &= ~(ADDA_PR_DATA_IN_MASK << ADDA_PR_DATA_IN_SHIFT);
	tmp |= (val & ADDA_PR_DATA_IN_MASK) << ADDA_PR_DATA_IN_SHIFT;
	writel(tmp, base);

	 
	writel(readl(base) | ADDA_PR_WRITE, base);

	 
	writel(readl(base) & ~ADDA_PR_WRITE, base);

	return 0;
}

static const struct regmap_config adda_pr_regmap_cfg = {
	.name		= "adda-pr",
	.reg_bits	= 5,
	.reg_stride	= 1,
	.val_bits	= 8,
	.reg_read	= adda_reg_read,
	.reg_write	= adda_reg_write,
	.fast_io	= true,
	.max_register	= 31,
};

struct regmap *sun8i_adda_pr_regmap_init(struct device *dev,
					 void __iomem *base)
{
	return devm_regmap_init(dev, NULL, base, &adda_pr_regmap_cfg);
}
EXPORT_SYMBOL_GPL(sun8i_adda_pr_regmap_init);

MODULE_DESCRIPTION("Allwinner analog audio codec regmap driver");
MODULE_AUTHOR("Vasily Khoruzhick <anarsoul@gmail.com>");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sunxi-adda-pr");
