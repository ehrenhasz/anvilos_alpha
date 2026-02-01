
 
#include <linux/clockchips.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/spinlock.h>
#include <linux/timex.h>
#include <linux/module.h>
#include <linux/i8253.h>
#include <linux/smp.h>

 
DEFINE_RAW_SPINLOCK(i8253_lock);
EXPORT_SYMBOL(i8253_lock);

 
bool i8253_clear_counter_on_shutdown __ro_after_init = true;

#ifdef CONFIG_CLKSRC_I8253
 
static u64 i8253_read(struct clocksource *cs)
{
	static int old_count;
	static u32 old_jifs;
	unsigned long flags;
	int count;
	u32 jifs;

	raw_spin_lock_irqsave(&i8253_lock, flags);
	 
	jifs = jiffies;
	outb_p(0x00, PIT_MODE);	 
	count = inb_p(PIT_CH0);	 
	count |= inb_p(PIT_CH0) << 8;

	 
	if (count > PIT_LATCH) {
		outb_p(0x34, PIT_MODE);
		outb_p(PIT_LATCH & 0xff, PIT_CH0);
		outb_p(PIT_LATCH >> 8, PIT_CH0);
		count = PIT_LATCH - 1;
	}

	 
	if (count > old_count && jifs == old_jifs)
		count = old_count;

	old_count = count;
	old_jifs = jifs;

	raw_spin_unlock_irqrestore(&i8253_lock, flags);

	count = (PIT_LATCH - 1) - count;

	return (u64)(jifs * PIT_LATCH) + count;
}

static struct clocksource i8253_cs = {
	.name		= "pit",
	.rating		= 110,
	.read		= i8253_read,
	.mask		= CLOCKSOURCE_MASK(32),
};

int __init clocksource_i8253_init(void)
{
	return clocksource_register_hz(&i8253_cs, PIT_TICK_RATE);
}
#endif

#ifdef CONFIG_CLKEVT_I8253
static int pit_shutdown(struct clock_event_device *evt)
{
	if (!clockevent_state_oneshot(evt) && !clockevent_state_periodic(evt))
		return 0;

	raw_spin_lock(&i8253_lock);

	outb_p(0x30, PIT_MODE);

	if (i8253_clear_counter_on_shutdown) {
		outb_p(0, PIT_CH0);
		outb_p(0, PIT_CH0);
	}

	raw_spin_unlock(&i8253_lock);
	return 0;
}

static int pit_set_oneshot(struct clock_event_device *evt)
{
	raw_spin_lock(&i8253_lock);
	outb_p(0x38, PIT_MODE);
	raw_spin_unlock(&i8253_lock);
	return 0;
}

static int pit_set_periodic(struct clock_event_device *evt)
{
	raw_spin_lock(&i8253_lock);

	 
	outb_p(0x34, PIT_MODE);
	outb_p(PIT_LATCH & 0xff, PIT_CH0);	 
	outb_p(PIT_LATCH >> 8, PIT_CH0);	 

	raw_spin_unlock(&i8253_lock);
	return 0;
}

 
static int pit_next_event(unsigned long delta, struct clock_event_device *evt)
{
	raw_spin_lock(&i8253_lock);
	outb_p(delta & 0xff , PIT_CH0);	 
	outb_p(delta >> 8 , PIT_CH0);		 
	raw_spin_unlock(&i8253_lock);

	return 0;
}

 
struct clock_event_device i8253_clockevent = {
	.name			= "pit",
	.features		= CLOCK_EVT_FEAT_PERIODIC,
	.set_state_shutdown	= pit_shutdown,
	.set_state_periodic	= pit_set_periodic,
	.set_next_event		= pit_next_event,
};

 
void __init clockevent_i8253_init(bool oneshot)
{
	if (oneshot) {
		i8253_clockevent.features |= CLOCK_EVT_FEAT_ONESHOT;
		i8253_clockevent.set_state_oneshot = pit_set_oneshot;
	}
	 
	i8253_clockevent.cpumask = cpumask_of(smp_processor_id());

	clockevents_config_and_register(&i8253_clockevent, PIT_TICK_RATE,
					0xF, 0x7FFF);
}
#endif
