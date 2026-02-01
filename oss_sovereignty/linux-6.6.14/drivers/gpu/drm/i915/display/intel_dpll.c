
 

#include <linux/kernel.h>
#include <linux/string_helpers.h>

#include "i915_reg.h"
#include "intel_crtc.h"
#include "intel_cx0_phy.h"
#include "intel_de.h"
#include "intel_display.h"
#include "intel_display_types.h"
#include "intel_dpio_phy.h"
#include "intel_dpll.h"
#include "intel_lvds.h"
#include "intel_panel.h"
#include "intel_pps.h"
#include "intel_snps_phy.h"
#include "vlv_sideband.h"

struct intel_dpll_funcs {
	int (*crtc_compute_clock)(struct intel_atomic_state *state,
				  struct intel_crtc *crtc);
	int (*crtc_get_shared_dpll)(struct intel_atomic_state *state,
				    struct intel_crtc *crtc);
};

struct intel_limit {
	struct {
		int min, max;
	} dot, vco, n, m, m1, m2, p, p1;

	struct {
		int dot_limit;
		int p2_slow, p2_fast;
	} p2;
};
static const struct intel_limit intel_limits_i8xx_dac = {
	.dot = { .min = 25000, .max = 350000 },
	.vco = { .min = 908000, .max = 1512000 },
	.n = { .min = 2, .max = 16 },
	.m = { .min = 96, .max = 140 },
	.m1 = { .min = 18, .max = 26 },
	.m2 = { .min = 6, .max = 16 },
	.p = { .min = 4, .max = 128 },
	.p1 = { .min = 2, .max = 33 },
	.p2 = { .dot_limit = 165000,
		.p2_slow = 4, .p2_fast = 2 },
};

static const struct intel_limit intel_limits_i8xx_dvo = {
	.dot = { .min = 25000, .max = 350000 },
	.vco = { .min = 908000, .max = 1512000 },
	.n = { .min = 2, .max = 16 },
	.m = { .min = 96, .max = 140 },
	.m1 = { .min = 18, .max = 26 },
	.m2 = { .min = 6, .max = 16 },
	.p = { .min = 4, .max = 128 },
	.p1 = { .min = 2, .max = 33 },
	.p2 = { .dot_limit = 165000,
		.p2_slow = 4, .p2_fast = 4 },
};

static const struct intel_limit intel_limits_i8xx_lvds = {
	.dot = { .min = 25000, .max = 350000 },
	.vco = { .min = 908000, .max = 1512000 },
	.n = { .min = 2, .max = 16 },
	.m = { .min = 96, .max = 140 },
	.m1 = { .min = 18, .max = 26 },
	.m2 = { .min = 6, .max = 16 },
	.p = { .min = 4, .max = 128 },
	.p1 = { .min = 1, .max = 6 },
	.p2 = { .dot_limit = 165000,
		.p2_slow = 14, .p2_fast = 7 },
};

static const struct intel_limit intel_limits_i9xx_sdvo = {
	.dot = { .min = 20000, .max = 400000 },
	.vco = { .min = 1400000, .max = 2800000 },
	.n = { .min = 1, .max = 6 },
	.m = { .min = 70, .max = 120 },
	.m1 = { .min = 8, .max = 18 },
	.m2 = { .min = 3, .max = 7 },
	.p = { .min = 5, .max = 80 },
	.p1 = { .min = 1, .max = 8 },
	.p2 = { .dot_limit = 200000,
		.p2_slow = 10, .p2_fast = 5 },
};

static const struct intel_limit intel_limits_i9xx_lvds = {
	.dot = { .min = 20000, .max = 400000 },
	.vco = { .min = 1400000, .max = 2800000 },
	.n = { .min = 1, .max = 6 },
	.m = { .min = 70, .max = 120 },
	.m1 = { .min = 8, .max = 18 },
	.m2 = { .min = 3, .max = 7 },
	.p = { .min = 7, .max = 98 },
	.p1 = { .min = 1, .max = 8 },
	.p2 = { .dot_limit = 112000,
		.p2_slow = 14, .p2_fast = 7 },
};


static const struct intel_limit intel_limits_g4x_sdvo = {
	.dot = { .min = 25000, .max = 270000 },
	.vco = { .min = 1750000, .max = 3500000},
	.n = { .min = 1, .max = 4 },
	.m = { .min = 104, .max = 138 },
	.m1 = { .min = 17, .max = 23 },
	.m2 = { .min = 5, .max = 11 },
	.p = { .min = 10, .max = 30 },
	.p1 = { .min = 1, .max = 3},
	.p2 = { .dot_limit = 270000,
		.p2_slow = 10,
		.p2_fast = 10
	},
};

static const struct intel_limit intel_limits_g4x_hdmi = {
	.dot = { .min = 22000, .max = 400000 },
	.vco = { .min = 1750000, .max = 3500000},
	.n = { .min = 1, .max = 4 },
	.m = { .min = 104, .max = 138 },
	.m1 = { .min = 16, .max = 23 },
	.m2 = { .min = 5, .max = 11 },
	.p = { .min = 5, .max = 80 },
	.p1 = { .min = 1, .max = 8},
	.p2 = { .dot_limit = 165000,
		.p2_slow = 10, .p2_fast = 5 },
};

static const struct intel_limit intel_limits_g4x_single_channel_lvds = {
	.dot = { .min = 20000, .max = 115000 },
	.vco = { .min = 1750000, .max = 3500000 },
	.n = { .min = 1, .max = 3 },
	.m = { .min = 104, .max = 138 },
	.m1 = { .min = 17, .max = 23 },
	.m2 = { .min = 5, .max = 11 },
	.p = { .min = 28, .max = 112 },
	.p1 = { .min = 2, .max = 8 },
	.p2 = { .dot_limit = 0,
		.p2_slow = 14, .p2_fast = 14
	},
};

static const struct intel_limit intel_limits_g4x_dual_channel_lvds = {
	.dot = { .min = 80000, .max = 224000 },
	.vco = { .min = 1750000, .max = 3500000 },
	.n = { .min = 1, .max = 3 },
	.m = { .min = 104, .max = 138 },
	.m1 = { .min = 17, .max = 23 },
	.m2 = { .min = 5, .max = 11 },
	.p = { .min = 14, .max = 42 },
	.p1 = { .min = 2, .max = 6 },
	.p2 = { .dot_limit = 0,
		.p2_slow = 7, .p2_fast = 7
	},
};

static const struct intel_limit pnv_limits_sdvo = {
	.dot = { .min = 20000, .max = 400000},
	.vco = { .min = 1700000, .max = 3500000 },
	 
	.n = { .min = 3, .max = 6 },
	.m = { .min = 2, .max = 256 },
	 
	.m1 = { .min = 0, .max = 0 },
	.m2 = { .min = 0, .max = 254 },
	.p = { .min = 5, .max = 80 },
	.p1 = { .min = 1, .max = 8 },
	.p2 = { .dot_limit = 200000,
		.p2_slow = 10, .p2_fast = 5 },
};

static const struct intel_limit pnv_limits_lvds = {
	.dot = { .min = 20000, .max = 400000 },
	.vco = { .min = 1700000, .max = 3500000 },
	.n = { .min = 3, .max = 6 },
	.m = { .min = 2, .max = 256 },
	.m1 = { .min = 0, .max = 0 },
	.m2 = { .min = 0, .max = 254 },
	.p = { .min = 7, .max = 112 },
	.p1 = { .min = 1, .max = 8 },
	.p2 = { .dot_limit = 112000,
		.p2_slow = 14, .p2_fast = 14 },
};

 
static const struct intel_limit ilk_limits_dac = {
	.dot = { .min = 25000, .max = 350000 },
	.vco = { .min = 1760000, .max = 3510000 },
	.n = { .min = 1, .max = 5 },
	.m = { .min = 79, .max = 127 },
	.m1 = { .min = 12, .max = 22 },
	.m2 = { .min = 5, .max = 9 },
	.p = { .min = 5, .max = 80 },
	.p1 = { .min = 1, .max = 8 },
	.p2 = { .dot_limit = 225000,
		.p2_slow = 10, .p2_fast = 5 },
};

static const struct intel_limit ilk_limits_single_lvds = {
	.dot = { .min = 25000, .max = 350000 },
	.vco = { .min = 1760000, .max = 3510000 },
	.n = { .min = 1, .max = 3 },
	.m = { .min = 79, .max = 118 },
	.m1 = { .min = 12, .max = 22 },
	.m2 = { .min = 5, .max = 9 },
	.p = { .min = 28, .max = 112 },
	.p1 = { .min = 2, .max = 8 },
	.p2 = { .dot_limit = 225000,
		.p2_slow = 14, .p2_fast = 14 },
};

static const struct intel_limit ilk_limits_dual_lvds = {
	.dot = { .min = 25000, .max = 350000 },
	.vco = { .min = 1760000, .max = 3510000 },
	.n = { .min = 1, .max = 3 },
	.m = { .min = 79, .max = 127 },
	.m1 = { .min = 12, .max = 22 },
	.m2 = { .min = 5, .max = 9 },
	.p = { .min = 14, .max = 56 },
	.p1 = { .min = 2, .max = 8 },
	.p2 = { .dot_limit = 225000,
		.p2_slow = 7, .p2_fast = 7 },
};

 
static const struct intel_limit ilk_limits_single_lvds_100m = {
	.dot = { .min = 25000, .max = 350000 },
	.vco = { .min = 1760000, .max = 3510000 },
	.n = { .min = 1, .max = 2 },
	.m = { .min = 79, .max = 126 },
	.m1 = { .min = 12, .max = 22 },
	.m2 = { .min = 5, .max = 9 },
	.p = { .min = 28, .max = 112 },
	.p1 = { .min = 2, .max = 8 },
	.p2 = { .dot_limit = 225000,
		.p2_slow = 14, .p2_fast = 14 },
};

static const struct intel_limit ilk_limits_dual_lvds_100m = {
	.dot = { .min = 25000, .max = 350000 },
	.vco = { .min = 1760000, .max = 3510000 },
	.n = { .min = 1, .max = 3 },
	.m = { .min = 79, .max = 126 },
	.m1 = { .min = 12, .max = 22 },
	.m2 = { .min = 5, .max = 9 },
	.p = { .min = 14, .max = 42 },
	.p1 = { .min = 2, .max = 6 },
	.p2 = { .dot_limit = 225000,
		.p2_slow = 7, .p2_fast = 7 },
};

static const struct intel_limit intel_limits_vlv = {
	  
	.dot = { .min = 25000, .max = 270000 },
	.vco = { .min = 4000000, .max = 6000000 },
	.n = { .min = 1, .max = 7 },
	.m1 = { .min = 2, .max = 3 },
	.m2 = { .min = 11, .max = 156 },
	.p1 = { .min = 2, .max = 3 },
	.p2 = { .p2_slow = 2, .p2_fast = 20 },  
};

static const struct intel_limit intel_limits_chv = {
	 
	.dot = { .min = 25000, .max = 540000 },
	.vco = { .min = 4800000, .max = 6480000 },
	.n = { .min = 1, .max = 1 },
	.m1 = { .min = 2, .max = 2 },
	.m2 = { .min = 24 << 22, .max = 175 << 22 },
	.p1 = { .min = 2, .max = 4 },
	.p2 = {	.p2_slow = 1, .p2_fast = 14 },
};

static const struct intel_limit intel_limits_bxt = {
	.dot = { .min = 25000, .max = 594000 },
	.vco = { .min = 4800000, .max = 6700000 },
	.n = { .min = 1, .max = 1 },
	.m1 = { .min = 2, .max = 2 },
	 
	.m2 = { .min = 2 << 22, .max = 255 << 22 },
	.p1 = { .min = 2, .max = 4 },
	.p2 = { .p2_slow = 1, .p2_fast = 20 },
};

 
 
int pnv_calc_dpll_params(int refclk, struct dpll *clock)
{
	clock->m = clock->m2 + 2;
	clock->p = clock->p1 * clock->p2;
	if (WARN_ON(clock->n == 0 || clock->p == 0))
		return 0;
	clock->vco = DIV_ROUND_CLOSEST(refclk * clock->m, clock->n);
	clock->dot = DIV_ROUND_CLOSEST(clock->vco, clock->p);

	return clock->dot;
}

static u32 i9xx_dpll_compute_m(const struct dpll *dpll)
{
	return 5 * (dpll->m1 + 2) + (dpll->m2 + 2);
}

int i9xx_calc_dpll_params(int refclk, struct dpll *clock)
{
	clock->m = i9xx_dpll_compute_m(clock);
	clock->p = clock->p1 * clock->p2;
	if (WARN_ON(clock->n + 2 == 0 || clock->p == 0))
		return 0;
	clock->vco = DIV_ROUND_CLOSEST(refclk * clock->m, clock->n + 2);
	clock->dot = DIV_ROUND_CLOSEST(clock->vco, clock->p);

	return clock->dot;
}

int vlv_calc_dpll_params(int refclk, struct dpll *clock)
{
	clock->m = clock->m1 * clock->m2;
	clock->p = clock->p1 * clock->p2 * 5;
	if (WARN_ON(clock->n == 0 || clock->p == 0))
		return 0;
	clock->vco = DIV_ROUND_CLOSEST(refclk * clock->m, clock->n);
	clock->dot = DIV_ROUND_CLOSEST(clock->vco, clock->p);

	return clock->dot;
}

int chv_calc_dpll_params(int refclk, struct dpll *clock)
{
	clock->m = clock->m1 * clock->m2;
	clock->p = clock->p1 * clock->p2 * 5;
	if (WARN_ON(clock->n == 0 || clock->p == 0))
		return 0;
	clock->vco = DIV_ROUND_CLOSEST_ULL(mul_u32_u32(refclk, clock->m),
					   clock->n << 22);
	clock->dot = DIV_ROUND_CLOSEST(clock->vco, clock->p);

	return clock->dot;
}

 
static bool intel_pll_is_valid(struct drm_i915_private *dev_priv,
			       const struct intel_limit *limit,
			       const struct dpll *clock)
{
	if (clock->n < limit->n.min || limit->n.max < clock->n)
		return false;
	if (clock->p1 < limit->p1.min || limit->p1.max < clock->p1)
		return false;
	if (clock->m2 < limit->m2.min || limit->m2.max < clock->m2)
		return false;
	if (clock->m1 < limit->m1.min || limit->m1.max < clock->m1)
		return false;

	if (!IS_PINEVIEW(dev_priv) && !IS_LP(dev_priv))
		if (clock->m1 <= clock->m2)
			return false;

	if (!IS_LP(dev_priv)) {
		if (clock->p < limit->p.min || limit->p.max < clock->p)
			return false;
		if (clock->m < limit->m.min || limit->m.max < clock->m)
			return false;
	}

	if (clock->vco < limit->vco.min || limit->vco.max < clock->vco)
		return false;
	 
	if (clock->dot < limit->dot.min || limit->dot.max < clock->dot)
		return false;

	return true;
}

static int
i9xx_select_p2_div(const struct intel_limit *limit,
		   const struct intel_crtc_state *crtc_state,
		   int target)
{
	struct drm_i915_private *dev_priv = to_i915(crtc_state->uapi.crtc->dev);

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS)) {
		 
		if (intel_is_dual_link_lvds(dev_priv))
			return limit->p2.p2_fast;
		else
			return limit->p2.p2_slow;
	} else {
		if (target < limit->p2.dot_limit)
			return limit->p2.p2_slow;
		else
			return limit->p2.p2_fast;
	}
}

 
static bool
i9xx_find_best_dpll(const struct intel_limit *limit,
		    struct intel_crtc_state *crtc_state,
		    int target, int refclk,
		    const struct dpll *match_clock,
		    struct dpll *best_clock)
{
	struct drm_device *dev = crtc_state->uapi.crtc->dev;
	struct dpll clock;
	int err = target;

	memset(best_clock, 0, sizeof(*best_clock));

	clock.p2 = i9xx_select_p2_div(limit, crtc_state, target);

	for (clock.m1 = limit->m1.min; clock.m1 <= limit->m1.max;
	     clock.m1++) {
		for (clock.m2 = limit->m2.min;
		     clock.m2 <= limit->m2.max; clock.m2++) {
			if (clock.m2 >= clock.m1)
				break;
			for (clock.n = limit->n.min;
			     clock.n <= limit->n.max; clock.n++) {
				for (clock.p1 = limit->p1.min;
					clock.p1 <= limit->p1.max; clock.p1++) {
					int this_err;

					i9xx_calc_dpll_params(refclk, &clock);
					if (!intel_pll_is_valid(to_i915(dev),
								limit,
								&clock))
						continue;
					if (match_clock &&
					    clock.p != match_clock->p)
						continue;

					this_err = abs(clock.dot - target);
					if (this_err < err) {
						*best_clock = clock;
						err = this_err;
					}
				}
			}
		}
	}

	return (err != target);
}

 
static bool
pnv_find_best_dpll(const struct intel_limit *limit,
		   struct intel_crtc_state *crtc_state,
		   int target, int refclk,
		   const struct dpll *match_clock,
		   struct dpll *best_clock)
{
	struct drm_device *dev = crtc_state->uapi.crtc->dev;
	struct dpll clock;
	int err = target;

	memset(best_clock, 0, sizeof(*best_clock));

	clock.p2 = i9xx_select_p2_div(limit, crtc_state, target);

	for (clock.m1 = limit->m1.min; clock.m1 <= limit->m1.max;
	     clock.m1++) {
		for (clock.m2 = limit->m2.min;
		     clock.m2 <= limit->m2.max; clock.m2++) {
			for (clock.n = limit->n.min;
			     clock.n <= limit->n.max; clock.n++) {
				for (clock.p1 = limit->p1.min;
					clock.p1 <= limit->p1.max; clock.p1++) {
					int this_err;

					pnv_calc_dpll_params(refclk, &clock);
					if (!intel_pll_is_valid(to_i915(dev),
								limit,
								&clock))
						continue;
					if (match_clock &&
					    clock.p != match_clock->p)
						continue;

					this_err = abs(clock.dot - target);
					if (this_err < err) {
						*best_clock = clock;
						err = this_err;
					}
				}
			}
		}
	}

	return (err != target);
}

 
static bool
g4x_find_best_dpll(const struct intel_limit *limit,
		   struct intel_crtc_state *crtc_state,
		   int target, int refclk,
		   const struct dpll *match_clock,
		   struct dpll *best_clock)
{
	struct drm_device *dev = crtc_state->uapi.crtc->dev;
	struct dpll clock;
	int max_n;
	bool found = false;
	 
	int err_most = (target >> 8) + (target >> 9);

	memset(best_clock, 0, sizeof(*best_clock));

	clock.p2 = i9xx_select_p2_div(limit, crtc_state, target);

	max_n = limit->n.max;
	 
	for (clock.n = limit->n.min; clock.n <= max_n; clock.n++) {
		 
		for (clock.m1 = limit->m1.max;
		     clock.m1 >= limit->m1.min; clock.m1--) {
			for (clock.m2 = limit->m2.max;
			     clock.m2 >= limit->m2.min; clock.m2--) {
				for (clock.p1 = limit->p1.max;
				     clock.p1 >= limit->p1.min; clock.p1--) {
					int this_err;

					i9xx_calc_dpll_params(refclk, &clock);
					if (!intel_pll_is_valid(to_i915(dev),
								limit,
								&clock))
						continue;

					this_err = abs(clock.dot - target);
					if (this_err < err_most) {
						*best_clock = clock;
						err_most = this_err;
						max_n = clock.n;
						found = true;
					}
				}
			}
		}
	}
	return found;
}

 
static bool vlv_PLL_is_optimal(struct drm_device *dev, int target_freq,
			       const struct dpll *calculated_clock,
			       const struct dpll *best_clock,
			       unsigned int best_error_ppm,
			       unsigned int *error_ppm)
{
	 
	if (IS_CHERRYVIEW(to_i915(dev))) {
		*error_ppm = 0;

		return calculated_clock->p > best_clock->p;
	}

	if (drm_WARN_ON_ONCE(dev, !target_freq))
		return false;

	*error_ppm = div_u64(1000000ULL *
				abs(target_freq - calculated_clock->dot),
			     target_freq);
	 
	if (*error_ppm < 100 && calculated_clock->p > best_clock->p) {
		*error_ppm = 0;

		return true;
	}

	return *error_ppm + 10 < best_error_ppm;
}

 
static bool
vlv_find_best_dpll(const struct intel_limit *limit,
		   struct intel_crtc_state *crtc_state,
		   int target, int refclk,
		   const struct dpll *match_clock,
		   struct dpll *best_clock)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_device *dev = crtc->base.dev;
	struct dpll clock;
	unsigned int bestppm = 1000000;
	 
	int max_n = min(limit->n.max, refclk / 19200);
	bool found = false;

	memset(best_clock, 0, sizeof(*best_clock));

	 
	for (clock.n = limit->n.min; clock.n <= max_n; clock.n++) {
		for (clock.p1 = limit->p1.max; clock.p1 >= limit->p1.min; clock.p1--) {
			for (clock.p2 = limit->p2.p2_fast; clock.p2 >= limit->p2.p2_slow;
			     clock.p2 -= clock.p2 > 10 ? 2 : 1) {
				clock.p = clock.p1 * clock.p2 * 5;
				 
				for (clock.m1 = limit->m1.min; clock.m1 <= limit->m1.max; clock.m1++) {
					unsigned int ppm;

					clock.m2 = DIV_ROUND_CLOSEST(target * clock.p * clock.n,
								     refclk * clock.m1);

					vlv_calc_dpll_params(refclk, &clock);

					if (!intel_pll_is_valid(to_i915(dev),
								limit,
								&clock))
						continue;

					if (!vlv_PLL_is_optimal(dev, target,
								&clock,
								best_clock,
								bestppm, &ppm))
						continue;

					*best_clock = clock;
					bestppm = ppm;
					found = true;
				}
			}
		}
	}

	return found;
}

 
static bool
chv_find_best_dpll(const struct intel_limit *limit,
		   struct intel_crtc_state *crtc_state,
		   int target, int refclk,
		   const struct dpll *match_clock,
		   struct dpll *best_clock)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_device *dev = crtc->base.dev;
	unsigned int best_error_ppm;
	struct dpll clock;
	u64 m2;
	int found = false;

	memset(best_clock, 0, sizeof(*best_clock));
	best_error_ppm = 1000000;

	 
	clock.n = 1;
	clock.m1 = 2;

	for (clock.p1 = limit->p1.max; clock.p1 >= limit->p1.min; clock.p1--) {
		for (clock.p2 = limit->p2.p2_fast;
				clock.p2 >= limit->p2.p2_slow;
				clock.p2 -= clock.p2 > 10 ? 2 : 1) {
			unsigned int error_ppm;

			clock.p = clock.p1 * clock.p2 * 5;

			m2 = DIV_ROUND_CLOSEST_ULL(mul_u32_u32(target, clock.p * clock.n) << 22,
						   refclk * clock.m1);

			if (m2 > INT_MAX/clock.m1)
				continue;

			clock.m2 = m2;

			chv_calc_dpll_params(refclk, &clock);

			if (!intel_pll_is_valid(to_i915(dev), limit, &clock))
				continue;

			if (!vlv_PLL_is_optimal(dev, target, &clock, best_clock,
						best_error_ppm, &error_ppm))
				continue;

			*best_clock = clock;
			best_error_ppm = error_ppm;
			found = true;
		}
	}

	return found;
}

bool bxt_find_best_dpll(struct intel_crtc_state *crtc_state,
			struct dpll *best_clock)
{
	const struct intel_limit *limit = &intel_limits_bxt;
	int refclk = 100000;

	return chv_find_best_dpll(limit, crtc_state,
				  crtc_state->port_clock, refclk,
				  NULL, best_clock);
}

u32 i9xx_dpll_compute_fp(const struct dpll *dpll)
{
	return dpll->n << 16 | dpll->m1 << 8 | dpll->m2;
}

static u32 pnv_dpll_compute_fp(const struct dpll *dpll)
{
	return (1 << dpll->n) << 16 | dpll->m2;
}

static void i9xx_update_pll_dividers(struct intel_crtc_state *crtc_state,
				     const struct dpll *clock,
				     const struct dpll *reduced_clock)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 fp, fp2;

	if (IS_PINEVIEW(dev_priv)) {
		fp = pnv_dpll_compute_fp(clock);
		fp2 = pnv_dpll_compute_fp(reduced_clock);
	} else {
		fp = i9xx_dpll_compute_fp(clock);
		fp2 = i9xx_dpll_compute_fp(reduced_clock);
	}

	crtc_state->dpll_hw_state.fp0 = fp;
	crtc_state->dpll_hw_state.fp1 = fp2;
}

static void i9xx_compute_dpll(struct intel_crtc_state *crtc_state,
			      const struct dpll *clock,
			      const struct dpll *reduced_clock)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 dpll;

	i9xx_update_pll_dividers(crtc_state, clock, reduced_clock);

	dpll = DPLL_VGA_MODE_DIS;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS))
		dpll |= DPLLB_MODE_LVDS;
	else
		dpll |= DPLLB_MODE_DAC_SERIAL;

	if (IS_I945G(dev_priv) || IS_I945GM(dev_priv) ||
	    IS_G33(dev_priv) || IS_PINEVIEW(dev_priv)) {
		dpll |= (crtc_state->pixel_multiplier - 1)
			<< SDVO_MULTIPLIER_SHIFT_HIRES;
	}

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_SDVO) ||
	    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		dpll |= DPLL_SDVO_HIGH_SPEED;

	if (intel_crtc_has_dp_encoder(crtc_state))
		dpll |= DPLL_SDVO_HIGH_SPEED;

	 
	if (IS_G4X(dev_priv)) {
		dpll |= (1 << (clock->p1 - 1)) << DPLL_FPA01_P1_POST_DIV_SHIFT;
		dpll |= (1 << (reduced_clock->p1 - 1)) << DPLL_FPA1_P1_POST_DIV_SHIFT;
	} else if (IS_PINEVIEW(dev_priv)) {
		dpll |= (1 << (clock->p1 - 1)) << DPLL_FPA01_P1_POST_DIV_SHIFT_PINEVIEW;
		WARN_ON(reduced_clock->p1 != clock->p1);
	} else {
		dpll |= (1 << (clock->p1 - 1)) << DPLL_FPA01_P1_POST_DIV_SHIFT;
		WARN_ON(reduced_clock->p1 != clock->p1);
	}

	switch (clock->p2) {
	case 5:
		dpll |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_5;
		break;
	case 7:
		dpll |= DPLLB_LVDS_P2_CLOCK_DIV_7;
		break;
	case 10:
		dpll |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_10;
		break;
	case 14:
		dpll |= DPLLB_LVDS_P2_CLOCK_DIV_14;
		break;
	}
	WARN_ON(reduced_clock->p2 != clock->p2);

	if (DISPLAY_VER(dev_priv) >= 4)
		dpll |= (6 << PLL_LOAD_PULSE_PHASE_SHIFT);

	if (crtc_state->sdvo_tv_clock)
		dpll |= PLL_REF_INPUT_TVCLKINBC;
	else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS) &&
		 intel_panel_use_ssc(dev_priv))
		dpll |= PLLB_REF_INPUT_SPREADSPECTRUMIN;
	else
		dpll |= PLL_REF_INPUT_DREFCLK;

	dpll |= DPLL_VCO_ENABLE;
	crtc_state->dpll_hw_state.dpll = dpll;

	if (DISPLAY_VER(dev_priv) >= 4) {
		u32 dpll_md = (crtc_state->pixel_multiplier - 1)
			<< DPLL_MD_UDI_MULTIPLIER_SHIFT;
		crtc_state->dpll_hw_state.dpll_md = dpll_md;
	}
}

static void i8xx_compute_dpll(struct intel_crtc_state *crtc_state,
			      const struct dpll *clock,
			      const struct dpll *reduced_clock)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 dpll;

	i9xx_update_pll_dividers(crtc_state, clock, reduced_clock);

	dpll = DPLL_VGA_MODE_DIS;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS)) {
		dpll |= (1 << (clock->p1 - 1)) << DPLL_FPA01_P1_POST_DIV_SHIFT;
	} else {
		if (clock->p1 == 2)
			dpll |= PLL_P1_DIVIDE_BY_TWO;
		else
			dpll |= (clock->p1 - 2) << DPLL_FPA01_P1_POST_DIV_SHIFT;
		if (clock->p2 == 4)
			dpll |= PLL_P2_DIVIDE_BY_4;
	}
	WARN_ON(reduced_clock->p1 != clock->p1);
	WARN_ON(reduced_clock->p2 != clock->p2);

	 
	if (IS_I830(dev_priv) ||
	    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DVO))
		dpll |= DPLL_DVO_2X_MODE;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS) &&
	    intel_panel_use_ssc(dev_priv))
		dpll |= PLLB_REF_INPUT_SPREADSPECTRUMIN;
	else
		dpll |= PLL_REF_INPUT_DREFCLK;

	dpll |= DPLL_VCO_ENABLE;
	crtc_state->dpll_hw_state.dpll = dpll;
}

static int hsw_crtc_compute_clock(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	struct intel_encoder *encoder =
		intel_get_crtc_new_encoder(state, crtc_state);
	int ret;

	if (DISPLAY_VER(dev_priv) < 11 &&
	    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DSI))
		return 0;

	ret = intel_compute_shared_dplls(state, crtc, encoder);
	if (ret)
		return ret;

	 
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DSI))
		return 0;

	 
	if (!crtc_state->has_pch_encoder)
		crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return 0;
}

static int hsw_crtc_get_shared_dpll(struct intel_atomic_state *state,
				    struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	struct intel_encoder *encoder =
		intel_get_crtc_new_encoder(state, crtc_state);

	if (DISPLAY_VER(dev_priv) < 11 &&
	    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DSI))
		return 0;

	return intel_reserve_shared_dplls(state, crtc, encoder);
}

static int dg2_crtc_compute_clock(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	struct intel_encoder *encoder =
		intel_get_crtc_new_encoder(state, crtc_state);
	int ret;

	ret = intel_mpllb_calc_state(crtc_state, encoder);
	if (ret)
		return ret;

	crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return 0;
}

static int mtl_crtc_compute_clock(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct drm_i915_private *i915 = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	struct intel_encoder *encoder =
		intel_get_crtc_new_encoder(state, crtc_state);
	enum phy phy = intel_port_to_phy(i915, encoder->port);
	int ret;

	ret = intel_cx0pll_calc_state(crtc_state, encoder);
	if (ret)
		return ret;

	 
	if (intel_is_c10phy(i915, phy))
		crtc_state->port_clock = intel_c10pll_calc_port_clock(encoder, &crtc_state->cx0pll_state.c10);
	else
		crtc_state->port_clock = intel_c20pll_calc_port_clock(encoder, &crtc_state->cx0pll_state.c20);

	crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return 0;
}

static bool ilk_needs_fb_cb_tune(const struct dpll *dpll, int factor)
{
	return dpll->m < factor * dpll->n;
}

static void ilk_update_pll_dividers(struct intel_crtc_state *crtc_state,
				    const struct dpll *clock,
				    const struct dpll *reduced_clock)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 fp, fp2;
	int factor;

	 
	factor = 21;
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS)) {
		if ((intel_panel_use_ssc(dev_priv) &&
		     dev_priv->display.vbt.lvds_ssc_freq == 100000) ||
		    (HAS_PCH_IBX(dev_priv) &&
		     intel_is_dual_link_lvds(dev_priv)))
			factor = 25;
	} else if (crtc_state->sdvo_tv_clock) {
		factor = 20;
	}

	fp = i9xx_dpll_compute_fp(clock);
	if (ilk_needs_fb_cb_tune(clock, factor))
		fp |= FP_CB_TUNE;

	fp2 = i9xx_dpll_compute_fp(reduced_clock);
	if (ilk_needs_fb_cb_tune(reduced_clock, factor))
		fp2 |= FP_CB_TUNE;

	crtc_state->dpll_hw_state.fp0 = fp;
	crtc_state->dpll_hw_state.fp1 = fp2;
}

static void ilk_compute_dpll(struct intel_crtc_state *crtc_state,
			     const struct dpll *clock,
			     const struct dpll *reduced_clock)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 dpll;

	ilk_update_pll_dividers(crtc_state, clock, reduced_clock);

	dpll = 0;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS))
		dpll |= DPLLB_MODE_LVDS;
	else
		dpll |= DPLLB_MODE_DAC_SERIAL;

	dpll |= (crtc_state->pixel_multiplier - 1)
		<< PLL_REF_SDVO_HDMI_MULTIPLIER_SHIFT;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_SDVO) ||
	    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		dpll |= DPLL_SDVO_HIGH_SPEED;

	if (intel_crtc_has_dp_encoder(crtc_state))
		dpll |= DPLL_SDVO_HIGH_SPEED;

	 
	if (INTEL_NUM_PIPES(dev_priv) == 3 &&
	    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_ANALOG))
		dpll |= DPLL_SDVO_HIGH_SPEED;

	 
	dpll |= (1 << (clock->p1 - 1)) << DPLL_FPA01_P1_POST_DIV_SHIFT;
	 
	dpll |= (1 << (reduced_clock->p1 - 1)) << DPLL_FPA1_P1_POST_DIV_SHIFT;

	switch (clock->p2) {
	case 5:
		dpll |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_5;
		break;
	case 7:
		dpll |= DPLLB_LVDS_P2_CLOCK_DIV_7;
		break;
	case 10:
		dpll |= DPLL_DAC_SERIAL_P2_CLOCK_DIV_10;
		break;
	case 14:
		dpll |= DPLLB_LVDS_P2_CLOCK_DIV_14;
		break;
	}
	WARN_ON(reduced_clock->p2 != clock->p2);

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS) &&
	    intel_panel_use_ssc(dev_priv))
		dpll |= PLLB_REF_INPUT_SPREADSPECTRUMIN;
	else
		dpll |= PLL_REF_INPUT_DREFCLK;

	dpll |= DPLL_VCO_ENABLE;

	crtc_state->dpll_hw_state.dpll = dpll;
}

static int ilk_crtc_compute_clock(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	const struct intel_limit *limit;
	int refclk = 120000;
	int ret;

	 
	if (!crtc_state->has_pch_encoder)
		return 0;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS)) {
		if (intel_panel_use_ssc(dev_priv)) {
			drm_dbg_kms(&dev_priv->drm,
				    "using SSC reference clock of %d kHz\n",
				    dev_priv->display.vbt.lvds_ssc_freq);
			refclk = dev_priv->display.vbt.lvds_ssc_freq;
		}

		if (intel_is_dual_link_lvds(dev_priv)) {
			if (refclk == 100000)
				limit = &ilk_limits_dual_lvds_100m;
			else
				limit = &ilk_limits_dual_lvds;
		} else {
			if (refclk == 100000)
				limit = &ilk_limits_single_lvds_100m;
			else
				limit = &ilk_limits_single_lvds;
		}
	} else {
		limit = &ilk_limits_dac;
	}

	if (!crtc_state->clock_set &&
	    !g4x_find_best_dpll(limit, crtc_state, crtc_state->port_clock,
				refclk, NULL, &crtc_state->dpll))
		return -EINVAL;

	ilk_compute_dpll(crtc_state, &crtc_state->dpll,
			 &crtc_state->dpll);

	ret = intel_compute_shared_dplls(state, crtc, NULL);
	if (ret)
		return ret;

	crtc_state->port_clock = crtc_state->dpll.dot;
	crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return ret;
}

static int ilk_crtc_get_shared_dpll(struct intel_atomic_state *state,
				    struct intel_crtc *crtc)
{
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);

	 
	if (!crtc_state->has_pch_encoder)
		return 0;

	return intel_reserve_shared_dplls(state, crtc, NULL);
}

void vlv_compute_dpll(struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);

	crtc_state->dpll_hw_state.dpll = DPLL_INTEGRATED_REF_CLK_VLV |
		DPLL_REF_CLK_ENABLE_VLV | DPLL_VGA_MODE_DIS;
	if (crtc->pipe != PIPE_A)
		crtc_state->dpll_hw_state.dpll |= DPLL_INTEGRATED_CRI_CLK_VLV;

	 
	if (!intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DSI))
		crtc_state->dpll_hw_state.dpll |= DPLL_VCO_ENABLE |
			DPLL_EXT_BUFFER_ENABLE_VLV;

	crtc_state->dpll_hw_state.dpll_md =
		(crtc_state->pixel_multiplier - 1) << DPLL_MD_UDI_MULTIPLIER_SHIFT;
}

void chv_compute_dpll(struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);

	crtc_state->dpll_hw_state.dpll = DPLL_SSC_REF_CLK_CHV |
		DPLL_REF_CLK_ENABLE_VLV | DPLL_VGA_MODE_DIS;
	if (crtc->pipe != PIPE_A)
		crtc_state->dpll_hw_state.dpll |= DPLL_INTEGRATED_CRI_CLK_VLV;

	 
	if (!intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DSI))
		crtc_state->dpll_hw_state.dpll |= DPLL_VCO_ENABLE;

	crtc_state->dpll_hw_state.dpll_md =
		(crtc_state->pixel_multiplier - 1) << DPLL_MD_UDI_MULTIPLIER_SHIFT;
}

static int chv_crtc_compute_clock(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	const struct intel_limit *limit = &intel_limits_chv;
	int refclk = 100000;

	if (!crtc_state->clock_set &&
	    !chv_find_best_dpll(limit, crtc_state, crtc_state->port_clock,
				refclk, NULL, &crtc_state->dpll))
		return -EINVAL;

	chv_compute_dpll(crtc_state);

	 
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DSI))
		return 0;

	crtc_state->port_clock = crtc_state->dpll.dot;
	crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return 0;
}

static int vlv_crtc_compute_clock(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	const struct intel_limit *limit = &intel_limits_vlv;
	int refclk = 100000;

	if (!crtc_state->clock_set &&
	    !vlv_find_best_dpll(limit, crtc_state, crtc_state->port_clock,
				refclk, NULL, &crtc_state->dpll)) {
		return -EINVAL;
	}

	vlv_compute_dpll(crtc_state);

	 
	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DSI))
		return 0;

	crtc_state->port_clock = crtc_state->dpll.dot;
	crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return 0;
}

static int g4x_crtc_compute_clock(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	const struct intel_limit *limit;
	int refclk = 96000;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS)) {
		if (intel_panel_use_ssc(dev_priv)) {
			refclk = dev_priv->display.vbt.lvds_ssc_freq;
			drm_dbg_kms(&dev_priv->drm,
				    "using SSC reference clock of %d kHz\n",
				    refclk);
		}

		if (intel_is_dual_link_lvds(dev_priv))
			limit = &intel_limits_g4x_dual_channel_lvds;
		else
			limit = &intel_limits_g4x_single_channel_lvds;
	} else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI) ||
		   intel_crtc_has_type(crtc_state, INTEL_OUTPUT_ANALOG)) {
		limit = &intel_limits_g4x_hdmi;
	} else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_SDVO)) {
		limit = &intel_limits_g4x_sdvo;
	} else {
		 
		limit = &intel_limits_i9xx_sdvo;
	}

	if (!crtc_state->clock_set &&
	    !g4x_find_best_dpll(limit, crtc_state, crtc_state->port_clock,
				refclk, NULL, &crtc_state->dpll))
		return -EINVAL;

	i9xx_compute_dpll(crtc_state, &crtc_state->dpll,
			  &crtc_state->dpll);

	crtc_state->port_clock = crtc_state->dpll.dot;
	 
	if (!intel_crtc_has_type(crtc_state, INTEL_OUTPUT_TVOUT))
		crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return 0;
}

static int pnv_crtc_compute_clock(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	const struct intel_limit *limit;
	int refclk = 96000;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS)) {
		if (intel_panel_use_ssc(dev_priv)) {
			refclk = dev_priv->display.vbt.lvds_ssc_freq;
			drm_dbg_kms(&dev_priv->drm,
				    "using SSC reference clock of %d kHz\n",
				    refclk);
		}

		limit = &pnv_limits_lvds;
	} else {
		limit = &pnv_limits_sdvo;
	}

	if (!crtc_state->clock_set &&
	    !pnv_find_best_dpll(limit, crtc_state, crtc_state->port_clock,
				refclk, NULL, &crtc_state->dpll))
		return -EINVAL;

	i9xx_compute_dpll(crtc_state, &crtc_state->dpll,
			  &crtc_state->dpll);

	crtc_state->port_clock = crtc_state->dpll.dot;
	crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return 0;
}

static int i9xx_crtc_compute_clock(struct intel_atomic_state *state,
				   struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	const struct intel_limit *limit;
	int refclk = 96000;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS)) {
		if (intel_panel_use_ssc(dev_priv)) {
			refclk = dev_priv->display.vbt.lvds_ssc_freq;
			drm_dbg_kms(&dev_priv->drm,
				    "using SSC reference clock of %d kHz\n",
				    refclk);
		}

		limit = &intel_limits_i9xx_lvds;
	} else {
		limit = &intel_limits_i9xx_sdvo;
	}

	if (!crtc_state->clock_set &&
	    !i9xx_find_best_dpll(limit, crtc_state, crtc_state->port_clock,
				 refclk, NULL, &crtc_state->dpll))
		return -EINVAL;

	i9xx_compute_dpll(crtc_state, &crtc_state->dpll,
			  &crtc_state->dpll);

	crtc_state->port_clock = crtc_state->dpll.dot;
	 
	if (!intel_crtc_has_type(crtc_state, INTEL_OUTPUT_TVOUT))
		crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return 0;
}

static int i8xx_crtc_compute_clock(struct intel_atomic_state *state,
				   struct intel_crtc *crtc)
{
	struct drm_i915_private *dev_priv = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	const struct intel_limit *limit;
	int refclk = 48000;

	if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_LVDS)) {
		if (intel_panel_use_ssc(dev_priv)) {
			refclk = dev_priv->display.vbt.lvds_ssc_freq;
			drm_dbg_kms(&dev_priv->drm,
				    "using SSC reference clock of %d kHz\n",
				    refclk);
		}

		limit = &intel_limits_i8xx_lvds;
	} else if (intel_crtc_has_type(crtc_state, INTEL_OUTPUT_DVO)) {
		limit = &intel_limits_i8xx_dvo;
	} else {
		limit = &intel_limits_i8xx_dac;
	}

	if (!crtc_state->clock_set &&
	    !i9xx_find_best_dpll(limit, crtc_state, crtc_state->port_clock,
				 refclk, NULL, &crtc_state->dpll))
		return -EINVAL;

	i8xx_compute_dpll(crtc_state, &crtc_state->dpll,
			  &crtc_state->dpll);

	crtc_state->port_clock = crtc_state->dpll.dot;
	crtc_state->hw.adjusted_mode.crtc_clock = intel_crtc_dotclock(crtc_state);

	return 0;
}

static const struct intel_dpll_funcs mtl_dpll_funcs = {
	.crtc_compute_clock = mtl_crtc_compute_clock,
};

static const struct intel_dpll_funcs dg2_dpll_funcs = {
	.crtc_compute_clock = dg2_crtc_compute_clock,
};

static const struct intel_dpll_funcs hsw_dpll_funcs = {
	.crtc_compute_clock = hsw_crtc_compute_clock,
	.crtc_get_shared_dpll = hsw_crtc_get_shared_dpll,
};

static const struct intel_dpll_funcs ilk_dpll_funcs = {
	.crtc_compute_clock = ilk_crtc_compute_clock,
	.crtc_get_shared_dpll = ilk_crtc_get_shared_dpll,
};

static const struct intel_dpll_funcs chv_dpll_funcs = {
	.crtc_compute_clock = chv_crtc_compute_clock,
};

static const struct intel_dpll_funcs vlv_dpll_funcs = {
	.crtc_compute_clock = vlv_crtc_compute_clock,
};

static const struct intel_dpll_funcs g4x_dpll_funcs = {
	.crtc_compute_clock = g4x_crtc_compute_clock,
};

static const struct intel_dpll_funcs pnv_dpll_funcs = {
	.crtc_compute_clock = pnv_crtc_compute_clock,
};

static const struct intel_dpll_funcs i9xx_dpll_funcs = {
	.crtc_compute_clock = i9xx_crtc_compute_clock,
};

static const struct intel_dpll_funcs i8xx_dpll_funcs = {
	.crtc_compute_clock = i8xx_crtc_compute_clock,
};

int intel_dpll_crtc_compute_clock(struct intel_atomic_state *state,
				  struct intel_crtc *crtc)
{
	struct drm_i915_private *i915 = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	int ret;

	drm_WARN_ON(&i915->drm, !intel_crtc_needs_modeset(crtc_state));

	memset(&crtc_state->dpll_hw_state, 0,
	       sizeof(crtc_state->dpll_hw_state));

	if (!crtc_state->hw.enable)
		return 0;

	ret = i915->display.funcs.dpll->crtc_compute_clock(state, crtc);
	if (ret) {
		drm_dbg_kms(&i915->drm, "[CRTC:%d:%s] Couldn't calculate DPLL settings\n",
			    crtc->base.base.id, crtc->base.name);
		return ret;
	}

	return 0;
}

int intel_dpll_crtc_get_shared_dpll(struct intel_atomic_state *state,
				    struct intel_crtc *crtc)
{
	struct drm_i915_private *i915 = to_i915(state->base.dev);
	struct intel_crtc_state *crtc_state =
		intel_atomic_get_new_crtc_state(state, crtc);
	int ret;

	drm_WARN_ON(&i915->drm, !intel_crtc_needs_modeset(crtc_state));
	drm_WARN_ON(&i915->drm, !crtc_state->hw.enable && crtc_state->shared_dpll);

	if (!crtc_state->hw.enable || crtc_state->shared_dpll)
		return 0;

	if (!i915->display.funcs.dpll->crtc_get_shared_dpll)
		return 0;

	ret = i915->display.funcs.dpll->crtc_get_shared_dpll(state, crtc);
	if (ret) {
		drm_dbg_kms(&i915->drm, "[CRTC:%d:%s] Couldn't get a shared DPLL\n",
			    crtc->base.base.id, crtc->base.name);
		return ret;
	}

	return 0;
}

void
intel_dpll_init_clock_hook(struct drm_i915_private *dev_priv)
{
	if (DISPLAY_VER(dev_priv) >= 14)
		dev_priv->display.funcs.dpll = &mtl_dpll_funcs;
	else if (IS_DG2(dev_priv))
		dev_priv->display.funcs.dpll = &dg2_dpll_funcs;
	else if (DISPLAY_VER(dev_priv) >= 9 || HAS_DDI(dev_priv))
		dev_priv->display.funcs.dpll = &hsw_dpll_funcs;
	else if (HAS_PCH_SPLIT(dev_priv))
		dev_priv->display.funcs.dpll = &ilk_dpll_funcs;
	else if (IS_CHERRYVIEW(dev_priv))
		dev_priv->display.funcs.dpll = &chv_dpll_funcs;
	else if (IS_VALLEYVIEW(dev_priv))
		dev_priv->display.funcs.dpll = &vlv_dpll_funcs;
	else if (IS_G4X(dev_priv))
		dev_priv->display.funcs.dpll = &g4x_dpll_funcs;
	else if (IS_PINEVIEW(dev_priv))
		dev_priv->display.funcs.dpll = &pnv_dpll_funcs;
	else if (DISPLAY_VER(dev_priv) != 2)
		dev_priv->display.funcs.dpll = &i9xx_dpll_funcs;
	else
		dev_priv->display.funcs.dpll = &i8xx_dpll_funcs;
}

static bool i9xx_has_pps(struct drm_i915_private *dev_priv)
{
	if (IS_I830(dev_priv))
		return false;

	return IS_PINEVIEW(dev_priv) || IS_MOBILE(dev_priv);
}

void i9xx_enable_pll(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	u32 dpll = crtc_state->dpll_hw_state.dpll;
	enum pipe pipe = crtc->pipe;
	int i;

	assert_transcoder_disabled(dev_priv, crtc_state->cpu_transcoder);

	 
	if (i9xx_has_pps(dev_priv))
		assert_pps_unlocked(dev_priv, pipe);

	intel_de_write(dev_priv, FP0(pipe), crtc_state->dpll_hw_state.fp0);
	intel_de_write(dev_priv, FP1(pipe), crtc_state->dpll_hw_state.fp1);

	 
	intel_de_write(dev_priv, DPLL(pipe), dpll & ~DPLL_VGA_MODE_DIS);
	intel_de_write(dev_priv, DPLL(pipe), dpll);

	 
	intel_de_posting_read(dev_priv, DPLL(pipe));
	udelay(150);

	if (DISPLAY_VER(dev_priv) >= 4) {
		intel_de_write(dev_priv, DPLL_MD(pipe),
			       crtc_state->dpll_hw_state.dpll_md);
	} else {
		 
		intel_de_write(dev_priv, DPLL(pipe), dpll);
	}

	 
	for (i = 0; i < 3; i++) {
		intel_de_write(dev_priv, DPLL(pipe), dpll);
		intel_de_posting_read(dev_priv, DPLL(pipe));
		udelay(150);  
	}
}

static void vlv_pllb_recal_opamp(struct drm_i915_private *dev_priv,
				 enum pipe pipe)
{
	u32 reg_val;

	 
	reg_val = vlv_dpio_read(dev_priv, pipe, VLV_PLL_DW9(1));
	reg_val &= 0xffffff00;
	reg_val |= 0x00000030;
	vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW9(1), reg_val);

	reg_val = vlv_dpio_read(dev_priv, pipe, VLV_REF_DW13);
	reg_val &= 0x00ffffff;
	reg_val |= 0x8c000000;
	vlv_dpio_write(dev_priv, pipe, VLV_REF_DW13, reg_val);

	reg_val = vlv_dpio_read(dev_priv, pipe, VLV_PLL_DW9(1));
	reg_val &= 0xffffff00;
	vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW9(1), reg_val);

	reg_val = vlv_dpio_read(dev_priv, pipe, VLV_REF_DW13);
	reg_val &= 0x00ffffff;
	reg_val |= 0xb0000000;
	vlv_dpio_write(dev_priv, pipe, VLV_REF_DW13, reg_val);
}

static void vlv_prepare_pll(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	u32 mdiv;
	u32 bestn, bestm1, bestm2, bestp1, bestp2;
	u32 coreclk, reg_val;

	vlv_dpio_get(dev_priv);

	bestn = crtc_state->dpll.n;
	bestm1 = crtc_state->dpll.m1;
	bestm2 = crtc_state->dpll.m2;
	bestp1 = crtc_state->dpll.p1;
	bestp2 = crtc_state->dpll.p2;

	 

	 
	if (pipe == PIPE_B)
		vlv_pllb_recal_opamp(dev_priv, pipe);

	 
	vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW9_BCAST, 0x0100000f);

	 
	reg_val = vlv_dpio_read(dev_priv, pipe, VLV_PLL_DW8(pipe));
	reg_val &= 0x00ffffff;
	vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW8(pipe), reg_val);

	 
	vlv_dpio_write(dev_priv, pipe, VLV_CMN_DW0, 0x610);

	 
	mdiv = ((bestm1 << DPIO_M1DIV_SHIFT) | (bestm2 & DPIO_M2DIV_MASK));
	mdiv |= ((bestp1 << DPIO_P1_SHIFT) | (bestp2 << DPIO_P2_SHIFT));
	mdiv |= ((bestn << DPIO_N_SHIFT));
	mdiv |= (1 << DPIO_K_SHIFT);

	 
	mdiv |= (DPIO_POST_DIV_HDMIDP << DPIO_POST_DIV_SHIFT);
	vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW3(pipe), mdiv);

	mdiv |= DPIO_ENABLE_CALIBRATION;
	vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW3(pipe), mdiv);

	 
	if (crtc_state->port_clock == 162000 ||
	    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_ANALOG) ||
	    intel_crtc_has_type(crtc_state, INTEL_OUTPUT_HDMI))
		vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW10(pipe),
				 0x009f0003);
	else
		vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW10(pipe),
				 0x00d0000f);

	if (intel_crtc_has_dp_encoder(crtc_state)) {
		 
		if (pipe == PIPE_A)
			vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW5(pipe),
					 0x0df40000);
		else
			vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW5(pipe),
					 0x0df70000);
	} else {  
		 
		if (pipe == PIPE_A)
			vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW5(pipe),
					 0x0df70000);
		else
			vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW5(pipe),
					 0x0df40000);
	}

	coreclk = vlv_dpio_read(dev_priv, pipe, VLV_PLL_DW7(pipe));
	coreclk = (coreclk & 0x0000ff00) | 0x01c00000;
	if (intel_crtc_has_dp_encoder(crtc_state))
		coreclk |= 0x01000000;
	vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW7(pipe), coreclk);

	vlv_dpio_write(dev_priv, pipe, VLV_PLL_DW11(pipe), 0x87871000);

	vlv_dpio_put(dev_priv);
}

static void _vlv_enable_pll(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;

	intel_de_write(dev_priv, DPLL(pipe), crtc_state->dpll_hw_state.dpll);
	intel_de_posting_read(dev_priv, DPLL(pipe));
	udelay(150);

	if (intel_de_wait_for_set(dev_priv, DPLL(pipe), DPLL_LOCK_VLV, 1))
		drm_err(&dev_priv->drm, "DPLL %d failed to lock\n", pipe);
}

void vlv_enable_pll(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;

	assert_transcoder_disabled(dev_priv, crtc_state->cpu_transcoder);

	 
	assert_pps_unlocked(dev_priv, pipe);

	 
	intel_de_write(dev_priv, DPLL(pipe),
		       crtc_state->dpll_hw_state.dpll &
		       ~(DPLL_VCO_ENABLE | DPLL_EXT_BUFFER_ENABLE_VLV));

	if (crtc_state->dpll_hw_state.dpll & DPLL_VCO_ENABLE) {
		vlv_prepare_pll(crtc_state);
		_vlv_enable_pll(crtc_state);
	}

	intel_de_write(dev_priv, DPLL_MD(pipe),
		       crtc_state->dpll_hw_state.dpll_md);
	intel_de_posting_read(dev_priv, DPLL_MD(pipe));
}

static void chv_prepare_pll(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	enum dpio_channel port = vlv_pipe_to_channel(pipe);
	u32 loopfilter, tribuf_calcntr;
	u32 bestm2, bestp1, bestp2, bestm2_frac;
	u32 dpio_val;
	int vco;

	bestm2_frac = crtc_state->dpll.m2 & 0x3fffff;
	bestm2 = crtc_state->dpll.m2 >> 22;
	bestp1 = crtc_state->dpll.p1;
	bestp2 = crtc_state->dpll.p2;
	vco = crtc_state->dpll.vco;
	dpio_val = 0;
	loopfilter = 0;

	vlv_dpio_get(dev_priv);

	 
	vlv_dpio_write(dev_priv, pipe, CHV_CMN_DW13(port),
			5 << DPIO_CHV_S1_DIV_SHIFT |
			bestp1 << DPIO_CHV_P1_DIV_SHIFT |
			bestp2 << DPIO_CHV_P2_DIV_SHIFT |
			1 << DPIO_CHV_K_DIV_SHIFT);

	 
	vlv_dpio_write(dev_priv, pipe, CHV_PLL_DW0(port), bestm2);

	 
	vlv_dpio_write(dev_priv, pipe, CHV_PLL_DW1(port),
			DPIO_CHV_M1_DIV_BY_2 |
			1 << DPIO_CHV_N_DIV_SHIFT);

	 
	vlv_dpio_write(dev_priv, pipe, CHV_PLL_DW2(port), bestm2_frac);

	 
	dpio_val = vlv_dpio_read(dev_priv, pipe, CHV_PLL_DW3(port));
	dpio_val &= ~(DPIO_CHV_FEEDFWD_GAIN_MASK | DPIO_CHV_FRAC_DIV_EN);
	dpio_val |= (2 << DPIO_CHV_FEEDFWD_GAIN_SHIFT);
	if (bestm2_frac)
		dpio_val |= DPIO_CHV_FRAC_DIV_EN;
	vlv_dpio_write(dev_priv, pipe, CHV_PLL_DW3(port), dpio_val);

	 
	dpio_val = vlv_dpio_read(dev_priv, pipe, CHV_PLL_DW9(port));
	dpio_val &= ~(DPIO_CHV_INT_LOCK_THRESHOLD_MASK |
					DPIO_CHV_INT_LOCK_THRESHOLD_SEL_COARSE);
	dpio_val |= (0x5 << DPIO_CHV_INT_LOCK_THRESHOLD_SHIFT);
	if (!bestm2_frac)
		dpio_val |= DPIO_CHV_INT_LOCK_THRESHOLD_SEL_COARSE;
	vlv_dpio_write(dev_priv, pipe, CHV_PLL_DW9(port), dpio_val);

	 
	if (vco == 5400000) {
		loopfilter |= (0x3 << DPIO_CHV_PROP_COEFF_SHIFT);
		loopfilter |= (0x8 << DPIO_CHV_INT_COEFF_SHIFT);
		loopfilter |= (0x1 << DPIO_CHV_GAIN_CTRL_SHIFT);
		tribuf_calcntr = 0x9;
	} else if (vco <= 6200000) {
		loopfilter |= (0x5 << DPIO_CHV_PROP_COEFF_SHIFT);
		loopfilter |= (0xB << DPIO_CHV_INT_COEFF_SHIFT);
		loopfilter |= (0x3 << DPIO_CHV_GAIN_CTRL_SHIFT);
		tribuf_calcntr = 0x9;
	} else if (vco <= 6480000) {
		loopfilter |= (0x4 << DPIO_CHV_PROP_COEFF_SHIFT);
		loopfilter |= (0x9 << DPIO_CHV_INT_COEFF_SHIFT);
		loopfilter |= (0x3 << DPIO_CHV_GAIN_CTRL_SHIFT);
		tribuf_calcntr = 0x8;
	} else {
		 
		loopfilter |= (0x4 << DPIO_CHV_PROP_COEFF_SHIFT);
		loopfilter |= (0x9 << DPIO_CHV_INT_COEFF_SHIFT);
		loopfilter |= (0x3 << DPIO_CHV_GAIN_CTRL_SHIFT);
		tribuf_calcntr = 0;
	}
	vlv_dpio_write(dev_priv, pipe, CHV_PLL_DW6(port), loopfilter);

	dpio_val = vlv_dpio_read(dev_priv, pipe, CHV_PLL_DW8(port));
	dpio_val &= ~DPIO_CHV_TDC_TARGET_CNT_MASK;
	dpio_val |= (tribuf_calcntr << DPIO_CHV_TDC_TARGET_CNT_SHIFT);
	vlv_dpio_write(dev_priv, pipe, CHV_PLL_DW8(port), dpio_val);

	 
	vlv_dpio_write(dev_priv, pipe, CHV_CMN_DW14(port),
			vlv_dpio_read(dev_priv, pipe, CHV_CMN_DW14(port)) |
			DPIO_AFC_RECAL);

	vlv_dpio_put(dev_priv);
}

static void _chv_enable_pll(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;
	enum dpio_channel port = vlv_pipe_to_channel(pipe);
	u32 tmp;

	vlv_dpio_get(dev_priv);

	 
	tmp = vlv_dpio_read(dev_priv, pipe, CHV_CMN_DW14(port));
	tmp |= DPIO_DCLKP_EN;
	vlv_dpio_write(dev_priv, pipe, CHV_CMN_DW14(port), tmp);

	vlv_dpio_put(dev_priv);

	 
	udelay(1);

	 
	intel_de_write(dev_priv, DPLL(pipe), crtc_state->dpll_hw_state.dpll);

	 
	if (intel_de_wait_for_set(dev_priv, DPLL(pipe), DPLL_LOCK_VLV, 1))
		drm_err(&dev_priv->drm, "PLL %d failed to lock\n", pipe);
}

void chv_enable_pll(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;

	assert_transcoder_disabled(dev_priv, crtc_state->cpu_transcoder);

	 
	assert_pps_unlocked(dev_priv, pipe);

	 
	intel_de_write(dev_priv, DPLL(pipe),
		       crtc_state->dpll_hw_state.dpll & ~DPLL_VCO_ENABLE);

	if (crtc_state->dpll_hw_state.dpll & DPLL_VCO_ENABLE) {
		chv_prepare_pll(crtc_state);
		_chv_enable_pll(crtc_state);
	}

	if (pipe != PIPE_A) {
		 
		intel_de_write(dev_priv, CBR4_VLV, CBR_DPLLBMD_PIPE(pipe));
		intel_de_write(dev_priv, DPLL_MD(PIPE_B),
			       crtc_state->dpll_hw_state.dpll_md);
		intel_de_write(dev_priv, CBR4_VLV, 0);
		dev_priv->display.state.chv_dpll_md[pipe] = crtc_state->dpll_hw_state.dpll_md;

		 
		drm_WARN_ON(&dev_priv->drm,
			    (intel_de_read(dev_priv, DPLL(PIPE_B)) &
			     DPLL_VGA_MODE_DIS) == 0);
	} else {
		intel_de_write(dev_priv, DPLL_MD(pipe),
			       crtc_state->dpll_hw_state.dpll_md);
		intel_de_posting_read(dev_priv, DPLL_MD(pipe));
	}
}

 
int vlv_force_pll_on(struct drm_i915_private *dev_priv, enum pipe pipe,
		     const struct dpll *dpll)
{
	struct intel_crtc *crtc = intel_crtc_for_pipe(dev_priv, pipe);
	struct intel_crtc_state *crtc_state;

	crtc_state = intel_crtc_state_alloc(crtc);
	if (!crtc_state)
		return -ENOMEM;

	crtc_state->cpu_transcoder = (enum transcoder)pipe;
	crtc_state->pixel_multiplier = 1;
	crtc_state->dpll = *dpll;
	crtc_state->output_types = BIT(INTEL_OUTPUT_EDP);

	if (IS_CHERRYVIEW(dev_priv)) {
		chv_compute_dpll(crtc_state);
		chv_enable_pll(crtc_state);
	} else {
		vlv_compute_dpll(crtc_state);
		vlv_enable_pll(crtc_state);
	}

	kfree(crtc_state);

	return 0;
}

void vlv_disable_pll(struct drm_i915_private *dev_priv, enum pipe pipe)
{
	u32 val;

	 
	assert_transcoder_disabled(dev_priv, (enum transcoder)pipe);

	val = DPLL_INTEGRATED_REF_CLK_VLV |
		DPLL_REF_CLK_ENABLE_VLV | DPLL_VGA_MODE_DIS;
	if (pipe != PIPE_A)
		val |= DPLL_INTEGRATED_CRI_CLK_VLV;

	intel_de_write(dev_priv, DPLL(pipe), val);
	intel_de_posting_read(dev_priv, DPLL(pipe));
}

void chv_disable_pll(struct drm_i915_private *dev_priv, enum pipe pipe)
{
	enum dpio_channel port = vlv_pipe_to_channel(pipe);
	u32 val;

	 
	assert_transcoder_disabled(dev_priv, (enum transcoder)pipe);

	val = DPLL_SSC_REF_CLK_CHV |
		DPLL_REF_CLK_ENABLE_VLV | DPLL_VGA_MODE_DIS;
	if (pipe != PIPE_A)
		val |= DPLL_INTEGRATED_CRI_CLK_VLV;

	intel_de_write(dev_priv, DPLL(pipe), val);
	intel_de_posting_read(dev_priv, DPLL(pipe));

	vlv_dpio_get(dev_priv);

	 
	val = vlv_dpio_read(dev_priv, pipe, CHV_CMN_DW14(port));
	val &= ~DPIO_DCLKP_EN;
	vlv_dpio_write(dev_priv, pipe, CHV_CMN_DW14(port), val);

	vlv_dpio_put(dev_priv);
}

void i9xx_disable_pll(const struct intel_crtc_state *crtc_state)
{
	struct intel_crtc *crtc = to_intel_crtc(crtc_state->uapi.crtc);
	struct drm_i915_private *dev_priv = to_i915(crtc->base.dev);
	enum pipe pipe = crtc->pipe;

	 
	if (IS_I830(dev_priv))
		return;

	 
	assert_transcoder_disabled(dev_priv, crtc_state->cpu_transcoder);

	intel_de_write(dev_priv, DPLL(pipe), DPLL_VGA_MODE_DIS);
	intel_de_posting_read(dev_priv, DPLL(pipe));
}


 
void vlv_force_pll_off(struct drm_i915_private *dev_priv, enum pipe pipe)
{
	if (IS_CHERRYVIEW(dev_priv))
		chv_disable_pll(dev_priv, pipe);
	else
		vlv_disable_pll(dev_priv, pipe);
}

 
static void assert_pll(struct drm_i915_private *dev_priv,
		       enum pipe pipe, bool state)
{
	bool cur_state;

	cur_state = intel_de_read(dev_priv, DPLL(pipe)) & DPLL_VCO_ENABLE;
	I915_STATE_WARN(dev_priv, cur_state != state,
			"PLL state assertion failure (expected %s, current %s)\n",
			str_on_off(state), str_on_off(cur_state));
}

void assert_pll_enabled(struct drm_i915_private *i915, enum pipe pipe)
{
	assert_pll(i915, pipe, true);
}

void assert_pll_disabled(struct drm_i915_private *i915, enum pipe pipe)
{
	assert_pll(i915, pipe, false);
}
