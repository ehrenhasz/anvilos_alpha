

#include <linux/types.h>

#include "../../../util/tsc.h"

u64 rdtsc(void)
{
	u64 val;

	 
	asm volatile("mrs %0, cntvct_el0" : "=r" (val));

	return val;
}
