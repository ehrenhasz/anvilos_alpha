 
 
#ifndef _KERNEL_WORKQUEUE_INTERNAL_H
#define _KERNEL_WORKQUEUE_INTERNAL_H

#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/preempt.h>

struct worker_pool;

 
struct worker {
	 
	union {
		struct list_head	entry;	 
		struct hlist_node	hentry;	 
	};

	struct work_struct	*current_work;	 
	work_func_t		current_func;	 
	struct pool_workqueue	*current_pwq;	 
	u64			current_at;	 
	unsigned int		current_color;	 

	int			sleeping;	 

	 
	work_func_t		last_func;	 

	struct list_head	scheduled;	 

	struct task_struct	*task;		 
	struct worker_pool	*pool;		 
						 
	struct list_head	node;		 
						 

	unsigned long		last_active;	 
	unsigned int		flags;		 
	int			id;		 

	 
	char			desc[WORKER_DESC_LEN];

	 
	struct workqueue_struct	*rescue_wq;	 
};

 
static inline struct worker *current_wq_worker(void)
{
	if (in_task() && (current->flags & PF_WQ_WORKER))
		return kthread_data(current);
	return NULL;
}

 
void wq_worker_running(struct task_struct *task);
void wq_worker_sleeping(struct task_struct *task);
void wq_worker_tick(struct task_struct *task);
work_func_t wq_worker_last_func(struct task_struct *task);

#endif  
