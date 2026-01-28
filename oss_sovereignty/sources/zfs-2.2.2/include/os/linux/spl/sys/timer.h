

#ifndef _SPL_TIMER_H
#define	_SPL_TIMER_H

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/timer.h>

#define	lbolt				((clock_t)jiffies)
#define	lbolt64				((int64_t)get_jiffies_64())

#define	ddi_get_lbolt()			((clock_t)jiffies)
#define	ddi_get_lbolt64()		((int64_t)get_jiffies_64())

#define	ddi_time_before(a, b)		(typecheck(clock_t, a) && \
					typecheck(clock_t, b) && \
					((a) - (b) < 0))
#define	ddi_time_after(a, b)		ddi_time_before(b, a)
#define	ddi_time_before_eq(a, b)	(!ddi_time_after(a, b))
#define	ddi_time_after_eq(a, b)		ddi_time_before_eq(b, a)

#define	ddi_time_before64(a, b)		(typecheck(int64_t, a) && \
					typecheck(int64_t, b) && \
					((a) - (b) < 0))
#define	ddi_time_after64(a, b)		ddi_time_before64(b, a)
#define	ddi_time_before_eq64(a, b)	(!ddi_time_after64(a, b))
#define	ddi_time_after_eq64(a, b)	ddi_time_before_eq64(b, a)

#define	delay(ticks)			schedule_timeout_uninterruptible(ticks)

#define	SEC_TO_TICK(sec)		((sec) * HZ)
#define	MSEC_TO_TICK(ms)		msecs_to_jiffies(ms)
#define	USEC_TO_TICK(us)		usecs_to_jiffies(us)
#define	NSEC_TO_TICK(ns)		usecs_to_jiffies(ns / NSEC_PER_USEC)

#ifndef from_timer
#define	from_timer(var, timer, timer_field) \
	container_of(timer, typeof(*var), timer_field)
#endif

#ifdef HAVE_KERNEL_TIMER_FUNCTION_TIMER_LIST
typedef struct timer_list *spl_timer_list_t;
#else
typedef unsigned long spl_timer_list_t;
#endif

#ifndef HAVE_KERNEL_TIMER_SETUP

static inline void
timer_setup(struct timer_list *timer, void (*func)(spl_timer_list_t), u32 fl)
{
#ifdef HAVE_KERNEL_TIMER_LIST_FLAGS
	(timer)->flags = fl;
#endif
	init_timer(timer);
	setup_timer(timer, func, (spl_timer_list_t)(timer));
}

#endif 

#endif  
