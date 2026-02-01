
 
#include <linux/flex_proportions.h>

int fprop_global_init(struct fprop_global *p, gfp_t gfp)
{
	int err;

	p->period = 0;
	 
	err = percpu_counter_init(&p->events, 1, gfp);
	if (err)
		return err;
	seqcount_init(&p->sequence);
	return 0;
}

void fprop_global_destroy(struct fprop_global *p)
{
	percpu_counter_destroy(&p->events);
}

 
bool fprop_new_period(struct fprop_global *p, int periods)
{
	s64 events = percpu_counter_sum(&p->events);

	 
	if (events <= 1)
		return false;
	preempt_disable_nested();
	write_seqcount_begin(&p->sequence);
	if (periods < 64)
		events -= events >> periods;
	 
	percpu_counter_add(&p->events, -events);
	p->period += periods;
	write_seqcount_end(&p->sequence);
	preempt_enable_nested();

	return true;
}

 

int fprop_local_init_single(struct fprop_local_single *pl)
{
	pl->events = 0;
	pl->period = 0;
	raw_spin_lock_init(&pl->lock);
	return 0;
}

void fprop_local_destroy_single(struct fprop_local_single *pl)
{
}

static void fprop_reflect_period_single(struct fprop_global *p,
					struct fprop_local_single *pl)
{
	unsigned int period = p->period;
	unsigned long flags;

	 
	if (pl->period == period)
		return;
	raw_spin_lock_irqsave(&pl->lock, flags);
	 
	if (pl->period >= period) {
		raw_spin_unlock_irqrestore(&pl->lock, flags);
		return;
	}
	 
	if (period - pl->period < BITS_PER_LONG)
		pl->events >>= period - pl->period;
	else
		pl->events = 0;
	pl->period = period;
	raw_spin_unlock_irqrestore(&pl->lock, flags);
}

 
void __fprop_inc_single(struct fprop_global *p, struct fprop_local_single *pl)
{
	fprop_reflect_period_single(p, pl);
	pl->events++;
	percpu_counter_add(&p->events, 1);
}

 
void fprop_fraction_single(struct fprop_global *p,
			   struct fprop_local_single *pl,
			   unsigned long *numerator, unsigned long *denominator)
{
	unsigned int seq;
	s64 num, den;

	do {
		seq = read_seqcount_begin(&p->sequence);
		fprop_reflect_period_single(p, pl);
		num = pl->events;
		den = percpu_counter_read_positive(&p->events);
	} while (read_seqcount_retry(&p->sequence, seq));

	 
	if (den <= num) {
		if (num)
			den = num;
		else
			den = 1;
	}
	*denominator = den;
	*numerator = num;
}

 
#define PROP_BATCH (8*(1+ilog2(nr_cpu_ids)))

int fprop_local_init_percpu(struct fprop_local_percpu *pl, gfp_t gfp)
{
	int err;

	err = percpu_counter_init(&pl->events, 0, gfp);
	if (err)
		return err;
	pl->period = 0;
	raw_spin_lock_init(&pl->lock);
	return 0;
}

void fprop_local_destroy_percpu(struct fprop_local_percpu *pl)
{
	percpu_counter_destroy(&pl->events);
}

static void fprop_reflect_period_percpu(struct fprop_global *p,
					struct fprop_local_percpu *pl)
{
	unsigned int period = p->period;
	unsigned long flags;

	 
	if (pl->period == period)
		return;
	raw_spin_lock_irqsave(&pl->lock, flags);
	 
	if (pl->period >= period) {
		raw_spin_unlock_irqrestore(&pl->lock, flags);
		return;
	}
	 
	if (period - pl->period < BITS_PER_LONG) {
		s64 val = percpu_counter_read(&pl->events);

		if (val < (nr_cpu_ids * PROP_BATCH))
			val = percpu_counter_sum(&pl->events);

		percpu_counter_add_batch(&pl->events,
			-val + (val >> (period-pl->period)), PROP_BATCH);
	} else
		percpu_counter_set(&pl->events, 0);
	pl->period = period;
	raw_spin_unlock_irqrestore(&pl->lock, flags);
}

 
void __fprop_add_percpu(struct fprop_global *p, struct fprop_local_percpu *pl,
		long nr)
{
	fprop_reflect_period_percpu(p, pl);
	percpu_counter_add_batch(&pl->events, nr, PROP_BATCH);
	percpu_counter_add(&p->events, nr);
}

void fprop_fraction_percpu(struct fprop_global *p,
			   struct fprop_local_percpu *pl,
			   unsigned long *numerator, unsigned long *denominator)
{
	unsigned int seq;
	s64 num, den;

	do {
		seq = read_seqcount_begin(&p->sequence);
		fprop_reflect_period_percpu(p, pl);
		num = percpu_counter_read_positive(&pl->events);
		den = percpu_counter_read_positive(&p->events);
	} while (read_seqcount_retry(&p->sequence, seq));

	 
	if (den <= num) {
		if (num)
			den = num;
		else
			den = 1;
	}
	*denominator = den;
	*numerator = num;
}

 
void __fprop_add_percpu_max(struct fprop_global *p,
		struct fprop_local_percpu *pl, int max_frac, long nr)
{
	if (unlikely(max_frac < FPROP_FRAC_BASE)) {
		unsigned long numerator, denominator;
		s64 tmp;

		fprop_fraction_percpu(p, pl, &numerator, &denominator);
		 
		tmp = (u64)denominator * max_frac -
					((u64)numerator << FPROP_FRAC_SHIFT);
		if (tmp < 0) {
			 
			return;
		} else if (tmp < nr * (FPROP_FRAC_BASE - max_frac)) {
			 
			nr = div_u64(tmp + FPROP_FRAC_BASE - max_frac - 1,
					FPROP_FRAC_BASE - max_frac);
		}
	}

	__fprop_add_percpu(p, pl, nr);
}
