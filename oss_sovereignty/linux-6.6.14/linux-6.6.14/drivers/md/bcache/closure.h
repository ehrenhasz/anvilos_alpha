#ifndef _LINUX_CLOSURE_H
#define _LINUX_CLOSURE_H
#include <linux/llist.h>
#include <linux/sched.h>
#include <linux/sched/task_stack.h>
#include <linux/workqueue.h>
struct closure;
struct closure_syncer;
typedef void (closure_fn) (struct closure *);
extern struct dentry *bcache_debug;
struct closure_waitlist {
	struct llist_head	list;
};
enum closure_state {
	CLOSURE_BITS_START	= (1U << 26),
	CLOSURE_DESTRUCTOR	= (1U << 26),
	CLOSURE_WAITING		= (1U << 28),
	CLOSURE_RUNNING		= (1U << 30),
};
#define CLOSURE_GUARD_MASK					\
	((CLOSURE_DESTRUCTOR|CLOSURE_WAITING|CLOSURE_RUNNING) << 1)
#define CLOSURE_REMAINING_MASK		(CLOSURE_BITS_START - 1)
#define CLOSURE_REMAINING_INITIALIZER	(1|CLOSURE_RUNNING)
struct closure {
	union {
		struct {
			struct workqueue_struct *wq;
			struct closure_syncer	*s;
			struct llist_node	list;
			closure_fn		*fn;
		};
		struct work_struct	work;
	};
	struct closure		*parent;
	atomic_t		remaining;
#ifdef CONFIG_BCACHE_CLOSURES_DEBUG
#define CLOSURE_MAGIC_DEAD	0xc054dead
#define CLOSURE_MAGIC_ALIVE	0xc054a11e
	unsigned int		magic;
	struct list_head	all;
	unsigned long		ip;
	unsigned long		waiting_on;
#endif
};
void closure_sub(struct closure *cl, int v);
void closure_put(struct closure *cl);
void __closure_wake_up(struct closure_waitlist *list);
bool closure_wait(struct closure_waitlist *list, struct closure *cl);
void __closure_sync(struct closure *cl);
static inline void closure_sync(struct closure *cl)
{
	if ((atomic_read(&cl->remaining) & CLOSURE_REMAINING_MASK) != 1)
		__closure_sync(cl);
}
#ifdef CONFIG_BCACHE_CLOSURES_DEBUG
void closure_debug_init(void);
void closure_debug_create(struct closure *cl);
void closure_debug_destroy(struct closure *cl);
#else
static inline void closure_debug_init(void) {}
static inline void closure_debug_create(struct closure *cl) {}
static inline void closure_debug_destroy(struct closure *cl) {}
#endif
static inline void closure_set_ip(struct closure *cl)
{
#ifdef CONFIG_BCACHE_CLOSURES_DEBUG
	cl->ip = _THIS_IP_;
#endif
}
static inline void closure_set_ret_ip(struct closure *cl)
{
#ifdef CONFIG_BCACHE_CLOSURES_DEBUG
	cl->ip = _RET_IP_;
#endif
}
static inline void closure_set_waiting(struct closure *cl, unsigned long f)
{
#ifdef CONFIG_BCACHE_CLOSURES_DEBUG
	cl->waiting_on = f;
#endif
}
static inline void closure_set_stopped(struct closure *cl)
{
	atomic_sub(CLOSURE_RUNNING, &cl->remaining);
}
static inline void set_closure_fn(struct closure *cl, closure_fn *fn,
				  struct workqueue_struct *wq)
{
	closure_set_ip(cl);
	cl->fn = fn;
	cl->wq = wq;
	smp_mb__before_atomic();
}
static inline void closure_queue(struct closure *cl)
{
	struct workqueue_struct *wq = cl->wq;
	BUILD_BUG_ON(offsetof(struct closure, fn)
		     != offsetof(struct work_struct, func));
	if (wq) {
		INIT_WORK(&cl->work, cl->work.func);
		BUG_ON(!queue_work(wq, &cl->work));
	} else
		cl->fn(cl);
}
static inline void closure_get(struct closure *cl)
{
#ifdef CONFIG_BCACHE_CLOSURES_DEBUG
	BUG_ON((atomic_inc_return(&cl->remaining) &
		CLOSURE_REMAINING_MASK) <= 1);
#else
	atomic_inc(&cl->remaining);
#endif
}
static inline void closure_init(struct closure *cl, struct closure *parent)
{
	memset(cl, 0, sizeof(struct closure));
	cl->parent = parent;
	if (parent)
		closure_get(parent);
	atomic_set(&cl->remaining, CLOSURE_REMAINING_INITIALIZER);
	closure_debug_create(cl);
	closure_set_ip(cl);
}
static inline void closure_init_stack(struct closure *cl)
{
	memset(cl, 0, sizeof(struct closure));
	atomic_set(&cl->remaining, CLOSURE_REMAINING_INITIALIZER);
}
static inline void closure_wake_up(struct closure_waitlist *list)
{
	smp_mb();
	__closure_wake_up(list);
}
#define continue_at(_cl, _fn, _wq)					\
do {									\
	set_closure_fn(_cl, _fn, _wq);					\
	closure_sub(_cl, CLOSURE_RUNNING + 1);				\
} while (0)
#define closure_return(_cl)	continue_at((_cl), NULL, NULL)
#define continue_at_nobarrier(_cl, _fn, _wq)				\
do {									\
	set_closure_fn(_cl, _fn, _wq);					\
	closure_queue(_cl);						\
} while (0)
#define closure_return_with_destructor(_cl, _destructor)		\
do {									\
	set_closure_fn(_cl, _destructor, NULL);				\
	closure_sub(_cl, CLOSURE_RUNNING - CLOSURE_DESTRUCTOR + 1);	\
} while (0)
static inline void closure_call(struct closure *cl, closure_fn fn,
				struct workqueue_struct *wq,
				struct closure *parent)
{
	closure_init(cl, parent);
	continue_at_nobarrier(cl, fn, wq);
}
#endif  
