
 

#include <linux/bug.h>
#include <linux/err.h>
#include <linux/limits.h>
#include <linux/log2.h>
#include <linux/math64.h>
#include <linux/math.h>
#include <linux/minmax.h>

#include <linux/clk/analogbits-wrpll-cln28hpc.h>

 
#define MIN_INPUT_FREQ			7000000

 
#define MAX_INPUT_FREQ			600000000

 
#define MIN_POST_DIVR_FREQ		7000000

 
#define MAX_POST_DIVR_FREQ		200000000

 
#define MIN_VCO_FREQ			2400000000UL

 
#define MAX_VCO_FREQ			4800000000ULL

 
#define MAX_DIVQ_DIVISOR		64

 
#define MAX_DIVR_DIVISOR		64

 
#define MAX_LOCK_US			70

 
#define ROUND_SHIFT			20

 

 
static int __wrpll_calc_filter_range(unsigned long post_divr_freq)
{
	if (post_divr_freq < MIN_POST_DIVR_FREQ ||
	    post_divr_freq > MAX_POST_DIVR_FREQ) {
		WARN(1, "%s: post-divider reference freq out of range: %lu",
		     __func__, post_divr_freq);
		return -ERANGE;
	}

	switch (post_divr_freq) {
	case 0 ... 10999999:
		return 1;
	case 11000000 ... 17999999:
		return 2;
	case 18000000 ... 29999999:
		return 3;
	case 30000000 ... 49999999:
		return 4;
	case 50000000 ... 79999999:
		return 5;
	case 80000000 ... 129999999:
		return 6;
	}

	return 7;
}

 
static u8 __wrpll_calc_fbdiv(const struct wrpll_cfg *c)
{
	return (c->flags & WRPLL_FLAGS_INT_FEEDBACK_MASK) ? 2 : 1;
}

 
static u8 __wrpll_calc_divq(u32 target_rate, u64 *vco_rate)
{
	u64 s;
	u8 divq = 0;

	if (!vco_rate) {
		WARN_ON(1);
		goto wcd_out;
	}

	s = div_u64(MAX_VCO_FREQ, target_rate);
	if (s <= 1) {
		divq = 1;
		*vco_rate = MAX_VCO_FREQ;
	} else if (s > MAX_DIVQ_DIVISOR) {
		divq = ilog2(MAX_DIVQ_DIVISOR);
		*vco_rate = MIN_VCO_FREQ;
	} else {
		divq = ilog2(s);
		*vco_rate = (u64)target_rate << divq;
	}

wcd_out:
	return divq;
}

 
static int __wrpll_update_parent_rate(struct wrpll_cfg *c,
				      unsigned long parent_rate)
{
	u8 max_r_for_parent;

	if (parent_rate > MAX_INPUT_FREQ || parent_rate < MIN_POST_DIVR_FREQ)
		return -ERANGE;

	c->parent_rate = parent_rate;
	max_r_for_parent = div_u64(parent_rate, MIN_POST_DIVR_FREQ);
	c->max_r = min_t(u8, MAX_DIVR_DIVISOR, max_r_for_parent);

	c->init_r = DIV_ROUND_UP_ULL(parent_rate, MAX_POST_DIVR_FREQ);

	return 0;
}

 
int wrpll_configure_for_rate(struct wrpll_cfg *c, u32 target_rate,
			     unsigned long parent_rate)
{
	unsigned long ratio;
	u64 target_vco_rate, delta, best_delta, f_pre_div, vco, vco_pre;
	u32 best_f, f, post_divr_freq;
	u8 fbdiv, divq, best_r, r;
	int range;

	if (c->flags == 0) {
		WARN(1, "%s called with uninitialized PLL config", __func__);
		return -EINVAL;
	}

	 
	if (parent_rate != c->parent_rate) {
		if (__wrpll_update_parent_rate(c, parent_rate)) {
			pr_err("%s: PLL input rate is out of range\n",
			       __func__);
			return -ERANGE;
		}
	}

	c->flags &= ~WRPLL_FLAGS_RESET_MASK;

	 
	if (target_rate == parent_rate) {
		c->flags |= WRPLL_FLAGS_BYPASS_MASK;
		return 0;
	}

	c->flags &= ~WRPLL_FLAGS_BYPASS_MASK;

	 
	divq = __wrpll_calc_divq(target_rate, &target_vco_rate);
	if (!divq)
		return -1;
	c->divq = divq;

	 
	ratio = div64_u64((target_vco_rate << ROUND_SHIFT), parent_rate);

	fbdiv = __wrpll_calc_fbdiv(c);
	best_r = 0;
	best_f = 0;
	best_delta = MAX_VCO_FREQ;

	 
	for (r = c->init_r; r <= c->max_r; ++r) {
		f_pre_div = ratio * r;
		f = (f_pre_div + (1 << ROUND_SHIFT)) >> ROUND_SHIFT;
		f >>= (fbdiv - 1);

		post_divr_freq = div_u64(parent_rate, r);
		vco_pre = fbdiv * post_divr_freq;
		vco = vco_pre * f;

		 
		if (vco > target_vco_rate) {
			--f;
			vco = vco_pre * f;
		} else if (vco < MIN_VCO_FREQ) {
			++f;
			vco = vco_pre * f;
		}

		delta = abs(target_rate - vco);
		if (delta < best_delta) {
			best_delta = delta;
			best_r = r;
			best_f = f;
		}
	}

	c->divr = best_r - 1;
	c->divf = best_f - 1;

	post_divr_freq = div_u64(parent_rate, best_r);

	 
	range = __wrpll_calc_filter_range(post_divr_freq);
	if (range < 0)
		return range;
	c->range = range;

	return 0;
}

 
unsigned long wrpll_calc_output_rate(const struct wrpll_cfg *c,
				     unsigned long parent_rate)
{
	u8 fbdiv;
	u64 n;

	if (c->flags & WRPLL_FLAGS_EXT_FEEDBACK_MASK) {
		WARN(1, "external feedback mode not yet supported");
		return ULONG_MAX;
	}

	fbdiv = __wrpll_calc_fbdiv(c);
	n = parent_rate * fbdiv * (c->divf + 1);
	n = div_u64(n, c->divr + 1);
	n >>= c->divq;

	return n;
}

 
unsigned int wrpll_calc_max_lock_us(const struct wrpll_cfg *c)
{
	return MAX_LOCK_US;
}
