
 

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/minmax.h>
#include <linux/module.h>
#include <linux/overflow.h>
#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/types.h>
#include <linux/units.h>

#include <linux/iio/iio-gts-helper.h>
#include <linux/iio/types.h>

 
static int iio_gts_get_gain(const u64 max, const u64 scale)
{
	u64 full = max;
	int tmp = 1;

	if (scale > full || !scale)
		return -EINVAL;

	if (U64_MAX - full < scale) {
		 
		if (full - scale < scale)
			return 1;

		full -= scale;
		tmp++;
	}

	while (full > scale * (u64)tmp)
		tmp++;

	return tmp;
}

 
static int gain_get_scale_fraction(const u64 max, u64 scale, int known,
				   int *unknown)
{
	int tot_gain;

	tot_gain = iio_gts_get_gain(max, scale);
	if (tot_gain < 0)
		return tot_gain;

	*unknown = tot_gain / known;

	 
	if (!*unknown || *unknown * known != tot_gain)
		return -EINVAL;

	return 0;
}

static int iio_gts_delinearize(u64 lin_scale, unsigned long scaler,
			       int *scale_whole, int *scale_nano)
{
	int frac;

	if (scaler > NANO)
		return -EOVERFLOW;

	if (!scaler)
		return -EINVAL;

	frac = do_div(lin_scale, scaler);

	*scale_whole = lin_scale;
	*scale_nano = frac * (NANO / scaler);

	return 0;
}

static int iio_gts_linearize(int scale_whole, int scale_nano,
			     unsigned long scaler, u64 *lin_scale)
{
	 
	if (scaler > NANO || !scaler)
		return -EINVAL;

	*lin_scale = (u64)scale_whole * (u64)scaler +
		     (u64)(scale_nano / (NANO / scaler));

	return 0;
}

 
int iio_gts_total_gain_to_scale(struct iio_gts *gts, int total_gain,
				int *scale_int, int *scale_nano)
{
	u64 tmp;

	tmp = gts->max_scale;

	do_div(tmp, total_gain);

	return iio_gts_delinearize(tmp, NANO, scale_int, scale_nano);
}
EXPORT_SYMBOL_NS_GPL(iio_gts_total_gain_to_scale, IIO_GTS_HELPER);

 
static void iio_gts_purge_avail_scale_table(struct iio_gts *gts)
{
	int i;

	if (gts->per_time_avail_scale_tables) {
		for (i = 0; i < gts->num_itime; i++)
			kfree(gts->per_time_avail_scale_tables[i]);

		kfree(gts->per_time_avail_scale_tables);
		gts->per_time_avail_scale_tables = NULL;
	}

	kfree(gts->avail_all_scales_table);
	gts->avail_all_scales_table = NULL;

	gts->num_avail_all_scales = 0;
}

static int iio_gts_gain_cmp(const void *a, const void *b)
{
	return *(int *)a - *(int *)b;
}

static int gain_to_scaletables(struct iio_gts *gts, int **gains, int **scales)
{
	int ret, i, j, new_idx, time_idx;
	int *all_gains;
	size_t gain_bytes;

	for (i = 0; i < gts->num_itime; i++) {
		 
		sort(gains[i], gts->num_hwgain, sizeof(int), iio_gts_gain_cmp,
		     NULL);

		 
		for (j = 0; j < gts->num_hwgain; j++) {
			ret = iio_gts_total_gain_to_scale(gts, gains[i][j],
							  &scales[i][2 * j],
							  &scales[i][2 * j + 1]);
			if (ret)
				return ret;
		}
	}

	gain_bytes = array_size(gts->num_hwgain, sizeof(int));
	all_gains = kcalloc(gts->num_itime, gain_bytes, GFP_KERNEL);
	if (!all_gains)
		return -ENOMEM;

	 
	time_idx = gts->num_itime - 1;
	memcpy(all_gains, gains[time_idx], gain_bytes);
	new_idx = gts->num_hwgain;

	while (time_idx--) {
		for (j = 0; j < gts->num_hwgain; j++) {
			int candidate = gains[time_idx][j];
			int chk;

			if (candidate > all_gains[new_idx - 1]) {
				all_gains[new_idx] = candidate;
				new_idx++;

				continue;
			}
			for (chk = 0; chk < new_idx; chk++)
				if (candidate <= all_gains[chk])
					break;

			if (candidate == all_gains[chk])
				continue;

			memmove(&all_gains[chk + 1], &all_gains[chk],
				(new_idx - chk) * sizeof(int));
			all_gains[chk] = candidate;
			new_idx++;
		}
	}

	gts->avail_all_scales_table = kcalloc(new_idx, 2 * sizeof(int),
					      GFP_KERNEL);
	if (!gts->avail_all_scales_table) {
		ret = -ENOMEM;
		goto free_out;
	}
	gts->num_avail_all_scales = new_idx;

	for (i = 0; i < gts->num_avail_all_scales; i++) {
		ret = iio_gts_total_gain_to_scale(gts, all_gains[i],
					&gts->avail_all_scales_table[i * 2],
					&gts->avail_all_scales_table[i * 2 + 1]);

		if (ret) {
			kfree(gts->avail_all_scales_table);
			gts->num_avail_all_scales = 0;
			goto free_out;
		}
	}

free_out:
	kfree(all_gains);

	return ret;
}

 
static int iio_gts_build_avail_scale_table(struct iio_gts *gts)
{
	int **per_time_gains, **per_time_scales, i, j, ret = -ENOMEM;

	per_time_gains = kcalloc(gts->num_itime, sizeof(*per_time_gains), GFP_KERNEL);
	if (!per_time_gains)
		return ret;

	per_time_scales = kcalloc(gts->num_itime, sizeof(*per_time_scales), GFP_KERNEL);
	if (!per_time_scales)
		goto free_gains;

	for (i = 0; i < gts->num_itime; i++) {
		per_time_scales[i] = kcalloc(gts->num_hwgain, 2 * sizeof(int),
					     GFP_KERNEL);
		if (!per_time_scales[i])
			goto err_free_out;

		per_time_gains[i] = kcalloc(gts->num_hwgain, sizeof(int),
					    GFP_KERNEL);
		if (!per_time_gains[i]) {
			kfree(per_time_scales[i]);
			goto err_free_out;
		}

		for (j = 0; j < gts->num_hwgain; j++)
			per_time_gains[i][j] = gts->hwgain_table[j].gain *
					       gts->itime_table[i].mul;
	}

	ret = gain_to_scaletables(gts, per_time_gains, per_time_scales);
	if (ret)
		goto err_free_out;

	kfree(per_time_gains);
	gts->per_time_avail_scale_tables = per_time_scales;

	return 0;

err_free_out:
	for (i--; i; i--) {
		kfree(per_time_scales[i]);
		kfree(per_time_gains[i]);
	}
	kfree(per_time_scales);
free_gains:
	kfree(per_time_gains);

	return ret;
}

static void iio_gts_us_to_int_micro(int *time_us, int *int_micro_times,
				    int num_times)
{
	int i;

	for (i = 0; i < num_times; i++) {
		int_micro_times[i * 2] = time_us[i] / 1000000;
		int_micro_times[i * 2 + 1] = time_us[i] % 1000000;
	}
}

 
static int iio_gts_build_avail_time_table(struct iio_gts *gts)
{
	int *times, i, j, idx = 0, *int_micro_times;

	if (!gts->num_itime)
		return 0;

	times = kcalloc(gts->num_itime, sizeof(int), GFP_KERNEL);
	if (!times)
		return -ENOMEM;

	 
	for (i = gts->num_itime - 1; i >= 0; i--) {
		int new = gts->itime_table[i].time_us;

		if (times[idx] < new) {
			times[idx++] = new;
			continue;
		}

		for (j = 0; j <= idx; j++) {
			if (times[j] > new) {
				memmove(&times[j + 1], &times[j],
					(idx - j) * sizeof(int));
				times[j] = new;
				idx++;
			}
		}
	}

	 
	int_micro_times = kcalloc(idx, sizeof(int) * 2, GFP_KERNEL);
	if (int_micro_times) {
		 
		gts->num_avail_time_tables = idx;
		iio_gts_us_to_int_micro(times, int_micro_times, idx);
	}

	gts->avail_time_tables = int_micro_times;
	kfree(times);

	if (!int_micro_times)
		return -ENOMEM;

	return 0;
}

 
static void iio_gts_purge_avail_time_table(struct iio_gts *gts)
{
	if (gts->num_avail_time_tables) {
		kfree(gts->avail_time_tables);
		gts->avail_time_tables = NULL;
		gts->num_avail_time_tables = 0;
	}
}

 
static int iio_gts_build_avail_tables(struct iio_gts *gts)
{
	int ret;

	ret = iio_gts_build_avail_scale_table(gts);
	if (ret)
		return ret;

	ret = iio_gts_build_avail_time_table(gts);
	if (ret)
		iio_gts_purge_avail_scale_table(gts);

	return ret;
}

 
static void iio_gts_purge_avail_tables(struct iio_gts *gts)
{
	iio_gts_purge_avail_time_table(gts);
	iio_gts_purge_avail_scale_table(gts);
}

static void devm_iio_gts_avail_all_drop(void *res)
{
	iio_gts_purge_avail_tables(res);
}

 
static int devm_iio_gts_build_avail_tables(struct device *dev,
					   struct iio_gts *gts)
{
	int ret;

	ret = iio_gts_build_avail_tables(gts);
	if (ret)
		return ret;

	return devm_add_action_or_reset(dev, devm_iio_gts_avail_all_drop, gts);
}

static int sanity_check_time(const struct iio_itime_sel_mul *t)
{
	if (t->sel < 0 || t->time_us < 0 || t->mul <= 0)
		return -EINVAL;

	return 0;
}

static int sanity_check_gain(const struct iio_gain_sel_pair *g)
{
	if (g->sel < 0 || g->gain <= 0)
		return -EINVAL;

	return 0;
}

static int iio_gts_sanity_check(struct iio_gts *gts)
{
	int g, t, ret;

	if (!gts->num_hwgain && !gts->num_itime)
		return -EINVAL;

	for (t = 0; t < gts->num_itime; t++) {
		ret = sanity_check_time(&gts->itime_table[t]);
		if (ret)
			return ret;
	}

	for (g = 0; g < gts->num_hwgain; g++) {
		ret = sanity_check_gain(&gts->hwgain_table[g]);
		if (ret)
			return ret;
	}

	for (g = 0; g < gts->num_hwgain; g++) {
		for (t = 0; t < gts->num_itime; t++) {
			int gain, mul, res;

			gain = gts->hwgain_table[g].gain;
			mul = gts->itime_table[t].mul;

			if (check_mul_overflow(gain, mul, &res))
				return -EOVERFLOW;
		}
	}

	return 0;
}

static int iio_init_iio_gts(int max_scale_int, int max_scale_nano,
			const struct iio_gain_sel_pair *gain_tbl, int num_gain,
			const struct iio_itime_sel_mul *tim_tbl, int num_times,
			struct iio_gts *gts)
{
	int ret;

	memset(gts, 0, sizeof(*gts));

	ret = iio_gts_linearize(max_scale_int, max_scale_nano, NANO,
				   &gts->max_scale);
	if (ret)
		return ret;

	gts->hwgain_table = gain_tbl;
	gts->num_hwgain = num_gain;
	gts->itime_table = tim_tbl;
	gts->num_itime = num_times;

	return iio_gts_sanity_check(gts);
}

 
int devm_iio_init_iio_gts(struct device *dev, int max_scale_int, int max_scale_nano,
			  const struct iio_gain_sel_pair *gain_tbl, int num_gain,
			  const struct iio_itime_sel_mul *tim_tbl, int num_times,
			  struct iio_gts *gts)
{
	int ret;

	ret = iio_init_iio_gts(max_scale_int, max_scale_nano, gain_tbl,
			       num_gain, tim_tbl, num_times, gts);
	if (ret)
		return ret;

	return devm_iio_gts_build_avail_tables(dev, gts);
}
EXPORT_SYMBOL_NS_GPL(devm_iio_init_iio_gts, IIO_GTS_HELPER);

 
int iio_gts_all_avail_scales(struct iio_gts *gts, const int **vals, int *type,
			     int *length)
{
	if (!gts->num_avail_all_scales)
		return -EINVAL;

	*vals = gts->avail_all_scales_table;
	*type = IIO_VAL_INT_PLUS_NANO;
	*length = gts->num_avail_all_scales * 2;

	return IIO_AVAIL_LIST;
}
EXPORT_SYMBOL_NS_GPL(iio_gts_all_avail_scales, IIO_GTS_HELPER);

 
int iio_gts_avail_scales_for_time(struct iio_gts *gts, int time,
				  const int **vals, int *type, int *length)
{
	int i;

	for (i = 0; i < gts->num_itime; i++)
		if (gts->itime_table[i].time_us == time)
			break;

	if (i == gts->num_itime)
		return -EINVAL;

	*vals = gts->per_time_avail_scale_tables[i];
	*type = IIO_VAL_INT_PLUS_NANO;
	*length = gts->num_hwgain * 2;

	return IIO_AVAIL_LIST;
}
EXPORT_SYMBOL_NS_GPL(iio_gts_avail_scales_for_time, IIO_GTS_HELPER);

 
int iio_gts_avail_times(struct iio_gts *gts,  const int **vals, int *type,
			int *length)
{
	if (!gts->num_avail_time_tables)
		return -EINVAL;

	*vals = gts->avail_time_tables;
	*type = IIO_VAL_INT_PLUS_MICRO;
	*length = gts->num_avail_time_tables * 2;

	return IIO_AVAIL_LIST;
}
EXPORT_SYMBOL_NS_GPL(iio_gts_avail_times, IIO_GTS_HELPER);

 
int iio_gts_find_sel_by_gain(struct iio_gts *gts, int gain)
{
	int i;

	for (i = 0; i < gts->num_hwgain; i++)
		if (gts->hwgain_table[i].gain == gain)
			return gts->hwgain_table[i].sel;

	return -EINVAL;
}
EXPORT_SYMBOL_NS_GPL(iio_gts_find_sel_by_gain, IIO_GTS_HELPER);

 
int iio_gts_find_gain_by_sel(struct iio_gts *gts, int sel)
{
	int i;

	for (i = 0; i < gts->num_hwgain; i++)
		if (gts->hwgain_table[i].sel == sel)
			return gts->hwgain_table[i].gain;

	return -EINVAL;
}
EXPORT_SYMBOL_NS_GPL(iio_gts_find_gain_by_sel, IIO_GTS_HELPER);

 
int iio_gts_get_min_gain(struct iio_gts *gts)
{
	int i, min = -EINVAL;

	for (i = 0; i < gts->num_hwgain; i++) {
		int gain = gts->hwgain_table[i].gain;

		if (min == -EINVAL)
			min = gain;
		else
			min = min(min, gain);
	}

	return min;
}
EXPORT_SYMBOL_NS_GPL(iio_gts_get_min_gain, IIO_GTS_HELPER);

 
int iio_find_closest_gain_low(struct iio_gts *gts, int gain, bool *in_range)
{
	int i, diff = 0;
	int best = -1;

	*in_range = false;

	for (i = 0; i < gts->num_hwgain; i++) {
		if (gain == gts->hwgain_table[i].gain) {
			*in_range = true;
			return gain;
		}

		if (gain > gts->hwgain_table[i].gain) {
			if (!diff) {
				diff = gain - gts->hwgain_table[i].gain;
				best = i;
			} else {
				int tmp = gain - gts->hwgain_table[i].gain;

				if (tmp < diff) {
					diff = tmp;
					best = i;
				}
			}
		} else {
			 
			*in_range = true;
		}
	}
	 
	if (!diff) {
		*in_range = false;

		return -EINVAL;
	}

	return gts->hwgain_table[best].gain;
}
EXPORT_SYMBOL_NS_GPL(iio_find_closest_gain_low, IIO_GTS_HELPER);

static int iio_gts_get_int_time_gain_multiplier_by_sel(struct iio_gts *gts,
						       int sel)
{
	const struct iio_itime_sel_mul *time;

	time = iio_gts_find_itime_by_sel(gts, sel);
	if (!time)
		return -EINVAL;

	return time->mul;
}

 
static int iio_gts_find_gain_for_scale_using_time(struct iio_gts *gts, int time_sel,
						  int scale_int, int scale_nano,
						  int *gain)
{
	u64 scale_linear;
	int ret, mul;

	ret = iio_gts_linearize(scale_int, scale_nano, NANO, &scale_linear);
	if (ret)
		return ret;

	ret = iio_gts_get_int_time_gain_multiplier_by_sel(gts, time_sel);
	if (ret < 0)
		return ret;

	mul = ret;

	ret = gain_get_scale_fraction(gts->max_scale, scale_linear, mul, gain);
	if (ret)
		return ret;

	if (!iio_gts_valid_gain(gts, *gain))
		return -EINVAL;

	return 0;
}

 
int iio_gts_find_gain_sel_for_scale_using_time(struct iio_gts *gts, int time_sel,
					       int scale_int, int scale_nano,
					       int *gain_sel)
{
	int gain, ret;

	ret = iio_gts_find_gain_for_scale_using_time(gts, time_sel, scale_int,
						     scale_nano, &gain);
	if (ret)
		return ret;

	ret = iio_gts_find_sel_by_gain(gts, gain);
	if (ret < 0)
		return ret;

	*gain_sel = ret;

	return 0;
}
EXPORT_SYMBOL_NS_GPL(iio_gts_find_gain_sel_for_scale_using_time, IIO_GTS_HELPER);

static int iio_gts_get_total_gain(struct iio_gts *gts, int gain, int time)
{
	const struct iio_itime_sel_mul *itime;

	if (!iio_gts_valid_gain(gts, gain))
		return -EINVAL;

	if (!gts->num_itime)
		return gain;

	itime = iio_gts_find_itime_by_time(gts, time);
	if (!itime)
		return -EINVAL;

	return gain * itime->mul;
}

static int iio_gts_get_scale_linear(struct iio_gts *gts, int gain, int time,
				    u64 *scale)
{
	int total_gain;
	u64 tmp;

	total_gain = iio_gts_get_total_gain(gts, gain, time);
	if (total_gain < 0)
		return total_gain;

	tmp = gts->max_scale;

	do_div(tmp, total_gain);

	*scale = tmp;

	return 0;
}

 
int iio_gts_get_scale(struct iio_gts *gts, int gain, int time, int *scale_int,
		      int *scale_nano)
{
	u64 lin_scale;
	int ret;

	ret = iio_gts_get_scale_linear(gts, gain, time, &lin_scale);
	if (ret)
		return ret;

	return iio_gts_delinearize(lin_scale, NANO, scale_int, scale_nano);
}
EXPORT_SYMBOL_NS_GPL(iio_gts_get_scale, IIO_GTS_HELPER);

 
int iio_gts_find_new_gain_sel_by_old_gain_time(struct iio_gts *gts,
					       int old_gain, int old_time_sel,
					       int new_time_sel, int *new_gain)
{
	const struct iio_itime_sel_mul *itime_old, *itime_new;
	u64 scale;
	int ret;

	*new_gain = -1;

	itime_old = iio_gts_find_itime_by_sel(gts, old_time_sel);
	if (!itime_old)
		return -EINVAL;

	itime_new = iio_gts_find_itime_by_sel(gts, new_time_sel);
	if (!itime_new)
		return -EINVAL;

	ret = iio_gts_get_scale_linear(gts, old_gain, itime_old->time_us,
				       &scale);
	if (ret)
		return ret;

	ret = gain_get_scale_fraction(gts->max_scale, scale, itime_new->mul,
				      new_gain);
	if (ret)
		return ret;

	if (!iio_gts_valid_gain(gts, *new_gain))
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_NS_GPL(iio_gts_find_new_gain_sel_by_old_gain_time, IIO_GTS_HELPER);

 
int iio_gts_find_new_gain_by_old_gain_time(struct iio_gts *gts, int old_gain,
					   int old_time, int new_time,
					   int *new_gain)
{
	const struct iio_itime_sel_mul *itime_new;
	u64 scale;
	int ret;

	*new_gain = -1;

	itime_new = iio_gts_find_itime_by_time(gts, new_time);
	if (!itime_new)
		return -EINVAL;

	ret = iio_gts_get_scale_linear(gts, old_gain, old_time, &scale);
	if (ret)
		return ret;

	ret = gain_get_scale_fraction(gts->max_scale, scale, itime_new->mul,
				      new_gain);
	if (ret)
		return ret;

	if (!iio_gts_valid_gain(gts, *new_gain))
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL_NS_GPL(iio_gts_find_new_gain_by_old_gain_time, IIO_GTS_HELPER);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Matti Vaittinen <mazziesaccount@gmail.com>");
MODULE_DESCRIPTION("IIO light sensor gain-time-scale helpers");
