#ifndef __DRV_CLK_CPUMUX_H
#define __DRV_CLK_CPUMUX_H
struct clk_hw_onecell_data;
struct device_node;
struct mtk_composite;
int mtk_clk_register_cpumuxes(struct device *dev, struct device_node *node,
			      const struct mtk_composite *clks, int num,
			      struct clk_hw_onecell_data *clk_data);
void mtk_clk_unregister_cpumuxes(const struct mtk_composite *clks, int num,
				 struct clk_hw_onecell_data *clk_data);
#endif  
