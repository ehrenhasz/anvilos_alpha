
#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/cpuhotplug.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/err.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/sched_clock.h>

#include <linux/clk/clk-conf.h>

#include <clocksource/timer-ti-dm.h>
#include <dt-bindings/bus/ti-sysc.h>

 
#define DMTIMER_TYPE1_ENABLE	((1 << 9) | (SYSC_IDLE_SMART << 3) | \
				 SYSC_OMAP2_ENAWAKEUP | SYSC_OMAP2_AUTOIDLE)
#define DMTIMER_TYPE1_DISABLE	(SYSC_OMAP2_SOFTRESET | SYSC_OMAP2_AUTOIDLE)
#define DMTIMER_TYPE2_ENABLE	(SYSC_IDLE_SMART_WKUP << 2)
#define DMTIMER_RESET_WAIT	100000

#define DMTIMER_INST_DONT_CARE	~0U

static int counter_32k;
static u32 clocksource;
static u32 clockevent;

 
struct dmtimer_systimer {
	void __iomem *base;
	u8 sysc;
	u8 irq_stat;
	u8 irq_ena;
	u8 pend;
	u8 load;
	u8 counter;
	u8 ctrl;
	u8 wakeup;
	u8 ifctrl;
	struct clk *fck;
	struct clk *ick;
	unsigned long rate;
};

struct dmtimer_clockevent {
	struct clock_event_device dev;
	struct dmtimer_systimer t;
	u32 period;
};

struct dmtimer_clocksource {
	struct clocksource dev;
	struct dmtimer_systimer t;
	unsigned int loadval;
};

 
static bool dmtimer_systimer_revision1(struct dmtimer_systimer *t)
{
	u32 tidr = readl_relaxed(t->base);

	return !(tidr >> 16);
}

static void dmtimer_systimer_enable(struct dmtimer_systimer *t)
{
	u32 val;

	if (dmtimer_systimer_revision1(t))
		val = DMTIMER_TYPE1_ENABLE;
	else
		val = DMTIMER_TYPE2_ENABLE;

	writel_relaxed(val, t->base + t->sysc);
}

static void dmtimer_systimer_disable(struct dmtimer_systimer *t)
{
	if (!dmtimer_systimer_revision1(t))
		return;

	writel_relaxed(DMTIMER_TYPE1_DISABLE, t->base + t->sysc);
}

static int __init dmtimer_systimer_type1_reset(struct dmtimer_systimer *t)
{
	void __iomem *syss = t->base + OMAP_TIMER_V1_SYS_STAT_OFFSET;
	int ret;
	u32 l;

	dmtimer_systimer_enable(t);
	writel_relaxed(BIT(1) | BIT(2), t->base + t->ifctrl);
	ret = readl_poll_timeout_atomic(syss, l, l & BIT(0), 100,
					DMTIMER_RESET_WAIT);

	return ret;
}

 
static int __init dmtimer_systimer_type2_reset(struct dmtimer_systimer *t)
{
	void __iomem *sysc = t->base + t->sysc;
	u32 l;

	dmtimer_systimer_enable(t);
	l = readl_relaxed(sysc);
	l |= BIT(0);
	writel_relaxed(l, sysc);

	return readl_poll_timeout_atomic(sysc, l, !(l & BIT(0)), 100,
					 DMTIMER_RESET_WAIT);
}

static int __init dmtimer_systimer_reset(struct dmtimer_systimer *t)
{
	int ret;

	if (dmtimer_systimer_revision1(t))
		ret = dmtimer_systimer_type1_reset(t);
	else
		ret = dmtimer_systimer_type2_reset(t);
	if (ret < 0) {
		pr_err("%s failed with %i\n", __func__, ret);

		return ret;
	}

	return 0;
}

static const struct of_device_id counter_match_table[] = {
	{ .compatible = "ti,omap-counter32k" },
	{   },
};

 
static void __init dmtimer_systimer_check_counter32k(void)
{
	struct device_node *np;

	if (counter_32k)
		return;

	np = of_find_matching_node(NULL, counter_match_table);
	if (!np) {
		counter_32k = -ENODEV;

		return;
	}

	if (of_device_is_available(np))
		counter_32k = 1;
	else
		counter_32k = -ENODEV;

	of_node_put(np);
}

static const struct of_device_id dmtimer_match_table[] = {
	{ .compatible = "ti,omap2420-timer", },
	{ .compatible = "ti,omap3430-timer", },
	{ .compatible = "ti,omap4430-timer", },
	{ .compatible = "ti,omap5430-timer", },
	{ .compatible = "ti,am335x-timer", },
	{ .compatible = "ti,am335x-timer-1ms", },
	{ .compatible = "ti,dm814-timer", },
	{ .compatible = "ti,dm816-timer", },
	{   },
};

 
static bool __init dmtimer_is_preferred(struct device_node *np)
{
	if (!of_device_is_available(np))
		return false;

	if (!of_property_read_bool(np->parent,
				   "ti,no-reset-on-init"))
		return false;

	if (!of_property_read_bool(np->parent, "ti,no-idle"))
		return false;

	 
	if (!of_property_read_bool(np, "ti,timer-secure")) {
		if (!of_property_read_bool(np, "assigned-clocks"))
			return false;

		if (!of_property_read_bool(np, "assigned-clock-parents"))
			return false;
	}

	if (of_property_read_bool(np, "ti,timer-dsp"))
		return false;

	if (of_property_read_bool(np, "ti,timer-pwm"))
		return false;

	return true;
}

 
static void __init dmtimer_systimer_assign_alwon(void)
{
	struct device_node *np;
	u32 pa = 0;
	bool quirk_unreliable_oscillator = false;

	 
	if (of_machine_is_compatible("ti,omap3-beagle-ab4")) {
		quirk_unreliable_oscillator = true;
		counter_32k = -ENODEV;
	}

	 
	if (of_machine_is_compatible("ti,am43"))
		counter_32k = -ENODEV;

	for_each_matching_node(np, dmtimer_match_table) {
		struct resource res;
		if (!dmtimer_is_preferred(np))
			continue;

		if (!of_property_read_bool(np, "ti,timer-alwon"))
			continue;

		if (of_address_to_resource(np, 0, &res))
			continue;

		pa = res.start;

		 
		if (quirk_unreliable_oscillator && pa == 0x48318000)
			continue;

		of_node_put(np);
		break;
	}

	 
	if (counter_32k >= 0) {
		clockevent = pa;
		clocksource = 0;
	} else {
		clocksource = pa;
		clockevent = DMTIMER_INST_DONT_CARE;
	}
}

 
static u32 __init dmtimer_systimer_find_first_available(void)
{
	struct device_node *np;
	u32 pa = 0;

	for_each_matching_node(np, dmtimer_match_table) {
		struct resource res;
		if (!dmtimer_is_preferred(np))
			continue;

		if (of_address_to_resource(np, 0, &res))
			continue;

		if (res.start == clocksource || res.start == clockevent)
			continue;

		pa = res.start;
		of_node_put(np);
		break;
	}

	return pa;
}

 
static void __init dmtimer_systimer_select_best(void)
{
	dmtimer_systimer_check_counter32k();
	dmtimer_systimer_assign_alwon();

	if (clockevent == DMTIMER_INST_DONT_CARE)
		clockevent = dmtimer_systimer_find_first_available();

	pr_debug("%s: counter_32k: %i clocksource: %08x clockevent: %08x\n",
		 __func__, counter_32k, clocksource, clockevent);
}

 
static int __init dmtimer_systimer_init_clock(struct dmtimer_systimer *t,
					      struct device_node *np,
					      const char *name,
					      unsigned long *rate)
{
	struct clk *clock;
	unsigned long r;
	bool is_ick = false;
	int error;

	is_ick = !strncmp(name, "ick", 3);

	clock = of_clk_get_by_name(np, name);
	if ((PTR_ERR(clock) == -EINVAL) && is_ick)
		return 0;
	else if (IS_ERR(clock))
		return PTR_ERR(clock);

	error = clk_prepare_enable(clock);
	if (error)
		return error;

	r = clk_get_rate(clock);
	if (!r) {
		clk_disable_unprepare(clock);
		return -ENODEV;
	}

	if (is_ick)
		t->ick = clock;
	else
		t->fck = clock;

	*rate = r;

	return 0;
}

static int __init dmtimer_systimer_setup(struct device_node *np,
					 struct dmtimer_systimer *t)
{
	unsigned long rate;
	u8 regbase;
	int error;

	if (!of_device_is_compatible(np->parent, "ti,sysc"))
		return -EINVAL;

	t->base = of_iomap(np, 0);
	if (!t->base)
		return -ENXIO;

	 
	error = of_clk_set_defaults(np, false);
	if (error < 0)
		pr_err("%s: clock source init failed: %i\n", __func__, error);

	 
	error = dmtimer_systimer_init_clock(t, np->parent, "fck", &rate);
	if (error)
		goto err_unmap;

	t->rate = rate;

	error = dmtimer_systimer_init_clock(t, np->parent, "ick", &rate);
	if (error)
		goto err_unmap;

	if (dmtimer_systimer_revision1(t)) {
		t->irq_stat = OMAP_TIMER_V1_STAT_OFFSET;
		t->irq_ena = OMAP_TIMER_V1_INT_EN_OFFSET;
		t->pend = _OMAP_TIMER_WRITE_PEND_OFFSET;
		regbase = 0;
	} else {
		t->irq_stat = OMAP_TIMER_V2_IRQSTATUS;
		t->irq_ena = OMAP_TIMER_V2_IRQENABLE_SET;
		regbase = OMAP_TIMER_V2_FUNC_OFFSET;
		t->pend = regbase + _OMAP_TIMER_WRITE_PEND_OFFSET;
	}

	t->sysc = OMAP_TIMER_OCP_CFG_OFFSET;
	t->load = regbase + _OMAP_TIMER_LOAD_OFFSET;
	t->counter = regbase + _OMAP_TIMER_COUNTER_OFFSET;
	t->ctrl = regbase + _OMAP_TIMER_CTRL_OFFSET;
	t->wakeup = regbase + _OMAP_TIMER_WAKEUP_EN_OFFSET;
	t->ifctrl = regbase + _OMAP_TIMER_IF_CTRL_OFFSET;

	dmtimer_systimer_reset(t);
	dmtimer_systimer_enable(t);
	pr_debug("dmtimer rev %08x sysc %08x\n", readl_relaxed(t->base),
		 readl_relaxed(t->base + t->sysc));

	return 0;

err_unmap:
	iounmap(t->base);

	return error;
}

 
static struct dmtimer_clockevent *
to_dmtimer_clockevent(struct clock_event_device *clockevent)
{
	return container_of(clockevent, struct dmtimer_clockevent, dev);
}

static irqreturn_t dmtimer_clockevent_interrupt(int irq, void *data)
{
	struct dmtimer_clockevent *clkevt = data;
	struct dmtimer_systimer *t = &clkevt->t;

	writel_relaxed(OMAP_TIMER_INT_OVERFLOW, t->base + t->irq_stat);
	clkevt->dev.event_handler(&clkevt->dev);

	return IRQ_HANDLED;
}

static int dmtimer_set_next_event(unsigned long cycles,
				  struct clock_event_device *evt)
{
	struct dmtimer_clockevent *clkevt = to_dmtimer_clockevent(evt);
	struct dmtimer_systimer *t = &clkevt->t;
	void __iomem *pend = t->base + t->pend;

	while (readl_relaxed(pend) & WP_TCRR)
		cpu_relax();
	writel_relaxed(0xffffffff - cycles, t->base + t->counter);

	while (readl_relaxed(pend) & WP_TCLR)
		cpu_relax();
	writel_relaxed(OMAP_TIMER_CTRL_ST, t->base + t->ctrl);

	return 0;
}

static int dmtimer_clockevent_shutdown(struct clock_event_device *evt)
{
	struct dmtimer_clockevent *clkevt = to_dmtimer_clockevent(evt);
	struct dmtimer_systimer *t = &clkevt->t;
	void __iomem *ctrl = t->base + t->ctrl;
	u32 l;

	l = readl_relaxed(ctrl);
	if (l & OMAP_TIMER_CTRL_ST) {
		l &= ~BIT(0);
		writel_relaxed(l, ctrl);
		 
		l = readl_relaxed(ctrl);
		 
		udelay(3500000 / t->rate + 1);
	}
	writel_relaxed(OMAP_TIMER_INT_OVERFLOW, t->base + t->irq_stat);

	return 0;
}

static int dmtimer_set_periodic(struct clock_event_device *evt)
{
	struct dmtimer_clockevent *clkevt = to_dmtimer_clockevent(evt);
	struct dmtimer_systimer *t = &clkevt->t;
	void __iomem *pend = t->base + t->pend;

	dmtimer_clockevent_shutdown(evt);

	 
	while (readl_relaxed(pend) & WP_TLDR)
		cpu_relax();
	writel_relaxed(clkevt->period, t->base + t->load);

	while (readl_relaxed(pend) & WP_TCRR)
		cpu_relax();
	writel_relaxed(clkevt->period, t->base + t->counter);

	while (readl_relaxed(pend) & WP_TCLR)
		cpu_relax();
	writel_relaxed(OMAP_TIMER_CTRL_AR | OMAP_TIMER_CTRL_ST,
		       t->base + t->ctrl);

	return 0;
}

static void omap_clockevent_idle(struct clock_event_device *evt)
{
	struct dmtimer_clockevent *clkevt = to_dmtimer_clockevent(evt);
	struct dmtimer_systimer *t = &clkevt->t;

	dmtimer_systimer_disable(t);
	clk_disable(t->fck);
}

static void omap_clockevent_unidle(struct clock_event_device *evt)
{
	struct dmtimer_clockevent *clkevt = to_dmtimer_clockevent(evt);
	struct dmtimer_systimer *t = &clkevt->t;
	int error;

	error = clk_enable(t->fck);
	if (error)
		pr_err("could not enable timer fck on resume: %i\n", error);

	dmtimer_systimer_enable(t);
	writel_relaxed(OMAP_TIMER_INT_OVERFLOW, t->base + t->irq_ena);
	writel_relaxed(OMAP_TIMER_INT_OVERFLOW, t->base + t->wakeup);
}

static int __init dmtimer_clkevt_init_common(struct dmtimer_clockevent *clkevt,
					     struct device_node *np,
					     unsigned int features,
					     const struct cpumask *cpumask,
					     const char *name,
					     int rating)
{
	struct clock_event_device *dev;
	struct dmtimer_systimer *t;
	int error;

	t = &clkevt->t;
	dev = &clkevt->dev;

	 
	dev->features = features;
	dev->rating = rating;
	dev->set_next_event = dmtimer_set_next_event;
	dev->set_state_shutdown = dmtimer_clockevent_shutdown;
	dev->set_state_periodic = dmtimer_set_periodic;
	dev->set_state_oneshot = dmtimer_clockevent_shutdown;
	dev->set_state_oneshot_stopped = dmtimer_clockevent_shutdown;
	dev->tick_resume = dmtimer_clockevent_shutdown;
	dev->cpumask = cpumask;

	dev->irq = irq_of_parse_and_map(np, 0);
	if (!dev->irq)
		return -ENXIO;

	error = dmtimer_systimer_setup(np, &clkevt->t);
	if (error)
		return error;

	clkevt->period = 0xffffffff - DIV_ROUND_CLOSEST(t->rate, HZ);

	 
	writel_relaxed(OMAP_TIMER_CTRL_POSTED, t->base + t->ifctrl);

	error = request_irq(dev->irq, dmtimer_clockevent_interrupt,
			    IRQF_TIMER, name, clkevt);
	if (error)
		goto err_out_unmap;

	writel_relaxed(OMAP_TIMER_INT_OVERFLOW, t->base + t->irq_ena);
	writel_relaxed(OMAP_TIMER_INT_OVERFLOW, t->base + t->wakeup);

	pr_info("TI gptimer %s: %s%lu Hz at %pOF\n",
		name, of_property_read_bool(np, "ti,timer-alwon") ?
		"always-on " : "", t->rate, np->parent);

	return 0;

err_out_unmap:
	iounmap(t->base);

	return error;
}

static int __init dmtimer_clockevent_init(struct device_node *np)
{
	struct dmtimer_clockevent *clkevt;
	int error;

	clkevt = kzalloc(sizeof(*clkevt), GFP_KERNEL);
	if (!clkevt)
		return -ENOMEM;

	error = dmtimer_clkevt_init_common(clkevt, np,
					   CLOCK_EVT_FEAT_PERIODIC |
					   CLOCK_EVT_FEAT_ONESHOT,
					   cpu_possible_mask, "clockevent",
					   300);
	if (error)
		goto err_out_free;

	clockevents_config_and_register(&clkevt->dev, clkevt->t.rate,
					3,  
					0xffffffff);

	if (of_machine_is_compatible("ti,am33xx") ||
	    of_machine_is_compatible("ti,am43")) {
		clkevt->dev.suspend = omap_clockevent_idle;
		clkevt->dev.resume = omap_clockevent_unidle;
	}

	return 0;

err_out_free:
	kfree(clkevt);

	return error;
}

 
static DEFINE_PER_CPU(struct dmtimer_clockevent, dmtimer_percpu_timer);

static int __init dmtimer_percpu_timer_init(struct device_node *np, int cpu)
{
	struct dmtimer_clockevent *clkevt;
	int error;

	if (!cpu_possible(cpu))
		return -EINVAL;

	if (!of_property_read_bool(np->parent, "ti,no-reset-on-init") ||
	    !of_property_read_bool(np->parent, "ti,no-idle"))
		pr_warn("Incomplete dtb for percpu dmtimer %pOF\n", np->parent);

	clkevt = per_cpu_ptr(&dmtimer_percpu_timer, cpu);

	error = dmtimer_clkevt_init_common(clkevt, np, CLOCK_EVT_FEAT_ONESHOT,
					   cpumask_of(cpu), "percpu-dmtimer",
					   500);
	if (error)
		return error;

	return 0;
}

 
static int omap_dmtimer_starting_cpu(unsigned int cpu)
{
	struct dmtimer_clockevent *clkevt = per_cpu_ptr(&dmtimer_percpu_timer, cpu);
	struct clock_event_device *dev = &clkevt->dev;
	struct dmtimer_systimer *t = &clkevt->t;

	clockevents_config_and_register(dev, t->rate, 3, ULONG_MAX);
	irq_force_affinity(dev->irq, cpumask_of(cpu));

	return 0;
}

static int __init dmtimer_percpu_timer_startup(void)
{
	struct dmtimer_clockevent *clkevt = per_cpu_ptr(&dmtimer_percpu_timer, 0);
	struct dmtimer_systimer *t = &clkevt->t;

	if (t->sysc) {
		cpuhp_setup_state(CPUHP_AP_TI_GP_TIMER_STARTING,
				  "clockevents/omap/gptimer:starting",
				  omap_dmtimer_starting_cpu, NULL);
	}

	return 0;
}
subsys_initcall(dmtimer_percpu_timer_startup);

static int __init dmtimer_percpu_quirk_init(struct device_node *np, u32 pa)
{
	struct device_node *arm_timer;

	arm_timer = of_find_compatible_node(NULL, NULL, "arm,armv7-timer");
	if (of_device_is_available(arm_timer)) {
		pr_warn_once("ARM architected timer wrap issue i940 detected\n");
		return 0;
	}

	if (pa == 0x4882c000)            
		return dmtimer_percpu_timer_init(np, 0);
	else if (pa == 0x4882e000)       
		return dmtimer_percpu_timer_init(np, 1);

	return 0;
}

 
static struct dmtimer_clocksource *
to_dmtimer_clocksource(struct clocksource *cs)
{
	return container_of(cs, struct dmtimer_clocksource, dev);
}

static u64 dmtimer_clocksource_read_cycles(struct clocksource *cs)
{
	struct dmtimer_clocksource *clksrc = to_dmtimer_clocksource(cs);
	struct dmtimer_systimer *t = &clksrc->t;

	return (u64)readl_relaxed(t->base + t->counter);
}

static void __iomem *dmtimer_sched_clock_counter;

static u64 notrace dmtimer_read_sched_clock(void)
{
	return readl_relaxed(dmtimer_sched_clock_counter);
}

static void dmtimer_clocksource_suspend(struct clocksource *cs)
{
	struct dmtimer_clocksource *clksrc = to_dmtimer_clocksource(cs);
	struct dmtimer_systimer *t = &clksrc->t;

	clksrc->loadval = readl_relaxed(t->base + t->counter);
	dmtimer_systimer_disable(t);
	clk_disable(t->fck);
}

static void dmtimer_clocksource_resume(struct clocksource *cs)
{
	struct dmtimer_clocksource *clksrc = to_dmtimer_clocksource(cs);
	struct dmtimer_systimer *t = &clksrc->t;
	int error;

	error = clk_enable(t->fck);
	if (error)
		pr_err("could not enable timer fck on resume: %i\n", error);

	dmtimer_systimer_enable(t);
	writel_relaxed(clksrc->loadval, t->base + t->counter);
	writel_relaxed(OMAP_TIMER_CTRL_ST | OMAP_TIMER_CTRL_AR,
		       t->base + t->ctrl);
}

static int __init dmtimer_clocksource_init(struct device_node *np)
{
	struct dmtimer_clocksource *clksrc;
	struct dmtimer_systimer *t;
	struct clocksource *dev;
	int error;

	clksrc = kzalloc(sizeof(*clksrc), GFP_KERNEL);
	if (!clksrc)
		return -ENOMEM;

	dev = &clksrc->dev;
	t = &clksrc->t;

	error = dmtimer_systimer_setup(np, t);
	if (error)
		goto err_out_free;

	dev->name = "dmtimer";
	dev->rating = 300;
	dev->read = dmtimer_clocksource_read_cycles;
	dev->mask = CLOCKSOURCE_MASK(32);
	dev->flags = CLOCK_SOURCE_IS_CONTINUOUS;

	 
	if (of_machine_is_compatible("ti,am43")) {
		dev->suspend = dmtimer_clocksource_suspend;
		dev->resume = dmtimer_clocksource_resume;
	}

	writel_relaxed(0, t->base + t->counter);
	writel_relaxed(OMAP_TIMER_CTRL_ST | OMAP_TIMER_CTRL_AR,
		       t->base + t->ctrl);

	pr_info("TI gptimer clocksource: %s%pOF\n",
		of_property_read_bool(np, "ti,timer-alwon") ?
		"always-on " : "", np->parent);

	if (!dmtimer_sched_clock_counter) {
		dmtimer_sched_clock_counter = t->base + t->counter;
		sched_clock_register(dmtimer_read_sched_clock, 32, t->rate);
	}

	if (clocksource_register_hz(dev, t->rate))
		pr_err("Could not register clocksource %pOF\n", np);

	return 0;

err_out_free:
	kfree(clksrc);

	return -ENODEV;
}

 
static int __init dmtimer_systimer_init(struct device_node *np)
{
	struct resource res;
	u32 pa;

	 
	if (!clocksource && !clockevent)
		dmtimer_systimer_select_best();

	if (!clocksource && !clockevent) {
		pr_err("%s: unable to detect system timers, update dtb?\n",
		       __func__);

		return -EINVAL;
	}


	of_address_to_resource(np, 0, &res);
	pa = (u32)res.start;
	if (!pa)
		return -EINVAL;

	if (counter_32k <= 0 && clocksource == pa)
		return dmtimer_clocksource_init(np);

	if (clockevent == pa)
		return dmtimer_clockevent_init(np);

	if (of_machine_is_compatible("ti,dra7"))
		return dmtimer_percpu_quirk_init(np, pa);

	return 0;
}

TIMER_OF_DECLARE(systimer_omap2, "ti,omap2420-timer", dmtimer_systimer_init);
TIMER_OF_DECLARE(systimer_omap3, "ti,omap3430-timer", dmtimer_systimer_init);
TIMER_OF_DECLARE(systimer_omap4, "ti,omap4430-timer", dmtimer_systimer_init);
TIMER_OF_DECLARE(systimer_omap5, "ti,omap5430-timer", dmtimer_systimer_init);
TIMER_OF_DECLARE(systimer_am33x, "ti,am335x-timer", dmtimer_systimer_init);
TIMER_OF_DECLARE(systimer_am3ms, "ti,am335x-timer-1ms", dmtimer_systimer_init);
TIMER_OF_DECLARE(systimer_dm814, "ti,dm814-timer", dmtimer_systimer_init);
TIMER_OF_DECLARE(systimer_dm816, "ti,dm816-timer", dmtimer_systimer_init);
