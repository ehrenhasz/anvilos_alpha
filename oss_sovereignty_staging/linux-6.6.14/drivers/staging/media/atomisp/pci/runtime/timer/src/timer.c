
 

#include <type_support.h>		 
#include "ia_css_timer.h"  
#include "sh_css_legacy.h"  
#include "gp_timer.h"  
#include "assert_support.h"

int ia_css_timer_get_current_tick(struct ia_css_clock_tick *curr_ts)
{
	assert(curr_ts);
	if (!curr_ts)
		return -EINVAL;
	curr_ts->ticks = (clock_value_t)gp_timer_read(GP_TIMER_SEL);
	return 0;
}
