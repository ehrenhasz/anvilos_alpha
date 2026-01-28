#ifndef __IRQ_KERN_H__
#define __IRQ_KERN_H__
#include <linux/interrupt.h>
#include <linux/time-internal.h>
#include <asm/ptrace.h>
#include "irq_user.h"
#define UM_IRQ_ALLOC	-1
int um_request_irq(int irq, int fd, enum um_irq_type type,
		   irq_handler_t handler, unsigned long irqflags,
		   const char *devname, void *dev_id);
#ifdef CONFIG_UML_TIME_TRAVEL_SUPPORT
int um_request_irq_tt(int irq, int fd, enum um_irq_type type,
		      irq_handler_t handler, unsigned long irqflags,
		      const char *devname, void *dev_id,
		      void (*timetravel_handler)(int, int, void *,
						 struct time_travel_event *));
#else
static inline
int um_request_irq_tt(int irq, int fd, enum um_irq_type type,
		      irq_handler_t handler, unsigned long irqflags,
		      const char *devname, void *dev_id,
		      void (*timetravel_handler)(int, int, void *,
						 struct time_travel_event *))
{
	return um_request_irq(irq, fd, type, handler, irqflags,
			      devname, dev_id);
}
#endif
static inline bool um_irq_timetravel_handler_used(void)
{
	return time_travel_mode == TT_MODE_EXTERNAL;
}
void um_free_irq(int irq, void *dev_id);
void free_irqs(void);
#endif
