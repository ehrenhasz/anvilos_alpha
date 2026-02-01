
 

#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include "sifive-prci.h"
#include "fu540-prci.h"
#include "fu740-prci.h"

 

 
static u32 __prci_readl(struct __prci_data *pd, u32 offs)
{
	return readl_relaxed(pd->va + offs);
}

static void __prci_writel(u32 v, u32 offs, struct __prci_data *pd)
{
	writel_relaxed(v, pd->va + offs);
}

 

 
static void __prci_wrpll_unpack(struct wrpll_cfg *c, u32 r)
{
	u32 v;

	v = r & PRCI_COREPLLCFG0_DIVR_MASK;
	v >>= PRCI_COREPLLCFG0_DIVR_SHIFT;
	c->divr = v;

	v = r & PRCI_COREPLLCFG0_DIVF_MASK;
	v >>= PRCI_COREPLLCFG0_DIVF_SHIFT;
	c->divf = v;

	v = r & PRCI_COREPLLCFG0_DIVQ_MASK;
	v >>= PRCI_COREPLLCFG0_DIVQ_SHIFT;
	c->divq = v;

	v = r & PRCI_COREPLLCFG0_RANGE_MASK;
	v >>= PRCI_COREPLLCFG0_RANGE_SHIFT;
	c->range = v;

	c->flags &=
	    (WRPLL_FLAGS_INT_FEEDBACK_MASK | WRPLL_FLAGS_EXT_FEEDBACK_MASK);

	 
	c->flags |= WRPLL_FLAGS_INT_FEEDBACK_MASK;
}

 
static u32 __prci_wrpll_pack(const struct wrpll_cfg *c)
{
	u32 r = 0;

	r |= c->divr << PRCI_COREPLLCFG0_DIVR_SHIFT;
	r |= c->divf << PRCI_COREPLLCFG0_DIVF_SHIFT;
	r |= c->divq << PRCI_COREPLLCFG0_DIVQ_SHIFT;
	r |= c->range << PRCI_COREPLLCFG0_RANGE_SHIFT;

	 
	r |= PRCI_COREPLLCFG0_FSE_MASK;

	return r;
}

 
static void __prci_wrpll_read_cfg0(struct __prci_data *pd,
				   struct __prci_wrpll_data *pwd)
{
	__prci_wrpll_unpack(&pwd->c, __prci_readl(pd, pwd->cfg0_offs));
}

 
static void __prci_wrpll_write_cfg0(struct __prci_data *pd,
				    struct __prci_wrpll_data *pwd,
				    struct wrpll_cfg *c)
{
	__prci_writel(__prci_wrpll_pack(c), pwd->cfg0_offs, pd);

	memcpy(&pwd->c, c, sizeof(*c));
}

 
static void __prci_wrpll_write_cfg1(struct __prci_data *pd,
				    struct __prci_wrpll_data *pwd,
				    u32 enable)
{
	__prci_writel(enable, pwd->cfg1_offs, pd);
}

 

unsigned long sifive_prci_wrpll_recalc_rate(struct clk_hw *hw,
					    unsigned long parent_rate)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_wrpll_data *pwd = pc->pwd;

	return wrpll_calc_output_rate(&pwd->c, parent_rate);
}

long sifive_prci_wrpll_round_rate(struct clk_hw *hw,
				  unsigned long rate,
				  unsigned long *parent_rate)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_wrpll_data *pwd = pc->pwd;
	struct wrpll_cfg c;

	memcpy(&c, &pwd->c, sizeof(c));

	wrpll_configure_for_rate(&c, rate, *parent_rate);

	return wrpll_calc_output_rate(&c, *parent_rate);
}

int sifive_prci_wrpll_set_rate(struct clk_hw *hw,
			       unsigned long rate, unsigned long parent_rate)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_wrpll_data *pwd = pc->pwd;
	struct __prci_data *pd = pc->pd;
	int r;

	r = wrpll_configure_for_rate(&pwd->c, rate, parent_rate);
	if (r)
		return r;

	if (pwd->enable_bypass)
		pwd->enable_bypass(pd);

	__prci_wrpll_write_cfg0(pd, pwd, &pwd->c);

	udelay(wrpll_calc_max_lock_us(&pwd->c));

	return 0;
}

int sifive_clk_is_enabled(struct clk_hw *hw)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_wrpll_data *pwd = pc->pwd;
	struct __prci_data *pd = pc->pd;
	u32 r;

	r = __prci_readl(pd, pwd->cfg1_offs);

	if (r & PRCI_COREPLLCFG1_CKE_MASK)
		return 1;
	else
		return 0;
}

int sifive_prci_clock_enable(struct clk_hw *hw)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_wrpll_data *pwd = pc->pwd;
	struct __prci_data *pd = pc->pd;

	if (sifive_clk_is_enabled(hw))
		return 0;

	__prci_wrpll_write_cfg1(pd, pwd, PRCI_COREPLLCFG1_CKE_MASK);

	if (pwd->disable_bypass)
		pwd->disable_bypass(pd);

	return 0;
}

void sifive_prci_clock_disable(struct clk_hw *hw)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_wrpll_data *pwd = pc->pwd;
	struct __prci_data *pd = pc->pd;
	u32 r;

	if (pwd->enable_bypass)
		pwd->enable_bypass(pd);

	r = __prci_readl(pd, pwd->cfg1_offs);
	r &= ~PRCI_COREPLLCFG1_CKE_MASK;

	__prci_wrpll_write_cfg1(pd, pwd, r);
}

 

unsigned long sifive_prci_tlclksel_recalc_rate(struct clk_hw *hw,
					       unsigned long parent_rate)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_data *pd = pc->pd;
	u32 v;
	u8 div;

	v = __prci_readl(pd, PRCI_CLKMUXSTATUSREG_OFFSET);
	v &= PRCI_CLKMUXSTATUSREG_TLCLKSEL_STATUS_MASK;
	div = v ? 1 : 2;

	return div_u64(parent_rate, div);
}

 

unsigned long sifive_prci_hfpclkplldiv_recalc_rate(struct clk_hw *hw,
						   unsigned long parent_rate)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_data *pd = pc->pd;
	u32 div = __prci_readl(pd, PRCI_HFPCLKPLLDIV_OFFSET);

	return div_u64(parent_rate, div + 2);
}

 

 
void sifive_prci_coreclksel_use_hfclk(struct __prci_data *pd)
{
	u32 r;

	r = __prci_readl(pd, PRCI_CORECLKSEL_OFFSET);
	r |= PRCI_CORECLKSEL_CORECLKSEL_MASK;
	__prci_writel(r, PRCI_CORECLKSEL_OFFSET, pd);

	r = __prci_readl(pd, PRCI_CORECLKSEL_OFFSET);	 
}

 
void sifive_prci_coreclksel_use_corepll(struct __prci_data *pd)
{
	u32 r;

	r = __prci_readl(pd, PRCI_CORECLKSEL_OFFSET);
	r &= ~PRCI_CORECLKSEL_CORECLKSEL_MASK;
	__prci_writel(r, PRCI_CORECLKSEL_OFFSET, pd);

	r = __prci_readl(pd, PRCI_CORECLKSEL_OFFSET);	 
}

 
void sifive_prci_coreclksel_use_final_corepll(struct __prci_data *pd)
{
	u32 r;

	r = __prci_readl(pd, PRCI_CORECLKSEL_OFFSET);
	r &= ~PRCI_CORECLKSEL_CORECLKSEL_MASK;
	__prci_writel(r, PRCI_CORECLKSEL_OFFSET, pd);

	r = __prci_readl(pd, PRCI_CORECLKSEL_OFFSET);	 
}

 
void sifive_prci_corepllsel_use_dvfscorepll(struct __prci_data *pd)
{
	u32 r;

	r = __prci_readl(pd, PRCI_COREPLLSEL_OFFSET);
	r |= PRCI_COREPLLSEL_COREPLLSEL_MASK;
	__prci_writel(r, PRCI_COREPLLSEL_OFFSET, pd);

	r = __prci_readl(pd, PRCI_COREPLLSEL_OFFSET);	 
}

 
void sifive_prci_corepllsel_use_corepll(struct __prci_data *pd)
{
	u32 r;

	r = __prci_readl(pd, PRCI_COREPLLSEL_OFFSET);
	r &= ~PRCI_COREPLLSEL_COREPLLSEL_MASK;
	__prci_writel(r, PRCI_COREPLLSEL_OFFSET, pd);

	r = __prci_readl(pd, PRCI_COREPLLSEL_OFFSET);	 
}

 
void sifive_prci_hfpclkpllsel_use_hfclk(struct __prci_data *pd)
{
	u32 r;

	r = __prci_readl(pd, PRCI_HFPCLKPLLSEL_OFFSET);
	r |= PRCI_HFPCLKPLLSEL_HFPCLKPLLSEL_MASK;
	__prci_writel(r, PRCI_HFPCLKPLLSEL_OFFSET, pd);

	r = __prci_readl(pd, PRCI_HFPCLKPLLSEL_OFFSET);	 
}

 
void sifive_prci_hfpclkpllsel_use_hfpclkpll(struct __prci_data *pd)
{
	u32 r;

	r = __prci_readl(pd, PRCI_HFPCLKPLLSEL_OFFSET);
	r &= ~PRCI_HFPCLKPLLSEL_HFPCLKPLLSEL_MASK;
	__prci_writel(r, PRCI_HFPCLKPLLSEL_OFFSET, pd);

	r = __prci_readl(pd, PRCI_HFPCLKPLLSEL_OFFSET);	 
}

 
int sifive_prci_pcie_aux_clock_is_enabled(struct clk_hw *hw)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_data *pd = pc->pd;
	u32 r;

	r = __prci_readl(pd, PRCI_PCIE_AUX_OFFSET);

	if (r & PRCI_PCIE_AUX_EN_MASK)
		return 1;
	else
		return 0;
}

int sifive_prci_pcie_aux_clock_enable(struct clk_hw *hw)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_data *pd = pc->pd;
	u32 r __maybe_unused;

	if (sifive_prci_pcie_aux_clock_is_enabled(hw))
		return 0;

	__prci_writel(1, PRCI_PCIE_AUX_OFFSET, pd);
	r = __prci_readl(pd, PRCI_PCIE_AUX_OFFSET);	 

	return 0;
}

void sifive_prci_pcie_aux_clock_disable(struct clk_hw *hw)
{
	struct __prci_clock *pc = clk_hw_to_prci_clock(hw);
	struct __prci_data *pd = pc->pd;
	u32 r __maybe_unused;

	__prci_writel(0, PRCI_PCIE_AUX_OFFSET, pd);
	r = __prci_readl(pd, PRCI_PCIE_AUX_OFFSET);	 

}

 
static int __prci_register_clocks(struct device *dev, struct __prci_data *pd,
				  const struct prci_clk_desc *desc)
{
	struct clk_init_data init = { };
	struct __prci_clock *pic;
	int parent_count, i, r;

	parent_count = of_clk_get_parent_count(dev->of_node);
	if (parent_count != EXPECTED_CLK_PARENT_COUNT) {
		dev_err(dev, "expected only two parent clocks, found %d\n",
			parent_count);
		return -EINVAL;
	}

	 
	for (i = 0; i < desc->num_clks; ++i) {
		pic = &(desc->clks[i]);

		init.name = pic->name;
		init.parent_names = &pic->parent_name;
		init.num_parents = 1;
		init.ops = pic->ops;
		pic->hw.init = &init;

		pic->pd = pd;

		if (pic->pwd)
			__prci_wrpll_read_cfg0(pd, pic->pwd);

		r = devm_clk_hw_register(dev, &pic->hw);
		if (r) {
			dev_warn(dev, "Failed to register clock %s: %d\n",
				 init.name, r);
			return r;
		}

		r = clk_hw_register_clkdev(&pic->hw, pic->name, dev_name(dev));
		if (r) {
			dev_warn(dev, "Failed to register clkdev for %s: %d\n",
				 init.name, r);
			return r;
		}

		pd->hw_clks.hws[i] = &pic->hw;
	}

	pd->hw_clks.num = i;

	r = devm_of_clk_add_hw_provider(dev, of_clk_hw_onecell_get,
					&pd->hw_clks);
	if (r) {
		dev_err(dev, "could not add hw_provider: %d\n", r);
		return r;
	}

	return 0;
}

 
static int sifive_prci_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct __prci_data *pd;
	const struct prci_clk_desc *desc;
	int r;

	desc = of_device_get_match_data(&pdev->dev);

	pd = devm_kzalloc(dev, struct_size(pd, hw_clks.hws, desc->num_clks), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;

	pd->va = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(pd->va))
		return PTR_ERR(pd->va);

	pd->reset.rcdev.owner = THIS_MODULE;
	pd->reset.rcdev.nr_resets = PRCI_RST_NR;
	pd->reset.rcdev.ops = &reset_simple_ops;
	pd->reset.rcdev.of_node = pdev->dev.of_node;
	pd->reset.active_low = true;
	pd->reset.membase = pd->va + PRCI_DEVICESRESETREG_OFFSET;
	spin_lock_init(&pd->reset.lock);

	r = devm_reset_controller_register(&pdev->dev, &pd->reset.rcdev);
	if (r) {
		dev_err(dev, "could not register reset controller: %d\n", r);
		return r;
	}
	r = __prci_register_clocks(dev, pd, desc);
	if (r) {
		dev_err(dev, "could not register clocks: %d\n", r);
		return r;
	}

	dev_dbg(dev, "SiFive PRCI probed\n");

	return 0;
}

static const struct of_device_id sifive_prci_of_match[] = {
	{.compatible = "sifive,fu540-c000-prci", .data = &prci_clk_fu540},
	{.compatible = "sifive,fu740-c000-prci", .data = &prci_clk_fu740},
	{}
};

static struct platform_driver sifive_prci_driver = {
	.driver = {
		.name = "sifive-clk-prci",
		.of_match_table = sifive_prci_of_match,
	},
	.probe = sifive_prci_probe,
};

static int __init sifive_prci_init(void)
{
	return platform_driver_register(&sifive_prci_driver);
}
core_initcall(sifive_prci_init);
