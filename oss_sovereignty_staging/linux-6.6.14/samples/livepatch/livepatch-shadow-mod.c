
 

 


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/workqueue.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Joe Lawrence <joe.lawrence@redhat.com>");
MODULE_DESCRIPTION("Buggy module for shadow variable demo");

 
#define ALLOC_PERIOD	1
 
#define CLEANUP_PERIOD	(3 * ALLOC_PERIOD)
 
#define EXPIRE_PERIOD	(4 * CLEANUP_PERIOD)

 
static LIST_HEAD(dummy_list);
static DEFINE_MUTEX(dummy_list_mutex);

struct dummy {
	struct list_head list;
	unsigned long jiffies_expire;
};

static __used noinline struct dummy *dummy_alloc(void)
{
	struct dummy *d;
	int *leak;

	d = kzalloc(sizeof(*d), GFP_KERNEL);
	if (!d)
		return NULL;

	d->jiffies_expire = jiffies +
		msecs_to_jiffies(1000 * EXPIRE_PERIOD);

	 
	leak = kzalloc(sizeof(*leak), GFP_KERNEL);
	if (!leak) {
		kfree(d);
		return NULL;
	}

	pr_info("%s: dummy @ %p, expires @ %lx\n",
		__func__, d, d->jiffies_expire);

	return d;
}

static __used noinline void dummy_free(struct dummy *d)
{
	pr_info("%s: dummy @ %p, expired = %lx\n",
		__func__, d, d->jiffies_expire);

	kfree(d);
}

static __used noinline bool dummy_check(struct dummy *d,
					   unsigned long jiffies)
{
	return time_after(jiffies, d->jiffies_expire);
}

 

static void alloc_work_func(struct work_struct *work);
static DECLARE_DELAYED_WORK(alloc_dwork, alloc_work_func);

static void alloc_work_func(struct work_struct *work)
{
	struct dummy *d;

	d = dummy_alloc();
	if (!d)
		return;

	mutex_lock(&dummy_list_mutex);
	list_add(&d->list, &dummy_list);
	mutex_unlock(&dummy_list_mutex);

	schedule_delayed_work(&alloc_dwork,
		msecs_to_jiffies(1000 * ALLOC_PERIOD));
}

 

static void cleanup_work_func(struct work_struct *work);
static DECLARE_DELAYED_WORK(cleanup_dwork, cleanup_work_func);

static void cleanup_work_func(struct work_struct *work)
{
	struct dummy *d, *tmp;
	unsigned long j;

	j = jiffies;
	pr_info("%s: jiffies = %lx\n", __func__, j);

	mutex_lock(&dummy_list_mutex);
	list_for_each_entry_safe(d, tmp, &dummy_list, list) {

		 
		if (dummy_check(d, j)) {
			list_del(&d->list);
			dummy_free(d);
		}
	}
	mutex_unlock(&dummy_list_mutex);

	schedule_delayed_work(&cleanup_dwork,
		msecs_to_jiffies(1000 * CLEANUP_PERIOD));
}

static int livepatch_shadow_mod_init(void)
{
	schedule_delayed_work(&alloc_dwork,
		msecs_to_jiffies(1000 * ALLOC_PERIOD));
	schedule_delayed_work(&cleanup_dwork,
		msecs_to_jiffies(1000 * CLEANUP_PERIOD));

	return 0;
}

static void livepatch_shadow_mod_exit(void)
{
	struct dummy *d, *tmp;

	 
	cancel_delayed_work_sync(&alloc_dwork);
	cancel_delayed_work_sync(&cleanup_dwork);

	 
	list_for_each_entry_safe(d, tmp, &dummy_list, list) {
		list_del(&d->list);
		dummy_free(d);
	}
}

module_init(livepatch_shadow_mod_init);
module_exit(livepatch_shadow_mod_exit);
