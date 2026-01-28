#ifndef __TIMER_INTERNAL_H__
#define __TIMER_INTERNAL_H__
#include <linux/list.h>
#include <asm/bug.h>
#include <shared/timetravel.h>
#define TIMER_MULTIPLIER 256
#define TIMER_MIN_DELTA  500
#ifdef CONFIG_UML_TIME_TRAVEL_SUPPORT
struct time_travel_event {
	unsigned long long time;
	void (*fn)(struct time_travel_event *d);
	struct list_head list;
	bool pending, onstack;
};
void time_travel_sleep(void);
static inline void
time_travel_set_event_fn(struct time_travel_event *e,
			 void (*fn)(struct time_travel_event *d))
{
	e->fn = fn;
}
void __time_travel_propagate_time(void);
static inline void time_travel_propagate_time(void)
{
	if (time_travel_mode == TT_MODE_EXTERNAL)
		__time_travel_propagate_time();
}
void __time_travel_wait_readable(int fd);
static inline void time_travel_wait_readable(int fd)
{
	if (time_travel_mode == TT_MODE_EXTERNAL)
		__time_travel_wait_readable(fd);
}
void time_travel_add_irq_event(struct time_travel_event *e);
void time_travel_add_event_rel(struct time_travel_event *e,
			       unsigned long long delay_ns);
bool time_travel_del_event(struct time_travel_event *e);
#else
struct time_travel_event {
};
static inline void time_travel_sleep(void)
{
}
#define time_travel_set_event_fn(e, fn) do {} while (0)
static inline void time_travel_propagate_time(void)
{
}
static inline void time_travel_wait_readable(int fd)
{
}
static inline void time_travel_add_irq_event(struct time_travel_event *e)
{
	WARN_ON(1);
}
extern void time_travel_not_configured(void);
#define time_travel_add_event_rel(...) time_travel_not_configured()
#define time_travel_del_event(...) time_travel_not_configured()
#endif  
void time_travel_ndelay(unsigned long nsec);
#endif  
