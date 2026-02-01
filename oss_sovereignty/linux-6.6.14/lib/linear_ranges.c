
 

#include <linux/errno.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/linear_range.h>
#include <linux/module.h>

 
unsigned int linear_range_values_in_range(const struct linear_range *r)
{
	if (!r)
		return 0;
	return r->max_sel - r->min_sel + 1;
}
EXPORT_SYMBOL_GPL(linear_range_values_in_range);

 
unsigned int linear_range_values_in_range_array(const struct linear_range *r,
						int ranges)
{
	int i, values_in_range = 0;

	for (i = 0; i < ranges; i++) {
		int values;

		values = linear_range_values_in_range(&r[i]);
		if (!values)
			return values;

		values_in_range += values;
	}
	return values_in_range;
}
EXPORT_SYMBOL_GPL(linear_range_values_in_range_array);

 
unsigned int linear_range_get_max_value(const struct linear_range *r)
{
	return r->min + (r->max_sel - r->min_sel) * r->step;
}
EXPORT_SYMBOL_GPL(linear_range_get_max_value);

 
int linear_range_get_value(const struct linear_range *r, unsigned int selector,
			   unsigned int *val)
{
	if (r->min_sel > selector || r->max_sel < selector)
		return -EINVAL;

	*val = r->min + (selector - r->min_sel) * r->step;

	return 0;
}
EXPORT_SYMBOL_GPL(linear_range_get_value);

 
int linear_range_get_value_array(const struct linear_range *r, int ranges,
				 unsigned int selector, unsigned int *val)
{
	int i;

	for (i = 0; i < ranges; i++)
		if (r[i].min_sel <= selector && r[i].max_sel >= selector)
			return linear_range_get_value(&r[i], selector, val);

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(linear_range_get_value_array);

 
int linear_range_get_selector_low(const struct linear_range *r,
				  unsigned int val, unsigned int *selector,
				  bool *found)
{
	*found = false;

	if (r->min > val)
		return -EINVAL;

	if (linear_range_get_max_value(r) < val) {
		*selector = r->max_sel;
		return 0;
	}

	*found = true;

	if (r->step == 0)
		*selector = r->min_sel;
	else
		*selector = (val - r->min) / r->step + r->min_sel;

	return 0;
}
EXPORT_SYMBOL_GPL(linear_range_get_selector_low);

 
int linear_range_get_selector_low_array(const struct linear_range *r,
					int ranges, unsigned int val,
					unsigned int *selector, bool *found)
{
	int i;
	int ret = -EINVAL;

	for (i = 0; i < ranges; i++) {
		int tmpret;

		tmpret = linear_range_get_selector_low(&r[i], val, selector,
						       found);
		if (!tmpret)
			ret = 0;

		if (*found)
			break;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(linear_range_get_selector_low_array);

 
int linear_range_get_selector_high(const struct linear_range *r,
				   unsigned int val, unsigned int *selector,
				   bool *found)
{
	*found = false;

	if (linear_range_get_max_value(r) < val)
		return -EINVAL;

	if (r->min > val) {
		*selector = r->min_sel;
		return 0;
	}

	*found = true;

	if (r->step == 0)
		*selector = r->max_sel;
	else
		*selector = DIV_ROUND_UP(val - r->min, r->step) + r->min_sel;

	return 0;
}
EXPORT_SYMBOL_GPL(linear_range_get_selector_high);

 
void linear_range_get_selector_within(const struct linear_range *r,
				      unsigned int val, unsigned int *selector)
{
	if (r->min > val) {
		*selector = r->min_sel;
		return;
	}

	if (linear_range_get_max_value(r) < val) {
		*selector = r->max_sel;
		return;
	}

	if (r->step == 0)
		*selector = r->min_sel;
	else
		*selector = (val - r->min) / r->step + r->min_sel;
}
EXPORT_SYMBOL_GPL(linear_range_get_selector_within);

MODULE_DESCRIPTION("linear-ranges helper");
MODULE_LICENSE("GPL");
