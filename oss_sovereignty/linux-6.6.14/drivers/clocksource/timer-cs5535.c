
 

#include <linux/kernel.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/cs5535.h>
#include <linux/clockchips.h>

#define DRV_NAME "cs5535-clockevt"

static int timer_irq;
module_param_hw_named(irq, timer_irq, int, irq, 0644);
MODULE_PARM_DESC(irq, "Which IRQ to use for the clock source MFGPT ticks.");

 

static struct cs5535_mfgpt_timer *cs5535_event_clock;

 

#define MFGPT_DIVISOR 16
#define MFGPT_SCALE  4      
#define MFGPT_HZ  (32768 / MFGPT_DIVISOR)
#define MFGPT_PERIODIC (MFGPT_HZ / HZ)

 

static void disable_timer(struct cs5535_mfgpt_timer *timer)
{
	 
	cs5535_mfgpt_write(timer, MFGPT_REG_SETUP,
			(uint16_t) ~MFGPT_SETUP_CNTEN | MFGPT_SETUP_CMP1 |
				MFGPT_SETUP_CMP2);
}

static void start_timer(struct cs5535_mfgpt_timer *timer, uint16_t delta)
{
	cs5535_mfgpt_write(timer, MFGPT_REG_CMP2, delta);
	cs5535_mfgpt_write(timer, MFGPT_REG_COUNTER, 0);

	cs5535_mfgpt_write(timer, MFGPT_REG_SETUP,
			MFGPT_SETUP_CNTEN | MFGPT_SETUP_CMP2);
}

static int mfgpt_shutdown(struct clock_event_device *evt)
{
	disable_timer(cs5535_event_clock);
	return 0;
}

static int mfgpt_set_periodic(struct clock_event_device *evt)
{
	disable_timer(cs5535_event_clock);
	start_timer(cs5535_event_clock, MFGPT_PERIODIC);
	return 0;
}

static int mfgpt_next_event(unsigned long delta, struct clock_event_device *evt)
{
	start_timer(cs5535_event_clock, delta);
	return 0;
}

static struct clock_event_device cs5535_clockevent = {
	.name = DRV_NAME,
	.features = CLOCK_EVT_FEAT_PERIODIC | CLOCK_EVT_FEAT_ONESHOT,
	.set_state_shutdown = mfgpt_shutdown,
	.set_state_periodic = mfgpt_set_periodic,
	.set_state_oneshot = mfgpt_shutdown,
	.tick_resume = mfgpt_shutdown,
	.set_next_event = mfgpt_next_event,
	.rating = 250,
};

static irqreturn_t mfgpt_tick(int irq, void *dev_id)
{
	uint16_t val = cs5535_mfgpt_read(cs5535_event_clock, MFGPT_REG_SETUP);

	 
	if (!(val & (MFGPT_SETUP_SETUP | MFGPT_SETUP_CMP2 | MFGPT_SETUP_CMP1)))
		return IRQ_NONE;

	 
	disable_timer(cs5535_event_clock);

	if (clockevent_state_detached(&cs5535_clockevent) ||
	    clockevent_state_shutdown(&cs5535_clockevent))
		return IRQ_HANDLED;

	 
	cs5535_mfgpt_write(cs5535_event_clock, MFGPT_REG_COUNTER, 0);

	 

	if (clockevent_state_periodic(&cs5535_clockevent))
		cs5535_mfgpt_write(cs5535_event_clock, MFGPT_REG_SETUP,
				MFGPT_SETUP_CNTEN | MFGPT_SETUP_CMP2);

	cs5535_clockevent.event_handler(&cs5535_clockevent);
	return IRQ_HANDLED;
}

static int __init cs5535_mfgpt_init(void)
{
	unsigned long flags = IRQF_NOBALANCING | IRQF_TIMER | IRQF_SHARED;
	struct cs5535_mfgpt_timer *timer;
	int ret;
	uint16_t val;

	timer = cs5535_mfgpt_alloc_timer(MFGPT_TIMER_ANY, MFGPT_DOMAIN_WORKING);
	if (!timer) {
		printk(KERN_ERR DRV_NAME ": Could not allocate MFGPT timer\n");
		return -ENODEV;
	}
	cs5535_event_clock = timer;

	 
	if (cs5535_mfgpt_setup_irq(timer, MFGPT_CMP2, &timer_irq)) {
		printk(KERN_ERR DRV_NAME ": Could not set up IRQ %d\n",
				timer_irq);
		goto err_timer;
	}

	 
	ret = request_irq(timer_irq, mfgpt_tick, flags, DRV_NAME, timer);
	if (ret) {
		printk(KERN_ERR DRV_NAME ": Unable to set up the interrupt.\n");
		goto err_irq;
	}

	 
	val = MFGPT_SCALE | (3 << 8);

	cs5535_mfgpt_write(cs5535_event_clock, MFGPT_REG_SETUP, val);

	 
	printk(KERN_INFO DRV_NAME
		": Registering MFGPT timer as a clock event, using IRQ %d\n",
		timer_irq);
	clockevents_config_and_register(&cs5535_clockevent, MFGPT_HZ,
					0xF, 0xFFFE);

	return 0;

err_irq:
	cs5535_mfgpt_release_irq(cs5535_event_clock, MFGPT_CMP2, &timer_irq);
err_timer:
	cs5535_mfgpt_free_timer(cs5535_event_clock);
	printk(KERN_ERR DRV_NAME ": Unable to set up the MFGPT clock source\n");
	return -EIO;
}

module_init(cs5535_mfgpt_init);

MODULE_AUTHOR("Andres Salomon <dilinger@queued.net>");
MODULE_DESCRIPTION("CS5535/CS5536 MFGPT clock event driver");
MODULE_LICENSE("GPL");
