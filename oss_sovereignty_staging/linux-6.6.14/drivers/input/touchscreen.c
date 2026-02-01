
 

#include <linux/property.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/module.h>

static bool touchscreen_get_prop_u32(struct device *dev,
				     const char *property,
				     unsigned int default_value,
				     unsigned int *value)
{
	u32 val;
	int error;

	error = device_property_read_u32(dev, property, &val);
	if (error) {
		*value = default_value;
		return false;
	}

	*value = val;
	return true;
}

static void touchscreen_set_params(struct input_dev *dev,
				   unsigned long axis,
				   int min, int max, int fuzz)
{
	struct input_absinfo *absinfo;

	if (!test_bit(axis, dev->absbit)) {
		dev_warn(&dev->dev,
			 "Parameters are specified but the axis %lu is not set up\n",
			 axis);
		return;
	}

	absinfo = &dev->absinfo[axis];
	absinfo->minimum = min;
	absinfo->maximum = max;
	absinfo->fuzz = fuzz;
}

 
void touchscreen_parse_properties(struct input_dev *input, bool multitouch,
				  struct touchscreen_properties *prop)
{
	struct device *dev = input->dev.parent;
	struct input_absinfo *absinfo;
	unsigned int axis, axis_x, axis_y;
	unsigned int minimum, maximum, fuzz;
	bool data_present;

	input_alloc_absinfo(input);
	if (!input->absinfo)
		return;

	axis_x = multitouch ? ABS_MT_POSITION_X : ABS_X;
	axis_y = multitouch ? ABS_MT_POSITION_Y : ABS_Y;

	data_present = touchscreen_get_prop_u32(dev, "touchscreen-min-x",
						input_abs_get_min(input, axis_x),
						&minimum);
	data_present |= touchscreen_get_prop_u32(dev, "touchscreen-size-x",
						 input_abs_get_max(input,
								   axis_x) + 1,
						 &maximum);
	data_present |= touchscreen_get_prop_u32(dev, "touchscreen-fuzz-x",
						 input_abs_get_fuzz(input, axis_x),
						 &fuzz);
	if (data_present)
		touchscreen_set_params(input, axis_x, minimum, maximum - 1, fuzz);

	data_present = touchscreen_get_prop_u32(dev, "touchscreen-min-y",
						input_abs_get_min(input, axis_y),
						&minimum);
	data_present |= touchscreen_get_prop_u32(dev, "touchscreen-size-y",
						 input_abs_get_max(input,
								   axis_y) + 1,
						 &maximum);
	data_present |= touchscreen_get_prop_u32(dev, "touchscreen-fuzz-y",
						 input_abs_get_fuzz(input, axis_y),
						 &fuzz);
	if (data_present)
		touchscreen_set_params(input, axis_y, minimum, maximum - 1, fuzz);

	axis = multitouch ? ABS_MT_PRESSURE : ABS_PRESSURE;
	data_present = touchscreen_get_prop_u32(dev,
						"touchscreen-max-pressure",
						input_abs_get_max(input, axis),
						&maximum);
	data_present |= touchscreen_get_prop_u32(dev,
						 "touchscreen-fuzz-pressure",
						 input_abs_get_fuzz(input, axis),
						 &fuzz);
	if (data_present)
		touchscreen_set_params(input, axis, 0, maximum, fuzz);

	if (!prop)
		return;

	prop->max_x = input_abs_get_max(input, axis_x);
	prop->max_y = input_abs_get_max(input, axis_y);

	prop->invert_x =
		device_property_read_bool(dev, "touchscreen-inverted-x");
	if (prop->invert_x) {
		absinfo = &input->absinfo[axis_x];
		absinfo->maximum -= absinfo->minimum;
		absinfo->minimum = 0;
	}

	prop->invert_y =
		device_property_read_bool(dev, "touchscreen-inverted-y");
	if (prop->invert_y) {
		absinfo = &input->absinfo[axis_y];
		absinfo->maximum -= absinfo->minimum;
		absinfo->minimum = 0;
	}

	prop->swap_x_y =
		device_property_read_bool(dev, "touchscreen-swapped-x-y");
	if (prop->swap_x_y)
		swap(input->absinfo[axis_x], input->absinfo[axis_y]);
}
EXPORT_SYMBOL(touchscreen_parse_properties);

static void
touchscreen_apply_prop_to_x_y(const struct touchscreen_properties *prop,
			      unsigned int *x, unsigned int *y)
{
	if (prop->invert_x)
		*x = prop->max_x - *x;

	if (prop->invert_y)
		*y = prop->max_y - *y;

	if (prop->swap_x_y)
		swap(*x, *y);
}

 
void touchscreen_set_mt_pos(struct input_mt_pos *pos,
			    const struct touchscreen_properties *prop,
			    unsigned int x, unsigned int y)
{
	touchscreen_apply_prop_to_x_y(prop, &x, &y);
	pos->x = x;
	pos->y = y;
}
EXPORT_SYMBOL(touchscreen_set_mt_pos);

 
void touchscreen_report_pos(struct input_dev *input,
			    const struct touchscreen_properties *prop,
			    unsigned int x, unsigned int y,
			    bool multitouch)
{
	touchscreen_apply_prop_to_x_y(prop, &x, &y);
	input_report_abs(input, multitouch ? ABS_MT_POSITION_X : ABS_X, x);
	input_report_abs(input, multitouch ? ABS_MT_POSITION_Y : ABS_Y, y);
}
EXPORT_SYMBOL(touchscreen_report_pos);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Helper functions for touchscreens and other devices");
