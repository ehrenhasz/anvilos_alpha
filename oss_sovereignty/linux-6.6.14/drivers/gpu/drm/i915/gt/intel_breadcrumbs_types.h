 
 

#ifndef __INTEL_BREADCRUMBS_TYPES__
#define __INTEL_BREADCRUMBS_TYPES__

#include <linux/irq_work.h>
#include <linux/kref.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "intel_engine_types.h"

 
struct intel_breadcrumbs {
	struct kref ref;
	atomic_t active;

	spinlock_t signalers_lock;  
	struct list_head signalers;
	struct llist_head signaled_requests;
	atomic_t signaler_active;

	spinlock_t irq_lock;  
	struct irq_work irq_work;  
	unsigned int irq_enabled;
	bool irq_armed;

	 
	intel_engine_mask_t	engine_mask;
	struct intel_engine_cs *irq_engine;
	bool	(*irq_enable)(struct intel_breadcrumbs *b);
	void	(*irq_disable)(struct intel_breadcrumbs *b);
};

#endif  
