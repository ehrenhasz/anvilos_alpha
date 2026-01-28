#ifndef _SECURITY_LANDLOCK_OBJECT_H
#define _SECURITY_LANDLOCK_OBJECT_H
#include <linux/compiler_types.h>
#include <linux/refcount.h>
#include <linux/spinlock.h>
struct landlock_object;
struct landlock_object_underops {
	void (*release)(struct landlock_object *const object)
		__releases(object->lock);
};
struct landlock_object {
	refcount_t usage;
	spinlock_t lock;
	void *underobj;
	union {
		struct rcu_head rcu_free;
		const struct landlock_object_underops *underops;
	};
};
struct landlock_object *
landlock_create_object(const struct landlock_object_underops *const underops,
		       void *const underobj);
void landlock_put_object(struct landlock_object *const object);
static inline void landlock_get_object(struct landlock_object *const object)
{
	if (object)
		refcount_inc(&object->usage);
}
#endif  
