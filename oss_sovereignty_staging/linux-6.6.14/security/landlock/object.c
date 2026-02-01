
 

#include <linux/bug.h>
#include <linux/compiler_types.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/rcupdate.h>
#include <linux/refcount.h>
#include <linux/slab.h>
#include <linux/spinlock.h>

#include "object.h"

struct landlock_object *
landlock_create_object(const struct landlock_object_underops *const underops,
		       void *const underobj)
{
	struct landlock_object *new_object;

	if (WARN_ON_ONCE(!underops || !underobj))
		return ERR_PTR(-ENOENT);
	new_object = kzalloc(sizeof(*new_object), GFP_KERNEL_ACCOUNT);
	if (!new_object)
		return ERR_PTR(-ENOMEM);
	refcount_set(&new_object->usage, 1);
	spin_lock_init(&new_object->lock);
	new_object->underops = underops;
	new_object->underobj = underobj;
	return new_object;
}

 
void landlock_put_object(struct landlock_object *const object)
{
	 
	might_sleep();
	if (!object)
		return;

	 
	if (refcount_dec_and_lock(&object->usage, &object->lock)) {
		__acquire(&object->lock);
		 
		object->underops->release(object);
		kfree_rcu(object, rcu_free);
	}
}
