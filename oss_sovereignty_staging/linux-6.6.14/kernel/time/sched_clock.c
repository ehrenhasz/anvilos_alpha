
 
#include <linux/clocksource.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/ktime.h>
#include <linux/kernel.h>
#include <linux/math.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/sched/clock.h>
#include <linux/syscore_ops.h>
#include <linux/hrtimer.h>
#include <linux/sched_clock.h>
#include <linux/seqlock.h>
#include <linux/bitops.h>

#include "timekeeping.h"

 
struct clock_data {
	seqcount_latch_t	seq;
	struct clock_read_data	read_data[2];
	ktime_t			wrap_kt;
	unsigned long		rate;

	u64 (*actual_read_sched_clock)(void);
};

static struct hrtimer sched_clock_timer;
static int irqtime = -1;

core_param(irqtime, irqtime, int, 0400);

static u64 notrace jiffy_sched_clock_read(void)
{
	 
	return (u64)(jiffies - INITIAL_JIFFIES);
}

static struct clock_data cd ____cacheline_aligned = {
	.read_data[0] = { .mult = NSEC_PER_SEC / HZ,
			  .read_sched_clock = jiffy_sched_clock_read, },
	.actual_read_sched_clock = jiffy_sched_clock_read,
};

static __always_inline u64 cyc_to_ns(u64 cyc, u32 mult, u32 shift)
{
	return (cyc * mult) >> shift;
}

notrace struct clock_read_data *sched_clock_read_begin(unsigned int *seq)
{
	*seq = raw_read_seqcount_latch(&cd.seq);
	return cd.read_data + (*seq & 1);
}

notrace int sched_clock_read_retry(unsigned int seq)
{
	return raw_read_seqcount_latch_retry(&cd.seq, seq);
}

unsigned long long noinstr sched_clock_noinstr(void)
{
	struct clock_read_data *rd;
	unsigned int seq;
	u64 cyc, res;

	do {
		seq = raw_read_seqcount_latch(&cd.seq);
		rd = cd.read_data + (seq & 1);

		cyc = (rd->read_sched_clock() - rd->epoch_cyc) &
		      rd->sched_clock_mask;
		res = rd->epoch_ns + cyc_to_ns(cyc, rd->mult, rd->shift);
	} while (raw_read_seqcount_latch_retry(&cd.seq, seq));

	return res;
}

unsigned long long notrace sched_clock(void)
{
	unsigned long long ns;
	preempt_disable_notrace();
	ns = sched_clock_noinstr();
	preempt_enable_notrace();
	return ns;
}

 
static void update_clock_read_data(struct clock_read_data *rd)
{
	 
	cd.read_data[1] = *rd;

	 
	raw_write_seqcount_latch(&cd.seq);

	 
	cd.read_data[0] = *rd;

	 
	raw_write_seqcount_latch(&cd.seq);
}

 
static void update_sched_clock(void)
{
	u64 cyc;
	u64 ns;
	struct clock_read_data rd;

	rd = cd.read_data[0];

	cyc = cd.actual_read_sched_clock();
	ns = rd.epoch_ns + cyc_to_ns((cyc - rd.epoch_cyc) & rd.sched_clock_mask, rd.mult, rd.shift);

	rd.epoch_ns = ns;
	rd.epoch_cyc = cyc;

	update_clock_read_data(&rd);
}

static enum hrtimer_restart sched_clock_poll(struct hrtimer *hrt)
{
	update_sched_clock();
	hrtimer_forward_now(hrt, cd.wrap_kt);

	return HRTIMER_RESTART;
}

void __init
sched_clock_register(u64 (*read)(void), int bits, unsigned long rate)
{
	u64 res, wrap, new_mask, new_epoch, cyc, ns;
	u32 new_mult, new_shift;
	unsigned long r, flags;
	char r_unit;
	struct clock_read_data rd;

	if (cd.rate > rate)
		return;

	 
	local_irq_save(flags);

	 
	clocks_calc_mult_shift(&new_mult, &new_shift, rate, NSEC_PER_SEC, 3600);

	new_mask = CLOCKSOURCE_MASK(bits);
	cd.rate = rate;

	 
	wrap = clocks_calc_max_nsecs(new_mult, new_shift, 0, new_mask, NULL);
	cd.wrap_kt = ns_to_ktime(wrap);

	rd = cd.read_data[0];

	 
	new_epoch = read();
	cyc = cd.actual_read_sched_clock();
	ns = rd.epoch_ns + cyc_to_ns((cyc - rd.epoch_cyc) & rd.sched_clock_mask, rd.mult, rd.shift);
	cd.actual_read_sched_clock = read;

	rd.read_sched_clock	= read;
	rd.sched_clock_mask	= new_mask;
	rd.mult			= new_mult;
	rd.shift		= new_shift;
	rd.epoch_cyc		= new_epoch;
	rd.epoch_ns		= ns;

	update_clock_read_data(&rd);

	if (sched_clock_timer.function != NULL) {
		 
		hrtimer_start(&sched_clock_timer, cd.wrap_kt,
			      HRTIMER_MODE_REL_HARD);
	}

	r = rate;
	if (r >= 4000000) {
		r = DIV_ROUND_CLOSEST(r, 1000000);
		r_unit = 'M';
	} else if (r >= 4000) {
		r = DIV_ROUND_CLOSEST(r, 1000);
		r_unit = 'k';
	} else {
		r_unit = ' ';
	}

	 
	res = cyc_to_ns(1ULL, new_mult, new_shift);

	pr_info("sched_clock: %u bits at %lu%cHz, resolution %lluns, wraps every %lluns\n",
		bits, r, r_unit, res, wrap);

	 
	if (irqtime > 0 || (irqtime == -1 && rate >= 1000000))
		enable_sched_clock_irqtime();

	local_irq_restore(flags);

	pr_debug("Registered %pS as sched_clock source\n", read);
}

void __init generic_sched_clock_init(void)
{
	 
	if (cd.actual_read_sched_clock == jiffy_sched_clock_read)
		sched_clock_register(jiffy_sched_clock_read, BITS_PER_LONG, HZ);

	update_sched_clock();

	 
	hrtimer_init(&sched_clock_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL_HARD);
	sched_clock_timer.function = sched_clock_poll;
	hrtimer_start(&sched_clock_timer, cd.wrap_kt, HRTIMER_MODE_REL_HARD);
}

 
static u64 notrace suspended_sched_clock_read(void)
{
	unsigned int seq = raw_read_seqcount_latch(&cd.seq);

	return cd.read_data[seq & 1].epoch_cyc;
}

int sched_clock_suspend(void)
{
	struct clock_read_data *rd = &cd.read_data[0];

	update_sched_clock();
	hrtimer_cancel(&sched_clock_timer);
	rd->read_sched_clock = suspended_sched_clock_read;

	return 0;
}

void sched_clock_resume(void)
{
	struct clock_read_data *rd = &cd.read_data[0];

	rd->epoch_cyc = cd.actual_read_sched_clock();
	hrtimer_start(&sched_clock_timer, cd.wrap_kt, HRTIMER_MODE_REL_HARD);
	rd->read_sched_clock = cd.actual_read_sched_clock;
}

static struct syscore_ops sched_clock_ops = {
	.suspend	= sched_clock_suspend,
	.resume		= sched_clock_resume,
};

static int __init sched_clock_syscore_init(void)
{
	register_syscore_ops(&sched_clock_ops);

	return 0;
}
device_initcall(sched_clock_syscore_init);
