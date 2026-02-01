
 
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <video/display_timing.h>
#include <video/of_display_timing.h>
#include <video/of_videomode.h>
#include <video/videomode.h>

 
int of_get_videomode(struct device_node *np, struct videomode *vm,
		     int index)
{
	struct display_timings *disp;
	int ret;

	disp = of_get_display_timings(np);
	if (!disp) {
		pr_err("%pOF: no timings specified\n", np);
		return -EINVAL;
	}

	if (index == OF_USE_NATIVE_MODE)
		index = disp->native_mode;

	ret = videomode_from_timings(disp, vm, index);

	display_timings_release(disp);

	return ret;
}
EXPORT_SYMBOL_GPL(of_get_videomode);
