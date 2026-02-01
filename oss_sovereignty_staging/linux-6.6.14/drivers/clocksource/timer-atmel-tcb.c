
#include <linux/init.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sched_clock.h>
#include <linux/syscore_ops.h>
#include <soc/at91/atmel_tcb.h>


 

static void __iomem *tcaddr;
static struct
{
	u32 cmr;
	u32 imr;
	u32 rc;
	bool clken;
} tcb_cache[3];
static u32 bmr_cache;

static const u8 atmel_tcb_divisors[] = { 2, 8, 32, 128 };

static u64 tc_get_cycles(struct clocksource *cs)
{
	unsigned long	flags;
	u32		lower, upper;

	raw_local_irq_save(flags);
	do {
		upper = readl_relaxed(tcaddr + ATMEL_TC_REG(1, CV));
		lower = readl_relaxed(tcaddr + ATMEL_TC_REG(0, CV));
	} while (upper != readl_relaxed(tcaddr + ATMEL_TC_REG(1, CV)));

	raw_local_irq_restore(flags);
	return (upper << 16) | lower;
}

static u64 tc_get_cycles32(struct clocksource *cs)
{
	return readl_relaxed(tcaddr + ATMEL_TC_REG(0, CV));
}

static void tc_clksrc_suspend(struct clocksource *cs)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tcb_cache); i++) {
		tcb_cache[i].cmr = readl(tcaddr + ATMEL_TC_REG(i, CMR));
		tcb_cache[i].imr = readl(tcaddr + ATMEL_TC_REG(i, IMR));
		tcb_cache[i].rc = readl(tcaddr + ATMEL_TC_REG(i, RC));
		tcb_cache[i].clken = !!(readl(tcaddr + ATMEL_TC_REG(i, SR)) &
					ATMEL_TC_CLKSTA);
	}

	bmr_cache = readl(tcaddr + ATMEL_TC_BMR);
}

static void tc_clksrc_resume(struct clocksource *cs)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(tcb_cache); i++) {
		 
		writel(tcb_cache[i].cmr, tcaddr + ATMEL_TC_REG(i, CMR));
		writel(tcb_cache[i].rc, tcaddr + ATMEL_TC_REG(i, RC));
		writel(0, tcaddr + ATMEL_TC_REG(i, RA));
		writel(0, tcaddr + ATMEL_TC_REG(i, RB));
		 
		writel(0xff, tcaddr + ATMEL_TC_REG(i, IDR));
		 
		writel(tcb_cache[i].imr, tcaddr + ATMEL_TC_REG(i, IER));
		 
		if (tcb_cache[i].clken)
			writel(ATMEL_TC_CLKEN, tcaddr + ATMEL_TC_REG(i, CCR));
	}

	 
	writel(bmr_cache, tcaddr + ATMEL_TC_BMR);
	 
	writel(ATMEL_TC_SYNC, tcaddr + ATMEL_TC_BCR);
}

static struct clocksource clksrc = {
	.rating         = 200,
	.read           = tc_get_cycles,
	.mask           = CLOCKSOURCE_MASK(32),
	.flags		= CLOCK_SOURCE_IS_CONTINUOUS,
	.suspend	= tc_clksrc_suspend,
	.resume		= tc_clksrc_resume,
};

static u64 notrace tc_sched_clock_read(void)
{
	return tc_get_cycles(&clksrc);
}

static u64 notrace tc_sched_clock_read32(void)
{
	return tc_get_cycles32(&clksrc);
}

static struct delay_timer tc_delay_timer;

static unsigned long tc_delay_timer_read(void)
{
	return tc_get_cycles(&clksrc);
}

static unsigned long notrace tc_delay_timer_read32(void)
{
	return tc_get_cycles32(&clksrc);
}

#ifdef CONFIG_GENERIC_CLOCKEVENTS

struct tc_clkevt_device {
	struct clock_event_device	clkevt;
	struct clk			*clk;
	u32				rate;
	void __iomem			*regs;
};

static struct tc_clkevt_device *to_tc_clkevt(struct clock_event_device *clkevt)
{
	return container_of(clkevt, struct tc_clkevt_device, clkevt);
}

static u32 timer_clock;

static int tc_shutdown(struct clock_event_device *d)
{
	struct tc_clkevt_device *tcd = to_tc_clkevt(d);
	void __iomem		*regs = tcd->regs;

	writel(0xff, regs + ATMEL_TC_REG(2, IDR));
	writel(ATMEL_TC_CLKDIS, regs + ATMEL_TC_REG(2, CCR));
	if (!clockevent_state_detached(d))
		clk_disable(tcd->clk);

	return 0;
}

static int tc_set_oneshot(struct clock_event_device *d)
{
	struct tc_clkevt_device *tcd = to_tc_clkevt(d);
	void __iomem		*regs = tcd->regs;

	if (clockevent_state_oneshot(d) || clockevent_state_periodic(d))
		tc_shutdown(d);

	clk_enable(tcd->clk);

	 
	writel(timer_clock | ATMEL_TC_CPCSTOP | ATMEL_TC_WAVE |
		     ATMEL_TC_WAVESEL_UP_AUTO, regs + ATMEL_TC_REG(2, CMR));
	writel(ATMEL_TC_CPCS, regs + ATMEL_TC_REG(2, IER));

	 
	return 0;
}

static int tc_set_periodic(struct clock_event_device *d)
{
	struct tc_clkevt_device *tcd = to_tc_clkevt(d);
	void __iomem		*regs = tcd->regs;

	if (clockevent_state_oneshot(d) || clockevent_state_periodic(d))
		tc_shutdown(d);

	 
	clk_enable(tcd->clk);

	 
	writel(timer_clock | ATMEL_TC_WAVE | ATMEL_TC_WAVESEL_UP_AUTO,
		     regs + ATMEL_TC_REG(2, CMR));
	writel((tcd->rate + HZ / 2) / HZ, tcaddr + ATMEL_TC_REG(2, RC));

	 
	writel(ATMEL_TC_CPCS, regs + ATMEL_TC_REG(2, IER));

	 
	writel(ATMEL_TC_CLKEN | ATMEL_TC_SWTRG, regs +
		     ATMEL_TC_REG(2, CCR));
	return 0;
}

static int tc_next_event(unsigned long delta, struct clock_event_device *d)
{
	writel_relaxed(delta, tcaddr + ATMEL_TC_REG(2, RC));

	 
	writel_relaxed(ATMEL_TC_CLKEN | ATMEL_TC_SWTRG,
			tcaddr + ATMEL_TC_REG(2, CCR));
	return 0;
}

static struct tc_clkevt_device clkevt = {
	.clkevt	= {
		.features		= CLOCK_EVT_FEAT_PERIODIC |
					  CLOCK_EVT_FEAT_ONESHOT,
		 
		.rating			= 125,
		.set_next_event		= tc_next_event,
		.set_state_shutdown	= tc_shutdown,
		.set_state_periodic	= tc_set_periodic,
		.set_state_oneshot	= tc_set_oneshot,
	},
};

static irqreturn_t ch2_irq(int irq, void *handle)
{
	struct tc_clkevt_device	*dev = handle;
	unsigned int		sr;

	sr = readl_relaxed(dev->regs + ATMEL_TC_REG(2, SR));
	if (sr & ATMEL_TC_CPCS) {
		dev->clkevt.event_handler(&dev->clkevt);
		return IRQ_HANDLED;
	}

	return IRQ_NONE;
}

static int __init setup_clkevents(struct atmel_tc *tc, int divisor_idx)
{
	int ret;
	struct clk *t2_clk = tc->clk[2];
	int irq = tc->irq[2];
	int bits = tc->tcb_config->counter_width;

	 
	ret = clk_prepare_enable(t2_clk);
	if (ret)
		return ret;

	clkevt.regs = tc->regs;
	clkevt.clk = t2_clk;

	if (bits == 32) {
		timer_clock = divisor_idx;
		clkevt.rate = clk_get_rate(t2_clk) / atmel_tcb_divisors[divisor_idx];
	} else {
		ret = clk_prepare_enable(tc->slow_clk);
		if (ret) {
			clk_disable_unprepare(t2_clk);
			return ret;
		}

		clkevt.rate = clk_get_rate(tc->slow_clk);
		timer_clock = ATMEL_TC_TIMER_CLOCK5;
	}

	clk_disable(t2_clk);

	clkevt.clkevt.cpumask = cpumask_of(0);

	ret = request_irq(irq, ch2_irq, IRQF_TIMER, "tc_clkevt", &clkevt);
	if (ret) {
		clk_unprepare(t2_clk);
		if (bits != 32)
			clk_disable_unprepare(tc->slow_clk);
		return ret;
	}

	clockevents_config_and_register(&clkevt.clkevt, clkevt.rate, 1, BIT(bits) - 1);

	return ret;
}

#else  

static int __init setup_clkevents(struct atmel_tc *tc, int divisor_idx)
{
	 
	return 0;
}

#endif

static void __init tcb_setup_dual_chan(struct atmel_tc *tc, int mck_divisor_idx)
{
	 
	writel(mck_divisor_idx			 
			| ATMEL_TC_WAVE
			| ATMEL_TC_WAVESEL_UP		 
			| ATMEL_TC_ASWTRG_SET		 
			| ATMEL_TC_ACPA_SET		 
			| ATMEL_TC_ACPC_CLEAR,		 
			tcaddr + ATMEL_TC_REG(0, CMR));
	writel(0x0000, tcaddr + ATMEL_TC_REG(0, RA));
	writel(0x8000, tcaddr + ATMEL_TC_REG(0, RC));
	writel(0xff, tcaddr + ATMEL_TC_REG(0, IDR));	 
	writel(ATMEL_TC_CLKEN, tcaddr + ATMEL_TC_REG(0, CCR));

	 
	writel(ATMEL_TC_XC1			 
			| ATMEL_TC_WAVE
			| ATMEL_TC_WAVESEL_UP,		 
			tcaddr + ATMEL_TC_REG(1, CMR));
	writel(0xff, tcaddr + ATMEL_TC_REG(1, IDR));	 
	writel(ATMEL_TC_CLKEN, tcaddr + ATMEL_TC_REG(1, CCR));

	 
	writel(ATMEL_TC_TC1XC1S_TIOA0, tcaddr + ATMEL_TC_BMR);
	 
	writel(ATMEL_TC_SYNC, tcaddr + ATMEL_TC_BCR);
}

static void __init tcb_setup_single_chan(struct atmel_tc *tc, int mck_divisor_idx)
{
	 
	writel(mck_divisor_idx			 
			| ATMEL_TC_WAVE
			| ATMEL_TC_WAVESEL_UP,		 
			tcaddr + ATMEL_TC_REG(0, CMR));
	writel(0xff, tcaddr + ATMEL_TC_REG(0, IDR));	 
	writel(ATMEL_TC_CLKEN, tcaddr + ATMEL_TC_REG(0, CCR));

	 
	writel(ATMEL_TC_SYNC, tcaddr + ATMEL_TC_BCR);
}

static struct atmel_tcb_config tcb_rm9200_config = {
	.counter_width = 16,
};

static struct atmel_tcb_config tcb_sam9x5_config = {
	.counter_width = 32,
};

static struct atmel_tcb_config tcb_sama5d2_config = {
	.counter_width = 32,
	.has_gclk = 1,
};

static const struct of_device_id atmel_tcb_of_match[] = {
	{ .compatible = "atmel,at91rm9200-tcb", .data = &tcb_rm9200_config, },
	{ .compatible = "atmel,at91sam9x5-tcb", .data = &tcb_sam9x5_config, },
	{ .compatible = "atmel,sama5d2-tcb", .data = &tcb_sama5d2_config, },
	{   }
};

static int __init tcb_clksrc_init(struct device_node *node)
{
	struct atmel_tc tc;
	struct clk *t0_clk;
	const struct of_device_id *match;
	u64 (*tc_sched_clock)(void);
	u32 rate, divided_rate = 0;
	int best_divisor_idx = -1;
	int bits;
	int i;
	int ret;

	 
	if (tcaddr)
		return 0;

	tc.regs = of_iomap(node->parent, 0);
	if (!tc.regs)
		return -ENXIO;

	t0_clk = of_clk_get_by_name(node->parent, "t0_clk");
	if (IS_ERR(t0_clk))
		return PTR_ERR(t0_clk);

	tc.slow_clk = of_clk_get_by_name(node->parent, "slow_clk");
	if (IS_ERR(tc.slow_clk))
		return PTR_ERR(tc.slow_clk);

	tc.clk[0] = t0_clk;
	tc.clk[1] = of_clk_get_by_name(node->parent, "t1_clk");
	if (IS_ERR(tc.clk[1]))
		tc.clk[1] = t0_clk;
	tc.clk[2] = of_clk_get_by_name(node->parent, "t2_clk");
	if (IS_ERR(tc.clk[2]))
		tc.clk[2] = t0_clk;

	tc.irq[2] = of_irq_get(node->parent, 2);
	if (tc.irq[2] <= 0) {
		tc.irq[2] = of_irq_get(node->parent, 0);
		if (tc.irq[2] <= 0)
			return -EINVAL;
	}

	match = of_match_node(atmel_tcb_of_match, node->parent);
	if (!match)
		return -ENODEV;

	tc.tcb_config = match->data;
	bits = tc.tcb_config->counter_width;

	for (i = 0; i < ARRAY_SIZE(tc.irq); i++)
		writel(ATMEL_TC_ALL_IRQ, tc.regs + ATMEL_TC_REG(i, IDR));

	ret = clk_prepare_enable(t0_clk);
	if (ret) {
		pr_debug("can't enable T0 clk\n");
		return ret;
	}

	 
	rate = (u32) clk_get_rate(t0_clk);
	i = 0;
	if (tc.tcb_config->has_gclk)
		i = 1;
	for (; i < ARRAY_SIZE(atmel_tcb_divisors); i++) {
		unsigned divisor = atmel_tcb_divisors[i];
		unsigned tmp;

		tmp = rate / divisor;
		pr_debug("TC: %u / %-3u [%d] --> %u\n", rate, divisor, i, tmp);
		if ((best_divisor_idx >= 0) && (tmp < 5 * 1000 * 1000))
			break;
		divided_rate = tmp;
		best_divisor_idx = i;
	}

	clksrc.name = kbasename(node->parent->full_name);
	clkevt.clkevt.name = kbasename(node->parent->full_name);
	pr_debug("%s at %d.%03d MHz\n", clksrc.name, divided_rate / 1000000,
			((divided_rate % 1000000) + 500) / 1000);

	tcaddr = tc.regs;

	if (bits == 32) {
		 
		clksrc.read = tc_get_cycles32;
		 
		tcb_setup_single_chan(&tc, best_divisor_idx);
		tc_sched_clock = tc_sched_clock_read32;
		tc_delay_timer.read_current_timer = tc_delay_timer_read32;
	} else {
		 
		ret = clk_prepare_enable(tc.clk[1]);
		if (ret) {
			pr_debug("can't enable T1 clk\n");
			goto err_disable_t0;
		}
		 
		tcb_setup_dual_chan(&tc, best_divisor_idx);
		tc_sched_clock = tc_sched_clock_read;
		tc_delay_timer.read_current_timer = tc_delay_timer_read;
	}

	 
	ret = clocksource_register_hz(&clksrc, divided_rate);
	if (ret)
		goto err_disable_t1;

	 
	ret = setup_clkevents(&tc, best_divisor_idx);
	if (ret)
		goto err_unregister_clksrc;

	sched_clock_register(tc_sched_clock, 32, divided_rate);

	tc_delay_timer.freq = divided_rate;
	register_current_timer_delay(&tc_delay_timer);

	return 0;

err_unregister_clksrc:
	clocksource_unregister(&clksrc);

err_disable_t1:
	if (bits != 32)
		clk_disable_unprepare(tc.clk[1]);

err_disable_t0:
	clk_disable_unprepare(t0_clk);

	tcaddr = NULL;

	return ret;
}
TIMER_OF_DECLARE(atmel_tcb_clksrc, "atmel,tcb-timer", tcb_clksrc_init);
