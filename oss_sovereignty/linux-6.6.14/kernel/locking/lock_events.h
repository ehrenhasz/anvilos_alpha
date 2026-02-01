 
 

#ifndef __LOCKING_LOCK_EVENTS_H
#define __LOCKING_LOCK_EVENTS_H

enum lock_events {

#include "lock_events_list.h"

	lockevent_num,	 
	LOCKEVENT_reset_cnts = lockevent_num,
};

#ifdef CONFIG_LOCK_EVENT_COUNTS
 
DECLARE_PER_CPU(unsigned long, lockevents[lockevent_num]);

 
static inline void __lockevent_inc(enum lock_events event, bool cond)
{
	if (cond)
		raw_cpu_inc(lockevents[event]);
}

#define lockevent_inc(ev)	  __lockevent_inc(LOCKEVENT_ ##ev, true)
#define lockevent_cond_inc(ev, c) __lockevent_inc(LOCKEVENT_ ##ev, c)

static inline void __lockevent_add(enum lock_events event, int inc)
{
	raw_cpu_add(lockevents[event], inc);
}

#define lockevent_add(ev, c)	__lockevent_add(LOCKEVENT_ ##ev, c)

#else   

#define lockevent_inc(ev)
#define lockevent_add(ev, c)
#define lockevent_cond_inc(ev, c)

#endif  

ssize_t lockevent_read(struct file *file, char __user *user_buf,
		       size_t count, loff_t *ppos);

#endif  
