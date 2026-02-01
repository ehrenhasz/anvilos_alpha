 
 

#ifndef __CLK_DAVINCI_PLL_H___
#define __CLK_DAVINCI_PLL_H___

#include <linux/bitops.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/types.h>

#define PLL_HAS_CLKMODE			BIT(0)  
#define PLL_HAS_PREDIV			BIT(1)  
#define PLL_PREDIV_ALWAYS_ENABLED	BIT(2)  
#define PLL_PREDIV_FIXED_DIV		BIT(3)  
#define PLL_HAS_POSTDIV			BIT(4)  
#define PLL_POSTDIV_ALWAYS_ENABLED	BIT(5)  
#define PLL_POSTDIV_FIXED_DIV		BIT(6)  
#define PLL_HAS_EXTCLKSRC		BIT(7)  
#define PLL_PLLM_2X			BIT(8)  
#define PLL_PREDIV_FIXED8		BIT(9)  

 
struct davinci_pll_clk_info {
	const char *name;
	u32 unlock_reg;
	u32 unlock_mask;
	u32 pllm_mask;
	u32 pllm_min;
	u32 pllm_max;
	unsigned long pllout_min_rate;
	unsigned long pllout_max_rate;
	u32 flags;
};

#define SYSCLK_ARM_RATE		BIT(0)  
#define SYSCLK_ALWAYS_ENABLED	BIT(1)  
#define SYSCLK_FIXED_DIV	BIT(2)  

 
struct davinci_pll_sysclk_info {
	const char *name;
	const char *parent_name;
	u32 id;
	u32 ratio_width;
	u32 flags;
};

#define SYSCLK(i, n, p, w, f)				\
static const struct davinci_pll_sysclk_info n = {	\
	.name		= #n,				\
	.parent_name	= #p,				\
	.id		= (i),				\
	.ratio_width	= (w),				\
	.flags		= (f),				\
}

 
struct davinci_pll_obsclk_info {
	const char *name;
	const char * const *parent_names;
	u8 num_parents;
	u32 *table;
	u32 ocsrc_mask;
};

struct clk *davinci_pll_clk_register(struct device *dev,
				     const struct davinci_pll_clk_info *info,
				     const char *parent_name,
				     void __iomem *base,
				     struct regmap *cfgchip);
struct clk *davinci_pll_auxclk_register(struct device *dev,
					const char *name,
					void __iomem *base);
struct clk *davinci_pll_sysclkbp_clk_register(struct device *dev,
					      const char *name,
					      void __iomem *base);
struct clk *
davinci_pll_obsclk_register(struct device *dev,
			    const struct davinci_pll_obsclk_info *info,
			    void __iomem *base);
struct clk *
davinci_pll_sysclk_register(struct device *dev,
			    const struct davinci_pll_sysclk_info *info,
			    void __iomem *base);

int of_davinci_pll_init(struct device *dev, struct device_node *node,
			const struct davinci_pll_clk_info *info,
			const struct davinci_pll_obsclk_info *obsclk_info,
			const struct davinci_pll_sysclk_info **div_info,
			u8 max_sysclk_id,
			void __iomem *base,
			struct regmap *cfgchip);

 

int da850_pll1_init(struct device *dev, void __iomem *base, struct regmap *cfgchip);
void of_da850_pll0_init(struct device_node *node);
int of_da850_pll1_init(struct device *dev, void __iomem *base, struct regmap *cfgchip);

#endif  
