
 

#include <linux/export.h>
#include <linux/slab.h>
#include <video/display_timing.h>

void display_timings_release(struct display_timings *disp)
{
	if (disp->timings) {
		unsigned int i;

		for (i = 0; i < disp->num_timings; i++)
			kfree(disp->timings[i]);
		kfree(disp->timings);
	}
	kfree(disp);
}
EXPORT_SYMBOL_GPL(display_timings_release);
