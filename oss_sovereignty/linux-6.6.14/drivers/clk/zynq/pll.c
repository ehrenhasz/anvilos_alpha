
 
#include <linux/clk/zynq.h>
#include <linux/clk-provider.h>
#include <linux/slab.h>
#include <linux/io.h>

 
struct zynq_pll {
	struct clk_hw	hw;
	void __iomem	*pll_ctrl;
	void __iomem	*pll_status;
	spinlock_t	*lock;
	u8		lockbit;
};
#define to_zynq_pll(_hw)	container_of(_hw, struct zynq_pll, hw)

 
#define PLLCTRL_FBDIV_MASK	0x7f000
#define PLLCTRL_FBDIV_SHIFT	12
#define PLLCTRL_BPQUAL_MASK	(1 << 3)
#define PLLCTRL_PWRDWN_MASK	2
#define PLLCTRL_PWRDWN_SHIFT	1
#define PLLCTRL_RESET_MASK	1
#define PLLCTRL_RESET_SHIFT	0

#define PLL_FBDIV_MIN	13
#define PLL_FBDIV_MAX	66

 
static long zynq_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *prate)
{
	u32 fbdiv;

	fbdiv = DIV_ROUND_CLOSEST(rate, *prate);
	if (fbdiv < PLL_FBDIV_MIN)
		fbdiv = PLL_FBDIV_MIN;
	else if (fbdiv > PLL_FBDIV_MAX)
		fbdiv = PLL_FBDIV_MAX;

	return *prate * fbdiv;
}

 
static unsigned long zynq_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct zynq_pll *clk = to_zynq_pll(hw);
	u32 fbdiv;

	 
	fbdiv = (readl(clk->pll_ctrl) & PLLCTRL_FBDIV_MASK) >>
			PLLCTRL_FBDIV_SHIFT;

	return parent_rate * fbdiv;
}

 
static int zynq_pll_is_enabled(struct clk_hw *hw)
{
	unsigned long flags = 0;
	u32 reg;
	struct zynq_pll *clk = to_zynq_pll(hw);

	spin_lock_irqsave(clk->lock, flags);

	reg = readl(clk->pll_ctrl);

	spin_unlock_irqrestore(clk->lock, flags);

	return !(reg & (PLLCTRL_RESET_MASK | PLLCTRL_PWRDWN_MASK));
}

 
static int zynq_pll_enable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	u32 reg;
	struct zynq_pll *clk = to_zynq_pll(hw);

	if (zynq_pll_is_enabled(hw))
		return 0;

	pr_info("PLL: enable\n");

	 
	spin_lock_irqsave(clk->lock, flags);

	reg = readl(clk->pll_ctrl);
	reg &= ~(PLLCTRL_RESET_MASK | PLLCTRL_PWRDWN_MASK);
	writel(reg, clk->pll_ctrl);
	while (!(readl(clk->pll_status) & (1 << clk->lockbit)))
		;

	spin_unlock_irqrestore(clk->lock, flags);

	return 0;
}

 
static void zynq_pll_disable(struct clk_hw *hw)
{
	unsigned long flags = 0;
	u32 reg;
	struct zynq_pll *clk = to_zynq_pll(hw);

	if (!zynq_pll_is_enabled(hw))
		return;

	pr_info("PLL: shutdown\n");

	 
	spin_lock_irqsave(clk->lock, flags);

	reg = readl(clk->pll_ctrl);
	reg |= PLLCTRL_RESET_MASK | PLLCTRL_PWRDWN_MASK;
	writel(reg, clk->pll_ctrl);

	spin_unlock_irqrestore(clk->lock, flags);
}

static const struct clk_ops zynq_pll_ops = {
	.enable = zynq_pll_enable,
	.disable = zynq_pll_disable,
	.is_enabled = zynq_pll_is_enabled,
	.round_rate = zynq_pll_round_rate,
	.recalc_rate = zynq_pll_recalc_rate
};

 
struct clk *clk_register_zynq_pll(const char *name, const char *parent,
		void __iomem *pll_ctrl, void __iomem *pll_status, u8 lock_index,
		spinlock_t *lock)
{
	struct zynq_pll *pll;
	struct clk *clk;
	u32 reg;
	const char *parent_arr[1] = {parent};
	unsigned long flags = 0;
	struct clk_init_data initd = {
		.name = name,
		.parent_names = parent_arr,
		.ops = &zynq_pll_ops,
		.num_parents = 1,
		.flags = 0
	};

	pll = kmalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	 
	pll->hw.init = &initd;
	pll->pll_ctrl = pll_ctrl;
	pll->pll_status = pll_status;
	pll->lockbit = lock_index;
	pll->lock = lock;

	spin_lock_irqsave(pll->lock, flags);

	reg = readl(pll->pll_ctrl);
	reg &= ~PLLCTRL_BPQUAL_MASK;
	writel(reg, pll->pll_ctrl);

	spin_unlock_irqrestore(pll->lock, flags);

	clk = clk_register(NULL, &pll->hw);
	if (WARN_ON(IS_ERR(clk)))
		goto free_pll;

	return clk;

free_pll:
	kfree(pll);

	return clk;
}
