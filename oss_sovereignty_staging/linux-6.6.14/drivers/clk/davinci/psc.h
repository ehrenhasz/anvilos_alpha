 
 

#ifndef __CLK_DAVINCI_PSC_H__
#define __CLK_DAVINCI_PSC_H__

#include <linux/clk-provider.h>
#include <linux/types.h>

 
#define LPSC_ALWAYS_ENABLED	BIT(0)  
#define LPSC_SET_RATE_PARENT	BIT(1)  
#define LPSC_FORCE		BIT(2)  
#define LPSC_LOCAL_RESET	BIT(3)  

struct davinci_lpsc_clkdev_info {
	const char *con_id;
	const char *dev_id;
};

#define LPSC_CLKDEV(c, d) {	\
	.con_id = (c),		\
	.dev_id = (d)		\
}

#define LPSC_CLKDEV1(n, c, d) \
static const struct davinci_lpsc_clkdev_info n[] __initconst = {	\
	LPSC_CLKDEV((c), (d)),						\
	{ }								\
}

#define LPSC_CLKDEV2(n, c1, d1, c2, d2) \
static const struct davinci_lpsc_clkdev_info n[] __initconst = {	\
	LPSC_CLKDEV((c1), (d1)),					\
	LPSC_CLKDEV((c2), (d2)),					\
	{ }								\
}

#define LPSC_CLKDEV3(n, c1, d1, c2, d2, c3, d3) \
static const struct davinci_lpsc_clkdev_info n[] __initconst = {	\
	LPSC_CLKDEV((c1), (d1)),					\
	LPSC_CLKDEV((c2), (d2)),					\
	LPSC_CLKDEV((c3), (d3)),					\
	{ }								\
}

 
struct davinci_lpsc_clk_info {
	const char *name;
	const char *parent;
	const struct davinci_lpsc_clkdev_info *cdevs;
	u32 md;
	u32 pd;
	unsigned long flags;
};

#define LPSC(m, d, n, p, c, f)	\
{				\
	.name	= #n,		\
	.parent	= #p,		\
	.cdevs	= (c),		\
	.md	= (m),		\
	.pd	= (d),		\
	.flags	= (f),		\
}

int davinci_psc_register_clocks(struct device *dev,
				const struct davinci_lpsc_clk_info *info,
				u8 num_clks,
				void __iomem *base);

int of_davinci_psc_clk_init(struct device *dev,
			    const struct davinci_lpsc_clk_info *info,
			    u8 num_clks,
			    void __iomem *base);

 

struct davinci_psc_init_data {
	struct clk_bulk_data *parent_clks;
	int num_parent_clks;
	int (*psc_init)(struct device *dev, void __iomem *base);
};

#ifdef CONFIG_ARCH_DAVINCI_DA830
extern const struct davinci_psc_init_data da830_psc0_init_data;
extern const struct davinci_psc_init_data da830_psc1_init_data;
#endif
#ifdef CONFIG_ARCH_DAVINCI_DA850
extern const struct davinci_psc_init_data da850_psc0_init_data;
extern const struct davinci_psc_init_data da850_psc1_init_data;
extern const struct davinci_psc_init_data of_da850_psc0_init_data;
extern const struct davinci_psc_init_data of_da850_psc1_init_data;
#endif
#endif  
