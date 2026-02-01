
 

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/clocksource.h>
#include <linux/clockchips.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/bitops.h>

#define DRIVER_NAME	"asm9260-timer"

 
#define SET_REG 4
#define CLR_REG 8

#define HW_IR           0x0000  
#define BM_IR_CR0	BIT(4)
#define BM_IR_MR3	BIT(3)
#define BM_IR_MR2	BIT(2)
#define BM_IR_MR1	BIT(1)
#define BM_IR_MR0	BIT(0)

#define HW_TCR		0x0010  
 
#define BM_C3_RST	BIT(7)
#define BM_C2_RST	BIT(6)
#define BM_C1_RST	BIT(5)
#define BM_C0_RST	BIT(4)
 
#define BM_C3_EN	BIT(3)
#define BM_C2_EN	BIT(2)
#define BM_C1_EN	BIT(1)
#define BM_C0_EN	BIT(0)

#define HW_DIR		0x0020  
 
#define BM_DIR_COUNT_UP		0
#define BM_DIR_COUNT_DOWN	1
#define BM_DIR0_SHIFT	0
#define BM_DIR1_SHIFT	4
#define BM_DIR2_SHIFT	8
#define BM_DIR3_SHIFT	12
#define BM_DIR_DEFAULT		(BM_DIR_COUNT_UP << BM_DIR0_SHIFT | \
				 BM_DIR_COUNT_UP << BM_DIR1_SHIFT | \
				 BM_DIR_COUNT_UP << BM_DIR2_SHIFT | \
				 BM_DIR_COUNT_UP << BM_DIR3_SHIFT)

#define HW_TC0		0x0030  
 
#define HW_TC1          0x0040
#define HW_TC2		0x0050
#define HW_TC3		0x0060

#define HW_PR		0x0070  
#define BM_PR_DISABLE	0
#define HW_PC		0x0080  
#define HW_MCR		0x0090  
 
#define BM_MCR_INT_EN(n)	(1 << (n * 3 + 0))
 
#define BM_MCR_RES_EN(n)	(1 << (n * 3 + 1))
 
#define BM_MCR_STOP_EN(n)	(1 << (n * 3 + 2))

#define HW_MR0		0x00a0  
#define HW_MR1		0x00b0
#define HW_MR2		0x00C0
#define HW_MR3		0x00D0

#define HW_CTCR		0x0180  
#define BM_CTCR0_SHIFT	0
#define BM_CTCR1_SHIFT	2
#define BM_CTCR2_SHIFT	4
#define BM_CTCR3_SHIFT	6
#define BM_CTCR_TM	0	 
#define BM_CTCR_DEFAULT	(BM_CTCR_TM << BM_CTCR0_SHIFT | \
			 BM_CTCR_TM << BM_CTCR1_SHIFT | \
			 BM_CTCR_TM << BM_CTCR2_SHIFT | \
			 BM_CTCR_TM << BM_CTCR3_SHIFT)

static struct asm9260_timer_priv {
	void __iomem *base;
	unsigned long ticks_per_jiffy;
} priv;

static int asm9260_timer_set_next_event(unsigned long delta,
					 struct clock_event_device *evt)
{
	 
	writel_relaxed(delta, priv.base + HW_MR0);
	 
	writel_relaxed(BM_C0_EN, priv.base + HW_TCR + SET_REG);
	return 0;
}

static inline void __asm9260_timer_shutdown(struct clock_event_device *evt)
{
	 
	writel_relaxed(BM_C0_EN, priv.base + HW_TCR + CLR_REG);
}

static int asm9260_timer_shutdown(struct clock_event_device *evt)
{
	__asm9260_timer_shutdown(evt);
	return 0;
}

static int asm9260_timer_set_oneshot(struct clock_event_device *evt)
{
	__asm9260_timer_shutdown(evt);

	 
	writel_relaxed(BM_MCR_RES_EN(0) | BM_MCR_STOP_EN(0),
		       priv.base + HW_MCR + SET_REG);
	return 0;
}

static int asm9260_timer_set_periodic(struct clock_event_device *evt)
{
	__asm9260_timer_shutdown(evt);

	 
	writel_relaxed(BM_MCR_RES_EN(0) | BM_MCR_STOP_EN(0),
		       priv.base + HW_MCR + CLR_REG);
	 
	writel_relaxed(priv.ticks_per_jiffy, priv.base + HW_MR0);
	 
	writel_relaxed(BM_C0_EN, priv.base + HW_TCR + SET_REG);
	return 0;
}

static struct clock_event_device event_dev = {
	.name			= DRIVER_NAME,
	.rating			= 200,
	.features		= CLOCK_EVT_FEAT_PERIODIC |
				  CLOCK_EVT_FEAT_ONESHOT,
	.set_next_event		= asm9260_timer_set_next_event,
	.set_state_shutdown	= asm9260_timer_shutdown,
	.set_state_periodic	= asm9260_timer_set_periodic,
	.set_state_oneshot	= asm9260_timer_set_oneshot,
	.tick_resume		= asm9260_timer_shutdown,
};

static irqreturn_t asm9260_timer_interrupt(int irq, void *dev_id)
{
	struct clock_event_device *evt = dev_id;

	evt->event_handler(evt);

	writel_relaxed(BM_IR_MR0, priv.base + HW_IR);

	return IRQ_HANDLED;
}

 
static int __init asm9260_timer_init(struct device_node *np)
{
	int irq;
	struct clk *clk;
	int ret;
	unsigned long rate;

	priv.base = of_io_request_and_map(np, 0, np->name);
	if (IS_ERR(priv.base)) {
		pr_err("%pOFn: unable to map resource\n", np);
		return PTR_ERR(priv.base);
	}

	clk = of_clk_get(np, 0);
	if (IS_ERR(clk)) {
		pr_err("Failed to get clk!\n");
		return PTR_ERR(clk);
	}

	ret = clk_prepare_enable(clk);
	if (ret) {
		pr_err("Failed to enable clk!\n");
		return ret;
	}

	irq = irq_of_parse_and_map(np, 0);
	ret = request_irq(irq, asm9260_timer_interrupt, IRQF_TIMER,
			DRIVER_NAME, &event_dev);
	if (ret) {
		pr_err("Failed to setup irq!\n");
		return ret;
	}

	 
	writel_relaxed(BM_DIR_DEFAULT, priv.base + HW_DIR);
	 
	writel_relaxed(BM_PR_DISABLE, priv.base + HW_PR);
	 
	writel_relaxed(BM_CTCR_DEFAULT, priv.base + HW_CTCR);
	 
	writel_relaxed(BM_MCR_INT_EN(0) , priv.base + HW_MCR);

	rate = clk_get_rate(clk);
	clocksource_mmio_init(priv.base + HW_TC1, DRIVER_NAME, rate,
			200, 32, clocksource_mmio_readl_up);

	 
	writel_relaxed(0xffffffff, priv.base + HW_MR1);
	 
	writel_relaxed(BM_C1_EN, priv.base + HW_TCR + SET_REG);

	priv.ticks_per_jiffy = DIV_ROUND_CLOSEST(rate, HZ);
	event_dev.cpumask = cpumask_of(0);
	clockevents_config_and_register(&event_dev, rate, 0x2c00, 0xfffffffe);

	return 0;
}
TIMER_OF_DECLARE(asm9260_timer, "alphascale,asm9260-timer",
		asm9260_timer_init);
