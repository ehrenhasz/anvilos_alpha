 

#ifndef _SPL_TIMER_H_
#define	_SPL_TIMER_H_
#define	ddi_time_after(a, b) ((a) > (b))
#define	ddi_time_after64(a, b) ((a) > (b))
#define	usleep_range(wakeup, wakeupepsilon)				   \
	pause_sbt("usleep_range", ustosbt(wakeup), \
	ustosbt(wakeupepsilon - wakeup), 0)
#endif
