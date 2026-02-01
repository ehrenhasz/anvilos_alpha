 
 

#ifndef __IA_CSS_TIMER_H
#define __IA_CSS_TIMER_H

 
#include <type_support.h>		 
#include "ia_css_err.h"

 
typedef u32 clock_value_t;

 
struct ia_css_clock_tick {
	clock_value_t ticks;  
};

 
enum ia_css_tm_event {
	IA_CSS_TM_EVENT_AFTER_INIT,
	 
	IA_CSS_TM_EVENT_MAIN_END,
	 
	IA_CSS_TM_EVENT_THREAD_START,
	 
	IA_CSS_TM_EVENT_FRAME_PROC_START,
	 
	IA_CSS_TM_EVENT_FRAME_PROC_END
	 
};

 
struct ia_css_time_meas {
	clock_value_t	start_timer_value;	 
	clock_value_t	end_timer_value;	 
};

 
#define SIZE_OF_IA_CSS_CLOCK_TICK_STRUCT sizeof(clock_value_t)
 
#define SIZE_OF_IA_CSS_TIME_MEAS_STRUCT (sizeof(clock_value_t) \
					+ sizeof(clock_value_t))

 
int
ia_css_timer_get_current_tick(
    struct ia_css_clock_tick *curr_ts);

#endif   
